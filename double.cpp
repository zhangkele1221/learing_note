#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>


#include <sys/time.h>
#include <iostream>
#include <map>
#include <random>
#include <string>





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
    std::mutex write_mtx_;
};

}// namespace util


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


