# async_simple 实验性组件

这个目录包含了 async_simple 框架的实验性组件，主要用于网络协程异常防护与资源安全治理。

## 组件列表

### 1. Common.h
扩展了 Common.h 中的 logicAssert 功能，新增生产级 SafeAccess 宏，支持在资源访问异常时记录上下文并触发可配置的回调。

**使用示例：**
```cpp
#include "async_simple/experimental/Common.h"

// 设置全局安全访问回调
setSafeAccessCallback([](const std::string& context) {
    std::cout << "Safe access violation: " << context << std::endl;
    // 可以在这里添加告警上报等逻辑
});

// 使用 SAFE_ACCESS 宏访问资源
int* ptr = nullptr;
SAFE_ACCESS(ptr, *ptr = 42, "Setting value to null pointer");

// 使用 SAFE_ACCESS_WITH_DEFAULT 宏访问资源并返回默认值
int value = SAFE_ACCESS_WITH_DEFAULT(ptr, *ptr, 0, "Accessing null pointer");
```

### 2. NetworkException.h
定义了 NetworkException 系列异常，含连接超时、读写失败、协议错误等子类型，在 asio 错误码与异常间建立映射表。

**使用示例：**
```cpp
#include "async_simple/experimental/NetworkException.h"

try {
    // 执行网络操作
} catch (const ConnectionTimeoutException& e) {
    std::cout << "Connection timeout: " << e.what() << std::endl;
} catch (const IoException& e) {
    std::cout << "IO error: " << e.what() << ", error code: " << e.errorCode() << std::endl;
} catch (const NetworkException& e) {
    std::cout << "Network error: " << e.what() << std::endl;
}
```

### 3. NetworkOperationWrapper.h
基于现有 asio 适配层构建 NetworkOperationWrapper 统一包装器，为所有异步网络操作注入超时控制。

**使用示例：**
```cpp
#include "async_simple/experimental/NetworkOperationWrapper.h"

// 使用包装后的 async_connect 函数
auto [error, socket] = co_await async_simple::experimental::async_connect(
    io_context, "example.com", "80", std::chrono::milliseconds(5000));

// 使用包装后的 async_read_some 函数
char buffer[1024];
auto [read_error, bytes_read] = co_await async_simple::experimental::async_read_some(
    socket, asio::buffer(buffer), std::chrono::milliseconds(3000));

// 使用包装后的 async_write 函数
std::string data = "Hello, World!";
auto [write_error, bytes_written] = co_await async_simple::experimental::async_write(
    socket, asio::buffer(data), std::chrono::milliseconds(3000));
```

### 4. CoroExceptionMonitor.h
实现了 CoroExceptionMonitor 单例组件，提供全局异常监控与自愈能力。

**使用示例：**
```cpp
#include "async_simple/experimental/CoroExceptionMonitor.h"

// 注册全局异常处理函数
CoroExceptionMonitor::instance().registerGlobalHandler(
    [](const std::exception& e, const CoroutineContext& context) {
        std::cout << "Unhandled exception in coroutine " << context.coro_id << std::endl;
        std::cout << "Exception message: " << e.what() << std::endl;
        std::cout << "Coroutine context: " << std::endl << context.toString() << std::endl;
        // 可以在这里添加日志记录、告警推送等逻辑
    });

// 生成唯一协程 ID
uint64_t coro_id = CURRENT_CORO_ID();

// 记录协程上下文
CoroutineContext context;
RECORD_CORO_CONTEXT(context);
```

### 5. ResourceTracker.h
实现了 ResourceTracker 组件，用于资源泄漏检测与自动回收。

**使用示例：**
```cpp
#include "async_simple/experimental/ResourceTracker.h"

// 注册资源
uint64_t resource_id = ResourceTracker::instance().registerResource(
    ResourceType::Socket,
    "TCP socket to example.com:80",
    coro_id,
    [](uint64_t id) {
        // 资源清理逻辑
        std::cout << "Cleaning up resource " << id << std::endl;
    });

// 更新资源最后访问时间
ResourceTracker::instance().updateLastAccessTime(resource_id);

// 注销资源
ResourceTracker::instance().unregisterResource(resource_id);

// 扫描泄漏资源
std::string leak_report = ResourceTracker::instance().scanLeakedResources();
std::cout << leak_report << std::endl;

// 清理所有泄漏资源
ResourceTracker::instance().cleanupLeakedResources();

// 使用 ResourceGuard 进行 RAII 资源管理
ResourceGuard<int> guard(
    ResourceType::Other,
    "Test resource",
    coro_id,
    42,
    [](int handle) {
        std::cout << "Cleaning up resource with handle " << handle << std::endl;
    });
```

### 6. CoroutineResourceGuard.h
实现了 CoroutineResourceGuard 机制，为 Lazy 任务提供资源访问安全保障。

**使用示例：**
```cpp
#include "async_simple/experimental/CoroutineResourceGuard.h"

// 创建执行器和执行器守卫
AsioExecutor executor(io_context);
ExecutorGuard executor_guard(&executor);

// 创建 Promise 和 Promise 守卫
Promise<int> promise;
PromiseGuard<int> promise_guard(&promise);

// 创建协程资源守卫并添加资源
CoroutineResourceGuard resource_guard;
resource_guard.setExecutorGuard(executor_guard);
resource_guard.addPromiseGuard(promise_guard);

// 检查资源有效性
if (resource_guard.areAllResourcesValid()) {
    // 执行协程任务
}

// 使用 GUARDED_EXECUTOR_ACCESS 宏访问执行器
GUARDED_EXECUTOR_ACCESS(executor_guard, {
    executor_guard->schedule([]() {
        std::cout << "Executing task on guarded executor" << std::endl;
    });
});

// 使用 GUARDED_PROMISE_ACCESS 宏访问 Promise
GUARDED_PROMISE_ACCESS(promise_guard, {
    promise_guard->setValue(42);
});
```

## 测试

异常注入测试模块 ExceptionInjectionTest.cpp 包含了各种异常场景的测试用例，用于验证防护机制的有效性。

**运行测试：**
```bash
cd build
cmake ..
make ExceptionInjectionTest
./ExceptionInjectionTest
```

## 注意事项

1. 这些组件目前处于实验性阶段，API 可能会在未来版本中发生变化。
2. 建议在生产环境中谨慎使用，充分测试后再部署。
3. 某些功能（如栈跟踪捕获）目前尚未实现，需要根据实际需求进行扩展。
4. 资源泄漏检测功能会带来一定的性能开销，建议在调试阶段使用。
