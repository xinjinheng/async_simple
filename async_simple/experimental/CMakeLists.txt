# CMakeLists.txt for experimental components

# Add header files
file(GLOB experimental_headers "*.h")

# Add test files
if (ASYNC_SIMPLE_ENABLE_TESTS)
    file(GLOB experimental_test_src "test/*.cpp")
    add_executable(experimental_test ${experimental_test_src})
    target_link_libraries(experimental_test async_simple ${deplibs} ${testdeplibs})
    add_test(NAME run_experimental_test COMMAND experimental_test)
endif()
