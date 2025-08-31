/*做一个“迷你 TensorFlow（MiniTF）”的**迭代式实现学习计划**：每一迭代只引入极少但关键的概念与代码，逐步逼近 TensorFlow C++ 的架构
（Tensor/Allocator → Op/Registry → Graph → Executor/并发 → Device 抽象 → 内存与生命周期优化 → 自动求导/XLA 风格 IR（选学）→ 分布式雏形（选学））。

下面给你一份清晰路线，并**直接从第 1 迭代开始交付可以编译运行的代码**。你跟着做就行，每次跑通后我再带你上一个台阶。

---

# 学习路线（8 个迭代）

1) **Tensor & Allocator（本次）**  
   - 目标：搞定张量的形状、数据类型、内存分配（对齐/跨平台释放），奠定一切的基座。  
   - 对应 TF：`tensorflow/core/framework/tensor.h`、`allocator.h`、`tensor_shape.h`

2) **Op & Kernel & Registry**  
   - 目标：定义 `Op` 接口（算子）与 `Kernel` 实现、注册/查表创建实例。  
   - 对应 TF：`op.h`、`op_kernel.h`、`registry.h`

3) **Graph（节点/边/属性）**  
   - 目标：有向无环图建模、拓扑序与依赖表示。  
   - 对应 TF：`graph/graph.h`

4) **Executor（并发调度与错误传播）**  
   - 目标：线程池调度、就绪队列、失败传播与取消。  
   - 对应 TF：`common_runtime/executor.cc`

5) **Device 抽象（CPU 起步）**  
   - 目标：设备接口、Tensor 在设备上的分配/拷贝。  
   - 对应 TF：`device.h`、`cpu_device_factory.cc`

6) **内存与生命周期优化**  
   - 目标：Buffer 复用、Arena/Slab、小对象优化、零拷贝路径。  
   - 对应 TF：`BFCAllocator` 等

7) **自动求导/IR（选学）**  
   - 目标：静态图的梯度构造，或构建简化 IR 与 Pass 管线。  
   - 对应 TF：`graph/gradients`、XLA HLO 基础

8) **分布式雏形（选学）**  
   - 目标：最小 RPC 通信与参数同步（AllReduce 原型）。  
   - 对应 TF：`distributed_runtime/`

---

# 迭代 1：Tensor & Allocator（可直接编译运行）

**本迭代完成后你将掌握：**
- 基本 DType/Shape 表达  
- 跨平台对齐分配与释放（Linux/macOS/Windows）  
- `Tensor` 的生命周期、`bytes()`/`NumElements()`/类型安全的 `data()` 访问

> 编译要求：C++17；Linux/macOS/Windows 任一；命令示例：  
> `g++ -std=gnu++17 -O2 -DNDEBUG minitf_iter1.cpp -o iter1`

把下面这段保存为 `minitf_iter1.cpp`，直接编译运行即可。

*/
// minitf_iter1.cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
  #include <malloc.h>
#endif

// ---------- DType ----------
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

// ---------- Shape ----------
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

// ---------- 跨平台对齐分配 ----------
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

// ---------- Allocator ----------
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

// ---------- Tensor ----------
class Tensor {
 public:
  Tensor() = default;

  Tensor(DType dtype, Shape shape, Allocator& alloc)
      : dtype_(dtype), shape_(std::move(shape)), alloc_(&alloc) {
    bytes_ = static_cast<size_t>(shape_.NumElements()) * SizeOf(dtype_);
    if (bytes_ > 0) {
      data_ = alloc_->Allocate(bytes_, 64);
    }
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

  template <typename T>
  T* data() { return reinterpret_cast<T*>(data_); }

  template <typename T>
  const T* data() const { return reinterpret_cast<const T*>(data_); }

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
    data_ = nullptr;
    bytes_ = 0;
    alloc_ = nullptr;
  }

  void MoveFrom(Tensor&& other) {
    dtype_ = other.dtype_;
    shape_ = std::move(other.shape_);
    alloc_ = other.alloc_;
    data_  = other.data_;
    bytes_ = other.bytes_;
    other.alloc_ = nullptr;
    other.data_ = nullptr;
    other.bytes_ = 0;
  }

  DType dtype_{DType::kFloat32};
  Shape shape_;
  Allocator* alloc_{nullptr};
  void* data_{nullptr};
  size_t bytes_{0};
};

// ---------- Demo ----------
int main() {
  CPUAllocator alloc;

  Tensor t(DType::kFloat32, Shape({2, 3}), alloc);
  assert(t.NumElements() == 6);
  assert(t.bytes() == 6 * SizeOf(DType::kFloat32));

  float* p = t.data<float>();
  for (int i = 0; i < 6; ++i) p[i] = static_cast<float>(i) * 0.5f;

  std::cout << t.DebugString() << "\n";
  std::cout << "t[5] = " << p[5] << "\n";

  Tensor t2 = std::move(t);
  std::cout << t2.DebugString() << "\n";
  std::cout << "OK\n";
  return 0;
}



/*

做两个小练习（非常关键）：
练习 A： 新增 DType::kFloat64 并在 Demo 中用 double 跑同样的测试。
练习 B： 给 Tensor 添加一个 Resize(new_shape) 方法（确保重新分配内存、旧数据作废），并验证 bytes() 变化正确。

与 TensorFlow 正文的映射与差异（本迭代）
映射：
Tensor 的基本职责、Allocator 抽象、对齐分配的必要性，与 TF 十分接近。
简化：
我们没有实现 TensorBuffer/引用计数、也没有跨设备拷贝路径与小对象优化，这些将在第 5、6 迭代逐步补齐。
注意：
这里禁用了拷贝，仅支持移动，避免新手阶段的隐式深拷贝成本；TF 内部通过更严谨的 buffer 共享/引用计数来解决，这会在后续演进。


*/