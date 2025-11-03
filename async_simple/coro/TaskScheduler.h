/*
 * Copyright (c) 2023, Alibaba Group Holding Limited;
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

#ifndef ASYNC_SIMPLE_CORO_TASKSCHEDULER_H
#define ASYNC_SIMPLE_CORO_TASKSCHEDULER_H

#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include "async_simple/Common.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/Future.h"
#include "async_simple/Promise.h"
#include "async_simple/Executor.h"

namespace async_simple {
namespace coro {

// 任务ID类型
using TaskId = uint64_t;

// 任务状态
enum class TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

// 任务配置
struct TaskConfig {
    size_t max_retries = 3;  // 最大重试次数
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(100);  // 重试延迟
    Executor* executor = nullptr;  // 执行任务的执行器
};

// 任务结果
struct TaskResult {
    TaskStatus status = TaskStatus::Pending;
    std::exception_ptr exception = nullptr;
};

// 分布式任务调度器类
class TaskScheduler {
public:
    TaskScheduler(Executor* default_executor = nullptr);
    ~TaskScheduler();
    
    // 提交任务，返回任务ID
    template <typename Func, typename... Args>
    TaskId submitTask(Func&& func, Args&&... args, const TaskConfig& config = TaskConfig());
    
    // 提交依赖任务，当所有依赖任务完成后执行
    template <typename Func, typename... Args>
    TaskId submitDependentTask(const std::vector<TaskId>& dependencies, Func&& func, Args&&... args, 
        const TaskConfig& config = TaskConfig());
    
    // 取消任务
    bool cancelTask(TaskId task_id);
    
    // 获取任务状态
    TaskStatus getTaskStatus(TaskId task_id) const;
    
    // 等待任务完成
    Future<TaskResult> waitForTask(TaskId task_id);
    
    // 等待所有任务完成
    Future<std::vector<TaskResult>> waitForAllTasks();
    
private:
    Executor* _default_executor;
    
    // 任务信息
    struct TaskInfo {
        TaskId id;
        TaskConfig config;
        std::function<Lazy<void>()> task_func;
        std::vector<TaskId> dependencies;
        std::vector<TaskId> dependents;
        std::atomic<TaskStatus> status;
        std::atomic<size_t> remaining_retries;
        std::exception_ptr exception;
        Promise<TaskResult> promise;
        
        TaskInfo(TaskId id, TaskConfig config, std::function<Lazy<void>()> task_func)
            : id(id)
            , config(config)
            , task_func(std::move(task_func))
            , status(TaskStatus::Pending)
            , remaining_retries(config.max_retries)
        {}
    };
    
    std::atomic<TaskId> _next_task_id{1};
    std::map<TaskId, std::shared_ptr<TaskInfo>> _tasks;
    mutable std::mutex _tasks_mutex;
    
    // 执行任务
    void executeTask(std::shared_ptr<TaskInfo> task_info);
    
    // 任务完成处理
    void onTaskCompleted(std::shared_ptr<TaskInfo> task_info, std::exception_ptr exception = nullptr);
    
    // 重试任务
    void retryTask(std::shared_ptr<TaskInfo> task_info);
    
    // 检查依赖是否完成
    bool checkDependenciesCompleted(const std::shared_ptr<TaskInfo>& task_info) const;
    
    // 获取可用的执行器
    Executor* getExecutorForTask(const TaskConfig& config) const;
};

// 任务调度器实现
TaskScheduler::TaskScheduler(Executor* default_executor)
    : _default_executor(default_executor)
{
}

TaskScheduler::~TaskScheduler() {
    // 取消所有未完成的任务
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    for (auto& [task_id, task_info] : _tasks) {
        if (task_info->status.load() == TaskStatus::Pending || 
            task_info->status.load() == TaskStatus::Running) {
            cancelTask(task_id);
        }
    }
}

template <typename Func, typename... Args>
TaskId TaskScheduler::submitTask(Func&& func, Args&&... args, const TaskConfig& config) {
    // 创建任务函数
    auto task_func = [func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Lazy<void> {
        co_await std::apply(func, std::move(args));
    };
    
    // 生成任务ID
    TaskId task_id = _next_task_id.fetch_add(1);
    
    // 创建任务信息
    auto task_info = std::make_shared<TaskInfo>(task_id, config, std::move(task_func));
    
    // 添加到任务列表
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        _tasks.emplace(task_id, task_info);
    }
    
    // 执行任务
    executeTask(task_info);
    
    return task_id;
}

template <typename Func, typename... Args>
TaskId TaskScheduler::submitDependentTask(const std::vector<TaskId>& dependencies, Func&& func, Args&&... args, 
    const TaskConfig& config) {
    // 创建任务函数
    auto task_func = [func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Lazy<void> {
        co_await std::apply(func, std::move(args));
    };
    
    // 生成任务ID
    TaskId task_id = _next_task_id.fetch_add(1);
    
    // 创建任务信息
    auto task_info = std::make_shared<TaskInfo>(task_id, config, std::move(task_func));
    task_info->dependencies = dependencies;
    
    // 添加到任务列表
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        _tasks.emplace(task_id, task_info);
        
        // 更新依赖关系
        for (TaskId dep_id : dependencies) {
            auto dep_it = _tasks.find(dep_id);
            if (dep_it != _tasks.end()) {
                dep_it->second->dependents.push_back(task_id);
            } else {
                // 依赖任务不存在，标记当前任务为失败
                task_info->status.store(TaskStatus::Failed);
                task_info->exception = std::make_exception_ptr(std::runtime_error("Dependency task not found"));
                task_info->promise.setValue({TaskStatus::Failed, task_info->exception});
                return task_id;
            }
        }
    }
    
    // 检查依赖是否已经完成
    if (checkDependenciesCompleted(task_info)) {
        executeTask(task_info);
    }
    
    return task_id;
}

bool TaskScheduler::cancelTask(TaskId task_id) {
    std::shared_ptr<TaskInfo> task_info;
    
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        auto it = _tasks.find(task_id);
        if (it == _tasks.end()) {
            return false;
        }
        task_info = it->second;
    }
    
    TaskStatus current_status = task_info->status.load();
    if (current_status != TaskStatus::Pending && current_status != TaskStatus::Running) {
        return false;
    }
    
    if (task_info->status.compare_exchange_strong(current_status, TaskStatus::Cancelled)) {
        // 这里简化处理，实际应该取消正在执行的任务
        onTaskCompleted(task_info, std::make_exception_ptr(std::runtime_error("Task cancelled")));
        return true;
    }
    
    return false;
}

TaskStatus TaskScheduler::getTaskStatus(TaskId task_id) const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) {
        return TaskStatus::Failed;
    }
    return it->second->status.load();
}

Future<TaskResult> TaskScheduler::waitForTask(TaskId task_id) {
    std::shared_ptr<TaskInfo> task_info;
    
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        auto it = _tasks.find(task_id);
        if (it == _tasks.end()) {
            Promise<TaskResult> promise;
            promise.setValue({TaskStatus::Failed, std::make_exception_ptr(std::runtime_error("Task not found"))});
            return promise.getFuture();
        }
        task_info = it->second;
    }
    
    return task_info->promise.getFuture();
}

Future<std::vector<TaskResult>> TaskScheduler::waitForAllTasks() {
    Promise<std::vector<TaskResult>> promise;
    auto future = promise.getFuture();
    
    std::vector<std::shared_ptr<TaskInfo>> tasks;
    
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        for (auto& [task_id, task_info] : _tasks) {
            tasks.push_back(task_info);
        }
    }
    
    if (tasks.empty()) {
        promise.setValue({});
        return future;
    }
    
    // 使用CollectAll来等待所有任务完成
    auto collect_lazy = [tasks = std::move(tasks)]() mutable -> Lazy<std::vector<TaskResult>> {
        std::vector<Lazy<TaskResult>> lazy_tasks;
        for (auto& task_info : tasks) {
            lazy_tasks.emplace_back([task_info]() -> Lazy<TaskResult> {
                co_return co_await task_info->promise.getFuture();
            });
        }
        
        auto results = co_await collectAll(std::move(lazy_tasks));
        std::vector<TaskResult> task_results;
        for (auto& result : results) {
            task_results.push_back(result.value());
        }
        
        co_return task_results;
    };
    
    collect_lazy().start([promise = std::move(promise)](Try<std::vector<TaskResult>>&& result) {
        if (result.hasValue()) {
            promise.setValue(std::move(result.value()));
        } else {
            promise.setException(result.exception());
        }
    });
    
    return future;
}

void TaskScheduler::executeTask(std::shared_ptr<TaskInfo> task_info) {
    if (task_info->status.load() != TaskStatus::Pending) {
        return;
    }
    
    if (!task_info->status.compare_exchange_strong(TaskStatus::Pending, TaskStatus::Running)) {
        return;
    }
    
    Executor* executor = getExecutorForTask(task_info->config);
    if (!executor) {
        onTaskCompleted(task_info, std::make_exception_ptr(std::runtime_error("No executor available")));
        return;
    }
    
    // 执行任务
    task_info->task_func().via(executor).start([this, task_info](Try<void>&& result) {
        if (result.hasException()) {
            onTaskCompleted(task_info, result.exception());
        } else {
            onTaskCompleted(task_info);
        }
    });
}

void TaskScheduler::onTaskCompleted(std::shared_ptr<TaskInfo> task_info, std::exception_ptr exception) {
    TaskStatus current_status = task_info->status.load();
    if (current_status == TaskStatus::Cancelled) {
        return;
    }
    
    if (exception) {
        size_t remaining_retries = task_info->remaining_retries.fetch_sub(1);
        if (remaining_retries > 0) {
            // 还有重试次数，重试任务
            retryTask(task_info);
            return;
        } else {
            // 重试次数耗尽，标记任务为失败
            task_info->status.store(TaskStatus::Failed);
            task_info->exception = exception;
        }
    } else {
        // 任务成功完成
        task_info->status.store(TaskStatus::Completed);
    }
    
    // 通知任务完成
    task_info->promise.setValue({task_info->status.load(), task_info->exception});
    
    // 执行依赖于当前任务的任务
    std::vector<TaskId> dependents;
    
    {   
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        dependents = task_info->dependents;
    }
    
    for (TaskId dep_id : dependents) {
        std::shared_ptr<TaskInfo> dep_task_info;
        
        {   
            std::lock_guard<std::mutex> lock(_tasks_mutex);
            auto it = _tasks.find(dep_id);
            if (it == _tasks.end()) {
                continue;
            }
            dep_task_info = it->second;
        }
        
        if (checkDependenciesCompleted(dep_task_info)) {
            executeTask(dep_task_info);
        }
    }
}

void TaskScheduler::retryTask(std::shared_ptr<TaskInfo> task_info) {
    Executor* executor = getExecutorForTask(task_info->config);
    if (!executor) {
        onTaskCompleted(task_info, std::make_exception_ptr(std::runtime_error("No executor available for retry")));
        return;
    }
    
    // 在指定延迟后重试任务
    executor->scheduleAfter(task_info->config.retry_delay.count(), [this, task_info]() {
        executeTask(task_info);
    });
}

bool TaskScheduler::checkDependenciesCompleted(const std::shared_ptr<TaskInfo>& task_info) const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    for (TaskId dep_id : task_info->dependencies) {
        auto dep_it = _tasks.find(dep_id);
        if (dep_it == _tasks.end() || 
            dep_it->second->status.load() != TaskStatus::Completed) {
            return false;
        }
    }
    
    return true;
}

Executor* TaskScheduler::getExecutorForTask(const TaskConfig& config) const {
    if (config.executor) {
        return config.executor;
    }
    return _default_executor;
}

} // namespace coro
} // namespace async_simple

#endif // ASYNC_SIMPLE_CORO_TASKSCHEDULER_H