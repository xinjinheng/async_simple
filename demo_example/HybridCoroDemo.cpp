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

#include <iostream>
#include <chrono>
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Timeout.h"
#include "async_simple/coro/HybridScheduler.h"
#include "async_simple/coro/TaskScheduler.h"
#include "async_simple/coro/MemoryPool.h"
#include "async_simple/executors/SimpleExecutor.h"

using namespace async_simple;
using namespace async_simple::coro;
using namespace std::chrono_literals;

// 模拟一个深函数调用场景
Lazy<int> deepFunctionCall(size_t depth) {
    if (depth == 0) {
        co_return 42;
    }
    
    // 模拟函数调用
    int result = co_await deepFunctionCall(depth - 1);
    co_return result * 2;
}

// 模拟一个IO密集型操作
Lazy<std::string> ioIntensiveOperation() {
    // 模拟IO延迟
    co_await sleep(50ms);
    co_return "IO operation completed";
}

// 模拟一个HTTP请求处理流程
Lazy<void> handleHttpRequest(HybridScheduler& scheduler) {
    try {
        // 前期：参数解析、权限校验等深函数调用场景
        scheduler.setIORatio(0.0);  // 低IO占比
        std::cout << "Handling HTTP request - deep function calls..." << std::endl;
        int result = co_await scheduler.schedule(deepFunctionCall, 40);  // 深调用
        std::cout << "Deep function call result: " << result << std::endl;
        
        // 后期：数据库IO、网络请求等IO密集操作
        scheduler.setIORatio(0.8);  // 高IO占比
        std::cout << "Handling HTTP request - IO operations..." << std::endl;
        std::string io_result = co_await scheduler.schedule(ioIntensiveOperation);
        std::cout << "IO operation result: " << io_result << std::endl;
        
        std::cout << "HTTP request handled successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error handling HTTP request: " << e.what() << std::endl;
    }
    
    co_return;
}

// 模拟分布式任务A
Lazy<void> taskA() {
    std::cout << "Executing task A..." << std::endl;
    co_await sleep(100ms);
    std::cout << "Task A completed" << std::endl;
    co_return;
}

// 模拟分布式任务B，依赖于任务A
Lazy<void> taskB() {
    std::cout << "Executing task B..." << std::endl;
    co_await sleep(50ms);
    std::cout << "Task B completed" << std::endl;
    co_return;
}

// 模拟分布式任务C，依赖于任务B
Lazy<void> taskC() {
    std::cout << "Executing task C..." << std::endl;
    co_await sleep(75ms);
    std::cout << "Task C completed" << std::endl;
    co_return;
}

// 模拟一个可能超时的任务
Lazy<int> potentiallySlowTask() {
    co_await sleep(200ms);
    co_return 42;
}

int main() {
    // 初始化内存池
    MemoryPool::instance().initialize();
    
    // 创建执行器
    executors::SimpleExecutor executor(4);
    
    // 创建混合调度器
    HybridScheduler hybrid_scheduler(&executor);
    
    // 创建任务调度器
    TaskScheduler task_scheduler(&executor);
    
    // 测试1：混合协程HTTP服务调度
    std::cout << "=== Test 1: Hybrid Coroutine HTTP Service Scheduling ===" << std::endl;
    syncAwait(handleHttpRequest(hybrid_scheduler));
    std::cout << std::endl;
    
    // 测试2：分布式任务调度
    std::cout << "=== Test 2: Distributed Task Scheduling ===" << std::endl;
    TaskId task_a_id = task_scheduler.submitTask(taskA);
    TaskId task_b_id = task_scheduler.submitDependentTask({task_a_id}, taskB);
    TaskId task_c_id = task_scheduler.submitDependentTask({task_b_id}, taskC);
    
    auto all_tasks_result = syncAwait(task_scheduler.waitForAllTasks());
    std::cout << "All tasks completed. Results: " << std::endl;
    for (size_t i = 0; i < all_tasks_result.size(); ++i) {
        std::cout << "Task " << i + 1 << ": " << 
            (all_tasks_result[i].status == TaskStatus::Completed ? "Completed" : "Failed") << std::endl;
    }
    std::cout << std::endl;
    
    // 测试3：超时控制
    std::cout << "=== Test 3: Coroutine Timeout Control ===" << std::endl;
    try {
        // 超时时间设置为100ms，而任务需要200ms
        auto result = syncAwait(withTimeout(potentiallySlowTask(), 100ms));
        std::cout << "Task completed successfully with result: " << result << std::endl;
    } catch (const TimeoutError& e) {
        std::cout << "Task timed out as expected: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }
    
    // 测试4：内存池状态
    std::cout << "=== Test 4: Memory Pool Status ===" << std::endl;
    auto stats = MemoryPool::instance().getStats();
    std::cout << "Total allocated memory: " << stats.total_allocated << " bytes" << std::endl;
    std::cout << "Total used memory: " << stats.total_used << " bytes" << std::endl;
    std::cout << "Total memory blocks: " << stats.total_blocks << std::endl;
    std::cout << "Free memory blocks: " << stats.free_blocks << std::endl;
    
    return 0;
}