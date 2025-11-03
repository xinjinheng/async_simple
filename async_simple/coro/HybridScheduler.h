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

#ifndef ASYNC_SIMPLE_CORO_HYBRIDSCHEDULER_H
#define ASYNC_SIMPLE_CORO_HYBRIDSCHEDULER_H

#include <atomic>
#include <memory>
#include <functional>
#include "async_simple/Common.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/uthread/Async.h"
#include "async_simple/uthread/Await.h"
#include "async_simple/Executor.h"

namespace async_simple {
namespace coro {

// 混合调度器配置
struct HybridSchedulerConfig {
    size_t max_call_depth = 32;  // 切换到有栈协程的最大调用深度
    double io_ratio_threshold = 0.5;  // 切换到无栈协程的IO操作占比阈值
    size_t stack_size = 128 * 1024;  // 有栈协程的栈大小
};

// 混合调度器类
class HybridScheduler {
public:
    HybridScheduler(Executor* executor, const HybridSchedulerConfig& config = HybridSchedulerConfig());
    
    // 调度任务，根据当前上下文自动选择协程类型
    template <typename Func, typename... Args>
    auto schedule(Func&& func, Args&&... args);
    
    // 获取当前调用深度
    size_t getCurrentCallDepth() const;
    
    // 设置IO操作占比
    void setIORatio(double ratio);
    
private:
    Executor* _executor;
    HybridSchedulerConfig _config;
    
    // 线程本地上下文
    struct ThreadLocalContext {
        size_t call_depth = 0;
        double io_ratio = 0.0;
    };
    
    static thread_local ThreadLocalContext _thread_local_context;
    
    // 执行Lazy任务（无栈协程）
    template <typename Func, typename... Args>
    auto executeLazy(Func&& func, Args&&... args);
    
    // 执行Uthread任务（有栈协程）
    template <typename Func, typename... Args>
    auto executeUthread(Func&& func, Args&&... args);
};

// 混合调度器实现
HybridScheduler::HybridScheduler(Executor* executor, const HybridSchedulerConfig& config)
    : _executor(executor)
    , _config(config)
{
    assert(executor != nullptr && "Executor is required for HybridScheduler");
}

thread_local HybridScheduler::ThreadLocalContext HybridScheduler::_thread_local_context;

size_t HybridScheduler::getCurrentCallDepth() const {
    return _thread_local_context.call_depth;
}

void HybridScheduler::setIORatio(double ratio) {
    _thread_local_context.io_ratio = ratio;
}

template <typename Func, typename... Args>
auto HybridScheduler::schedule(Func&& func, Args&&... args) {
    ThreadLocalContext& ctx = _thread_local_context;
    
    // 根据调用深度和IO占比决定使用哪种协程
    if (ctx.call_depth > _config.max_call_depth || ctx.io_ratio < _config.io_ratio_threshold) {
        // 调用深度较深或IO占比较低，使用有栈协程
        return executeUthread(std::forward<Func>(func), std::forward<Args>(args)...);
    } else {
        // 调用深度较浅或IO占比较高，使用无栈协程
        return executeLazy(std::forward<Func>(func), std::forward<Args>(args)...);
    }
}

template <typename Func, typename... Args>
auto HybridScheduler::executeLazy(Func&& func, Args&&... args) {
    ThreadLocalContext& ctx = _thread_local_context;
    
    // 创建一个包装函数，用于跟踪调用深度
    auto wrapped_func = [&ctx, func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        ctx.call_depth++;
        auto result = std::apply(func, std::move(args));
        ctx.call_depth--;
        return result;
    };
    
    // 返回Lazy任务
    return wrapped_func();
}

template <typename Func, typename... Args>
auto HybridScheduler::executeUthread(Func&& func, Args&&... args) {
    using ReturnType = std::invoke_result_t<Func, Args...>;
    
    // 如果返回类型是Lazy，则直接执行
    if constexpr (std::is_convertible_v<ReturnType, Lazy<void>>) {
        return uthread::async(uthread::Launch::Schedule, uthread::Attribute{_executor, _config.stack_size}, 
            std::forward<Func>(func), std::forward<Args>(args)...);
    } 
    // 如果返回类型是Future，则将其转换为Lazy
    else if constexpr (std::is_convertible_v<ReturnType, Future<void>>) {
        return uthread::async(uthread::Launch::Schedule, uthread::Attribute{_executor, _config.stack_size}, 
            [func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                auto future = std::apply(func, std::move(args));
                uthread::await(future);
            });
    }
    // 其他类型，直接执行并返回Future
    else {
        return uthread::async(uthread::Launch::Schedule, uthread::Attribute{_executor, _config.stack_size}, 
            std::forward<Func>(func), std::forward<Args>(args)...);
    }
}

} // namespace coro
} // namespace async_simple

#endif // ASYNC_SIMPLE_CORO_HYBRIDSCHEDULER_H