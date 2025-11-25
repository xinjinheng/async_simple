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
#include <gtest/gtest.h>
#include <asio.hpp>
#include <thread>
#include <atomic>
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "async_simple/Executor.h"
#include "async_simple/experimental/NetworkOperationWrapper.h"
#include "async_simple/experimental/NetworkException.h"
#include "async_simple/experimental/CoroExceptionMonitor.h"
#include "async_simple/experimental/ResourceTracker.h"
#include "async_simple/experimental/CoroutineResourceGuard.h"

using namespace async_simple;
using namespace async_simple::coro;
using namespace async_simple::experimental;

// Test 1: Network timeout exception
TEST(ExceptionInjectionTest, NetworkTimeoutTest) {
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });

    bool timeout_caught = false;
    auto test_coro = [] (asio::io_context& io_context) -> Lazy<void> {
        asio::ip::tcp::socket socket(io_context);
        asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string("127.0.0.1"), 9999);

        try {
            // Try to connect to a non-existent port with very short timeout
            auto [error, connected_socket] = co_await async_connect(
                io_context, "127.0.0.1", "9999", std::chrono::milliseconds(100));
            EXPECT_TRUE(error);
        } catch (const ConnectionTimeoutException& e) {
            // This is expected
            timeout_caught = true;
        } catch (const std::exception& e) {
            // Other exceptions are not expected
            FAIL() << "Unexpected exception: " << e.what();
        }
    };

    syncAwait(test_coro(io_context));
    EXPECT_TRUE(timeout_caught);

    io_context.stop();
    if (thd.joinable()) {
        thd.join();
    }
}

// Test 2: Coroutine exception propagation
TEST(ExceptionInjectionTest, CoroExceptionPropagationTest) {
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });

    bool exception_caught = false;
    std::string exception_message;

    auto inner_coro = [] () -> Lazy<void> {
        // Throw an exception
        throw std::runtime_error("Test exception from inner coroutine");
    };

    auto outer_coro = [&] () -> Lazy<void> {
        try {
            co_await inner_coro();
        } catch (const std::exception& e) {
            exception_caught = true;
            exception_message = e.what();
        }
    };

    syncAwait(outer_coro());
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(exception_message, "Test exception from inner coroutine");

    io_context.stop();
    if (thd.joinable()) {
        thd.join();
    }
}

// Test 3: Global exception monitor
TEST(ExceptionInjectionTest, GlobalExceptionMonitorTest) {
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });

    bool monitor_caught = false;
    std::string monitor_message;
    uint64_t monitored_coro_id = 0;

    // Register global exception handler
    CoroExceptionMonitor::instance().registerGlobalHandler(
        [&](const std::exception& e, const CoroutineContext& context) {
            monitor_caught = true;
            monitor_message = e.what();
            monitored_coro_id = context.coro_id;
        });

    auto test_coro = [] () -> Lazy<void> {
        // Throw an exception
        throw std::runtime_error("Test exception for global monitor");
    };

    // Run the coroutine and let the exception propagate
    EXPECT_THROW(syncAwait(test_coro()), std::runtime_error);
    EXPECT_TRUE(monitor_caught);
    EXPECT_EQ(monitor_message, "Test exception for global monitor");
    EXPECT_NE(monitored_coro_id, 0);

    // Unregister all handlers
    CoroExceptionMonitor::instance().unregisterAllHandlers();

    io_context.stop();
    if (thd.joinable()) {
        thd.join();
    }
}

// Test 4: Resource leak detection
TEST(ExceptionInjectionTest, ResourceLeakDetectionTest) {
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });

    // Register a resource but don't unregister it
    uint64_t resource_id = ResourceTracker::instance().registerResource(
        ResourceType::Socket,
        "Test socket resource",
        12345,
        [](uint64_t id) {
            // Cleanup function
        });

    // Check that the resource is tracked
    std::string report = ResourceTracker::instance().scanLeakedResources();
    EXPECT_NE(report.find("Test socket resource"), std::string::npos);

    // Cleanup the resource
    ResourceTracker::instance().cleanupLeakedResources();

    // Check that the resource is no longer tracked
    report = ResourceTracker::instance().scanLeakedResources();
    EXPECT_EQ(report.find("Test socket resource"), std::string::npos);

    io_context.stop();
    if (thd.joinable()) {
        thd.join();
    }
}

// Test 5: Coroutine resource guard
TEST(ExceptionInjectionTest, CoroutineResourceGuardTest) {
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });

    AsioExecutor executor(io_context);
    ExecutorGuard executor_guard(&executor);
    CoroutineResourceGuard resource_guard;
    resource_guard.setExecutorGuard(executor_guard);

    // Check that the executor is valid
    EXPECT_TRUE(resource_guard.getExecutorGuard().isValid());
    EXPECT_TRUE(resource_guard.areAllResourcesValid());

    // Invalidate the executor
    if (auto guarded_resource = dynamic_cast<GuardedResource*>(&executor)) {
        guarded_resource->invalidate();
    }

    // Check that the executor is now invalid
    EXPECT_FALSE(resource_guard.getExecutorGuard().isValid());
    EXPECT_FALSE(resource_guard.areAllResourcesValid());

    io_context.stop();
    if (thd.joinable()) {
        thd.join();
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
