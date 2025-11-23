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
#include "async_simple/coro/Lazy.h"
#include <atomic>
#include <thread_local>

namespace async_simple {
namespace coro {

static std::atomic<uint64_t> g_coroutineIdCounter(1);
thread_local uint64_t g_currentCoroutineId = 0;

ASYNC_SIMPLE_API uint64_t getCurrentCoroutineId() {
    return g_currentCoroutineId;
}

ASYNC_SIMPLE_API void setCurrentCoroutineId(uint64_t coroId) {
    g_currentCoroutineId = coroId;
}

ASYNC_SIMPLE_API uint64_t generateCoroutineId() {
    return g_coroutineIdCounter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace coro
}  // namespace async_simple
