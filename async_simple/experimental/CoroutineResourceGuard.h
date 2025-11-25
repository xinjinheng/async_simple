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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_COROUTINE_RESOURCE_GUARD_H
#define ASYNC_SIMPLE_EXPERIMENTAL_COROUTINE_RESOURCE_GUARD_H

#include <memory>
#include <weak_ptr>
#include <atomic>
#include <string>
#include <stdexcept>
#include "async_simple/Executor.h"
#include "async_simple/Promise.h"
#include "async_simple/experimental/Common.h"

namespace async_simple {
namespace experimental {

// Resource state enum
enum class ResourceState {
    Valid,
    Invalid,
    Destroyed
};

// Base class for resources that need to be guarded
class GuardedResource {
public:
    GuardedResource() : state_(ResourceState::Valid) {}
    virtual ~GuardedResource() {
        state_.store(ResourceState::Destroyed, std::memory_order_release);
    }

    // Disable copy and move
    GuardedResource(const GuardedResource&) = delete;
    GuardedResource& operator=(const GuardedResource&) = delete;
    GuardedResource(GuardedResource&&) = delete;
    GuardedResource& operator=(GuardedResource&&) = delete;

    // Check if the resource is valid
    bool isValid() const {
        return state_.load(std::memory_order_acquire) == ResourceState::Valid;
    }

    // Invalidate the resource
    void invalidate() {
        state_.store(ResourceState::Invalid, std::memory_order_release);
    }

    // Get the resource state
    ResourceState getState() const {
        return state_.load(std::memory_order_acquire);
    }

private:
    std::atomic<ResourceState> state_;
};

// Guard for Executor resource
class ExecutorGuard {
public:
    ExecutorGuard(Executor* executor)
        : executor_(executor), guarded_resource_(dynamic_cast<GuardedResource*>(executor)) {}

    // Check if the executor is valid
    bool isValid() const {
        if (!executor_) {
            return false;
        }
        if (guarded_resource_) {
            return guarded_resource_->isValid();
        }
        // For non-guarded executors, assume valid if not null
        return true;
    }

    // Get the executor pointer
    Executor* get() const {
        return isValid() ? executor_ : nullptr;
    }

    // Dereference operator
    Executor& operator*() const {
        Executor* ptr = get();
        if (!ptr) {
            SAFE_ACCESS_CHECK(false, "Executor access after invalidation");
            throw std::runtime_error("Executor access after invalidation");
        }
        return *ptr;
    }

    // Arrow operator
    Executor* operator->() const {
        Executor* ptr = get();
        if (!ptr) {
            SAFE_ACCESS_CHECK(false, "Executor access after invalidation");
            throw std::runtime_error("Executor access after invalidation");
        }
        return ptr;
    }

private:
    Executor* executor_;
    GuardedResource* guarded_resource_;
};

// Guard for Promise resource
template <typename T>
class PromiseGuard {
public:
    PromiseGuard(Promise<T>* promise)
        : promise_(promise), guarded_resource_(dynamic_cast<GuardedResource*>(promise)) {}

    // Check if the promise is valid
    bool isValid() const {
        if (!promise_) {
            return false;
        }
        if (guarded_resource_) {
            return guarded_resource_->isValid();
        }
        // For non-guarded promises, assume valid if not null
        return true;
    }

    // Get the promise pointer
    Promise<T>* get() const {
        return isValid() ? promise_ : nullptr;
    }

    // Dereference operator
    Promise<T>& operator*() const {
        Promise<T>* ptr = get();
        if (!ptr) {
            SAFE_ACCESS_CHECK(false, "Promise access after invalidation");
            throw std::runtime_error("Promise access after invalidation");
        }
        return *ptr;
    }

    // Arrow operator
    Promise<T>* operator->() const {
        Promise<T>* ptr = get();
        if (!ptr) {
            SAFE_ACCESS_CHECK(false, "Promise access after invalidation");
            throw std::runtime_error("Promise access after invalidation");
        }
        return ptr;
    }

private:
    Promise<T>* promise_;
    GuardedResource* guarded_resource_;
};

// Coroutine resource guard class
class CoroutineResourceGuard {
public:
    CoroutineResourceGuard() : executor_guard_(nullptr) {}

    // Set the executor guard
    void setExecutorGuard(ExecutorGuard executor_guard) {
        executor_guard_ = std::move(executor_guard);
    }

    // Get the executor guard
    const ExecutorGuard& getExecutorGuard() const {
        return executor_guard_;
    }

    // Add a promise guard
    template <typename T>
    void addPromiseGuard(PromiseGuard<T> promise_guard) {
        // Store as any type
        promise_guards_.emplace_back(std::make_shared<PromiseGuardHolderBase>(std::move(promise_guard)));
    }

    // Check if all resources are valid
    bool areAllResourcesValid() const {
        if (!executor_guard_.isValid()) {
            return false;
        }
        for (const auto& holder : promise_guards_) {
            if (!holder->isValid()) {
                return false;
            }
        }
        return true;
    }

    // Invalidate all resources
    void invalidateAllResources() {
        if (executor_guard_.get()) {
            if (auto guarded_resource = dynamic_cast<GuardedResource*>(executor_guard_.get())) {
                guarded_resource->invalidate();
            }
        }
        for (const auto& holder : promise_guards_) {
            holder->invalidate();
        }
    }

private:
    // Base class for promise guard holders
    class PromiseGuardHolderBase {
    public:
        virtual ~PromiseGuardHolderBase() = default;
        virtual bool isValid() const = 0;
        virtual void invalidate() = 0;
    };

    // Template class for promise guard holders
    template <typename T>
    class PromiseGuardHolder : public PromiseGuardHolderBase {
    public:
        PromiseGuardHolder(PromiseGuard<T> promise_guard)
            : promise_guard_(std::move(promise_guard)) {}

        bool isValid() const override {
            return promise_guard_.isValid();
        }

        void invalidate() override {
            if (auto guarded_resource = dynamic_cast<GuardedResource*>(promise_guard_.get())) {
                guarded_resource->invalidate();
            }
        }

    private:
        PromiseGuard<T> promise_guard_;
    };

    ExecutorGuard executor_guard_;
    std::vector<std::shared_ptr<PromiseGuardHolderBase>> promise_guards_;
};

// Helper macro to guard executor access
#define GUARDED_EXECUTOR_ACCESS(executor_guard, code) \
    do { \
        if (executor_guard.isValid()) { \
            code; \
        } else { \
            SAFE_ACCESS_CHECK(false, "Executor access after invalidation"); \
            // Handle invalid executor (e.g., retry, fallback) \
        } \
    } while (0)

// Helper macro to guard promise access
#define GUARDED_PROMISE_ACCESS(promise_guard, code) \
    do { \
        if (promise_guard.isValid()) { \
            code; \
        } else { \
            SAFE_ACCESS_CHECK(false, "Promise access after invalidation"); \
            // Handle invalid promise (e.g., retry, fallback) \
        } \
    } while (0)

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_COROUTINE_RESOURCE_GUARD_H
