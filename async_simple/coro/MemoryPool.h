/*
 * Copyright (c) 2023, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ASYNC_SIMPLE_CORO_MEMORYPOOL_H
#define ASYNC_SIMPLE_CORO_MEMORYPOOL_H

#include <cstddef>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "async_simple/Common.h"

namespace async_simple {
namespace coro {

// 内存池配置
struct MemoryPoolConfig {
    size_t block_size = 4096;  // 默认内存块大小
    size_t max_blocks_per_thread = 1024;  // 每个线程最大内存块数量
    size_t preallocate_blocks = 8;  // 线程本地预分配块数量
};

// 内存池单例类
class MemoryPool {
public:
    static MemoryPool& instance();
    
    // 初始化内存池
    void initialize(const MemoryPoolConfig& config = MemoryPoolConfig());
    
    // 分配内存
    void* allocate(size_t size);
    
    // 释放内存
    void deallocate(void* ptr, size_t size);
    
    // 获取内存池状态
    struct PoolStats {
        size_t total_allocated = 0;
        size_t total_used = 0;
        size_t total_blocks = 0;
        size_t free_blocks = 0;
    };
    
    PoolStats getStats() const;
    
private:
    MemoryPool() = default;
    ~MemoryPool() = default;
    
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    struct Block {
        Block* next = nullptr;
        char data[0];  // 柔性数组，用于存储实际数据
    };
    
    struct ThreadLocalPool {
        Block* free_list = nullptr;
        size_t block_count = 0;
        ~ThreadLocalPool();
    };
    
    void* allocateFromPool(size_t size);
    void deallocateToPool(void* ptr, size_t size);
    
    Block* createBlock(size_t size);
    void destroyBlock(Block* block);
    
    MemoryPoolConfig _config;
    std::atomic<size_t> _total_allocated{0};
    std::atomic<size_t> _total_used{0};
    std::atomic<size_t> _total_blocks{0};
    
    // 线程本地内存池
    static thread_local ThreadLocalPool _thread_local_pool;
};

// 内存池分配器
template <typename T>
class PoolAllocator {
public:
    using value_type = T;
    
    PoolAllocator() = default;
    
    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept {
    }
    
    T* allocate(size_t n) {
        size_t size = n * sizeof(T);
        void* ptr = MemoryPool::instance().allocate(size);
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }
    
    void deallocate(T* ptr, size_t n) noexcept {
        size_t size = n * sizeof(T);
        MemoryPool::instance().deallocate(ptr, size);
    }
    
    template <typename U>
    bool operator==(const PoolAllocator<U>&) const noexcept {
        return true;
    }
    
    template <typename U>
    bool operator!=(const PoolAllocator<U>&) const noexcept {
        return false;
    }
};

} // namespace coro
} // namespace async_simple

#endif // ASYNC_SIMPLE_CORO_MEMORYPOOL_H