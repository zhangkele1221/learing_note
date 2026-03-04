"""第 5 步：基于哈希链的 prefix cache（完整 block 复用）。

对应 vLLM / nano-vllm 的核心思路：
完整 block 的哈希 = hash(父前缀哈希, 当前 block token_ids)。
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
import hashlib


@dataclass
class PrefixBlock:
    block_id: int
    ref_count: int = 0
    hash_key: int = -1
    token_ids: list[int] = field(default_factory=list)


@dataclass
class PrefixSequence:
    name: str
    token_ids: list[int]
    block_table: list[int] = field(default_factory=list)
    num_cached_tokens: int = 0


class PrefixBlockManager:
    def __init__(self, num_blocks: int, block_size: int):
        self.block_size = block_size
        self.blocks = [PrefixBlock(i) for i in range(num_blocks)]
        self.free_ids = deque(range(num_blocks))
        self.used_ids: set[int] = set()
        self.hash_to_block_id: dict[int, int] = {}

    @staticmethod
    def compute_hash(token_ids: list[int], prefix_hash: int = -1) -> int:
        h = hashlib.blake2b(digest_size=8)
        if prefix_hash != -1:
            h.update(prefix_hash.to_bytes(8, "little", signed=False))
        for token in token_ids:
            h.update(token.to_bytes(4, "little", signed=False))
        return int.from_bytes(h.digest(), "little", signed=False)

    def _allocate(self, block_id: int) -> PrefixBlock:
        if block_id not in self.free_ids:
            raise RuntimeError(f"Block {block_id} is not free")
        self.free_ids.remove(block_id)
        self.used_ids.add(block_id)
        block = self.blocks[block_id]
        block.ref_count = 1
        block.hash_key = -1
        block.token_ids = []
        return block

    def allocate(self, seq: PrefixSequence) -> None:
        prefix_hash = -1
        cache_miss = False

        for i in range((len(seq.token_ids) + self.block_size - 1) // self.block_size):
            start = i * self.block_size
            end = min(start + self.block_size, len(seq.token_ids))
            block_tokens = seq.token_ids[start:end]

            full_block = len(block_tokens) == self.block_size
            block_hash = self.compute_hash(block_tokens, prefix_hash) if full_block else -1

            cached_block_id = self.hash_to_block_id.get(block_hash, -1)
            if (
                full_block
                and not cache_miss
                and cached_block_id != -1
                and self.blocks[cached_block_id].token_ids == block_tokens
            ):
                # 命中前缀缓存：复用已有物理块。
                seq.num_cached_tokens += self.block_size
                if cached_block_id in self.used_ids:
                    self.blocks[cached_block_id].ref_count += 1
                    block_id = cached_block_id
                else:
                    block = self._allocate(cached_block_id)
                    block.hash_key = block_hash
                    block.token_ids = list(block_tokens)
                    block_id = block.block_id
            else:
                # 出现 miss 后，后续块不能再继续 prefix 命中。
                cache_miss = True
                if not self.free_ids:
                    raise RuntimeError("Out of blocks")
                block_id = self.free_ids[0]
                block = self._allocate(block_id)
                if full_block:
                    block.hash_key = block_hash
                    block.token_ids = list(block_tokens)
                    self.hash_to_block_id[block_hash] = block_id
                else:
                    block.hash_key = -1
                    block.token_ids = list(block_tokens)

            seq.block_table.append(block_id)
            prefix_hash = block_hash


def main() -> None:
    manager = PrefixBlockManager(num_blocks=12, block_size=4)

    seq_a = PrefixSequence(name="seq_a", token_ids=[1, 2, 3, 4, 5, 6, 7, 8, 9])
    seq_b = PrefixSequence(name="seq_b", token_ids=[1, 2, 3, 4, 5, 6, 7, 8, 42])
    seq_c = PrefixSequence(name="seq_c", token_ids=[1, 2, 3, 4, 100, 101, 102, 103])

    manager.allocate(seq_a)
    manager.allocate(seq_b)
    manager.allocate(seq_c)

    print("=== 第 5 步：Prefix Cache 复用 ===")
    print(f"{seq_a.name}: block_table={seq_a.block_table}, cached={seq_a.num_cached_tokens}")
    print(f"{seq_b.name}: block_table={seq_b.block_table}, cached={seq_b.num_cached_tokens}")
    print(f"{seq_c.name}: block_table={seq_c.block_table}, cached={seq_c.num_cached_tokens}")

    print()
    print("seq_b 复用了 seq_a 的前两个完整 block，最后一个尾块不同。")
    print("seq_c 仅复用了第一个完整 block，因为第二个 block token 已经变化。")


if __name__ == "__main__":
    main()
