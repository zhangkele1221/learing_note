/*
2 迭代：Op & Kernel & Registry。
这一版在第 1 迭代（Tensor & Allocator）的基础上，新增：

NodeDef：描述“要执行哪个算子”
OpKernel：算子基类（接口）
OpRegistry：算子注册与工厂创建
一个示例算子 Add（逐元素加法）
迷你 OpContext：算子在执行时向它“要输出 Tensor”
先不引入 Graph/Executor（放到第 3 迭代），本次用 “注册 → 创建 → 直接执行” 的最短路径把算子体系跑通。




本迭代要完成的小练习（超有用）
再注册一个算子 Mul（逐元素乘法）

复制 AddKernel 改成 MulKernel，注册名 "Mul"，并在 main() 里演示一次。
类型支持扩展

让 Add 同时支持 kInt32（可用模板或简单的 switch 分支）。
简易“检查器”

封装一个 CheckSameShape(inputs) 帮助函数，未来 Graph/Executor 会复用。
和 TensorFlow 的映射
你已经拥有了：
Kernel 抽象（OpKernel）、注册表（OpRegistry）、执行上下文（OpContext）。
和 TF 的差别（刻意简化）：
这里只有单输出；没有 AttrSlice/NodeAttrs；没有 Device/AllocatorAttributes；没有异步 Compute。
这些会在后续迭代逐步加上。



*/




// minitf_iter2.cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
  #include <malloc.h>
#endif

// ========== 迭代1：DType / Shape / Allocator / Tensor ==========

enum class DType {
  kFloat32,
  kInt32,
  kInt64,
  kUInt8,
  kBool,
};

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

// 跨平台对齐分配
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

// ========== 迭代2：NodeDef / OpKernel / OpRegistry / Add ==========

// NodeDef：本迭代仅保留必要字段（名字、op类型名）
struct NodeDef {
  std::string name;
  std::string op;
  // 未来可加入属性字典（如 dtype、常量值等）
};

// 执行上下文：算子通过它申请输出 Tensor（统一由框架分配）
class OpContext {
 public:
  explicit OpContext(Allocator& alloc) : alloc_(alloc) {}
  Tensor AllocateOutput(DType dt, const Shape& shape) {
    return Tensor(dt, shape, alloc_);
  }
 private:
  Allocator& alloc_;
};

// 算子基类
class OpKernel {
 public:
  explicit OpKernel(const NodeDef& def) : def_(def) {}
  virtual ~OpKernel() = default;

  // 约定：inputs 为只读；返回值为算子唯一输出（多输出在后续迭代扩展）
  virtual Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) = 0;

 protected:
  const NodeDef& def_;
};

// 注册表：字符串 op → 工厂（NodeDef -> unique_ptr<OpKernel>）
class OpRegistry {
 public:
  using Factory = std::function<std::unique_ptr<OpKernel>(const NodeDef&)>;
  static OpRegistry& Global() {
    static OpRegistry inst;
    return inst;
  }
  void Register(const std::string& op, Factory f) {
    auto it = reg_.find(op);
    if (it != reg_.end()) throw std::runtime_error("Op already registered: " + op);
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

// 注册宏（静态初始化一次）
#define REGISTER_KERNEL(OPNAME, KERNEL_TYPE)                      \
  static bool _reg_token_##KERNEL_TYPE = [](){                    \
    OpRegistry::Global().Register(                                \
      OPNAME,                                                     \
      [](const NodeDef& def){ return std::make_unique<KERNEL_TYPE>(def);} ); \
    return true;                                                  \
  }();

// ========== 示例算子：Add（逐元素加法，当前仅支持 float32，同形状） ==========

class AddKernel final : public OpKernel {
 public:
  using OpKernel::OpKernel;
  Tensor Compute(const std::vector<const Tensor*>& inputs, OpContext& ctx) override {
    if (inputs.size() != 2) throw std::runtime_error("Add expects 2 inputs");
    const Tensor* a = inputs[0];
    const Tensor* b = inputs[1];
    if (a->dtype() != DType::kFloat32 || b->dtype() != DType::kFloat32)
      throw std::runtime_error("Add only supports float32 in iter2");
    if (a->shape().Dims() != b->shape().Dims())
      throw std::runtime_error("Add requires same shape (no broadcast in iter2)");

    Tensor out = ctx.AllocateOutput(DType::kFloat32, a->shape());
    const float* pa = a->data<float>();
    const float* pb = b->data<float>();
    float* po = out.data<float>();
    const int64_t n = a->NumElements();
    for (int64_t i = 0; i < n; ++i) {
      po[i] = pa[i] + pb[i];
    }
    return out;
  }
};

REGISTER_KERNEL("Add", AddKernel)

// ========== 小工具：填充与打印 ==========
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

// ========== Demo：通过注册表创建 AddKernel 并执行 ==========
int main() {
  CPUAllocator alloc;
  OpContext ctx(alloc);

  // 准备两个输入张量
  Tensor a(DType::kFloat32, Shape({2, 3}), alloc);
  Tensor b(DType::kFloat32, Shape({2, 3}), alloc);
  FillRange(a, /*start=*/0.0f, /*step=*/1.0f);   // a: 0,1,2,3,4,5
  FillRange(b, /*start=*/10.0f, /*step=*/0.5f);  // b: 10,10.5,11,...

  PrintTensor(a, "A:");
  PrintTensor(b, "B:");

  // 通过 NodeDef 指定 op 类型
  NodeDef def;
  def.name = "add_1";
  def.op   = "Add";

  // 创建内核并执行
  auto kernel = OpRegistry::Global().Create(def);
  std::vector<const Tensor*> inputs = { &a, &b };
  Tensor out = kernel->Compute(inputs, ctx);

  PrintTensor(out, "Out:");

  // 简单校验
  const float* pa = a.data<float>();
  const float* pb = b.data<float>();
  const float* po = out.data<float>();
  for (int64_t i = 0; i < out.NumElements(); ++i) {
    assert(std::abs(po[i] - (pa[i] + pb[i])) < 1e-6f);
  }
  std::cout << "OK (iter2)\n";
  return 0;
}

