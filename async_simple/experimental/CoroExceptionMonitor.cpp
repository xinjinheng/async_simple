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
#include "CoroExceptionMonitor.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace async_simple {
namespace experimental {

CoroExceptionMonitor& CoroExceptionMonitor::instance() {
    static CoroExceptionMonitor instance;
    return instance;
}

void CoroExceptionMonitor::registerGlobalHandler(CoroExceptionHandler handler) {
    std::lock_guard<std::mutex> lock(_mutex);
    _globalHandler = std::move(handler);
}

void CoroExceptionMonitor::unregisterAllHandlers() {
    std::lock_guard<std::mutex> lock(_mutex);
    _globalHandler = nullptr;
}

void CoroExceptionMonitor::handleException(const std::exception_ptr& eptr, const CoroutineContext& context) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (_globalHandler) {
        try {
            _globalHandler(eptr, context);
        } catch (...) {
            // Ignore exceptions from the handler itself
            std::cerr << "Exception occurred in CoroExceptionHandler" << std::endl;
        }
    } else {
        // Default handler: print exception information to stderr
        std::cerr << "Uncaught exception in coroutine" << std::endl;
        std::cerr << "Coroutine ID: " << context.coroId << std::endl;
        std::cerr << "Create time: " << context.createTime << std::endl;
        std::cerr << "Associated resources: " << context.associatedResources << std::endl;
        
        if (!context.callStack.empty()) {
            std::cerr << "Call stack: " << std::endl << context.callStack << std::endl;
        }
        
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception type: " << typeid(e).name() << std::endl;
            std::cerr << "Exception message: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception type" << std::endl;
        }
    }
}

}  // namespace experimental
}  // namespace async_simple
