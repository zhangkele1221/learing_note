/*
第 3 次迭代：Graph（节点/依赖/拓扑序）。
这版在前两次的基础上，加入：

NodeDef{ name, op, inputs }：节点定义（含依赖）
Graph：存节点、校验、拓扑排序（Kahn）
Executor（单线程）：按拓扑序执行整个图
新增算子 Placeholder（用 feed 提供输入），沿用 Add（逐元素加法）
你可以：用两个 Placeholder（a、b）作为输入，连到 Add，通过 feed 提供 a/b 的张量，执行得到 z=a+b。


本迭代可以做的练习
检错：把 z 的 inputs 写错成 {"a","x"}，确认能得到“输入节点不存在”的友好报错。
环检测：手动制造环（如令 a.inputs={"z"}），确认能触发“Graph has cycles”。
新增算子 Mul 并添加 NodeDef m{ "m", "Mul", {"z","a"} };，测试 m=z*a。
多输出（挑战）：把 OpKernel::Compute 改成返回 std::vector<Tensor>，让 NodeDef 的输入可以写成 node:idx 形式（为迭代 4 铺路）。



*/



// minitf_iter3.cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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
    if (dims_.empty()) return 1; // 标量
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
  std::vector<std::string> inputs; // 依赖的节点名（其输出作为本节点输入）
  // 简化：属性留空，后续迭代再加
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
    if (it == feeds_->end()) return nullptr;
    return it->second;
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

// ---- Placeholder：从 feed 读取数据作为输出 ----
class PlaceholderKernel final : public OpKernel {
 public:
  using OpKernel::OpKernel;
  Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) override {
    if (!inputs.empty()) throw std::runtime_error("Placeholder expects 0 inputs");
    const Tensor* fed = ctx.GetFeed(def_.name);
    if (!fed) throw std::runtime_error("Feed not found for Placeholder: " + def_.name);
    // 这里为了简单，分配同形状同类型的输出并拷贝数据
    Tensor out = ctx.AllocateOutput(fed->dtype(), fed->shape());
    std::memcpy(out.data<void>(), fed->data<const void>(), fed->bytes());
    return out;
  }
};
REGISTER_KERNEL("Placeholder", PlaceholderKernel)

// ---- Add：逐元素加法（float32，同形状） ----
class AddKernel final : public OpKernel {
 public:
  using OpKernel::OpKernel;
  Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) override {
    if (inputs.size() != 2) throw std::runtime_error("Add expects 2 inputs");
    const Tensor* a = inputs[0];
    const Tensor* b = inputs[1];
    if (a->dtype() != DType::kFloat32 || b->dtype() != DType::kFloat32)
      throw std::runtime_error("Add only supports float32 in iter3");
    if (a->shape().Dims() != b->shape().Dims())
      throw std::runtime_error("Add requires same shape (no broadcast in iter3)");

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

// ===================== 迭代3：Graph / TopoSort / Executor =====================
class Graph {
 public:
  void AddNode(const NodeDef& def) {
    if (name_to_idx_.count(def.name)) throw std::runtime_error("Duplicate node: " + def.name);
    nodes_.push_back(def);
    name_to_idx_[def.name] = static_cast<int>(nodes_.size()) - 1;
  }

  // 拓扑排序（Kahn），并校验依赖存在与无环
  std::vector<int> TopoOrder() const {
    const int N = static_cast<int>(nodes_.size());
    std::vector<int> indeg(N, 0);
    // 计算入度
    for (int i = 0; i < N; ++i) {
      for (const auto& in : nodes_[i].inputs) {
        auto it = name_to_idx_.find(in);
        if (it == name_to_idx_.end())
          throw std::runtime_error("Input node not found: " + in + " for " + nodes_[i].name);
        indeg[i]++;
      }
    }
    // 入度为 0 的入队
    std::queue<int> q;
    for (int i = 0; i < N; ++i) if (indeg[i] == 0) q.push(i);

    std::vector<int> order;
    order.reserve(N);
    while (!q.empty()) {
      int u = q.front(); q.pop();
      order.push_back(u);
      // u 的“出边”即所有把 u 当作输入的 v：我们需要扫描（O(N*E) 简化即可）
      for (int v = 0; v < N; ++v) {
        for (const auto& in : nodes_[v].inputs) {
          if (in == nodes_[u].name) {
            if (--indeg[v] == 0) q.push(v);
          }
        }
      }
    }
    if ((int)order.size() != N) throw std::runtime_error("Graph has cycles");
    return order;
  }

  const NodeDef& NodeAt(int idx) const { return nodes_[idx]; }
  const NodeDef& GetNode(const std::string& name) const {
    return nodes_.at(name_to_idx_.at(name));
  }
  const std::vector<NodeDef>& nodes() const { return nodes_; }

 private:
    std::vector<NodeDef> nodes_;
    std::unordered_map<std::string, int> name_to_idx_;
};

class Executor {
 public:
  explicit Executor(const Graph& g, Allocator& alloc,
                    const std::unordered_map<std::string, const Tensor*>* feeds = nullptr)
      : g_(g), alloc_(alloc), feeds_(feeds) {}

  // 运行整个图，返回每个节点的（唯一）输出
  std::unordered_map<std::string, Tensor> Run() {
    OpContext ctx(alloc_, feeds_);
    std::unordered_map<std::string, Tensor> outputs; // node_name -> output tensor

    // 按拓扑序执行
    auto order = g_.TopoOrder();
    for (int idx : order) {
      const NodeDef& n = g_.NodeAt(idx);
      // 收集输入张量
      std::vector<const Tensor*> in_tensors;
      in_tensors.reserve(n.inputs.size());
      for (const auto& in_name : n.inputs) {
        auto it = outputs.find(in_name);
        if (it == outputs.end())
          throw std::runtime_error("Missing input tensor for node: " + n.name + ", input: " + in_name);
        in_tensors.push_back(&it->second);
      }
      // 创建内核并计算
      auto kernel = OpRegistry::Global().Create(n);
      Tensor out = kernel->Compute(in_tensors, ctx);
      outputs.emplace(n.name, std::move(out));
    }
    return outputs;
  }

 private:
  const Graph& g_;
  Allocator& alloc_;
  const std::unordered_map<std::string, const Tensor*>* feeds_;
};

// ===================== 工具：填充/打印 =====================
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

  // 1) 构造输入张量（作为 feed 提供给 Placeholder）
  Tensor a_in(DType::kFloat32, Shape({2, 3}), alloc);
  Tensor b_in(DType::kFloat32, Shape({2, 3}), alloc);
  FillRange(a_in, 0.0f, 1.0f);   // 0,1,2,3,4,5
  FillRange(b_in, 10.0f, 0.5f);  // 10,10.5,11,11.5,12,12.5

  // 2) 定义计算图：  a + b -> z
  Graph g;
  NodeDef a{ "a", "Placeholder", {} };
  NodeDef b{ "b", "Placeholder", {} };
  NodeDef z{ "z", "Add", {"a", "b"} };
  g.AddNode(a);
  g.AddNode(b);
  g.AddNode(z);

  // 3) 准备 feeds（把占位符的值喂进去）
  std::unordered_map<std::string, const Tensor*> feeds;
  feeds["a"] = &a_in;
  feeds["b"] = &b_in;

  // 4) 执行（单线程拓扑序）
  Executor exe(g, alloc, &feeds);
  auto outputs = exe.Run();

  // 5) 取出并打印 z
  const Tensor& z_out = outputs.at("z");
  PrintTensor(a_in,  "A:");
  PrintTensor(b_in,  "B:");
  PrintTensor(z_out, "Z=A+B:");

  // 6) 校验
  const float* pa = a_in.data<float>();
  const float* pb = b_in.data<float>();
  const float* pz = z_out.data<float>();
  for (int64_t i = 0; i < z_out.NumElements(); ++i) {
    assert(std::abs(pz[i] - (pa[i] + pb[i])) < 1e-6f);
  }
  std::cout << "OK (iter3)\n";
  return 0;
}