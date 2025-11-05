#include <async_simple/Future.h>
#include <async_simple/executors/SimpleExecutor.h>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/Timeout.h>
#include <async_simple/coro/Retry.h>
#include <async_simple/DebugUtils.h>
#include <async_simple/experimental/ConnectionPool.h>

#include <gtest/gtest.h>
#include <memory>
#include <chrono>

using namespace async_simple;
using namespace async_simple::coro;
using namespace async_simple::executors;
using namespace async_simple::experimental;

// Test 1: Timeout mechanism
testing::Test* timeout_test = nullptr;
TEST(MyTest, TestTimeout) {
    SimpleExecutor executor(2);
    
    // Test with timeout
    auto task = []() -> Lazy<int> {
        co_await sleep(std::chrono::milliseconds(200));
        co_return 42;
    };
    
    auto result = syncAwait(timeout(task(), std::chrono::milliseconds(100), &executor));
    EXPECT_FALSE(result.has_value()); // Should timeout
    
    // Test without timeout
    auto result2 = syncAwait(timeout(task(), std::chrono::milliseconds(300), &executor));
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, 42);
}

// Test 2: Null pointer detection
testing::Test* null_ptr_test = nullptr;
TEST(MyTest, TestNullPtrDetection) {
    SimpleExecutor executor(2);
    
    // Test dynamic null pointer detection
    int* ptr = nullptr;
    
    EXPECT_DEATH(checkPointerNotNull(ptr, "Test null pointer"), "");
    
    // Test shared_ptr null detection
    std::shared_ptr<int> sharedPtr;
    
    EXPECT_DEATH(checkSharedPtrNotNull(sharedPtr, "Test null shared_ptr"), "");
    
    // Test unique_ptr null detection  
    std::unique_ptr<int> uniquePtr;
    
    EXPECT_DEATH(checkUniquePtrNotNull(uniquePtr, "Test null unique_ptr"), "");
}

// Test 3: Connection pool
testing::Test* connection_pool_test = nullptr;
struct TestConnection {
    TestConnection(int id) : id(id) {}
    
    bool isValid() const {
        return true;
    }
    
    Lazy<int> sendRequest(const std::string& request) {
        co_return id;
    }
    
    int id;
};

TEST(MyTest, TestConnectionPool) {
    ConnectionPoolConfig config;
    config.maxConnections = 5;
    config.maxIdleTime = std::chrono::seconds(30);
    config.healthCheckInterval = std::chrono::seconds(5);
    config.connectionTimeout = std::chrono::milliseconds(500);
    
    SimpleExecutor executor(2);
    
    auto createConnection = [&executor]() -> Lazy<std::shared_ptr<TestConnection>> {
        static int connectionId = 0;
        co_await sleep(std::chrono::milliseconds(50));
        co_return std::make_shared<TestConnection>(connectionId++);
    };
    
    ConnectionPool<TestConnection> pool(config, createConnection, &executor);
    
    // Test get connection
    auto conn1 = syncAwait(pool.getConnection());
    EXPECT_TRUE(conn1);
    EXPECT_EQ(conn1->id, 0);
    
    // Test release connection
    syncAwait(pool.releaseConnection(conn1));
    
    // Test get same connection again
    auto conn2 = syncAwait(pool.getConnection());
    EXPECT_TRUE(conn2);
    EXPECT_EQ(conn2->id, 0);
    
    // Test multiple connections
    auto conn3 = syncAwait(pool.getConnection());
    EXPECT_TRUE(conn3);
    EXPECT_EQ(conn3->id, 1);
    
    syncAwait(pool.releaseConnection(conn2));
    syncAwait(pool.releaseConnection(conn3));
}

// Test 4: Exception propagation in Future chain
testing::Test* exception_propagation_test = nullptr;
TEST(MyTest, TestExceptionPropagation) {
    SimpleExecutor executor(2);
    
    Promise<int> p;
    auto future = p.getFuture().via(&executor);
    
    auto f = std::move(future)
        .thenValue([](int x) { return x + 10; })
        .thenValue([](int x) {
            throw std::runtime_error("Test exception");
            return x + 5;
        })
        .thenTry([](Try<int> t) {
            EXPECT_TRUE(t.hasError());
            return 0;
        });
    
    p.setValue(100);
    f.wait();
    EXPECT_EQ(f.get(), 0);
}

// Test 5: Retry mechanism
testing::Test* retry_test = nullptr;
TEST(MyTest, TestRetry) {
    SimpleExecutor executor(2);
    
    int attemptCount = 0;
    auto task = [&attemptCount]() -> Lazy<int> {
        attemptCount++;
        if (attemptCount < 3) {
            throw std::runtime_error("Temporary error");
        }
        co_return 42;
    };
    
    auto result = syncAwait(retryExponential(task, std::chrono::milliseconds(10), std::chrono::milliseconds(100), 3, std::nullopt, &executor));
    
    EXPECT_TRUE(result.succeeded);
    EXPECT_EQ(result.value, 42);
    EXPECT_EQ(result.attempts, 3);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}