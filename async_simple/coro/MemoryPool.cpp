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

#include "async_simple/coro/MemoryPool.h"
#include <cstdlib>
#include <new>

namespace async_simple {
namespace coro {

thread_local MemoryPool::ThreadLocalPool MemoryPool::_thread_local_pool;

MemoryPool& MemoryPool::instance() {
    static MemoryPool pool;
    return pool;
}

void MemoryPool::initialize(const MemoryPoolConfig& config) {
    _config = config;
    
    // 预分配一些内存块到线程本地池
    ThreadLocalPool& tlp = _thread_local_pool;
    if (!tlp.free_list && tlp.block_count == 0) {
        for (size_t i = 0; i < config.preallocate_blocks; ++i) {
            Block* block = createBlock(config.block_size);
            if (block) {
                block->next = tlp.free_list;
                tlp.free_list = block;
                tlp.block_count++;
            }
        }
    }
}

void* MemoryPool::allocate(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    
    // 对齐到块大小
    size_t aligned_size = ((size + _config.block_size - 1) / _config.block_size) * _config.block_size;
    
    return allocateFromPool(aligned_size);
}

void MemoryPool::deallocate(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return;
    }
    
    size_t aligned_size = ((size + _config.block_size - 1) / _config.block_size) * _config.block_size;
    
    deallocateToPool(ptr, aligned_size);
}

MemoryPool::PoolStats MemoryPool::getStats() const {
    PoolStats stats;
    stats.total_allocated = _total_allocated.load();
    stats.total_used = _total_used.load();
    stats.total_blocks = _total_blocks.load();
    
    // 计算空闲块数量需要遍历所有线程本地池，这可能比较慢
    // 这里简化处理，只返回一个估计值
    stats.free_blocks = stats.total_blocks - (stats.total_used / _config.block_size);
    
    return stats;
}

MemoryPool::ThreadLocalPool::~ThreadLocalPool() {
    Block* current = free_list;
    while (current) {
        Block* next = current->next;
        MemoryPool::instance().destroyBlock(current);
        current = next;
    }
}

void* MemoryPool::allocateFromPool(size_t size) {
    ThreadLocalPool& tlp = _thread_local_pool;
    
    // 尝试从线程本地池获取内存
    if (tlp.free_list && size <= _config.block_size) {
        Block* block = tlp.free_list;
        tlp.free_list = block->next;
        tlp.block_count--;
        
        _total_used.fetch_add(size);
        return block->data;
    }
    
    // 线程本地池没有可用内存，创建新块
    Block* block = createBlock(size);
    if (!block) {
        return nullptr;
    }
    
    _total_used.fetch_add(size);
    return block->data;
}

void MemoryPool::deallocateToPool(void* ptr, size_t size) {
    ThreadLocalPool& tlp = _thread_local_pool;
    
    // 如果块大小超过配置或线程本地池已满，直接销毁
    if (size > _config.block_size || tlp.block_count >= _config.max_blocks_per_thread) {
        destroyBlock(reinterpret_cast<Block*>(ptr) - 1);
        _total_used.fetch_sub(size);
        return;
    }
    
    // 将块放回线程本地池
    Block* block = reinterpret_cast<Block*>(ptr) - 1;
    block->next = tlp.free_list;
    tlp.free_list = block;
    tlp.block_count++;
    
    _total_used.fetch_sub(size);
}

MemoryPool::Block* MemoryPool::createBlock(size_t size) {
    size_t total_size = sizeof(Block) + size;
    void* raw_memory = std::malloc(total_size);
    if (!raw_memory) {
        return nullptr;
    }
    
    Block* block = new (raw_memory) Block();
    
    _total_allocated.fetch_add(total_size);
    _total_blocks.fetch_add(1);
    
    return block;
}

void MemoryPool::destroyBlock(Block* block) {
    size_t total_size = sizeof(Block) + _config.block_size;
    
    block->~Block();
    std::free(block);
    
    _total_allocated.fetch_sub(total_size);
    _total_blocks.fetch_sub(1);
}

} // namespace coro
} // namespace async_simple