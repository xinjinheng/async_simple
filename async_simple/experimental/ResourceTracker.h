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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_RESOURCE_TRACKER_H
#define ASYNC_SIMPLE_EXPERIMENTAL_RESOURCE_TRACKER_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>

namespace async_simple {
namespace experimental {

// Resource type enum
enum class ResourceType {
    Socket,
    File,
    Memory,
    Other
};

// Resource information struct
struct ResourceInfo {
    uint64_t resource_id;
    ResourceType type;
    std::string description;
    uint64_t coro_id;
    std::chrono::time_point<std::chrono::system_clock> create_time;
    std::chrono::time_point<std::chrono::system_clock> last_access_time;
    std::string create_stack;

    std::string toString() const {
        std::ostringstream oss;
        oss << "Resource ID: " << resource_id << std::endl;
        oss << "Type: ";
        switch (type) {
            case ResourceType::Socket:
                oss << "Socket";
                break;
            case ResourceType::File:
                oss << "File";
                break;
            case ResourceType::Memory:
                oss << "Memory";
                break;
            default:
                oss << "Other";
                break;
        }
        oss << std::endl;
        oss << "Description: " << description << std::endl;
        oss << "Coroutine ID: " << coro_id << std::endl;
        oss << "Create Time: " << std::put_time(std::localtime(&create_time.time_since_epoch().count()), "%Y-%m-%d %H:%M:%S") << std::endl;
        oss << "Last Access Time: " << std::put_time(std::localtime(&last_access_time.time_since_epoch().count()), "%Y-%m-%d %H:%M:%S") << std::endl;
        oss << "Create Stack:" << std::endl << create_stack << std::endl;
        return oss.str();
    }
};

// Resource cleanup function type
typedef std::function<void(uint64_t resource_id)> ResourceCleanupFunc;

// Resource tracker singleton
class ResourceTracker {
public:
    // Get singleton instance
    static ResourceTracker& instance() {
        static ResourceTracker tracker;
        return tracker;
    }

    // Delete copy and move constructors
    ResourceTracker(const ResourceTracker&) = delete;
    ResourceTracker& operator=(const ResourceTracker&) = delete;
    ResourceTracker(ResourceTracker&&) = delete;
    ResourceTracker& operator=(ResourceTracker&&) = delete;

    // Register a resource
    uint64_t registerResource(
        ResourceType type,
        const std::string& description,
        uint64_t coro_id,
        ResourceCleanupFunc cleanup_func) {
        uint64_t resource_id = resource_id_counter_.fetch_add(1, std::memory_order_relaxed);
        ResourceInfo info;
        info.resource_id = resource_id;
        info.type = type;
        info.description = description;
        info.coro_id = coro_id;
        info.create_time = std::chrono::system_clock::now();
        info.last_access_time = info.create_time;
        // TODO: Implement stack trace capture
        info.create_stack = "Stack trace not implemented";

        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_[resource_id] = std::make_tuple(info, std::move(cleanup_func));

        return resource_id;
    }

    // Unregister a resource
    void unregisterResource(uint64_t resource_id) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        auto it = resources_.find(resource_id);
        if (it != resources_.end()) {
            resources_.erase(it);
        }
    }

    // Update resource last access time
    void updateLastAccessTime(uint64_t resource_id) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        auto it = resources_.find(resource_id);
        if (it != resources_.end()) {
            std::get<0>(it->second).last_access_time = std::chrono::system_clock::now();
        }
    }

    // Scan for leaked resources and generate a report
    std::string scanLeakedResources() {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        std::ostringstream oss;
        oss << "Resource Leak Report:" << std::endl;
        oss << "Total leaked resources: " << resources_.size() << std::endl;
        oss << "==============================" << std::endl;

        for (const auto& [resource_id, resource_tuple] : resources_) {
            const ResourceInfo& info = std::get<0>(resource_tuple);
            oss << info.toString() << std::endl;
            oss << "==============================" << std::endl;
        }

        return oss.str();
    }

    // Cleanup all leaked resources
    void cleanupLeakedResources() {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        for (const auto& [resource_id, resource_tuple] : resources_) {
            const ResourceCleanupFunc& cleanup_func = std::get<1>(resource_tuple);
            try {
                cleanup_func(resource_id);
            } catch (const std::exception& e) {
                // Ignore exceptions during cleanup
            }
        }
        resources_.clear();
    }

private:
    // Private constructor for singleton
    ResourceTracker() : resource_id_counter_(0) {}

    // Resource storage: key is resource ID, value is tuple of ResourceInfo and cleanup function
    std::unordered_map<uint64_t, std::tuple<ResourceInfo, ResourceCleanupFunc>> resources_;
    std::mutex resources_mutex_;

    // Resource ID counter
    std::atomic<uint64_t> resource_id_counter_;
};

// Helper class for RAII resource management
template <typename ResourceHandle>
class ResourceGuard {
public:
    ResourceGuard(
        ResourceType type,
        const std::string& description,
        uint64_t coro_id,
        ResourceHandle handle,
        std::function<void(ResourceHandle)> cleanup_func)
        : handle_(handle), resource_id_(0) {
        if (handle_ != ResourceHandle()) {
            resource_id_ = ResourceTracker::instance().registerResource(
                type,
                description,
                coro_id,
                [cleanup_func, handle = handle_](uint64_t) {
                    try {
                        cleanup_func(handle);
                    } catch (const std::exception& e) {
                        // Ignore exceptions during cleanup
                    }
                });
        }
    }

    ~ResourceGuard() {
        if (resource_id_ != 0) {
            ResourceTracker::instance().unregisterResource(resource_id_);
        }
    }

    // Delete copy constructor and assignment operator
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    // Move constructor and assignment operator
    ResourceGuard(ResourceGuard&& other) noexcept
        : handle_(other.handle_), resource_id_(other.resource_id_) {
        other.handle_ = ResourceHandle();
        other.resource_id_ = 0;
    }

    ResourceGuard& operator=(ResourceGuard&& other) noexcept {
        if (this != &other) {
            if (resource_id_ != 0) {
                ResourceTracker::instance().unregisterResource(resource_id_);
            }
            handle_ = other.handle_;
            resource_id_ = other.resource_id_;
            other.handle_ = ResourceHandle();
            other.resource_id_ = 0;
        }
        return *this;
    }

    // Get the resource handle
    ResourceHandle getHandle() const {
        return handle_;
    }

    // Update last access time
    void updateAccessTime() {
        if (resource_id_ != 0) {
            ResourceTracker::instance().updateLastAccessTime(resource_id_);
        }
    }

private:
    ResourceHandle handle_;
    uint64_t resource_id_;
};

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_RESOURCE_TRACKER_H
