# PagedAttention 中文讲义版（授课用）

> 面向对象：对 LLM 推理优化有兴趣、但还没系统理解 KV Cache 分页机制的同学。
>
> 配套代码目录：`nano-vllm/tutorials/paged_attention_lab/`

---

## 0. 课程目标（你上完这节课应掌握）

1. 能解释为什么连续 KV Cache 在在线服务场景下会浪费显存。
2. 能画出 block table，并说明逻辑 token 如何映射到物理 block + slot。
3. 能说明“PagedAttention 不改数学，只改内存布局和访问路径”。
4. 能解释 prefix sharing + COW 的必要性与触发条件。
5. 能讲清 prefix cache 的“哈希链命中条件”和“为何通常只复用完整 block”。

---

## 1. 先修知识与术语

### 1.1 先修知识

- Scaled Dot-Product Attention：
  \[
  \text{Attn}(q,K,V)=\text{softmax}(qK^T/\sqrt{d})V
  \]
- 了解“prefill 阶段（一次处理大量 prompt token）”与“decode 阶段（每步新生成 1 token）”。
- 知道 beam search / 多分支解码的基本含义。

### 1.2 本讲统一术语

- `block/page`：固定大小 KV 存储块（教学代码里等价）。
- `logical block`：序列视角下第 i 段 token。
- `physical block`：真实分配的物理块 ID。
- `block table`：`logical_block_id -> physical_block_id` 的映射表。
- `slot`：一个 physical block 内的位置偏移。
- `ref_count`：某个 physical block 被多少序列/分支共享。

---

## 2. 一句话直觉

**PagedAttention 的核心是“把连续地址空间的假象交给 block table 维护”，从而用较低碎片和可共享前缀支持高并发推理。**

也就是说：

- 序列逻辑上仍是连续 token 流；
- 物理上可以是离散块；
- 通过 block table 把逻辑连续“翻译”成物理离散访问。

---

## 3. 与 vLLM 实现的关系（纠正常见误解）

很多同学会问：“vLLM 是不是 C++ 写的？”

更准确说法：

- vLLM 整体是**混合架构**，并非纯 C++。
- 框架层、调度层大量是 Python。
- 关键高性能路径使用 CUDA/C++/Triton 等实现。

因此，教学版用 Python 把数据结构讲清楚是合理的：
它对齐的是“设计与不变量”，不是 GPU 内核微观实现细节。

---

## 4. 90 分钟授课节奏（建议）

- **10 分钟**：背景与动机（为什么要分页 KV）。
- **15 分钟**：Step 0 + Step 1（数学基线与 block table）。
- **20 分钟**：Step 2 + Step 3（分页写入、decode 一致性）。
- **20 分钟**：Step 4（prefix sharing + COW）。
- **15 分钟**：Step 5（prefix cache 哈希链命中）。
- **10 分钟**：总结 + 练习布置 + Q&A。

---

## 5. 代码导读主线（按脚本顺序）

### Step 0：Dense 基线

脚本：`00_dense_attention.py`

讲什么：

- 先固定注意力数学，不讨论内存布局。
- 让学生看到：后面所有优化都不能破坏这条基线。

课堂提问：

- 若读取顺序一致，布局改变是否会改变 attention 输出？为什么？

参考答案：

- 理论上不会改变。attention 的输出只由 `query/key/value` 数值与读取顺序决定。
- PagedAttention 改的是“KV 放在哪里、怎么找到”，不改公式本身。
- 只要 `iter_kv` 还原出的逻辑顺序与 dense 一致，输出应一致。

---

### Step 1：Block Table

脚本：`01_block_table.py`

讲什么：

- token 追加到序列时，如何得到 `(logical_block, slot)`。
- 为什么同一序列逻辑上连续，但物理上可以分散。

板书建议：

```text
seq_a token 时间线: t0 t1 t2 t3 | t4 ...
logical block id :  0  0  0  0 | 1
physical block id: P7 P7 P7 P7 | P2
```

课堂提问：

- block size 变大/变小，对 metadata 开销和内存利用率影响是什么？

参考答案：

- block size 变大：`block_table` 更短、元数据开销更低，但尾块浪费（内部碎片）可能更高。
- block size 变小：碎片通常更低、更细粒度，但元数据与调度管理开销变高。
- 工程上要在“元数据成本”和“碎片率”之间折中选取。

---

### Step 2：Paged KV 写入与重建

脚本：`02_paged_kv_cache.py`

讲什么：

- 真正把 K/V 向量写进 `physical block + slot`。
- 再通过 `block_table` 重建逻辑顺序，和 dense 历史对比校验。

关键不变量：

- `token_count` 必须准确。
- `used_slots` 不能超范围、不能回退错误。
- `block_table` 中每个物理块必须可读且与 token 顺序一致。

---

### Step 3：Paged Decode 一致性

脚本：`03_paged_decode.py`

讲什么：

- decode 时每步 1 个 query，通过 paged 路径读取历史 KV。
- 与 dense 路径做逐步误差对齐（应接近 0）。

课堂提问：

- 为什么这里误差可以做到 0（教学版 CPU list）？
- 在 GPU 上误差通常来自哪里（并行归约顺序、精度等）？

参考答案：

- 教学版里 dense 与 paged 使用同一组 Python 浮点运算路径，且顺序一致，所以误差可到 0。
- GPU 上常见误差来源：并行归约顺序不同、半精度（FP16/BF16）舍入、kernel 融合带来的计算次序变化。
- 因此线上通常验证“误差在阈值内”，而不是逐元素严格相等。

---

### Step 4：Prefix Sharing + COW

脚本：`04_prefix_cow.py`

讲什么：

- base 序列 fork 出 beam 分支后先共享前缀块（`ref_count` 增加）。
- 当共享块要写新 token 时触发 COW：
  - 先复制尾块；
  - 分支 block_table 指向新块；
  - 原共享块 `ref_count` 减 1。

这是“正确性关键点”：

- 不做 COW 会导致不同分支相互覆盖，生成结果互相污染。

---

### Step 5：Prefix Cache 哈希复用

脚本：`05_prefix_cache.py`

讲什么：

- 完整 block 才参与稳定复用（避免部分块变化导致高维护复杂度）。
- 哈希链：当前块哈希包含前缀哈希，确保“相同 token 串 + 相同前缀路径”才命中。
- 发生一次 cache miss 后，后续逻辑块不再是前缀命中链。

课堂提问：

- 为什么只按“当前块 token”做哈希不够？

参考答案：

- 因为同样的当前块 token 可能出现在不同前缀路径下，语义位置并不相同。
- 只看当前块会误把不同上下文当作同一块，导致错误复用。
- 加入“前缀哈希链”后，命中条件变成“前缀一致 + 当前块一致”，更安全。

---

## 6. 关键数据结构速记

- `KVBlock`：`ref_count / used_slots / keys / values / token_ids`
- `BlockPool`：`free_block_ids` + 分配/回收 + clone(COW)
- `SequenceState`：`block_table` + `token_count`
- `PagedKVCache`：`append / fork / iter_kv / decode_attention`

配套代码在：`paged_kv.py`

---

## 7. 两个最重要的不变量（考试常考）

1. **引用计数不变量**：
   - 每次共享必须 `inc_ref`；
   - 每次放弃引用必须 `dec_ref`；
   - `ref_count == 0` 才能回收进 free list。

2. **逻辑顺序不变量**：
   - 对任意序列，`iter_kv` 输出顺序必须与逻辑 token 时间线完全一致。

这两个不变量一旦被破坏，结果要么错、要么内存泄漏、要么崩溃。

---

## 8. 常见错误清单（debug 指南）

1. **COW 触发时机错**：在写之后才复制，导致已污染原块。
2. **used_slots 维护错**：block 读到未初始化 slot。
3. **token_count 与 block_table 不一致**：重建时少读/多读。
4. **prefix cache 误命中**：只比哈希不比 token 内容，出现冲突错误复用。
5. **分配器 free list 重复入队**：同一 block 多次释放，后续读写错乱。

---

## 9. 课堂现场演示命令

```bash
cd nano-vllm/tutorials/paged_attention_lab
./run_all.sh
```

建议你在 Step 3 和 Step 4 停下来讲：

- Step 3：强调“数学不变”。
- Step 4：强调“共享 + 隔离”的平衡。

---

## 10. 课后作业（分层）

### 基础题

1. 在 `PagedKVCache` 增加 `free(seq)`，并写 3 组引用计数回收测试。
2. 打印每步空闲块数量，画出 decode 过程中变化曲线。

### 进阶题

1. 支持 multi-head KV（先 head 内独立，再合并输出）。
2. 模拟内存不足：构造“必须抢占/回退”的调度场景。
3. 统计 prefix 命中率、COW 触发率，并解释与 prompt 相似度关系。

### 挑战题

1. 把热点循环改写为 `numpy`/`torch`，比较性能与可读性。
2. 设计一种 eviction 策略，并分析对吞吐和延迟的影响。

---

## 11. 讲师备注（可直接照着讲）

- 第一原则：先守住正确性不变量，再谈性能。
- 学生最容易混淆的是“逻辑连续 vs 物理连续”。
- 讲 COW 时一定画图：
  - fork 前后 `ref_count` 变化；
  - append 后 block_table 分叉。
- 讲 prefix cache 时强调“前缀链”概念，不要只讲单块哈希。

---

## 12. 1 分钟总结稿（下课收束）

PagedAttention 的精髓不是新 attention 公式，而是把 KV 的存储方式从“每序列连续数组”升级为“分页 + 映射 + 共享 + 按需复制”。

它解决的是服务系统层面的显存与并发问题：
- 用 block table 管理离散物理块，
- 用 prefix sharing 降低重复存储，
- 用 COW 保证分支正确性，
- 用 prefix cache 跨请求复用前缀。

这就是它在大规模推理服务里真正的价值。
