/*
第 4 次迭代：并发 Executor（线程池 + 就绪队列 + 错误传播/取消）。
这版在第 3 次的图执行基础上，把串行执行升级为并发：无依赖的节点可并行跑；一旦有算子失败，立刻取消下游并尽快收敛退出。

你将得到：

轻量 ThreadPool（固定线程、任务队列）
Ready Queue + pending_inputs（每个节点待就绪输入计数）
邻接表（节点完成 → 下游 pending-- → 变 0 入队）
错误传播（任一任务抛错 → 标记取消、停止调度、等待在途任务收敛）


z1 与 z2 会并行执行（你可把 num_threads 调成 1/2/8 观察差异）。
故意制造某个算子抛错时（比如把 z3 的输入写成 {"z1","x"}），应快速取消其他未执行节点，并抛出清晰错误。


本迭代你可以做的练习
可重复运行：把 Executor::Run() 多跑几次；确认没有数据竞争（地址消毒器可帮助：-fsanitize=thread / -fsanitize=address）。
错误传播测试：写一个会抛错的算子（比如 Fail），验证能即时停止。
简单性能对比：构造 N 条独立链，比较 num_threads = 1 与 =8 的总耗时差别（粗测并行效果）。




*/



// minitf_iter4.cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>

#if defined(_MSC_VER)
  #include <malloc.h>
#endif

// ===================== 迭代1：DType / Shape / Allocator / Tensor =====================
enum class DType { kFloat32, kInt32, kInt64, kUInt8, kBool };

inline size_t SizeOf(DType dt) {
  switch (dt) {
    case DType::kFloat32: return 4;
    case DType::kInt32:   return 4;
    case DType::kInt64:   return 8;
    case DType::kUInt8:   return 1;
    case DType::kBool:    return 1;
  }
  std::abort();
}

class Shape {
 public:
  Shape() = default;
  explicit Shape(std::vector<int64_t> dims) : dims_(std::move(dims)) {
    for (auto d : dims_) { assert(d >= 0); }
  }
  size_t Rank() const { return dims_.size(); }
  int64_t Dim(size_t i) const { return dims_[i]; }
  const std::vector<int64_t>& Dims() const { return dims_; }
  int64_t NumElements() const {
    if (dims_.empty()) return 1;
    long double prod = 1;
    for (auto d : dims_) prod *= static_cast<long double>(d);
    return static_cast<int64_t>(prod);
  }
  std::string DebugString() const {
    std::string s = "[";
    for (size_t i = 0; i < dims_.size(); ++i) {
      s += std::to_string(dims_[i]);
      if (i + 1 < dims_.size()) s += ", ";
    }
    s += "]";
    return s;
  }
 private:
  std::vector<int64_t> dims_;
};

static void* AlignedAlloc(size_t size, size_t alignment) {
#if defined(_MSC_VER)
  return _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__linux__)
  void* p = nullptr;
  if (posix_memalign(&p, alignment, size) != 0) return nullptr;
  return p;
#else
  size_t padded = (size + alignment - 1) / alignment * alignment;
  return std::aligned_alloc(alignment, padded);
#endif
}
static void AlignedFree(void* p) {
#if defined(_MSC_VER)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* Allocate(size_t bytes, size_t alignment = 64) = 0;
  virtual void  Deallocate(void* p, size_t /*bytes*/) = 0;
  virtual const char* Name() const = 0;
};

class CPUAllocator final : public Allocator {
 public:
  void* Allocate(size_t bytes, size_t alignment = 64) override {
    if (bytes == 0) return nullptr;
    void* p = AlignedAlloc(bytes, alignment);
    if (!p) throw std::bad_alloc();
    return p;
  }
  void Deallocate(void* p, size_t /*bytes*/) override { AlignedFree(p); }
  const char* Name() const override { return "CPUAllocator"; }
};

class Tensor {
 public:
  Tensor() = default;
  Tensor(DType dtype, Shape shape, Allocator& alloc)
      : dtype_(dtype), shape_(std::move(shape)), alloc_(&alloc) {
    bytes_ = static_cast<size_t>(shape_.NumElements()) * SizeOf(dtype_);
    if (bytes_ > 0) data_ = alloc_->Allocate(bytes_, 64);
  }
  Tensor(Tensor&& other) noexcept { MoveFrom(std::move(other)); }
  Tensor& operator=(Tensor&& other) noexcept {
    if (this != &other) { Destroy(); MoveFrom(std::move(other)); }
    return *this;
  }
  Tensor(const Tensor&) = delete;
  Tensor& operator=(const Tensor&) = delete;
  ~Tensor() { Destroy(); }

  DType dtype() const { return dtype_; }
  const Shape& shape() const { return shape_; }
  size_t bytes() const { return bytes_; }
  int64_t NumElements() const { return shape_.NumElements(); }
  bool empty() const { return data_ == nullptr; }

  template <typename T> T* data() { return reinterpret_cast<T*>(data_); }
  template <typename T> const T* data() const { return reinterpret_cast<const T*>(data_); }

  std::string DebugString() const {
    std::string dt;
    switch (dtype_) {
      case DType::kFloat32: dt = "f32"; break;
      case DType::kInt32:   dt = "i32"; break;
      case DType::kInt64:   dt = "i64"; break;
      case DType::kUInt8:   dt = "u8";  break;
      case DType::kBool:    dt = "bool";break;
    }
    return "Tensor<" + dt + ", shape=" + shape_.DebugString() +
           ", bytes=" + std::to_string(bytes_) + ">";
  }

 private:
  void Destroy() {
    if (data_ && alloc_) { alloc_->Deallocate(data_, bytes_); }
    data_ = nullptr; bytes_ = 0; alloc_ = nullptr;
  }
  void MoveFrom(Tensor&& other) {
    dtype_ = other.dtype_;
    shape_ = std::move(other.shape_);
    alloc_ = other.alloc_;
    data_  = other.data_;
    bytes_ = other.bytes_;
    other.alloc_ = nullptr; other.data_ = nullptr; other.bytes_ = 0;
  }

  DType dtype_{DType::kFloat32};
  Shape shape_;
  Allocator* alloc_{nullptr};
  void* data_{nullptr};
  size_t bytes_{0};
};

// ===================== 迭代2：OpKernel / Registry =====================
struct NodeDef {
  std::string name;
  std::string op;
  std::vector<std::string> inputs;
};

class OpContext {
 public:
  explicit OpContext(Allocator& alloc,
                     const std::unordered_map<std::string, const Tensor*>* feeds = nullptr)
      : alloc_(alloc), feeds_(feeds) {}
  Tensor AllocateOutput(DType dt, const Shape& shape) { return Tensor(dt, shape, alloc_); }
  const Tensor* GetFeed(const std::string& node_name) const {
    if (!feeds_) return nullptr;
    auto it = feeds_->find(node_name);
    return it == feeds_->end() ? nullptr : it->second;
  }
 private:
  Allocator& alloc_;
  const std::unordered_map<std::string, const Tensor*>* feeds_{nullptr};
};

class OpKernel {
 public:
  explicit OpKernel(const NodeDef& def) : def_(def) {}
  virtual ~OpKernel() = default;
  virtual Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) = 0;
 protected:
  const NodeDef& def_;
};

class OpRegistry {
 public:
  using Factory = std::function<std::unique_ptr<OpKernel>(const NodeDef&)>;
  static OpRegistry& Global(){ static OpRegistry inst; return inst; }
  void Register(const std::string& op, Factory f) {
    if (reg_.count(op)) throw std::runtime_error("Op already registered: " + op);
    reg_[op] = std::move(f);
  }
  std::unique_ptr<OpKernel> Create(const NodeDef& def) const {
    auto it = reg_.find(def.op);
    if (it == reg_.end()) throw std::runtime_error("Op not found: " + def.op);
    return it->second(def);
  }
 private:
  std::unordered_map<std::string, Factory> reg_;
};

#define REGISTER_KERNEL(OPNAME, KERNEL_TYPE)                      \
  static bool _reg_token_##KERNEL_TYPE = [](){                    \
    OpRegistry::Global().Register(                                \
      OPNAME,                                                     \
      [](const NodeDef& def){ return std::make_unique<KERNEL_TYPE>(def);} ); \
    return true;                                                  \
  }();

// ---- Placeholder ----
class PlaceholderKernel final : public OpKernel {
 public:
  using OpKernel::OpKernel;
  Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) override {
    if (!inputs.empty()) throw std::runtime_error("Placeholder expects 0 inputs");
    const Tensor* fed = ctx.GetFeed(def_.name);
    if (!fed) throw std::runtime_error("Feed not found for Placeholder: " + def_.name);
    Tensor out = ctx.AllocateOutput(fed->dtype(), fed->shape());
    std::memcpy(out.data<void>(), fed->data<const void>(), fed->bytes());
    return out;
  }
};
REGISTER_KERNEL("Placeholder", PlaceholderKernel)

// ---- Add（float32，同形状）----
class AddKernel final : public OpKernel {
 public:
  using OpKernel::OpKernel;
  Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) override {
    if (inputs.size() != 2) throw std::runtime_error("Add expects 2 inputs");
    const Tensor* a = inputs[0];
    const Tensor* b = inputs[1];
    if (a->dtype() != DType::kFloat32 || b->dtype() != DType::kFloat32)
      throw std::runtime_error("Add only supports float32 in iter4");
    if (a->shape().Dims() != b->shape().Dims())
      throw std::runtime_error("Add requires same shape (no broadcast in iter4)");
    Tensor out = ctx.AllocateOutput(DType::kFloat32, a->shape());
    const float* pa = a->data<float>();
    const float* pb = b->data<float>();
    float* po = out.data<float>();
    const int64_t n = a->NumElements();
    for (int64_t i = 0; i < n; ++i) po[i] = pa[i] + pb[i];
    return out;
  }
};
REGISTER_KERNEL("Add", AddKernel)

// ===================== 迭代3：Graph（增强：邻接表构建） =====================
class Graph {
 public:
  void AddNode(const NodeDef& def) {
    if (name_to_idx_.count(def.name)) throw std::runtime_error("Duplicate node: " + def.name);
    nodes_.push_back(def);
    name_to_idx_[def.name] = static_cast<int>(nodes_.size()) - 1;
  }
  const NodeDef& NodeAt(int idx) const { return nodes_[idx]; }
  int IndexOf(const std::string& name) const {
    auto it = name_to_idx_.find(name);
    if (it == name_to_idx_.end()) throw std::runtime_error("Node not found: " + name);
    return it->second;
  }
  const std::vector<NodeDef>& nodes() const { return nodes_; }

  // 构建入度 + 邻接表（下游列表）
  void BuildTopology(std::vector<int>& indeg, std::vector<std::vector<int>>& adj) const {
    int N = (int)nodes_.size();
    indeg.assign(N, 0);
    adj.assign(N, {});
    for (int i = 0; i < N; ++i) {
      for (const auto& in : nodes_[i].inputs) {
        int u = IndexOf(in);
        indeg[i] += 1;
        adj[u].push_back(i);
      }
    }
    // 校验无孤立引用（IndexOf 已经保证）
  }

 private:
  std::vector<NodeDef> nodes_;
  std::unordered_map<std::string, int> name_to_idx_;
};

// ===================== 迭代4：线程池 + 并发 Executor =====================
// 说明：
// 1. ThreadPool：一个极简的固定线程数任务队列，支持 Enqueue 任意可调用对象。
// 2. Executor：根据计算图拓扑，把每个 Node 的 Compute 任务投递到 ThreadPool，
//    用共享队列、原子计数、条件变量等机制保证：
//    - 无数据竞争
//    - 异常安全：任一节点抛异常 → 取消后续任务并向上层抛异常
//    - 及时唤醒：任务完成或出错时通过 done_cv 通知主线程
// 3. 线程数默认为硬件并发度，最小为 4。
// 4. 线程池析构时先置 stop_，再唤醒所有工作线程，最后 join，保证优雅退出。

class ThreadPool {
 public:
  // 构造函数：启动 n 个工作线程，每个线程死循环从队列取任务执行
  explicit ThreadPool(size_t n) : stop_(false) {
    if (n == 0) n = 1;  // 至少 1 个线程
    for (size_t i = 0; i < n; ++i) {
      workers_.emplace_back([this] {
        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lk(mu_);
            // 等待直到 stop_ 置位或队列非空
            cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
            // 若停止且队列为空，则退出线程
            if (stop_ && q_.empty()) return;
            task = std::move(q_.front());
            q_.pop();
          }
          task();  // 执行任务（可能抛异常，由调用方处理）
        }
      });
    }
  }

  // 析构函数：先置 stop_，再通知所有线程，最后 join
  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
  }

  // 向线程池投递一个任务（可复制或可移动）
  void Enqueue(std::function<void()> f) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      q_.push(std::move(f));
    }
    cv_.notify_one();  // 唤醒一个工作线程
  }

 private:
  std::vector<std::thread> workers_;      // 工作线程
  std::queue<std::function<void()>> q_;   // 任务队列
  std::mutex mu_;                         // 保护队列和 stop_
  std::condition_variable cv_;            // 等待队列非空或停止
  bool stop_;                             // 停止标志
};

// ------------------------------------------------------------------
// Executor：并发执行计算图
// ------------------------------------------------------------------
class Executor {
 public:
  // 构造函数：持有图、内存分配器、可选 feed 字典、线程数
  Executor(const Graph& g,
           Allocator& alloc,
           const std::unordered_map<std::string, const Tensor*>* feeds,
           size_t num_threads = std::thread::hardware_concurrency())
      : g_(g),
        alloc_(alloc),
        feeds_(feeds),
        pool_(num_threads ? num_threads : 4) {}

  // 主入口：并发执行整张图，返回所有节点输出
  // 若任一节点抛异常，则取消剩余任务并把异常信息通过 std::runtime_error 向上传递
  std::unordered_map<std::string, Tensor> Run() {
    const auto& nodes = g_.nodes();
    const int N = static_cast<int>(nodes.size());
    if (N == 0) return {};

    // 1) 拓扑辅助结构
    std::vector<int> pending;            // pending[i] = 节点 i 还缺多少个输入
    std::vector<std::vector<int>> adj;   // adj[u] = u 的所有下游节点索引
    g_.BuildTopology(pending, adj);      // 由 Graph 提供

    // 2) 全局共享状态
    std::unordered_map<std::string, Tensor> outputs;  // 节点名 -> 输出张量
    outputs.reserve(N);
    std::mutex out_mu;  // 保护 outputs

    std::queue<int> ready;  // 当前可执行的节点索引
    for (int i = 0; i < N; ++i)
      if (pending[i] == 0) ready.push(i);

    std::atomic<int> inflight{0};   // 正在执行的任务数
    std::atomic<bool> cancelled{false};
    std::string error_msg;
    std::mutex err_mu;              // 保护 error_msg 的写入
    std::condition_variable done_cv;

    OpContext ctx(alloc_, feeds_);  // 每个 Compute 需要的上下文

    // 3) 递归 lambda：把节点 idx 的任务投递到线程池
    //    注意：schedule_node 需要自引用，因此先声明再定义
    std::function<void(int)> schedule_node;

    schedule_node = [&](int idx) {
      inflight.fetch_add(1, std::memory_order_relaxed);
      pool_.Enqueue([&, idx] {
        try {
          // 若已取消，直接退出
          if (cancelled.load(std::memory_order_relaxed)) {
            inflight.fetch_sub(1, std::memory_order_relaxed);
            done_cv.notify_all();
            return;
          }

          const NodeDef& n = g_.NodeAt(idx);

          // 收集输入张量
          std::vector<const Tensor*> in_tensors;
          in_tensors.reserve(n.inputs.size());
          {
            std::lock_guard<std::mutex> lk(out_mu);
            for (const auto& in_name : n.inputs) {
              auto it = outputs.find(in_name);
              if (it == outputs.end())
                throw std::runtime_error("Missing input tensor for node: " +
                                         n.name + ", input: " + in_name);
              in_tensors.push_back(&it->second);
            }
          }

          // 创建算子并计算
          auto kernel = OpRegistry::Global().Create(n);
          Tensor out = kernel->Compute(in_tensors, ctx);

          // 保存输出
          {
            std::lock_guard<std::mutex> lk(out_mu);
            outputs.emplace(n.name, std::move(out));
          }

          // 推进下游：对每条出边 v，原子减 pending[v]
          for (int v : adj[idx]) {
            int left = --pending[v];
            if (left == 0 && !cancelled.load(std::memory_order_relaxed)) {
              schedule_node(v);  // 递归调度
            }
          }
        } catch (const std::exception& e) {
          // 记录首个异常
          {
            std::lock_guard<std::mutex> lk(err_mu);
            if (!cancelled.exchange(true)) {
              error_msg = e.what();
            }
          }
        }

        // 任务完成：减少计数并唤醒主线程
        inflight.fetch_sub(1, std::memory_order_relaxed);
        done_cv.notify_all();
      });
    };

    // 4) 启动所有初始就绪节点
    while (!ready.empty()) {
      schedule_node(ready.front());
      ready.pop();
    }

    // 5) 主线程等待：直到 inflight == 0
    {
      std::unique_lock<std::mutex> lk(out_mu);  // 任意 mutex 即可
      done_cv.wait(lk, [&] { return inflight.load() == 0; });
    }

    // 6) 若出错则抛异常
    if (cancelled.load() && !error_msg.empty())
      throw std::runtime_error(std::string("Execution failed: ") + error_msg);

    return outputs;
  }

 private:
  const Graph& g_;
  Allocator& alloc_;
  const std::unordered_map<std::string, const Tensor*>* feeds_;
  ThreadPool pool_;
};

// ===================== 辅助：打印/填充 =====================
static void FillRange(Tensor& t, float start, float step) {
  assert(t.dtype() == DType::kFloat32);
  float* p = t.data<float>();
  for (int64_t i = 0; i < t.NumElements(); ++i) p[i] = start + step * static_cast<float>(i);
}
static void PrintTensor(const Tensor& t, const std::string& tag, int max_elems = 16) {
  std::cout << tag << " " << t.DebugString() << " data=[";
  int64_t n = t.NumElements();
  const float* p = t.data<float>();
  for (int64_t i = 0; i < n && i < max_elems; ++i) {
    std::cout << p[i] << (i + 1 < n && i + 1 < max_elems ? ", " : "");
  }
  if (n > max_elems) std::cout << ", ...";
  std::cout << "]\n";
}

// ===================== Demo =====================
int main() {
  CPUAllocator alloc;

  // 输入
  Tensor a_in(DType::kFloat32, Shape({2, 3}), alloc);
  Tensor b_in(DType::kFloat32, Shape({2, 3}), alloc);
  Tensor c_in(DType::kFloat32, Shape({2, 3}), alloc);
  FillRange(a_in, 0.0f, 1.0f);    // 0..5
  FillRange(b_in, 10.0f, 0.5f);   // 10..12.5
  FillRange(c_in, 100.0f, 1.0f);  // 100..105

  // 图：z1 = a + b；z2 = b + c；z3 = z1 + z2   （z1,z2 可并行）
  Graph g;
  g.AddNode(NodeDef{"a", "Placeholder", {}});
  g.AddNode(NodeDef{"b", "Placeholder", {}});
  g.AddNode(NodeDef{"c", "Placeholder", {}});
  g.AddNode(NodeDef{"z1", "Add", {"a","b"}});
  g.AddNode(NodeDef{"z2", "Add", {"b","c"}});
  g.AddNode(NodeDef{"z3", "Add", {"z1","z2"}});

  std::unordered_map<std::string, const Tensor*> feeds{
    {"a",&a_in}, {"b",&b_in}, {"c",&c_in}
  };

  try {
    Executor exe(g, alloc, &feeds, /*num_threads=*/4);
    auto outputs = exe.Run();

    PrintTensor(outputs.at("z1"), "z1=a+b:");
    PrintTensor(outputs.at("z2"), "z2=b+c:");
    PrintTensor(outputs.at("z3"), "z3=z1+z2:");

    std::cout << "OK (iter4 concurrent)\n";
  } catch (const std::exception& e) {
    std::cerr << "Execution error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}