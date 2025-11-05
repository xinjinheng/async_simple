/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
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

#ifndef ASYNC_SIMPLE_USE_MODULES
#include <chrono>
#include <variant>
#include "async_simple/Signal.h"
#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Sleep.h"

#endif  // ASYNC_SIMPLE_USE_MODULES

namespace async_simple {
namespace coro {

// Timeout for Lazy coroutine
// If the task doesn't complete within the given duration, it will be canceled
// and a std::error_code with std::errc::operation_canceled will be returned
// Example usage:
// auto result = co_await timeout(executor, myLazyTask(), 1s);
// if (result.has_error() && result.error() == std::errc::operation_canceled) {
//     // handle timeout
// }

template <typename T, typename Rep, typename Period>
Lazy<Try<T>> timeout(Executor* ex, Lazy<T> task, std::chrono::duration<Rep, Period> timeout_duration) {
    auto timeout_task = sleep(ex, timeout_duration);
    
    auto result = co_await collectAny<SignalType::Terminate>(std::move(task), std::move(timeout_task));
    
    if (result.index() == 1) {
        // Timeout occurred
        co_return Try<T>(std::make_exception_ptr(std::system_error(
            std::errc::operation_canceled, std::generic_category(), "timeout")));
    }
    else {
        // Task completed successfully
        co_return std::get<0>(std::move(result));
    }
}

template <typename T, typename Rep, typename Period>
Lazy<Try<T>> timeout(Lazy<T> task, std::chrono::duration<Rep, Period> timeout_duration) {
    auto ex = co_await CurrentExecutor();
    co_return co_await timeout(ex, std::move(task), timeout_duration);
}

// Timeout for Future
// If the future doesn't complete within the given duration, it will be canceled
// and a std::error_code with std::errc::operation_canceled will be returned
template <typename T, typename Rep, typename Period>
Lazy<Try<T>> timeout(Executor* ex, Future<T> future, std::chrono::duration<Rep, Period> timeout_duration) {
    auto future_task = [future = std::move(future)]() mutable -> Lazy<T> {
        co_return co_await std::move(future);
    };
    
    co_return co_await timeout(ex, future_task(), timeout_duration);
}

template <typename T, typename Rep, typename Period>
Lazy<Try<T>> timeout(Future<T> future, std::chrono::duration<Rep, Period> timeout_duration) {
    auto ex = co_await CurrentExecutor();
    co_return co_await timeout(ex, std::move(future), timeout_duration);
}

}  // namespace coro
}  // namespace async_simple

#endif