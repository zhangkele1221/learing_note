# PagedAttention demo实验（vLLM 思路版）

这套实验是一个从零实现、仅 CPU 的学习版本，用来讲清楚 vLLM 类系统里
PagedAttention 的核心设计。

目标不是极致性能，而是把关键数据结构、状态变化和正确性约束讲透。

## 能学到什么

1. 为什么按序列连续分配 KV Cache 在在线推理场景里会浪费显存。
2. 为什么“固定大小块（page/block）+ block table”可以解耦逻辑顺序和物理位置。
3. 为什么 decode attention 在分页布局下仍与 dense attention 数学等价。
4. 前缀共享（prefix sharing）+ 写时复制（COW）如何支持分支解码（如 beam）。
5. 基于哈希链的 prefix cache 如何跨请求复用完整 block。

## 文件与学习顺序

- `00_dense_attention.py`
  - Dense 注意力基线，先对齐数学。
- `01_block_table.py`
  - 逻辑 block 到物理 block 的映射。
- `02_paged_kv_cache.py`
  - 把真实 K/V 写入分页缓存，并验证可重建。
- `03_paged_decode.py`
  - 分页 decode 与 dense 结果逐步对比。
- `04_prefix_cow.py`
  - 前缀共享与 COW（分支写入隔离）。
- `05_prefix_cache.py`
  - 哈希链 prefix cache 复用完整 block。
- `paged_kv.py`
  - 共享教学实现：块池、block table、COW、decode 遍历。
- `utils.py`
  - 数学与输出辅助函数。

## 运行方式

一次跑完：

```bash
cd nano-vllm/tutorials/paged_attention_lab
./run_all.sh
```

分步运行：

```bash
python3 00_dense_attention.py
python3 01_block_table.py
python3 02_paged_kv_cache.py
python3 03_paged_decode.py
python3 04_prefix_cow.py
python3 05_prefix_cache.py
```


## 讲义入口

- 授课讲义（中文）：`LECTURE_CN.md`

## 与生产实现的对应关系

这套实验对齐了 vLLM 类实现中的职责拆分：

- **Scheduler + Block Manager**：决定何时分配/释放/抢占 block。
- **Block Table**：每个序列的元数据，记录 logical block -> physical block。
- **KV Cache Pages**：按固定大小保存 K/V。
- **Decode Kernel**：按 block table + slot 读取 KV 计算注意力。
- **Prefix Cache**：通过“前缀哈希 + 当前 block 哈希”复用完整 block。
- **COW**：共享块只读，分支写入前复制，避免相互污染。

本仓库里可以对照的真实代码在：

- `nanovllm/engine/block_manager.py`
- `nanovllm/engine/scheduler.py`
- `nanovllm/layers/attention.py`

## 每步讨论问题（课堂提问建议）

1. 哪些操作是 O(1)？哪些是 O(sequence length)？原因是什么？
2. 哪些元数据必须始终正确（token_count / used_slots / ref_count）？
3. 什么时候可以共享 block？什么时候必须触发 COW？
4. 为什么 prefix cache 通常只复用“完整 block”？
5. 序列变长后，内存访问模式如何变化？

### 参考答案（简版）

1. `append`/分配通常是 O(1)；`decode_attention` 需要遍历历史 KV，通常是 O(sequence length)。
2. `token_count`、`used_slots`、`ref_count` 必须一致，否则会少读/越界/泄漏或重复释放。
3. 只读前缀可共享；当共享块将被写入新 token 时必须先 COW。
4. 完整 block 边界稳定、命中逻辑简单；部分块易变，复用和一致性维护更复杂。
5. 序列越长，读取会跨更多 block，局部性与调度策略对吞吐影响更明显。

详细答案见：`LECTURE_CN.md` 对应 Step 的“参考答案”小节。

## 建议练习

1. 给 `PagedKVCache` 增加 `free(seq)`，并验证引用计数回收正确。
2. 增加“内存不足”场景，模拟需要抢占/回退的调度。
3. 扩展到多头（multi-head）并按 head 对比输出。
4. 增加统计：block 复用率、COW 次数、空闲块平均数量。
5. 用 `numpy` 或 `torch` 改写热点循环并测性能差异。

## 教学版做了哪些简化

- 仅 CPU + Python list（强调可读性）。
- 不覆盖 GPU kernel 线程映射细节（warp/block/thread）。
- 不包含异步调度、复杂驱逐策略、多 worker。
- prefix cache 示例聚焦“完整 block + token 精确匹配”。

尽管做了简化，但核心不变量与生产实现是一致的。
