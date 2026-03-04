"""第 0 步：Dense 注意力基线。

先确认注意力数学，再引入分页存储。
"""

from __future__ import annotations

from utils import attention_one_query, format_vec, make_rng, random_vector


def main() -> None:
    # 固定随机种子，保证每次运行输出一致，便于教学对照。
    rng = make_rng(2026)

    # head_dim: 每个注意力头向量维度。
    head_dim = 8
    # history_len: 当前 query 可见的历史 token 数（上下文长度）。
    history_len = 6

    # history_keys/history_values 形状都可理解为 [history_len, head_dim]。
    history_keys = [random_vector(rng, head_dim) for _ in range(history_len)]
    history_values = [random_vector(rng, head_dim) for _ in range(history_len)]

    # current_query 形状是 [head_dim]，表示“当前 token 想从历史里取什么信息”。
    current_query = random_vector(rng, head_dim)

    # context_vector: 注意力输出向量，形状 [head_dim]。
    # attn_weights: 每个历史 token 的注意力权重，形状 [history_len]。
    context_vector, attn_weights = attention_one_query(
        current_query,
        history_keys,
        history_values,
    )

    print("=== 第 0 步：Dense Attention 基线 ===")
    print(f"head_dim={head_dim}, history_len={history_len}")
    print(
        "shapes:",
        f"query=({len(current_query)},)",
        f"keys=({len(history_keys)}, {len(history_keys[0])})",
        f"values=({len(history_values)}, {len(history_values[0])})",
        f"weights=({len(attn_weights)},)",
        f"out=({len(context_vector)},)",
    )
    print("query:", format_vec(current_query))
    print("第一个 key:", format_vec(history_keys[0]))
    print("第一个 value:", format_vec(history_values[0]))
    print("attention 权重:", [round(x, 4) for x in attn_weights])
    print("输出向量:", format_vec(context_vector, max_items=8))
    print()
    print("结论：Dense attention 把每个序列的全部 KV 连续放在一起。")
    print("PagedAttention 不改变数学公式，只改变 KV 的内存布局与访问方式。")


if __name__ == "__main__":
    main()
