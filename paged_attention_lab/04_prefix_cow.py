"""第 4 步：前缀共享 + 写时复制（COW）处理分支解码。"""

from __future__ import annotations

from paged_kv import BlockPool, PagedKVCache, SequenceState
from utils import format_vec, make_rng, random_vector


def summarize_refs(pool: BlockPool, block_ids: list[int]) -> str:
    parts = []
    for block_id in block_ids:
        block = pool.blocks[block_id]
        parts.append(f"P{block_id}(ref={block.ref_count}, used={block.used_slots})")
    return ", ".join(parts)


def token_storage_cost(pool: BlockPool) -> int:
    return sum(block.used_slots for block in pool.blocks if block.ref_count > 0)


def print_layout(cache: PagedKVCache, seq: SequenceState) -> None:
    print(f"{seq.name:5s}: {cache.sequence_layout(seq)}")


def main() -> None:
    rng = make_rng(19)
    head_dim = 8
    block_size = 4

    pool = BlockPool(num_blocks=16, block_size=block_size)
    cache = PagedKVCache(pool)

    base = cache.create_sequence("base")

    # 构造：1 个满 block + 1 个部分填充 block。
    prompt_len = 6
    for t in range(prompt_len):
        key = random_vector(rng, head_dim)
        value = random_vector(rng, head_dim)
        cache.append(base, key, value, token_id=1000 + t)

    print("=== 第 4 步：Prefix Sharing 与 COW ===")
    print("base 提示词布局:")
    print_layout(cache, base)
    print("refs:", summarize_refs(pool, base.block_table))
    print()

    # beam 分支：先共享同一份前缀 block。
    beam1 = cache.fork(base, "beam1")
    beam2 = cache.fork(base, "beam2")
    print("fork 后（全部 block 共享）:")
    print_layout(cache, base)
    print_layout(cache, beam1)
    print_layout(cache, beam2)
    print("refs:", summarize_refs(pool, base.block_table))
    print()

    # 共享尾块上要写新 token，需触发 COW，避免分支互相覆盖。
    cache.append(
        beam1,
        random_vector(rng, head_dim),
        random_vector(rng, head_dim),
        token_id=2001,
        enable_cow=True,
    )
    cache.append(
        beam2,
        random_vector(rng, head_dim),
        random_vector(rng, head_dim),
        token_id=2002,
        enable_cow=True,
    )

    print("每个分支各 append 1 token 后（共享尾块触发 COW）:")
    print_layout(cache, base)
    print_layout(cache, beam1)
    print_layout(cache, beam2)

    tracked_blocks = sorted(set(base.block_table + beam1.block_table + beam2.block_table))
    print("refs:", summarize_refs(pool, tracked_blocks))

    # 第 0 块仍共享；第 1 块被分裂成 3 份（base/beam1/beam2 各一份）。
    assert base.block_table[0] == beam1.block_table[0] == beam2.block_table[0]
    assert len({base.block_table[1], beam1.block_table[1], beam2.block_table[1]}) == 3

    shared_storage = token_storage_cost(pool)
    no_share_storage = base.token_count + beam1.token_count + beam2.token_count
    print()
    print(f"启用共享时实际存储 KV slots: {shared_storage}")
    print(f"完全不共享时需要 KV slots:    {no_share_storage}")
    print(f"节省 slots:                  {no_share_storage - shared_storage}")

    query = random_vector(rng, head_dim)
    out_base, _, _ = cache.decode_attention(query, base)
    out_beam1, _, _ = cache.decode_attention(query, beam1)
    out_beam2, _, _ = cache.decode_attention(query, beam2)

    print()
    print("同一个 query 在不同分支的 decode 输出:")
    print("base :", format_vec(out_base))
    print("beam1:", format_vec(out_beam1))
    print("beam2:", format_vec(out_beam2))
    print("结论：前缀共享省内存，COW 保证分支写入隔离。")


if __name__ == "__main__":
    main()
