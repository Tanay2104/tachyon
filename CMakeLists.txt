# CMakelists for the project
cmake_minimum_required(VERSION 3.20)
project (Matching-Engine VERSION 1.0 LANGUAGES CXX)

# 1. Standard setup
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generates files for clangd

# Strict warnings
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -g -O0)
    add_link_options(-fsanitize=address)
endif()
# Include directories
include_directories(include)

cmake_policy(SET CMP0167 NEW)

include_directories(${Boost_INCLUDE_DIRS})

# Dependency management
include(FetchContent)
# Fetch Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# Fetch Google Benchmark
FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googlebenchmark)



# Building the engine
add_library(core_engine
          # src/client.cpp
          src/engine.cpp
          # src/gateway.cpp
          src/orderbook.cpp
          src/exchange.cpp
          src/tcpserver.cpp
          src/logger.cpp
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Building the main executable
add_executable(tachyon
              src/main_tachyon.cpp)
target_link_libraries(tachyon PRIVATE core_engine)


# Building a client bot.
add_executable(trader_bot
              src/client.cpp
              src/main_client.cpp)

# Build tests
enable_testing()
add_executable(testorderbook 
             tests/testorderbook.cpp
              #TEST FILES
)

add_executable(testcircularbuffer
                tests/testcircularbuffer.cpp)
add_executable(testintrusivelist
              tests/testintrusivelist.cpp)
add_executable(testserialisation
            tests/testserialisation.cpp)
add_executable(testflathashmap
            tests/testflathashmap.cpp)


target_link_libraries(testorderbook PRIVATE core_engine gtest_main)
target_link_libraries(testcircularbuffer PRIVATE gtest_main)
target_link_libraries(testintrusivelist PRIVATE gtest_main)
target_link_libraries(testserialisation PRIVATE gtest_main)
target_link_libraries(testflathashmap PRIVATE gtest_main)

# Build benchmarks
add_executable(benchmarkorderbook
            benchmarks/benchmark_main.cpp
            # BENCHMARK FILE
            )
add_executable(benchmarklockqueue
            benchmarks/benchmark_queues.cpp
          )
add_executable(benchmarkintrusivelist
            benchmarks/benchmark_intrusive_list.cpp)
add_executable(benchmarkflathashmap
          benchmarks/benchmark_flat_hashmap.cpp)

target_link_libraries(benchmarkorderbook PRIVATE core_engine benchmark::benchmark)
target_link_libraries(benchmarklockqueue PRIVATE benchmark::benchmark)
target_link_libraries(benchmarkintrusivelist PRIVATE benchmark::benchmark)
target_link_libraries(benchmarkflathashmap PRIVATE benchmark::benchmark)

