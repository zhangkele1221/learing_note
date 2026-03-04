"""第 1 步：理解 block table（逻辑块到物理块映射）。

这个脚本只演示“映射关系”，不存真实 K/V 向量。
重点看清三件事：
1) token 在逻辑上按时间连续追加；
2) 每 `block_size` 个 token 组成一个 logical block；
3) 每个 logical block 会映射到一个 physical block（通过 block_table）。

图解（示例：block_size = 4）

    seq_a token 时间线（逻辑连续）
    t0   t1   t2   t3 | t4   t5
     |    |    |    |   |    |
     v    v    v    v   v    v
    L0  L0   L0   L0  L1   L1        (L = logical block)
     |   |    |    |   |    |
     |   |    |    |   |    +--> P2 slot1
     |   |    |    |   +-------> P2 slot0
     +---+----+----+----------> P7 slot0..3

    block_table(seq_a) = [7, 2]
      - L0 -> P7
      - L1 -> P2

也就是说：逻辑上连续，物理上可离散。
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field


@dataclass
class SequenceBlocks:
    # 序列名，便于打印区分。
    name: str
    # block_table[i] = 该序列第 i 个 logical block 对应的 physical block id。
    block_table: list[int] = field(default_factory=list)
    # 当前序列已追加 token 总数（也是下一个 token 的逻辑位置）。
    token_count: int = 0


class BlockAllocator:
    def __init__(self, num_blocks: int):
        # 空闲物理块队列：先进先出分配。
        self.free = deque(range(num_blocks))

    def allocate(self) -> int:
        # 模拟“向全局池子申请一个空闲物理块”。
        if not self.free:
            raise RuntimeError("Out of blocks")
        return self.free.popleft()


def append_token(
    seq: SequenceBlocks,
    token_id: int,
    allocator: BlockAllocator,
    block_size: int,
) -> tuple[int, int, int]:
    # 如果当前 token 正好落在新 logical block 的开头，则要先分配物理块。
    # 例如 block_size=4 时，token_count 为 0/4/8/... 都是新块起点。
    if seq.token_count % block_size == 0:
        seq.block_table.append(allocator.allocate())

    # logical_block: 这个 token 属于序列的第几个逻辑块。
    logical_block = seq.token_count // block_size
    # slot: 这个 token 在逻辑块(也是物理块)内的偏移位置。
    slot = seq.token_count % block_size
    # 通过 block_table 把 logical block 翻译成 physical block。
    physical_block = seq.block_table[logical_block]

    # 追加完成后，token 总数 +1。
    seq.token_count += 1
    return logical_block, slot, physical_block


def main() -> None:
    # 每个 block 最多容纳 4 个 token。
    block_size = 4
    # 全局共有 8 个可分配的物理块。
    allocator = BlockAllocator(num_blocks=8)

    seq_a = SequenceBlocks(name="seq_a")
    seq_b = SequenceBlocks(name="seq_b")

    print("=== 第 1 步：Block Table 基础 ===")
    print(f"block_size={block_size}")
    print()

    # 交错追加两个序列的 token，模拟线上多请求并发到来。
    stream = [
        (seq_a, 101),
        (seq_a, 102),
        (seq_a, 103),
        (seq_b, 201),
        (seq_b, 202),
        (seq_a, 104),
        (seq_a, 105),
        (seq_b, 203),
        (seq_b, 204),
        (seq_b, 205),
    ]

    for seq, token_id in stream:
        l_block, slot, p_block = append_token(seq, token_id, allocator, block_size)
        print(
            f"append token={token_id} to {seq.name:5s} -> "
            f"logical_block={l_block}, slot={slot}, physical_block={p_block}"
        )

    print()
    print(f"{seq_a.name} block_table: {seq_a.block_table}")
    print(f"{seq_b.name} block_table: {seq_b.block_table}")
    print()
    # 观察输出：同一序列逻辑连续，但物理块并不要求连续。
    print("结论：序列在逻辑上连续，但其每段逻辑块可映射到不同物理块。")


if __name__ == "__main__":
    main()
