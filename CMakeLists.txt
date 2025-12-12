# CMakelists for the project
cmake_minimum_required(VERSION 3.20)
project (Matching-Engine VERSION 1.0 LANGUAGES CXX)

# 1. Standard setup
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generates files for clangd

# Strict warnings
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=thread -g -O0)
    add_link_options(-fsanitize=thread)
endif()
# Include directories
include_directories(include)

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
          src/client.cpp
          src/engine.cpp
          src/gateway.cpp
          src/orderbook.cpp
          # PUT src FILES HERE
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Building the main executable
add_executable(app
              src/main.cpp)
target_link_libraries(app PRIVATE core_engine)

# Build tests
enable_testing()
add_executable(unit_tests 
             tests/testengine.cpp
              #TEST FILES
)

target_link_libraries(unit_tests PRIVATE core_engine gtest_main)

# Build benchmarks
add_executable(benchmarks benchmarks # BENCHMARK FILE
)
target_link_libraries(benchmarks PRIVATE core_engine benchmark::benchmark)

