"""第 2 步：把真实 K/V 写入分页缓存，并验证逻辑重建。

图解（写入路径）

    新 token 的 (K, V)
          |
          v
    token_count -> (logical_block, slot)
          |
          v
    block_table[logical_block] -> physical_block
          |
          v
    写入 physical_block[slot] = (K, V)

图解（重建路径 materialize）

    按 logical block 顺序遍历 block_table
          |
          v
    对每个 physical_block 按 slot 从 0..used_slots-1 读取
          |
          v
    拼回逻辑顺序 keys/values，与 dense 历史一一对比

核心目标：证明“分页存储”不会破坏“逻辑顺序”。
"""

from __future__ import annotations

from paged_kv import BlockPool, PagedKVCache, SequenceState
from utils import format_vec, l2_distance, make_rng, random_vector


def verify_materialize(
    cache: PagedKVCache,
    seq: SequenceState,
    dense_history: list[tuple[list[float], list[float]]],
) -> None:
    keys, values = cache.materialize(seq)
    if len(keys) != len(dense_history):
        raise AssertionError("Token count mismatch")

    total_error = 0.0
    for i, ((k_dense, v_dense), k_page, v_page) in enumerate(zip(dense_history, keys, values)):
        k_err = l2_distance(k_dense, k_page)
        v_err = l2_distance(v_dense, v_page)
        total_error += (k_err + v_err)
        if k_err > 1e-9 or v_err > 1e-9:
            raise AssertionError(f"Mismatch at token {i}: k_err={k_err}, v_err={v_err}")

    print(f"校验 {seq.name}: 分页重建 KV 与 dense 历史一致 (total_error={total_error:.2e})")


def main() -> None:
    rng = make_rng(7)
    head_dim = 8
    block_size = 4

    pool = BlockPool(num_blocks=12, block_size=block_size)
    cache = PagedKVCache(pool)
    seq_a = cache.create_sequence("seq_a")
    seq_b = cache.create_sequence("seq_b")

    dense: dict[str, list[tuple[list[float], list[float]]]] = {"seq_a": [], "seq_b": []}

    schedule = [
        (seq_a, 11),
        (seq_a, 12),
        (seq_b, 21),
        (seq_a, 13),
        (seq_b, 22),
        (seq_b, 23),
        (seq_a, 14),
        (seq_a, 15),
        (seq_b, 24),
        (seq_b, 25),
    ]

    print("=== 第 2 步：Paged KV Cache 写入 ===")
    for seq, token_id in schedule:
        key = random_vector(rng, head_dim)
        value = random_vector(rng, head_dim)
        cache.append(seq, key, value, token_id=token_id)
        dense[seq.name].append((key, value))

        print(f"append token={token_id} to {seq.name:5s} -> layout: {cache.sequence_layout(seq)}")

    print()
    print("seq_a 第 0 个 token 的 key:", format_vec(dense["seq_a"][0][0]))
    print("seq_b 第 2 个 token 的 value:", format_vec(dense["seq_b"][2][1]))
    print()

    verify_materialize(cache, seq_a, dense["seq_a"])
    verify_materialize(cache, seq_b, dense["seq_b"])

    print("结论：block_table + slot 足以重建逻辑顺序 KV。")


if __name__ == "__main__":
    main()
