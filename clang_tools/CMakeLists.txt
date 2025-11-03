cmake_minimum_required(VERSION 3.10)
project(SyncToAsyncConverter)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找Clang和LLVM库
find_package(Clang REQUIRED)
find_package(LLVM REQUIRED)

# 设置包含路径
include_directories(
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS}
)

# 设置链接路径
link_directories(
    ${CLANG_LIBRARY_DIRS}
    ${LLVM_LIBRARY_DIRS}
)

# 创建可执行文件
add_executable(SyncToAsyncConverter SyncToAsyncConverter.cpp)

# 链接所需的Clang和LLVM库
llvm_map_components_to_libnames(LLVM_LIBS support core)
target_link_libraries(SyncToAsyncConverter
    clangTooling
    clangAST
    clangASTMatchers
    clangRewrite
    clangFrontend
    clangBasic
    clangLex
    ${LLVM_LIBS}
)

# 设置编译器选项
target_compile_options(SyncToAsyncConverter PRIVATE -Wall -Wextra -Wpedantic)