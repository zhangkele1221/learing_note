//https://juejin.cn/post/6844904130989801479
//http://heavensheep.xyz/?p=295

/*  https://www.apude.com/blog/8855.html
数据分为前台和后台
所有读线程读前台数据，不用加锁，通过一个指针来指向当前读的前台数据
只有一个线程负责更新，更新的时候，先准备好后台数据，接着直接切指针，这之后所有新进来的读请求都看到了新的前台数据
有部分读还落在老的前台那里处理，因为更新还不算完成，也就不能退出写线程，写线程需要等待所有落在老前台的线程读完成后，才能退出，在退出之前，顺便再更新一遍老前台数据（也就当前的新后台），可以保证前后台数据一致，这点在做增量更新的时候有用
*/

/*
工程实现上需要攻克的难点
写线程要怎么知道所有的读线程在老前台中的读完成了呢？
一种做法是让各个读线程都维护一把锁，读的时候锁住，这时候不会影响其他线程的读，但会影响写，读完后释放锁(某些时候可能会有通知写线程的开销，但写本身很少)，写线程只需要确认锁有没有释放了，确认完了后马上释放，确认这个动作非常快（小于25ns，1s=103ms=106us=10^9ns），读线程几乎不会感觉到锁的存在。
每个线程都有一把自己的锁，需要用全局的map来做线程id和锁的映射吗？
不需要，而且这样做全局map就要加全局锁了，又回到了刚开始分析中遇到的问题了。其实，每个线程可以有私有存储（thread local storage，简称TLS），如果是协程，就对应这协程的TLS（但对于go语言，官方是不支持TLS的，想实现类似功能，要么就想办法获取到TLS，要么就不要基于协程锁，而是用全局锁，但尽量让锁粒度小，本文主要针对C++语言，暂时不深入讨论其他语言的实现）。这样每个读线程锁的是自己的锁，不会影响到其他的读线程，锁的目的仅仅是为了保证读优先。
对于线程私有存储，可以使用pthread_key_create, pthread_setspecific，pthread_getspecific系列函数
*/
#include <pthread.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

namespace common {

template <typename T>
std::lock_guard<typename std::remove_reference<T>::type> get_lock_guard();

#define SCOPED_LOCK(ref_of_lock)                    \
  decltype(get_lock_guard<decltype(ref_of_lock)>()) \
      scoped_locker_dummy_at_line_##__LINE__(ref_of_lock)

template <typename T>
void delete_object(void* arg) {
  delete static_cast<T*>(arg);
}

// This data structure makes Read() almost lock-free by making Modify()
// *much* slower. It's very suitable for implementing LoadBalancers which
// have a lot of concurrent read-only ops from many threads and occasional
// modifications of data. As a side effect, this data structure can store
// a thread-local data for user.
//
// Read(): begin with a thread-local mutex locked then read the foreground
// instance which will not be changed before the mutex is unlocked. Since the
// mutex is only locked by Modify() with an empty critical section, the
// function is almost lock-free.
//
// Modify(): Modify background instance which is not used by any Read(), flip
// foreground and background, lock thread-local mutexes one by one to make
// sure all existing Read() finish and later Read() see new foreground,
// then modify background(foreground before flip) again.

class Void {};

template <typename T, typename TLS = Void>
class DoublyBufferedData {
  class Wrapper;

 public:
  class ScopedPtr {
    friend class DoublyBufferedData;

   public:
    ScopedPtr() : _data(NULL), _w(NULL) {}
    ~ScopedPtr() {
      if (_w) {
        _w->EndRead();
      }
    }

    ScopedPtr(const ScopedPtr&) = delete;
    ScopedPtr& operator=(const ScopedPtr&) = delete;

    const T* get() const { return _data; }
    const T& operator*() const { return *_data; }
    const T* operator->() const { return _data; }
    TLS& tls() { return _w->user_tls(); }

   private:
    const T* _data;
    Wrapper* _w;
  };

  DoublyBufferedData();
  ~DoublyBufferedData();

  // Put foreground instance into ptr. The instance will not be changed until
  // ptr is destructed.
  // This function is not blocked by Read() and Modify() in other threads.
  // Returns 0 on success, -1 otherwise.
  int Read(ScopedPtr* ptr);

  // Modify background and foreground instances. fn(T&, ...) will be called
  // twice. Modify() from different threads are exclusive from each other.
  // NOTE: Call same series of fn to different equivalent instances should
  // result in equivalent instances, otherwise foreground and background
  // instance will be inconsistent.
  template <typename Fn>
  size_t Modify(Fn& fn);
  template <typename Fn, typename Arg1>
  size_t Modify(Fn& fn, const Arg1&);
  template <typename Fn, typename Arg1, typename Arg2>
  size_t Modify(Fn& fn, const Arg1&, const Arg2&);

  // fn(T& background, const T& foreground, ...) will be called to background
  // and foreground instances respectively.
  template <typename Fn>
  size_t ModifyWithForeground(Fn& fn);
  template <typename Fn, typename Arg1>
  size_t ModifyWithForeground(Fn& fn, const Arg1&);
  template <typename Fn, typename Arg1, typename Arg2>
  size_t ModifyWithForeground(Fn& fn, const Arg1&, const Arg2&);

 private:
  template <typename Fn>
  struct WithFG0 {
    WithFG0(Fn& fn, T* data) : _fn(fn), _data(data) {}
    size_t operator()(T& bg) { return _fn(bg, (const T&)_data[&bg == _data]); }

   private:
    Fn& _fn;
    T* _data;
  };

  template <typename Fn, typename Arg1>
  struct WithFG1 {
    WithFG1(Fn& fn, T* data, const Arg1& arg1)
        : _fn(fn), _data(data), _arg1(arg1) {}
    size_t operator()(T& bg) {
      return _fn(bg, (const T&)_data[&bg == _data], _arg1);
    }

   private:
    Fn& _fn;
    T* _data;
    const Arg1& _arg1;
  };

  template <typename Fn, typename Arg1, typename Arg2>
  struct WithFG2 {
    WithFG2(Fn& fn, T* data, const Arg1& arg1, const Arg2& arg2)
        : _fn(fn), _data(data), _arg1(arg1), _arg2(arg2) {}
    size_t operator()(T& bg) {
      return _fn(bg, (const T&)_data[&bg == _data], _arg1, _arg2);
    }

   private:
    Fn& _fn;
    T* _data;
    const Arg1& _arg1;
    const Arg2& _arg2;
  };

  template <typename Fn, typename Arg1>
  struct Closure1 {
    Closure1(Fn& fn, const Arg1& arg1) : _fn(fn), _arg1(arg1) {}
    size_t operator()(T& bg) { return _fn(bg, _arg1); }

   private:
    Fn& _fn;
    const Arg1& _arg1;
  };

  template <typename Fn, typename Arg1, typename Arg2>
  struct Closure2 {
    Closure2(Fn& fn, const Arg1& arg1, const Arg2& arg2)
        : _fn(fn), _arg1(arg1), _arg2(arg2) {}
    size_t operator()(T& bg) { return _fn(bg, _arg1, _arg2); }

   private:
    Fn& _fn;
    const Arg1& _arg1;
    const Arg2& _arg2;
  };

  const T* UnsafeRead() const {
    return _data + _index.load(std::memory_order_acquire);
  }
  Wrapper* AddWrapper();
  void RemoveWrapper(Wrapper*);

  // Foreground and background void.
  T _data[2];

  // Index of foreground instance.
  std::atomic<int> _index;

  // Key to access thread-local wrappers.
  bool _created_key;
  pthread_key_t _wrapper_key;

  // All thread-local instances.
  std::vector<Wrapper*> _wrappers;

  // Sequence access to _wrappers.
  std::mutex _wrappers_mutex;

  // Sequence modifications.
  std::mutex _modify_mutex;
};

static const pthread_key_t INVALID_PTHREAD_KEY = (pthread_key_t)-1;

template <typename T, typename TLS>
class DoublyBufferedDataWrapperBase {
 public:
  TLS& user_tls() { return _user_tls; }

 protected:
  TLS _user_tls;
};

template <typename T>
class DoublyBufferedDataWrapperBase<T, Void> {};

template <typename T, typename TLS>
class DoublyBufferedData<T, TLS>::Wrapper
    : public DoublyBufferedDataWrapperBase<T, TLS> {
  friend class DoublyBufferedData;

 public:
  explicit Wrapper(DoublyBufferedData* c) : _control(c) {}

  ~Wrapper() {
    if (_control != NULL) {
      _control->RemoveWrapper(this);
    }
  }

  // _mutex will be locked by the calling pthread and DoublyBufferedData.
  // Most of the time, no modifications are done, so the mutex is
  // uncontended and fast.
  inline void BeginRead() { _mutex.lock(); }

  inline void EndRead() { _mutex.unlock(); }

  inline void WaitReadDone() { SCOPED_LOCK(_mutex); }

 private:
  DoublyBufferedData* _control;
  std::mutex _mutex;
};

// Called when thread initializes thread-local wrapper.
template <typename T, typename TLS>
typename DoublyBufferedData<T, TLS>::Wrapper*
DoublyBufferedData<T, TLS>::AddWrapper() {
  std::unique_ptr<Wrapper> w(new (std::nothrow) Wrapper(this));
  if (NULL == w) {
    return NULL;
  }
  try {
    SCOPED_LOCK(_wrappers_mutex);
    _wrappers.push_back(w.get());
  } catch (std::exception& e) {
    return NULL;
  }
  return w.release();
}

// Called when thread quits.
template <typename T, typename TLS>
void DoublyBufferedData<T, TLS>::RemoveWrapper(
    typename DoublyBufferedData<T, TLS>::Wrapper* w) {
  if (NULL == w) {
    return;
  }
  SCOPED_LOCK(_wrappers_mutex);
  for (size_t i = 0; i < _wrappers.size(); ++i) {
    if (_wrappers[i] == w) {
      _wrappers[i] = _wrappers.back();
      _wrappers.pop_back();
      return;
    }
  }
}

template <typename T, typename TLS>
DoublyBufferedData<T, TLS>::DoublyBufferedData()
    : _index(0), _created_key(false), _wrapper_key(0) {
  _wrappers.reserve(64);
  const int rc = pthread_key_create(&_wrapper_key, delete_object<Wrapper>);
  if (rc != 0) {
    std::cout << "Fail to pthread_key_create: " << rc << std::endl;
  } else {
    _created_key = true;
  }
  // Initialize _data for some POD types. This is essential for pointer
  // types because they should be Read() as NULL before any Modify().
  if (std::is_integral<T>::value || std::is_floating_point<T>::value ||
      std::is_pointer<T>::value || std::is_member_function_pointer<T>::value) {
    _data[0] = T();
    _data[1] = T();
  }
}

template <typename T, typename TLS>
DoublyBufferedData<T, TLS>::~DoublyBufferedData() {
  // User is responsible for synchronizations between Read()/Modify() and
  // this function.
  if (_created_key) {
    pthread_key_delete(_wrapper_key);
  }

  {
    SCOPED_LOCK(_wrappers_mutex);
    for (size_t i = 0; i < _wrappers.size(); ++i) {
      _wrappers[i]->_control = NULL;  // hack: disable removal.
      delete _wrappers[i];
    }
    _wrappers.clear();
  }
}

template <typename T, typename TLS>
int DoublyBufferedData<T, TLS>::Read(
    typename DoublyBufferedData<T, TLS>::ScopedPtr* ptr) {
  if (__builtin_expect((bool)(!_created_key), false)) {
    return -1;
  }
  Wrapper* w = static_cast<Wrapper*>(pthread_getspecific(_wrapper_key));
  if (__builtin_expect((bool)(w != NULL), true)) {
    w->BeginRead();
    ptr->_data = UnsafeRead();
    ptr->_w = w;
    return 0;
  }
  w = AddWrapper();
  if (__builtin_expect((bool)(w != NULL), true)) {
    const int rc = pthread_setspecific(_wrapper_key, w);
    if (rc == 0) {
      w->BeginRead();
      ptr->_data = UnsafeRead();
      ptr->_w = w;
      return 0;
    }
  }
  return -1;
}

template <typename T, typename TLS>
template <typename Fn>
size_t DoublyBufferedData<T, TLS>::Modify(Fn& fn) {
  // _modify_mutex sequences modifications. Using a separate mutex rather
  // than _wrappers_mutex is to avoid blocking threads calling
  // AddWrapper() or RemoveWrapper() too long. Most of the time, modifications
  // are done by one thread, contention should be negligible.
  SCOPED_LOCK(_modify_mutex);
  //首先是获得修改锁，然后调用fn修改后台数据，如果修改失败就直接返回了
  int bg_index = !_index.load(std::memory_order_relaxed);
  // background instance is not accessed by other threads, being safe to
  // modify.
  const size_t ret = fn(_data[bg_index]);
  if (ret != 0) {
    std::cout << "ret:" << ret << std::endl;
    return 0;
  }

  // Publish, flip background and foreground.
  // The release fence matches with the acquire fence in UnsafeRead() to
  // make readers which just begin to read the new foreground instance see
  // all changes made in fn.
  //修改完之后反转 index 这里用的是 release 内存序，
  //可以保证读线程一旦读到了新的index 数据的修改也一定可见，
  //在这个操作之后，所有的 read 获得的就是修改后的 新前台数据了，
  //然后依次调用所有 wrapper 的 WaitReadDone 其实就是挨个每获取完一个锁就释放，
  //确保在index反转之前的所有读取都已经结束，这个循环过后当前的后台数据就没有线程在用了，
  //可以安全修改，再次调用fn。
  _index.store(bg_index, std::memory_order_release);
  bg_index = !bg_index;

  // Wait until all threads finishes current reading. When they begin next
  // read, they should see updated _index.
  {
    SCOPED_LOCK(_wrappers_mutex);
    for (size_t i = 0; i < _wrappers.size(); ++i) {
      _wrappers[i]->WaitReadDone();
    }
  }

  const size_t ret2 = fn(_data[bg_index]);
  std::cout << "index=" << _index.load(std::memory_order_relaxed) << std::endl;
  return ret2;
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1>
size_t DoublyBufferedData<T, TLS>::Modify(Fn& fn, const Arg1& arg1) {
  Closure1<Fn, Arg1> c(fn, arg1);
  return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1, typename Arg2>
size_t DoublyBufferedData<T, TLS>::Modify(Fn& fn, const Arg1& arg1,
                                          const Arg2& arg2) {
  Closure2<Fn, Arg1, Arg2> c(fn, arg1, arg2);
  return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn& fn) {
  WithFG0<Fn> c(fn, _data);
  return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn& fn,
                                                        const Arg1& arg1) {
  WithFG1<Fn, Arg1> c(fn, _data, arg1);
  return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1, typename Arg2>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn& fn,
                                                        const Arg1& arg1,
                                                        const Arg2& arg2) {
  WithFG2<Fn, Arg1, Arg2> c(fn, _data, arg1, arg2);
  return Modify(c);
}

}  // namespace common

struct TestData {
  std::set<int> list;
};

class TestDoubleBuffer {
 public:
  bool Read() {
    common::DoublyBufferedData<TestData>::ScopedPtr s;
    if (_reload_data.Read(&s) != 0) {
      return false;
    }

    for (auto& num : s->list) {
      std::cout << num << std::endl;
    }

    std::cout << "read end." << std::endl;

    return true;
  }

  bool Add(const int num) { return _reload_data.Modify(AddData, num); }

  bool Remove(const int num) { return _reload_data.Modify(RemoveData, num); }

  bool Reset(const std::set<int>& num_set) {
    return _reload_data.Modify(ResetData, num_set);
  }

  bool Clear() { return _reload_data.Modify(ClearData); }

 private:
  static bool AddData(TestData& data, const int num);
  static bool RemoveData(TestData& data, const int num);
  static bool ResetData(TestData& data, const std::set<int>& num_set);
  static bool ClearData(TestData& data);

 private:
  common::DoublyBufferedData<TestData> _reload_data;
};

bool TestDoubleBuffer::AddData(TestData& data, const int num) {
  if (data.list.find(num) != data.list.end()) {
    return true;
  } else {
    data.list.insert(num);
  }
  return true;
}

bool TestDoubleBuffer::RemoveData(TestData& data, const int num) {
  auto it = data.list.find(num);
  if (it != data.list.end()) {
    data.list.erase(it);
  }
  return true;
}

bool TestDoubleBuffer::ResetData(TestData& data, const std::set<int>& num_set) {
  data.list = num_set;
  return true;
}

bool TestDoubleBuffer::ClearData(TestData& data) {
  data.list.clear();
  return true;
}

int main() {
  std::set<int> num_set;
  num_set.insert(3);
  num_set.insert(4);

  TestDoubleBuffer test;
  test.Add(1);
  test.Add(2);
  test.Read();
  test.Remove(1);
  test.Read();
  test.Reset(num_set);
  test.Read();

  return 0;
}

