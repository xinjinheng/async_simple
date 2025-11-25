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

#include <async_simple/coro/Lazy.h>
#include <async_simple/Common.h>
#include <async_simple/experimental/CoroExceptionMonitor.h>
#include <async_simple/experimental/ExceptionInjection.h>
#include <iostream>
#include <vector>
#include <memory>

using namespace async_simple;
using namespace async_simple::coro;
using namespace async_simple::experimental;

// Test 1: Test coroutine ID generation
Lazy<void> testCoroutineId() {
    std::string coroId = getCurrentCoroutineId();
    std::cout << "Test 1: Current coroutine ID: " << coroId << std::endl;
    co_return;
}

// Test 2: Test AS_SAFE_ACCESS macro
Lazy<void> testSafeAccess() {
    std::cout << "Test 2: Testing AS_SAFE_ACCESS macro..." << std::endl;
    
    // Test with valid resource
    std::shared_ptr<int> validResource = std::make_shared<int>(42);
    AS_SAFE_ACCESS(validResource, "int", "testSafeAccess");
    std::cout << "Test 2: Valid resource access succeeded" << std::endl;
    
    // Test with invalid resource (should throw)
    std::shared_ptr<int> invalidResource;
    try {
        AS_SAFE_ACCESS(invalidResource, "int", "testSafeAccess");
        std::cout << "Test 2: Invalid resource access should have thrown" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test 2: Invalid resource access correctly threw: " << e.what() << std::endl;
    }
    
    co_return;
}

// Test 3: Test Latch resource management
Lazy<void> testLatchResource() {
    std::cout << "Test 3: Testing Latch resource management..." << std::endl;
    
    Latch latch(1);
    
    // Register a resource
    std::shared_ptr<int> resource = std::make_shared<int>(100);
    latch.registerResource(resource);
    std::cout << "Test 3: Registered resource with Latch" << std::endl;
    
    // Count down the latch (should release resource)
    latch.count_down();
    std::cout << "Test 3: Counted down Latch, resource should be released" << std::endl;
    
    co_return;
}

// Test 4: Test exception injection
Lazy<void> testExceptionInjection() {
    std::cout << "Test 4: Testing exception injection..." << std::endl;
    
    // Configure exception injection to inject executor release exception with 100% probability
    ExceptionInjectionConfig config;
    config.executorReleaseProbability = 1.0;
    ExceptionInjectionManager::instance().setConfig(config);
    
    try {
        AS_INJECT_EXCEPTION(ExecutorReleaseException);
        std::cout << "Test 4: Exception injection should have thrown" << std::endl;
    } catch (const ExecutorReleaseException& e) {
        std::cout << "Test 4: Exception injection correctly threw: " << e.what() << std::endl;
    }
    
    co_return;
}

// Test 5: Test global exception monitor
Lazy<void> testGlobalExceptionMonitor() {
    std::cout << "Test 5: Testing global exception monitor..." << std::endl;
    
    // Register a global exception handler
    CoroExceptionMonitor::instance().registerGlobalHandler(
        [](const CoroutineContext& ctx, const std::exception_ptr& e) {
            std::cout << "Test 5: Global exception handler called for coroutine " << ctx.coroId << std::endl;
            try {
                if (e) {
                    std::rethrow_exception(e);
                }
            } catch (const std::exception& ex) {
                std::cout << "Test 5: Exception type: " << typeid(ex).name() << ", message: " << ex.what() << std::endl;
            }
        }
    );
    
    // Throw an exception in the coroutine
    throw std::runtime_error("Test exception from coroutine");
    
    co_return;
}

int main() {
    std::cout << "Starting coroutine safety tests..." << std::endl;
    
    // Run test 1
    testCoroutineId().get();
    
    // Run test 2
    testSafeAccess().get();
    
    // Run test 3
    testLatchResource().get();
    
    // Run test 4
    testExceptionInjection().get();
    
    // Run test 5 (should trigger global exception handler)
    try {
        testGlobalExceptionMonitor().get();
    } catch (const std::exception& e) {
        std::cout << "Test 5: Exception caught in main: " << e.what() << std::endl;
    }
    
    std::cout << "All tests completed!" << std::endl;
    return 0;
}
