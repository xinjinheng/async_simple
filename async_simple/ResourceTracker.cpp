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
#include "async_simple/ResourceTracker.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <execinfo.h>
#include <cstdlib>
#include <algorithm>
#include <async_simple/coro/Lazy.h>

namespace async_simple {

ResourceTracker& ResourceTracker::instance() {
    static ResourceTracker instance;
    return instance;
}

ResourceTracker::ResourceTracker()
    : _nextResourceId(1),
      _leakDetectionEnabled(true),
      _isShuttingDown(false) {
    // Register atexit handler to generate leak report on program exit
    std::atexit([]() {
        ResourceTracker::instance().printLeakReport();
    });
}

ResourceTracker::~ResourceTracker() {
    _isShuttingDown = true;
    if (_leakDetectionEnabled) {
        printLeakReport();
    }
}

uint64_t ResourceTracker::registerResource(
    ResourceType type,
    const std::string& name,
    uint64_t coroId) {
    if (!_leakDetectionEnabled || _isShuttingDown) {
        return 0;
    }
    
    uint64_t resourceId = generateResourceId();
    
    ResourceInfo info;
    info.resourceId = resourceId;
    info.resourceType = type;
    info.resourceName = name;
    info.coroId = coroId;
    info.createTime = getCurrentTimestamp();
    info.lastAccessTime = info.createTime;
    info.createStack = getCurrentCallStack();
    info.lastAccessStack = info.createStack;
    
    std::lock_guard<std::mutex> lock(_resourcesMutex);
    _resources[resourceId] = info;
    
    return resourceId;
}

void ResourceTracker::unregisterResource(uint64_t resourceId) {
    if (!_leakDetectionEnabled || _isShuttingDown || resourceId == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_resourcesMutex);
    _resources.erase(resourceId);
}

void ResourceTracker::updateResourceAccess(uint64_t resourceId) {
    if (!_leakDetectionEnabled || _isShuttingDown || resourceId == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_resourcesMutex);
    auto it = _resources.find(resourceId);
    if (it != _resources.end()) {
        it->second.lastAccessTime = getCurrentTimestamp();
        it->second.lastAccessStack = getCurrentCallStack();
    }
}

ResourceLeakReport ResourceTracker::generateLeakReport() {
    ResourceLeakReport report;
    
    if (!_leakDetectionEnabled || _isShuttingDown) {
        return report;
    }
    
    std::lock_guard<std::mutex> lock(_resourcesMutex);
    
    report.totalLeakedResources = _resources.size();
    
    for (const auto& pair : _resources) {
        const ResourceInfo& info = pair.second;
        report.leakedByType[info.resourceType]++;
        report.leakedResources.push_back(info);
    }
    
    // Sort leaked resources by creation time
    std::sort(report.leakedResources.begin(), report.leakedResources.end(),
        [](const ResourceInfo& a, const ResourceInfo& b) {
            return a.createTime < b.createTime;
        });
    
    // Generate report time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    report.reportTime = oss.str();
    
    return report;
}

void ResourceTracker::printLeakReport() {
    ResourceLeakReport report = generateLeakReport();
    
    if (report.totalLeakedResources == 0) {
        std::cerr << "ResourceTracker: No resource leaks detected." << std::endl;
        return;
    }
    
    std::ostringstream oss;
    oss << "Resource Leak Report - Generated at " << report.reportTime << std::endl;
    oss << "================================================================" << std::endl;
    oss << "Total leaked resources: " << report.totalLeakedResources << std::endl;
    oss << std::endl;
    
    // Print leaked resources by type
    oss << "Leaked resources by type: " << std::endl;
    for (const auto& pair : report.leakedByType) {
        std::string typeName;
        switch (pair.first) {
            case ResourceType::Socket:
                typeName = "Socket";
                break;
            case ResourceType::FileDescriptor:
                typeName = "FileDescriptor";
                break;
            case ResourceType::Memory:
                typeName = "Memory";
                break;
            case ResourceType::Coroutine:
                typeName = "Coroutine";
                break;
            default:
                typeName = "Other";
                break;
        }
        oss << "  " << typeName << ": " << pair.second << std::endl;
    }
    oss << std::endl;
    
    // Print detailed information about each leaked resource
    oss << "Detailed leaked resources: " << std::endl;
    for (size_t i = 0; i < report.leakedResources.size(); ++i) {
        const ResourceInfo& info = report.leakedResources[i];
        
        oss << "  Resource " << i + 1 << ":" << std::endl;
        oss << "    ID: " << info.resourceId << std::endl;
        
        std::string typeName;
        switch (info.resourceType) {
            case ResourceType::Socket:
                typeName = "Socket";
                break;
            case ResourceType::FileDescriptor:
                typeName = "FileDescriptor";
                break;
            case ResourceType::Memory:
                typeName = "Memory";
                break;
            case ResourceType::Coroutine:
                typeName = "Coroutine";
                break;
            default:
                typeName = "Other";
                break;
        }
        oss << "    Type: " << typeName << std::endl;
        oss << "    Name: " << info.resourceName << std::endl;
        oss << "    Coroutine ID: " << info.coroId << std::endl;
        oss << "    Created at: " << info.createTime << "ms" << std::endl;
        oss << "    Last accessed at: " << info.lastAccessTime << "ms" << std::endl;
        oss << "    Creation stack: " << std::endl << info.createStack << std::endl;
        oss << "    Last access stack: " << std::endl << info.lastAccessStack << std::endl;
        oss << std::endl;
    }
    
    std::cerr << oss.str() << std::endl;
}

void ResourceTracker::setLeakDetectionEnabled(bool enabled) {
    _leakDetectionEnabled = enabled;
}

bool ResourceTracker::isLeakDetectionEnabled() const {
    return _leakDetectionEnabled;
}

uint64_t ResourceTracker::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string ResourceTracker::getCurrentCallStack() const {
    const int maxFrames = 32;
    void* frames[maxFrames];
    int numFrames = backtrace(frames, maxFrames);
    
    std::ostringstream oss;
    
    if (numFrames > 0) {
        char** symbols = backtrace_symbols(frames, numFrames);
        if (symbols) {
            for (int i = 0; i < numFrames; ++i) {
                oss << "      " << symbols[i] << std::endl;
            }
            free(symbols);
        }
    }
    
    return oss.str();
}

uint64_t ResourceTracker::generateResourceId() {
    return _nextResourceId.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace async_simple
