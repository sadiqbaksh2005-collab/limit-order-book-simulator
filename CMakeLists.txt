cmake_minimum_required(VERSION 3.14)
project(lob LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(book src/order_book.cpp)
target_include_directories(book PUBLIC include)

add_executable(bench bench/benchmark.cpp)
target_link_libraries(bench book)
add_executable(simulate bench/simulate.cpp)
target_link_libraries(simulate book)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(tests tests/test_book.cpp)
target_link_libraries(tests book GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(tests)