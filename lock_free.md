# 无锁代码
一般而言并发读写一个结构体三种方式.  
* 读写锁  
* 双buffer  
* RCU(read-copy update)  

上面这个三个优缺点
- 读写锁：实现简单，但高并发锁争抢严重导致性能不佳，一般用于并发度不高的多线程读写场景，并且读写临界区越大并发压力越大  
    * 说下读写锁的特性：  
        读和读没有竞争关系  
        写和写之间是互斥关系  
        读和写之间是同步互斥关系(这里的同步指的是写优先，即读写都在竞争锁的时候，写优先获得锁)

- 双 buffer：“ 无锁”双 buffer 可以实现无锁读，因此读性能极佳，但是存在写阻塞问题，所以一般用于“一写多读”配置热更新场景，且写线程往往是异步的对阻塞时长容忍度高线程安全双 buffer 读写均加锁了，但是临界区较小，相比于读写锁而言可抗的并发度更高

- RCU：实现比双 buffer 简单，并且可以实现无锁读写，但是每次更新数据都需要拷贝原始数据，在 高频写且数据较大 时存在大量的内存申请、内存拷贝和垃圾回收开销


# 
双 buffer 也被称为 PingPong Buffer  
- 它解决的是这样一个问题：在代码中可能存在一些需要热更新的配置，但是在更新过程中会被不停地读取。如果使用读写锁的话一方面存在性能问题，另一方面配置可能一直更新不了（读线程一直持有锁）   
- 原理它实现的大致原理如下：
  * 在内存中维护两个 buffer，一个是只读的（read buffer），一个是给写线程更新的（write buffer）
  * 读线程永远只从 read buffer 读取数据，在读之前令引用计数 +1，读之后令引用计数-1 
  * 写线程检查 write buffer 的引用计数，当引用计数为 1 时就会尝试独占 write buffer，抢到了就会开始 update
  * 写线程 update 结束后会切换 read buffer 和 write buffer，然后再释放 write buffer 的引用计数





## 问题及解决思路
一般而言双 buffer 都是应用在“一写多读”的场景，其中的写线程往往是异步且低频的，因此可以容忍一定的写阻塞。所以我们只需要做到无锁读即可，这中间会遇到一些问题：
1. 写线程需要执行两次写操作由于存在两份 buffer，这意味每次写线程执行写操作时都必须执行两次，保证两份 buffer 都能更新，否则就会存在版本不一致的问题。 

2. 并发写的数据不一致和 coredump 问题写线程的更新频率很低，但我并不想限制用户并发写。这主要是业务中异步写线程往往有两个：
    * update 线程：“高频”增量更新数据（这里的高频仅仅是相对于 reset 线程而言，相比于 read 线程频率依然很低），一般是秒级别增量更新数据
    * reset 线程：低频全量更新数据，一般是小时级别甚至天级别全量更新数据

并发写存在两个问题，一个是两个 buffer 的数据不一致问题。假设两个写线程 writer1 和 writer2 同时调用 Update 函数执行不同的增量更新逻辑（Update 底层需要更新两次以保证两份 buffer 都能更新），那么可能出现两份 buffer 会分别执行两次完全相同的增量更新逻辑，从而导致两份 buffer 数据不一致。  

第二个问题是并发写会导致触发 coredump 的概率更高，这是因为判断 writer buffer 引用计数为 1 和独占 write buffer 这两步不是原子的，有可能两个写线程同时以为自己独占了写 buffer。  

解决这个问题的思路很简单，给写线程加上互斥锁将写任务转成串行的即可，毕竟这不会影响我们的无锁读。

```cpp
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>


namespace util {

template <typename T>
// #include <map>
// #include <string>
// using T = std::map<int, std::string>;
class DoubleBuffer {
 public:
    using UpdaterFunc = std::function<void(T& write_buffer_data)>;

 public:
    explicit DoubleBuffer(const T& data) {
        buffers_[0] = std::make_shared<T>(data);
        buffers_[1] = std::make_shared<T>(data);
        read_idx_ = 0;
    }
    explicit DoubleBuffer(const T&& data) {
        buffers_[0] = std::make_shared<T>(std::move(data));
        buffers_[1] = std::make_shared<T>(*buffers_[0]);
        read_idx_ = 0;
    }

 public:
    /**
     * @brief Get read_buffer and you can read it without restriction
     * 
     * @return std::shared_ptr<T> 
     */
    std::shared_ptr<T> Load() {
        return buffers_[read_idx_];
    }

    /**
     * @brief It will block until it update both two buffers successfully
     *        Only once writer thread can perform this function at the same time
     *        
     */
    void Update(const UpdaterFunc& updater) {
        // use std::mutex to update exclusively in multiple writer thread scenarios
        std::lock_guard<std::mutex> lock(write_mtx_);
        std::shared_ptr<T> write_buffer = monopolizer_writer_buffer();

        // update both two buffers, sleep to avoid data race on std::shared_ptr
        updater(*write_buffer);
        this->swap_buffer();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        updater(*write_buffer);
    }

    /**
     * @brief Reset the value of write buffer
     * 
     * @param data 
     */
    void Reset(const T& data) {
        this->Update([&](T& write_buffer_data) {
            // deep copy
            write_buffer_data = data;
        });
    }

 private:
    // wait all reads task on this buffer to get write buffer
    std::shared_ptr<T> monopolizer_writer_buffer() {
        int write_idx = 1 - read_idx_;
        while (buffers_[write_idx].use_count() != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return buffers_[write_idx];
    }

    void swap_buffer() {
        read_idx_ = 1 - read_idx_;
    }

private:
    std::shared_ptr<T> buffers_[2];
    std::atomic<int> read_idx_;
    std::mutex write_mutx_;
};

}// namespace util

```
```cpp

#include <sys/time.h>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>

#include "double_buffer.h"

const int QPS = 10000;
const int READ_THREAD_CNT = 20;
const int UPDATE_THREAD_CNT = 3;
const int RESET_THREAD_CNT = 1;

int rand_int(int max) {
    static unsigned int seed = 1234;
    return rand_r(&seed) % max;
}

uint64_t timestamp_us() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 * 1000 + time.tv_usec;
}

void reader(util::DoubleBuffer<std::map<int, std::string>>& db) {
    while (1) {
        auto t_start_us = timestamp_us();
        int not_empty_cnt = 0;
        auto data_sptr = db.Load();
        for (int i = 0; i < 100; i++) {
            auto iter = data_sptr->find(rand_int(1000000));
            if (iter != data_sptr->end()) {
                not_empty_cnt++;
            }
        }
        if (rand_int(QPS * 10) == 0) {
            printf("[read]not empty key cnt:%d map len:%ld cost:%ld us\n", not_empty_cnt, data_sptr->size(),
                   timestamp_us() - t_start_us);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / QPS * READ_THREAD_CNT));
    }
}

void update_writer(util::DoubleBuffer<std::map<int, std::string>>& db) {
    while (1) {
        // you can use lambda expression to pass any parameter
        auto t_start_us = timestamp_us();
        db.Update([](std::map<int, std::string>& data) {
            for (int i = 100; i < 200; i++) {
                data[rand_int(1000000)] = std::to_string(i * i);
            }
        });
        if (rand_int(200) == 0) {
            printf("[update writer]cost %ld us\n", timestamp_us() - t_start_us);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void reset_writer(util::DoubleBuffer<std::map<int, std::string>>& db) {
    while (1) {
        std::map<int, std::string> data;
        for (int i = 0; i < 1000; i++) {
            data[rand_int(100)] = std::to_string(i);
        }
        auto t_start_us = timestamp_us();
        db.Reset(data);
        printf("[reset writer]cost %ld us\n", timestamp_us() - t_start_us);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// g++ -g test.cpp  -o test -pthread -std=c++11
int main() {
    // initialization
    std::map<int, std::string> data;
    for (int i = 0; i < 1000; i++) {
        data[i] = std::to_string(i * i);
    }
    util::DoubleBuffer<std::map<int, std::string>> double_buffer(std::move(data));
    std::vector<std::thread> threads;

    // reader thread
    for (int i = 0; i < READ_THREAD_CNT; i++) {
        threads.push_back(std::thread(reader, std::ref(double_buffer)));
    }

    // update thread
    for (int i = 0; i < UPDATE_THREAD_CNT; i++) {
        threads.push_back(std::thread(update_writer, std::ref(double_buffer)));
    }

    // reset thread
    for (int i = 0; i < RESET_THREAD_CNT; i++) {
        threads.push_back(std::thread(reset_writer, std::ref(double_buffer)));
    }

    for (auto&& thread : threads) {
        thread.join();
    }

    return 0;
}


```


