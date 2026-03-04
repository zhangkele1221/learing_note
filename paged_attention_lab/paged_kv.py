"""教学版 paged KV cache。

刻意保持小而清晰（CPU-only），便于理解：
- 固定大小 physical block
- 每个序列维护 logical->physical 的 block_table
- 共享 block 上写入前可触发 copy-on-write (COW)

数据流总览：

    append(K,V)
      -> token_count 计算 (logical_block, slot)
      -> block_table 映射到 physical_block
      -> 写入 block[slot]

    decode(query)
      -> iter_kv 按逻辑顺序读出历史 KV
      -> score -> softmax -> 加权求和
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
import math
from typing import Iterator

from utils import add_scaled, dot, softmax


@dataclass
class KVBlock:
    """一个物理 KV 块（固定容量）。"""

    # 物理块编号。
    block_id: int
    # 块容量：最多可存多少个 token 的 K/V。
    block_size: int
    # 被多少序列引用（用于共享与回收）。
    ref_count: int = 0
    # 当前已写入的 slot 数量，范围 [0, block_size]。
    used_slots: int = 0
    # keys[slot] = 该位置 token 的 key 向量。
    keys: list[list[float] | None] = field(default_factory=list)
    # values[slot] = 该位置 token 的 value 向量。
    values: list[list[float] | None] = field(default_factory=list)
    # 可选：保存 token id，便于调试展示。
    token_ids: list[int | None] = field(default_factory=list)

    def __post_init__(self) -> None:
        # 初始化为“全空槽”。
        self.keys = [None for _ in range(self.block_size)]
        self.values = [None for _ in range(self.block_size)]
        self.token_ids = [None for _ in range(self.block_size)]

    def reset_for_allocate(self) -> None:
        # 分配时复位：视为一块全新的独占块（ref_count=1）。
        self.ref_count = 1
        self.used_slots = 0
        for i in range(self.block_size):
            self.keys[i] = None
            self.values[i] = None
            self.token_ids[i] = None


class BlockPool:
    """全局物理块池：负责分配、回收、引用计数与 COW 复制。"""

    def __init__(self, num_blocks: int, block_size: int):
        self.block_size = block_size
        # 预先创建固定数量的物理块。
        self.blocks: list[KVBlock] = [KVBlock(block_id=i, block_size=block_size) for i in range(num_blocks)]
        # 空闲块队列（FIFO 仅用于教学，真实系统可更复杂）。
        self.free_block_ids: deque[int] = deque(range(num_blocks))

    def allocate_block(self) -> int:
        """申请一个空闲物理块。"""
        if not self.free_block_ids:
            raise RuntimeError("Out of KV blocks")
        block_id = self.free_block_ids.popleft()
        self.blocks[block_id].reset_for_allocate()
        return block_id

    def inc_ref(self, block_id: int) -> None:
        """共享该块时引用计数 +1。"""
        block = self.blocks[block_id]
        block.ref_count += 1

    def dec_ref(self, block_id: int) -> None:
        """放弃引用时计数 -1，归零则回收到空闲队列。"""
        block = self.blocks[block_id]
        if block.ref_count <= 0:
            raise RuntimeError(f"Block {block_id} has invalid ref_count {block.ref_count}")
        block.ref_count -= 1
        if block.ref_count == 0:
            self.free_block_ids.append(block_id)

    def clone_block(self, src_block_id: int) -> int:
        """复制一个 block（用于 COW），只复制已写入的 slots。"""
        src = self.blocks[src_block_id]
        dst_block_id = self.allocate_block()
        dst = self.blocks[dst_block_id]
        dst.used_slots = src.used_slots
        for i in range(src.used_slots):
            dst.keys[i] = list(src.keys[i]) if src.keys[i] is not None else None
            dst.values[i] = list(src.values[i]) if src.values[i] is not None else None
            dst.token_ids[i] = src.token_ids[i]
        return dst_block_id

    def write(
        self,
        block_id: int,
        slot: int,
        key: list[float],
        value: list[float],
        token_id: int | None = None,
    ) -> None:
        """向指定物理块 slot 写入 K/V。"""
        block = self.blocks[block_id]
        block.keys[slot] = list(key)
        block.values[slot] = list(value)
        block.token_ids[slot] = token_id
        if slot + 1 > block.used_slots:
            block.used_slots = slot + 1

    def read(self, block_id: int, slot: int) -> tuple[list[float], list[float]]:
        """读取指定物理块 slot 的 K/V。"""
        block = self.blocks[block_id]
        key = block.keys[slot]
        value = block.values[slot]
        if key is None or value is None:
            raise RuntimeError(f"Uninitialized KV slot block={block_id}, slot={slot}")
        return key, value


@dataclass
class SequenceState:
    """序列级状态：逻辑视角下的 KV 布局信息。"""

    # 序列标识（日志与调试使用）。
    name: str
    # block_table[i] = 第 i 个 logical block 对应的 physical block id。
    block_table: list[int] = field(default_factory=list)
    # 当前序列 token 总数。
    token_count: int = 0


class PagedKVCache:
    """教学版 paged KV cache 主接口。"""

    def __init__(self, pool: BlockPool):
        self.pool = pool
        self.block_size = pool.block_size

    def create_sequence(self, name: str) -> SequenceState:
        """创建一个空序列。"""
        return SequenceState(name=name)

    def fork(self, parent: SequenceState, child_name: str) -> SequenceState:
        """子序列共享父序列 block_table 指向的物理块。"""
        # 分支创建时，不复制数据，只提升引用计数并复制表项。
        for block_id in parent.block_table:
            self.pool.inc_ref(block_id)
        return SequenceState(name=child_name, block_table=list(parent.block_table), token_count=parent.token_count)

    def append(
        self,
        seq: SequenceState,
        key: list[float],
        value: list[float],
        token_id: int | None = None,
        enable_cow: bool = False,
    ) -> None:
        """向序列末尾追加一个 token 的 K/V。"""
        # 新 block 边界：直接分配新物理块。
        if seq.token_count % self.block_size == 0:
            block_id = self.pool.allocate_block()
            seq.block_table.append(block_id)
        else:
            block_id = seq.block_table[-1]
            # 写共享尾块前执行 COW，保证分支隔离。
            if enable_cow and self.pool.blocks[block_id].ref_count > 1:
                copied_block = self.pool.clone_block(block_id)
                # 当前序列改指向新块后，对旧共享块少一个引用。
                self.pool.dec_ref(block_id)
                seq.block_table[-1] = copied_block
                block_id = copied_block

        # slot 由 token_count 决定：append 只能写在当前尾部位置。
        slot = seq.token_count % self.block_size
        self.pool.write(block_id, slot, key, value, token_id=token_id)
        seq.token_count += 1

    def iter_kv(self, seq: SequenceState) -> Iterator[tuple[int, int, list[float], list[float]]]:
        """按逻辑 token 顺序产出 (block_id, slot, K, V)。"""
        # remaining 防止最后一个块是“部分填充块”时多读。
        remaining = seq.token_count
        for block_id in seq.block_table:
            block = self.pool.blocks[block_id]
            take = min(block.used_slots, remaining)
            for slot in range(take):
                key, value = self.pool.read(block_id, slot)
                yield block_id, slot, key, value
            remaining -= take
            if remaining == 0:
                break

    def materialize(self, seq: SequenceState) -> tuple[list[list[float]], list[list[float]]]:
        """把分页布局展开成连续 keys/values（用于校验与教学）。"""
        keys: list[list[float]] = []
        values: list[list[float]] = []
        for _, _, key, value in self.iter_kv(seq):
            keys.append(key)
            values.append(value)
        return keys, values

    def decode_attention(
        self,
        query: list[float],
        seq: SequenceState,
    ) -> tuple[list[float], list[float], list[tuple[int, int]]]:
        """对单个 query 做 decode attention（返回输出、权重、KV位置）。"""
        entries = list(self.iter_kv(seq))
        if not entries:
            raise ValueError("decode_attention requires at least one KV token")

        # 数学与 dense attention 一致，差异仅在 KV 的读取路径。
        scale = 1.0 / math.sqrt(len(query))
        scores: list[float] = []
        token_locations: list[tuple[int, int]] = []
        values: list[list[float]] = []

        for block_id, slot, key, value in entries:
            scores.append(dot(query, key) * scale)
            # 记录每个历史 token 实际来自哪个物理块位置，便于可视化。
            token_locations.append((block_id, slot))
            values.append(value)

        weights = softmax(scores)
        out = [0.0 for _ in range(len(values[0]))]
        for weight, value in zip(weights, values):
            add_scaled(out, value, weight)
        return out, weights, token_locations

    def sequence_layout(self, seq: SequenceState) -> str:
        """返回可读布局字符串：Lx->Py(tokens=?, ref=?)。"""
        parts: list[str] = []
        remaining = seq.token_count
        for logical_id, block_id in enumerate(seq.block_table):
            block = self.pool.blocks[block_id]
            take = min(block.used_slots, remaining)
            parts.append(f"L{logical_id}->P{block_id} (tokens={take}, ref={block.ref_count})")
            remaining -= take
            if remaining == 0:
                break
        return " | ".join(parts)
