"""第 3 步：按分页 KV 读取路径做 decode attention。

图解（paged decode）

    query q
      |
      v
    按 block_table 顺序读取历史 KV
      |
      v
    scores[i] = dot(q, k_i) / sqrt(d)
      |
      v
    weights = softmax(scores)
      |
      v
    out = sum_i weights[i] * v_i

图解（本脚本要验证的事）

    paged 路径输出 out_paged  ----- compare -----  out_dense（直接顺序 KV）
    paged 路径权重 w_paged   ----- compare -----  w_dense

如果两条路径读取到的是同一逻辑 KV 序列，则二者应一致。
"""

from __future__ import annotations

from paged_kv import BlockPool, PagedKVCache
from utils import attention_one_query, format_vec, l2_distance, make_rng, random_vector


def main() -> None:
    rng = make_rng(11)
    head_dim = 8
    block_size = 4

    pool = BlockPool(num_blocks=8, block_size=block_size)
    cache = PagedKVCache(pool)
    seq = cache.create_sequence("decode_seq")

    dense_keys: list[list[float]] = []
    dense_values: list[list[float]] = []

    print("=== 第 3 步：Paged Decode Attention ===")
    for step in range(1, 9):
        key = random_vector(rng, head_dim)
        value = random_vector(rng, head_dim)
        cache.append(seq, key, value, token_id=100 + step)
        dense_keys.append(key)
        dense_values.append(value)

        query = random_vector(rng, head_dim)

        # paged 路径：通过 block_table 读取 KV。
        paged_out, paged_weights, token_locs = cache.decode_attention(query, seq)
        # dense 路径：直接顺序读取 KV。
        dense_out, dense_weights = attention_one_query(query, dense_keys, dense_values)

        out_err = l2_distance(paged_out, dense_out)
        wt_err = l2_distance(paged_weights, dense_weights)
        if out_err > 1e-9 or wt_err > 1e-9:
            raise AssertionError(f"Mismatch at step={step}: out_err={out_err}, wt_err={wt_err}")

        preview_locs = token_locs[:4]
        preview = f"{preview_locs} ..." if len(token_locs) > 4 else str(preview_locs)

        print(
            f"step={step:2d} tokens={len(dense_keys):2d} "
            f"out_err={out_err:.2e} wt_err={wt_err:.2e} "
            f"locs={preview}"
        )

    print()
    print("最终输出:", format_vec(paged_out, max_items=8))
    print("结论：分页改变的是访问路径，不是注意力数学。")


if __name__ == "__main__":
    main()
