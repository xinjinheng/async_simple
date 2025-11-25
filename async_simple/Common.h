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
#ifndef ASYNC_SIMPLE_COMMON_H
#define ASYNC_SIMPLE_COMMON_H

#ifndef ASYNC_SIMPLE_USE_MODULES
#include <stdexcept>
#include <functional>
#include <string>
#include "async_simple/CommonMacros.h"

#endif  // ASYNC_SIMPLE_USE_MODULES

namespace async_simple {
// Different from assert, logicAssert is meaningful in
// release mode. logicAssert should be used in case that
// we need to make assumption for users.
// In another word, if assert fails, it means there is
// a bug in the library. If logicAssert fails, it means
// there is a bug in the user code.
inline void logicAssert(bool x, const char* errorMsg) {
    if (x)
        AS_LIKELY { return; }
    throw std::logic_error(errorMsg);
}

// SafeAccess macro for production environment
// It checks the validity of the resource before access
// and triggers a configurable callback if the resource is invalid
using SafeAccessCallback = std::function<void(const std::string& resourceType,
                                               const std::string& accessLocation,
                                               const std::string& errorMsg)>;

inline SafeAccessCallback& getSafeAccessCallback() {
    static SafeAccessCallback callback;
    return callback;
}

inline void setSafeAccessCallback(SafeAccessCallback cb) {
    getSafeAccessCallback() = std::move(cb);
}

inline bool safeAccessCheck(bool valid, const char* resourceType,
                            const char* accessLocation, const char* errorMsg) {
    if (valid)
        AS_LIKELY { return true; }
    
    auto& callback = getSafeAccessCallback();
    if (callback) {
        callback(resourceType, accessLocation, errorMsg);
    }
    return false;
}

#define SAFE_ACCESS(resource, resourceType, accessLocation, errorMsg) \
    if (!async_simple::safeAccessCheck(static_cast<bool>(resource), \
                                       resourceType, accessLocation, errorMsg)) \
        return

#define SAFE_ACCESS_WITH_DEFAULT(resource, resourceType, accessLocation, errorMsg, defaultValue) \
    if (!async_simple::safeAccessCheck(static_cast<bool>(resource), \
                                       resourceType, accessLocation, errorMsg)) \
        return defaultValue

}  // namespace async_simple

#endif  // ASYNC_SIMPLE_COMMON_H
