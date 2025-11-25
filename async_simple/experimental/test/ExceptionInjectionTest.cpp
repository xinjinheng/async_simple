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
#include "async_simple/experimental/ExceptionInjection.h"
#include "async_simple/experimental/NetworkException.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Latch.h"
#include "async_simple/executors/SimpleExecutor.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace async_simple;
using namespace async_simple::coro;
using namespace async_simple::executors;
using namespace async_simple::experimental;

TEST(ExceptionInjectionTest, TestExecutorReleaseInjection) {
    // Configure exception injection
    ExceptionInjectionConfig config;
    config.injectExecutorRelease = true;
    config.injectionProbability = 1.0;  // 100% probability
    ExceptionInjectionManager::instance().setConfig(config);
    
    // Test that executor release exception is injected
    bool exceptionCaught = false;
    try {
        AS_INJECT_EXCEPTION();
    } catch (const std::runtime_error& e) {
        exceptionCaught = true;
        EXPECT_EQ(std::string(e.what()), "Executor released unexpectedly");
    }
    EXPECT_TRUE(exceptionCaught);
}

TEST(ExceptionInjectionTest, TestNetworkTimeoutInjection) {
    // Configure exception injection
    ExceptionInjectionConfig config;
    config.injectNetworkTimeout = true;
    config.injectionProbability = 1.0;  // 100% probability
    ExceptionInjectionManager::instance().setConfig(config);
    
    // Test that network timeout exception is injected
    bool exceptionCaught = false;
    try {
        AS_INJECT_EXCEPTION();
    } catch (const ConnectionTimeoutException& e) {
        exceptionCaught = true;
        EXPECT_EQ(std::string(e.what()), "Network operation timed out");
    }
    EXPECT_TRUE(exceptionCaught);
}

TEST(ExceptionInjectionTest, TestResourceAccessConflictInjection) {
    // Configure exception injection
    ExceptionInjectionConfig config;
    config.injectResourceAccessConflict = true;
    config.injectionProbability = 1.0;  // 100% probability
    ExceptionInjectionManager::instance().setConfig(config);
    
    // Test that resource access conflict exception is injected
    bool exceptionCaught = false;
    try {
        AS_INJECT_EXCEPTION();
    } catch (const std::runtime_error& e) {
        exceptionCaught = true;
        EXPECT_EQ(std::string(e.what()), "Resource access conflict detected");
    }
    EXPECT_TRUE(exceptionCaught);
}

TEST(ExceptionInjectionTest, TestLatchResourceAutoRelease) {
    // Create a latch with count 1
    Latch latch(1);
    
    // Register a resource that sets a flag when released
    bool resourceReleased = false;
    latch.registerResource([&resourceReleased]() {
        resourceReleased = true;
    });
    
    // Test that resource is released when latch count reaches zero
    EXPECT_FALSE(resourceReleased);
    latch.count_down().get();
    EXPECT_TRUE(resourceReleased);
}

TEST(ExceptionInjectionTest, TestLatchFileDescriptorAutoRelease) {
    // Create a latch with count 1
    Latch latch(1);
    
    // Create a temporary file
    int fd = ::open("test_temp_file.txt", O_CREAT | O_RDWR, 0644);
    EXPECT_NE(fd, -1);
    
    // Register the file descriptor with the latch
    latch.registerFileDescriptor(fd);
    
    // Close the latch (should auto release the file descriptor)
    latch.~Latch();
    
    // Test that the file descriptor is no longer valid
    ssize_t result = ::write(fd, "test", 4);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(errno, EBADF);
    
    // Clean up the temporary file
    ::unlink("test_temp_file.txt");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
