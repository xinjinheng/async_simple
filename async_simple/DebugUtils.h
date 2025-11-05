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
#ifndef ASYNC_SIMPLE_DEBUG_UTILS_H
#define ASYNC_SIMPLE_DEBUG_UTILS_H

#ifndef ASYNC_SIMPLE_USE_MODULES
#include <cassert>
#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

#ifdef ASYNC_SIMPLE_DEBUG
#include <execinfo.h>
#include <cxxabi.h>
#include <cstdlib>
#endif

#endif  // ASYNC_SIMPLE_USE_MODULES

namespace async_simple {

// Pointer validity checking
// In debug mode, it will check if the pointer is null and print a stack trace
// In release mode, it will only check if the pointer is null

template <typename T>
inline void checkPointerNotNull(T* ptr, const char* message = "Null pointer detected") {
    if (ptr == nullptr) {
#ifdef ASYNC_SIMPLE_DEBUG
        std::cerr << "ERROR: " << message << std::endl;
        std::cerr << "Stack trace:" << std::endl;
        
        void* callstack[20];
        int frames = backtrace(callstack, 20);
        char** symbols = backtrace_symbols(callstack, frames);
        
        for (int i = 0; i < frames; ++i) {
            char* symbol = symbols[i];
            char* demangled = nullptr;
            
            // Try to demangle C++ symbols
            char* start = strchr(symbol, '(');
            char* end = strchr(symbol, '+');
            if (start && end) {
                *start++ = '\0';
                *end = '\0';
                
                int status;
                demangled = abi::__cxa_demangle(start, nullptr, nullptr, &status);
            }
            
            if (demangled) {
                std::cerr << "  " << symbol << "(" << demangled << "+" << (end ? end + 1 : "") << ")" << std::endl;
                free(demangled);
            }
            else {
                std::cerr << "  " << symbol << std::endl;
            }
        }
        
        free(symbols);
#endif
        
        assert(ptr != nullptr && message);
        throw std::runtime_error(message);
    }
}

// Check if a shared_ptr is not null
template <typename T>
inline void checkSharedPtrNotNull(const std::shared_ptr<T>& ptr, const char* message = "Null shared_ptr detected") {
    if (ptr == nullptr) {
        checkPointerNotNull(static_cast<T*>(nullptr), message);
    }
}

// Check if a unique_ptr is not null
template <typename T>
inline void checkUniquePtrNotNull(const std::unique_ptr<T>& ptr, const char* message = "Null unique_ptr detected") {
    if (ptr == nullptr) {
        checkPointerNotNull(static_cast<T*>(nullptr), message);
    }
}

}  // namespace async_simple

#endif