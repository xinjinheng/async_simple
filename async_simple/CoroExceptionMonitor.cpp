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
#include "async_simple/CoroExceptionMonitor.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <execinfo.h>
#include <cstdlib>
#include <async_simple/coro/Lazy.h>

namespace async_simple {

CoroExceptionMonitor& CoroExceptionMonitor::instance() {
    static CoroExceptionMonitor instance;
    return instance;
}

CoroExceptionMonitor::CoroExceptionMonitor() {
    // Register default handler to print exception info
    registerGlobalHandler([](const CoroExceptionContext& context) {
        std::ostringstream oss;
        oss << "Coroutine Exception Report:" << std::endl;
        oss << "  Coroutine ID: " << context.coroId << std::endl;
        oss << "  Coroutine Type: " << context.coroutineType << std::endl;
        oss << "  Exception Type: " << context.exceptionType << std::endl;
        oss << "  Exception Message: " << context.exceptionMessage << std::endl;
        oss << "  Creation Time: " << context.createTime << "ms" << std::endl;
        oss << "  Exception Time: " << context.exceptionTime << "ms" << std::endl;
        
        if (!context.callStack.empty()) {
            oss << "  Call Stack:" << std::endl;
            for (size_t i = 0; i < context.callStack.size(); ++i) {
                oss << "    " << i << ": " << context.callStack[i] << std::endl;
            }
        }
        
        if (!context.resourceList.empty()) {
            oss << "  Associated Resources:" << std::endl;
            for (const auto& resource : context.resourceList) {
                oss << "    - " << resource << std::endl;
            }
        }
        
        std::cerr << oss.str() << std::endl;
    });
}

void CoroExceptionMonitor::registerGlobalHandler(CoroExceptionHandler handler) {
    std::lock_guard<std::mutex> lock(_handlersMutex);
    _handlers.push_back(std::move(handler));
}

void CoroExceptionMonitor::unregisterGlobalHandler(CoroExceptionHandler handler) {
    std::lock_guard<std::mutex> lock(_handlersMutex);
    auto it = std::remove_if(_handlers.begin(), _handlers.end(),
        [&handler](const CoroExceptionHandler& h) {
            return h.target_type() == handler.target_type();
        });
    _handlers.erase(it, _handlers.end());
}

void CoroExceptionMonitor::handleException(const CoroExceptionContext& context) {
    std::lock_guard<std::mutex> lock(_handlersMutex);
    for (const auto& handler : _handlers) {
        try {
            handler(context);
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception in exception handler: " << ex.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception in exception handler" << std::endl;
        }
    }
}

CoroExceptionContext CoroExceptionMonitor::createExceptionContext(
    const std::exception_ptr& ex,
    const std::string& coroutineType) {
    CoroExceptionContext context;
    
    // Get coroutine ID
    context.coroId = coro::getCurrentCoroutineId();
    
    // Set coroutine type
    context.coroutineType = coroutineType;
    
    // Get exception info
    try {
        if (ex) {
            std::rethrow_exception(ex);
        }
        else {
            context.exceptionType = "UnknownException";
            context.exceptionMessage = "No exception pointer provided";
        }
    }
    catch (const std::exception& e) {
        context.exceptionType = typeid(e).name();
        context.exceptionMessage = e.what();
    }
    catch (...) {
        context.exceptionType = "UnknownException";
        context.exceptionMessage = "Unknown exception type";
    }
    
    // Get timestamps
    context.createTime = getCurrentTimestamp(); // TODO: Store creation time in coroutine context
    context.exceptionTime = getCurrentTimestamp();
    
    // Get call stack
    context.callStack = getCallStack();
    
    // Get resource list
    context.resourceList = getResourceList();
    
    return context;
}

uint64_t CoroExceptionMonitor::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::vector<std::string> CoroExceptionMonitor::getCallStack() const {
    std::vector<std::string> callStack;
    
    const int maxFrames = 64;
    void* frames[maxFrames];
    int numFrames = backtrace(frames, maxFrames);
    
    if (numFrames > 0) {
        char** symbols = backtrace_symbols(frames, numFrames);
        if (symbols) {
            for (int i = 0; i < numFrames; ++i) {
                callStack.emplace_back(symbols[i]);
            }
            free(symbols);
        }
    }
    
    return callStack;
}

std::vector<std::string> CoroExceptionMonitor::getResourceList() const {
    std::vector<std::string> resourceList;
    
    // TODO: Implement resource tracking
    // For now, return empty list
    
    return resourceList;
}

}  // namespace async_simple
