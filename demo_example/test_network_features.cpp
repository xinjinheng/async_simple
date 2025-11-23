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

#include <iostream>
#include <thread>
#include <chrono>
#include <asio.hpp>
#include <async_simple/coro/SyncAwait.h>
#include <async_simple/coro/Lazy.h>
#include "async_simple/NetworkException.h"
#include "async_simple/ResourceTracker.h"
#include "async_simple/CoroExceptionMonitor.h"
#include "asio_coro_util.hpp"

using namespace async_simple;
using namespace async_simple::coro;
using asio::ip::tcp;

// 测试超时控制
coro::Lazy<void> test_timeout() {
    std::cout << "Testing timeout control...\n";
    
    asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 0));
    tcp::socket socket(io_context);
    
    // 测试连接超时
    try {
        auto error = co_await async_connect(io_context, socket, "127.0.0.1", 
            std::to_string(acceptor.local_endpoint().port()), std::chrono::milliseconds(1000));
        std::cout << "Connect should have timed out but didn't, error: " << error.message() << std::endl;
    } catch (const NetworkException& e) {
        std::cout << "Connect timed out as expected, exception: " << e.what() << std::endl;
    }
    
    co_return;
}

// 测试资源跟踪
void test_resource_tracking() {
    std::cout << "Testing resource tracking...\n";
    
    asio::io_context io_context;
    tcp::socket socket(io_context);
    
    // 注册资源
    ResourceTracker::instance().registerResource(
        ResourceType::SOCKET,
        reinterpret_cast<uintptr_t>(&socket),
        "Test socket"
    );
    
    // 打印当前资源
    auto resources = ResourceTracker::instance().getAllResources();
    std::cout << "Current resources: " << resources.size() << std::endl;
    
    // 注销资源
    ResourceTracker::instance().unregisterResource(
        reinterpret_cast<uintptr_t>(&socket)
    );
    
    // 打印当前资源
    resources = ResourceTracker::instance().getAllResources();
    std::cout << "Current resources after unregister: " << resources.size() << std::endl;
}

// 测试异常监控
coro::Lazy<void> test_exception_monitor() {
    std::cout << "Testing exception monitor...\n";
    
    // 注册全局异常处理器
    auto handler_id = CoroExceptionMonitor::instance().registerGlobalHandler(
        [](const CoroExceptionContext& context) {
            std::cout << "Global exception handler called: " << context.exceptionMessage << std::endl;
        }
    );
    
    // 创建一个会抛出异常的协程
    auto bad_coro = []() -> coro::Lazy<void> {
        co_await coro::sleep(std::chrono::milliseconds(100));
        throw std::runtime_error("Test exception from coroutine");
    };
    
    // 启动协程
    bad_coro().detach();
    
    // 等待协程执行
    co_await coro::sleep(std::chrono::milliseconds(500));
    
    // 注销异常处理器
    CoroExceptionMonitor::instance().unregisterGlobalHandler(handler_id);
    
    co_return;
}

// 测试HTTP服务器超时
coro::Lazy<void> test_http_server_timeout() {
    std::cout << "Testing HTTP server timeout...\n";
    
    asio::io_context io_context;
    std::thread thd([&io_context] {
        asio::io_context::work work(io_context);
        io_context.run();
    });
    
    // 启动HTTP服务器
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9981));
    
    auto server_coro = [&acceptor]() -> coro::Lazy<void> {
        for (;;) {
            tcp::socket socket(acceptor.get_executor().context());
            try {
                auto error = co_await async_accept(acceptor, socket, std::chrono::milliseconds(5000));
                if (error) {
                    std::cout << "Accept error: " << error.message() << std::endl;
                    continue;
                }
                std::cout << "New connection accepted\n";
                
                // 读取数据，设置超时
                char buf[1024];
                auto [read_error, bytes_read] = co_await async_read_some(socket, asio::buffer(buf), std::chrono::milliseconds(3000));
                if (read_error) {
                    std::cout << "Read error: " << read_error.message() << std::endl;
                    continue;
                }
                
                std::cout << "Read " << bytes_read << " bytes\n";
            } catch (const NetworkException& e) {
                std::cout << "Server network exception: " << e.what() << std::endl;
            }
        }
    };
    
    server_coro().detach();
    
    // 等待服务器启动
    co_await coro::sleep(std::chrono::milliseconds(1000));
    
    // 测试客户端连接
    tcp::socket client_socket(io_context);
    auto error = co_await async_connect(io_context, client_socket, "127.0.0.1", "9981", std::chrono::milliseconds(3000));
    if (error) {
        std::cout << "Client connect error: " << error.message() << std::endl;
    } else {
        std::cout << "Client connected to server\n";
        
        // 不发送数据，等待服务器超时
        co_await coro::sleep(std::chrono::milliseconds(4000));
    }
    
    // 停止服务器
    io_context.stop();
    thd.join();
    
    co_return;
}

int main() {
    try {
        // 初始化异常监控
        CoroExceptionMonitor::instance();
        
        // 初始化资源跟踪
        ResourceTracker::instance();
        
        // 运行测试
        syncAwait(test_timeout());
        test_resource_tracking();
        syncAwait(test_exception_monitor());
        syncAwait(test_http_server_timeout());
        
        // 生成资源泄漏报告
        auto report = ResourceTracker::instance().generateLeakReport();
        std::cout << "\nResource leak report: " << std::endl;
        std::cout << "Total resources: " << report.totalResources << std::endl;
        std::cout << "Leaked resources: " << report.leakedResources << std::endl;
        
        std::cout << "\nAll tests completed successfully!\n";
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
