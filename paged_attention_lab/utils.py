"""PagedAttention 教学脚本使用的数学辅助函数。"""

from __future__ import annotations

import math
import random
from typing import Iterable


def make_rng(seed: int) -> random.Random:
    return random.Random(seed)


def random_vector(rng: random.Random, dim: int, scale: float = 1.0) -> list[float]:
    return [(rng.random() * 2.0 - 1.0) * scale for _ in range(dim)]


def dot(a: Iterable[float], b: Iterable[float]) -> float:
    return sum(x * y for x, y in zip(a, b))


def softmax(values: list[float]) -> list[float]:
    if not values:
        return []
    # 数值稳定写法：先减最大值再做 exp。
    max_value = max(values)
    exps = [math.exp(v - max_value) for v in values]
    total = sum(exps)
    return [e / total for e in exps]


def add_scaled(target: list[float], source: list[float], scale: float) -> None:
    for i in range(len(target)):
        target[i] += source[i] * scale


def l2_distance(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def format_vec(v: list[float], max_items: int = 10) -> str:
    clipped = v[:max_items]
    body = ", ".join(f"{x:+.4f}" for x in clipped)
    if len(v) > max_items:
        body += ", ..."
    return f"[{body}]"


def attention_one_query(
    query: list[float],
    keys: list[list[float]],
    values: list[list[float]],
) -> tuple[list[float], list[float]]:
    """计算单个 query 对整个历史 KV 的注意力。

    参数形状：
    - query: [head_dim]
    - keys: [seq_len, head_dim]
    - values: [seq_len, head_dim]

    返回：
    - out: [head_dim]
    - weights: [seq_len]，并且 sum(weights) == 1
    """
    if not keys:
        raise ValueError("keys must not be empty")
    if len(keys) != len(values):
        raise ValueError("keys/values length mismatch")

    # scale 对应公式里的 1/sqrt(d)，d 即 head_dim。
    scale = 1.0 / math.sqrt(len(query))

    # scores[i] = dot(query, keys[i]) * scale
    scores = [dot(query, key_i) * scale for key_i in keys]

    # 对所有历史 token 的打分做 softmax，得到权重。
    weights = softmax(scores)

    # out = sum_i weights[i] * values[i]
    out = [0.0 for _ in range(len(values[0]))]
    for weight_i, value_i in zip(weights, values):
        add_scaled(out, value_i, weight_i)
    return out, weights
