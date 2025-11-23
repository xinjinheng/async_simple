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
#ifndef ASYNC_SIMPLE_RESOURCE_TRACKER_H
#define ASYNC_SIMPLE_RESOURCE_TRACKER_H

#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>
#include <async_simple/coro/Lazy.h>

namespace async_simple {

// Resource type enum
enum class ResourceType {
    Socket,
    FileDescriptor,
    Memory,
    Coroutine,
    Other
};

// Resource information struct
struct ResourceInfo {
    uint64_t resourceId;             // Unique resource ID
    ResourceType resourceType;       // Type of resource
    std::string resourceName;        // Name/description of resource
    uint64_t coroId;                 // Coroutine ID that created the resource
    uint64_t createTime;             // Creation time (timestamp)
    uint64_t lastAccessTime;         // Last access time (timestamp)
    std::string createStack;         // Call stack when resource was created
    std::string lastAccessStack;     // Call stack when resource was last accessed
};

// Resource leak report struct
struct ResourceLeakReport {
    uint64_t totalLeakedResources;   // Total number of leaked resources
    std::unordered_map<ResourceType, uint64_t> leakedByType;  // Leaked resources by type
    std::vector<ResourceInfo> leakedResources;  // Detailed information about leaked resources
    std::string reportTime;          // Time when report was generated
};

// Resource tracker singleton
class ResourceTracker {
public:
    // Get the singleton instance
    static ResourceTracker& instance();
    
    // Delete copy and move constructors
    ResourceTracker(const ResourceTracker&) = delete;
    ResourceTracker& operator=(const ResourceTracker&) = delete;
    ResourceTracker(ResourceTracker&&) = delete;
    ResourceTracker& operator=(ResourceTracker&&) = delete;
    
    // Register a resource
    uint64_t registerResource(
        ResourceType type,
        const std::string& name,
        uint64_t coroId = 0);
    
    // Unregister a resource
    void unregisterResource(uint64_t resourceId);
    
    // Update resource access time
    void updateResourceAccess(uint64_t resourceId);
    
    // Generate resource leak report
    ResourceLeakReport generateLeakReport();
    
    // Print resource leak report to stderr
    void printLeakReport();
    
    // Set resource leak detection enabled/disabled
    void setLeakDetectionEnabled(bool enabled);
    
    // Check if resource leak detection is enabled
    bool isLeakDetectionEnabled() const;
    
private:
    // Private constructor
    ResourceTracker();
    
    // Destructor
    ~ResourceTracker();
    
    // Get current timestamp in milliseconds
    uint64_t getCurrentTimestamp() const;
    
    // Get current call stack as string
    std::string getCurrentCallStack() const;
    
    // Generate unique resource ID
    uint64_t generateResourceId();
    
private:
    std::unordered_map<uint64_t, ResourceInfo> _resources;
    std::mutex _resourcesMutex;
    std::atomic<uint64_t> _nextResourceId;
    bool _leakDetectionEnabled;
    bool _isShuttingDown;
};

// Helper class for automatic resource registration/unregistration
class ScopedResourceTracker {
public:
    ScopedResourceTracker(ResourceType type, const std::string& name, uint64_t coroId = 0)
        : _resourceId(ResourceTracker::instance().registerResource(type, name, coroId)) {
    }
    
    ~ScopedResourceTracker() {
        ResourceTracker::instance().unregisterResource(_resourceId);
    }
    
    // Get the resource ID
    uint64_t resourceId() const {
        return _resourceId;
    }
    
private:
    uint64_t _resourceId;
};

}  // namespace async_simple

#endif  // ASYNC_SIMPLE_RESOURCE_TRACKER_H
