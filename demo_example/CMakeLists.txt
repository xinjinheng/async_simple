cmake_minimum_required(VERSION 3.16)
project(async_simple_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find asio
find_path(ASIO_INCLUDE_DIR asio.hpp)
if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "asio not found")
endif()

# Include async_simple headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Add executable
add_executable(test_network_features test_network_features.cpp)

# Link libraries
target_link_libraries(test_network_features pthread)

# Add include directories
target_include_directories(test_network_features PRIVATE
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add HTTP server executable
add_executable(http_server http/coroutine_http/http_server.cpp)
target_link_libraries(http_server pthread)
target_include_directories(http_server PRIVATE
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add SMTP client executable
add_executable(smtp_client smtp/smtp_client.cpp)
target_link_libraries(smtp_client pthread)
target_include_directories(smtp_client PRIVATE
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add definitions
if(WIN32)
    target_compile_definitions(test_network_features PRIVATE
        _WIN32_WINNT=0x0601
        ASIO_STANDALONE
    )
    target_compile_definitions(http_server PRIVATE
        _WIN32_WINNT=0x0601
        ASIO_STANDALONE
    )
    target_compile_definitions(smtp_client PRIVATE
        _WIN32_WINNT=0x0601
        ASIO_STANDALONE
    )
else()
    target_compile_definitions(test_network_features PRIVATE
        ASIO_STANDALONE
    )
    target_compile_definitions(http_server PRIVATE
        ASIO_STANDALONE
    )
    target_compile_definitions(smtp_client PRIVATE
        ASIO_STANDALONE
    )
endif()
