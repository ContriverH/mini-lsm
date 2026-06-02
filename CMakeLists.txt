cmake_minimum_required(VERSION 3.14)
project(mini-lsm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Static library: all src/*.cpp files ---
file(GLOB LIB_SOURCES src/*.cpp)
add_library(mini_lsm STATIC ${LIB_SOURCES})
target_include_directories(mini_lsm PUBLIC src)

# --- CLI tool ---
add_executable(lsm-cli tools/lsm_cli.cpp)
target_link_libraries(lsm-cli PRIVATE mini_lsm)

# --- Tests (GoogleTest via FetchContent) ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB TEST_SOURCES tests/*_test.cpp)
foreach(test_source ${TEST_SOURCES})
  get_filename_component(test_name ${test_source} NAME_WE)
  add_executable(${test_name} ${test_source})
  target_link_libraries(${test_name} PRIVATE mini_lsm GTest::gtest_main)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
