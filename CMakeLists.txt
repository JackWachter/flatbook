cmake_minimum_required(VERSION 3.20)
project(flatbook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug build flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")

# Download GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.5
)
FetchContent_MakeAvailable(googlebenchmark)

# Testing
enable_testing()
add_executable(tests
    tests/test_orderbook.cpp
    tests/test_flatbook.cpp
    tests/harness.cpp
    src/orderbook.cpp
    src/flatbook.cpp
    src/events.cpp
)
target_include_directories(tests PRIVATE src)
target_link_libraries(tests PRIVATE GTest::gtest_main)

add_executable(benchmarks
    benchmarks/bench_books.cpp
    src/orderbook.cpp
    src/flatbook.cpp
    src/events.cpp
)
target_include_directories(benchmarks PRIVATE src)
target_link_libraries(benchmarks PRIVATE benchmark::benchmark)

add_executable(latency
    benchmarks/latency.cpp
    src/flatbook.cpp
    src/events.cpp
)
target_include_directories(latency PRIVATE src)

include(GoogleTest)
gtest_discover_tests(tests)