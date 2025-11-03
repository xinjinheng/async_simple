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

#ifndef ASYNC_SIMPLE_CORO_TIMEOUT_H
#define ASYNC_SIMPLE_CORO_TIMEOUT_H

#include <chrono>
#include <memory>
#include <system_error>
#include "async_simple/Common.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/Executor.h"
#include "async_simple/Signal.h"

namespace async_simple {
namespace coro {

// 超时错误类
class TimeoutError : public std::runtime_error {
public:
    TimeoutError() : std::runtime_error("Operation timed out") {}
};

// 超时控制器
class Timeout {
public:
    // 创建一个超时控制器，超时时间为duration
    template <typename Rep, typename Period>
    explicit Timeout(std::chrono::duration<Rep, Period> duration)
        : _duration(std::chrono::duration_cast<std::chrono::microseconds>(duration))
    {}
    
    // 为Lazy任务添加超时控制
    template <typename T>
    Lazy<T> operator()(Lazy<T> lazy);
    
private:
    std::chrono::microseconds _duration;
    
    // 超时任务的实现
    template <typename T>
    struct TimeoutAwaiter {
        TimeoutAwaiter(Lazy<T> lazy, std::chrono::microseconds duration)
            : _lazy(std::move(lazy))
            , _duration(duration)
            , _signal(Signal::create())
        {}
        
        bool await_ready() const noexcept {
            return false;
        }
        
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> handle) {
            // 获取当前执行器
            auto executor = handle.promise()._executor;
            assert(executor != nullptr && "Executor is required for timeout");
            
            // 设置超时定时器
            _timeout_task = [this, handle]() {
                if (!_completed) {
                    _completed = true;
                    _signal->emits(SignalType::Terminate);
                    handle.resume();
                }
            };
            
            // 在指定时间后执行超时任务
            executor->scheduleAfter(_duration.count(), std::move(_timeout_task));
            
            // 启动原始任务
            _lazy.setLazyLocal(_signal.get()).start([this, handle](Try<T> &&result) {
                if (!_completed) {
                    _completed = true;
                    _result = std::move(result);
                    handle.resume();
                }
            });
        }
        
        T await_resume() {
            if (_result.hasValue()) {
                return std::move(_result.value());
            } else if (_result.hasException()) {
                std::rethrow_exception(_result.exception());
            } else {
                throw TimeoutError();
            }
        }
        
    private:
        Lazy<T> _lazy;
        std::chrono::microseconds _duration;
        std::shared_ptr<Signal> _signal;
        Executor::Func _timeout_task;
        std::atomic<bool> _completed{false};
        Try<T> _result;
    };
};

// 为Lazy任务添加超时控制的函数模板
template <typename T>
Lazy<T> Timeout::operator()(Lazy<T> lazy) {
    return Lazy<T>([lazy = std::move(lazy), duration = _duration]() mutable -> Lazy<T> {
        co_return co_await TimeoutAwaiter<T>(std::move(lazy), duration);
    }());
}

// 便捷函数，为Lazy任务添加超时控制
template <typename Rep, typename Period, typename T>
Lazy<T> withTimeout(Lazy<T> lazy, std::chrono::duration<Rep, Period> duration) {
    return Timeout(duration)(std::move(lazy));
}

} // namespace coro
} // namespace async_simple

#endif // ASYNC_SIMPLE_CORO_TIMEOUT_H