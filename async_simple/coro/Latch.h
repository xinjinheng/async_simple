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

#ifndef ASYNC_SIMPLE_CORO_LATCH_H
#define ASYNC_SIMPLE_CORO_LATCH_H

#ifndef ASYNC_SIMPLE_USE_MODULES
#include <cstddef>
#include <vector>
#include <functional>
#include "async_simple/coro/ConditionVariable.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SpinLock.h"

#endif  // ASYNC_SIMPLE_USE_MODULES

namespace async_simple::coro {

// The latch class is a downward counter of type std::size_t which can be
// used to synchronize coroutines. The value of the counter is initialized on
// creation. Coroutines may block on the latch until the counter is decremented
// to zero. It will suspend the current coroutine and switch to other coroutines
// to run.
// There is no possibility to increase or reset the counter, which
// makes the latch a single-use barrier.
class Latch {
public:
    explicit Latch(std::size_t count) : count_(count) {}
    
    ~Latch() {
        // Auto-release all registered resources
        releaseAllResources();
    }
    
    // Register a resource to be automatically released
    template <typename Resource, typename ReleaseFunc = std::function<void(Resource&&)>>
    void registerResource(Resource&& resource, ReleaseFunc releaseFunc) {
        auto lk = mutex_.lock();
        _resources.emplace_back(
            [resource = std::forward<Resource>(resource), releaseFunc = std::move(releaseFunc)]() mutable {
                releaseFunc(std::forward<Resource>(resource));
            }
        );
    }
    
    // Register a file descriptor resource
    void registerFileDescriptor(int fd) {
        registerResource(fd, [](int fd) {
            if (fd != -1) {
                ::close(fd);
            }
        });
    }
    
    // Register a socket resource
    void registerSocket(int sockfd) {
        registerResource(sockfd, [](int sockfd) {
            if (sockfd != -1) {
                ::close(sockfd);
            }
        });
    }
    ~Latch() = default;
    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    // decrements the counter in a non-blocking manner
    Lazy<void> count_down(std::size_t update = 1) {
        auto lk = co_await mutex_.coScopedLock();
        assert(count_ >= update);
        count_ -= update;
        if (!count_) {
            cv_.notify();
        }
    }

    // tests if the internal counter equals zero
    Lazy<bool> try_wait() const noexcept {
        auto lk = co_await mutex_.coScopedLock();
        co_return !count_;
    }

    // blocks until the counter reaches zero
    // If the counter is not 0, the current coroutine will be suspended
    Lazy<void> wait() const noexcept {
        try {
            auto lk = co_await mutex_.coScopedLock();
            co_await cv_.wait(mutex_, [&] { return count_ == 0; });
            // Release resources when wait completes successfully
            const_cast<Latch*>(this)->releaseAllResources();
        }
        catch (...) {
            // Release resources when wait is interrupted by exception
            const_cast<Latch*>(this)->releaseAllResources();
            throw;
        }
    }

    // decrements the counter and blocks until it reaches zero
    Lazy<void> arrive_and_wait(std::size_t update = 1) noexcept {
        try {
            co_await count_down(update);
            co_await wait();
            // Release resources when wait completes successfully
            releaseAllResources();
        }
        catch (...) {
            // Release resources when wait is interrupted by exception
            releaseAllResources();
            throw;
        }
    }

private:
    // Release all registered resources
    void releaseAllResources() {
        auto lk = mutex_.lock();
        for (auto& releaseFunc : _resources) {
            try {
                releaseFunc();
            }
            catch (...) {
                // Ignore exceptions during resource release
            }
        }
        _resources.clear();
    }
    
private:
    using MutexType = SpinLock;

    mutable MutexType mutex_;
    mutable ConditionVariable<MutexType> cv_;
    std::size_t count_;
    
    // Resources to be automatically released
    std::vector<std::function<void()>> _resources;

};

}  // namespace async_simple::coro

#endif
