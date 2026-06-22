# Minimum CMake version required. CMake is the build system — it generates
# platform-specific build files (Makefiles on Linux/Mac, Visual Studio
# projects on Windows). Think of it as "webpack + package.json" for C++.
cmake_minimum_required(VERSION 3.14)

# Declare the project name and the language we use.
# LANGUAGES CXX = this is a C++ project (CXX is CMake's name for C++).
project(mini-lsm LANGUAGES CXX)

# Use C++17 standard. Like setting "target" in tsconfig.json.
# STANDARD_REQUIRED ON = fail if the compiler doesn't support C++17.
# EXTENSIONS OFF = don't use compiler-specific extensions (gcc extras, etc).
#   This keeps our code portable across compilers.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate a compile_commands.json file in the build directory.
# IDEs and tools (clangd, VS Code) read this to understand how each file
# is compiled — which flags, which includes, etc. Like a tsconfig for editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Static library: all src/*.cpp files ---
# GLOB = find all files matching the pattern (like fs.globSync("src/*.cpp")).
# We collect every .cpp file in src/ and compile them into a static library.
#
# Static library (.a file) = a bundle of compiled object files.
# Unlike a shared library (.so/.dylib), the code gets copied INTO the
# final executable at link time — no runtime dependency.
file(GLOB LIB_SOURCES src/*.cpp)
add_library(mini_lsm STATIC ${LIB_SOURCES})

# PUBLIC means: anyone who links against mini_lsm can also #include files
# from the src/ directory. This is how tests and the CLI find our headers.
target_include_directories(mini_lsm PUBLIC src)

# --- CLI tool ---
# Creates an executable binary from tools/lsm_cli.cpp.
# PRIVATE = the CLI uses mini_lsm internally but doesn't re-export it.
add_executable(lsm-cli tools/lsm_cli.cpp)
target_link_libraries(lsm-cli PRIVATE mini_lsm)

# --- Tests (GoogleTest via FetchContent) ---
# FetchContent downloads dependencies at configure time — like npm install.
# We pull GoogleTest v1.14.0 directly from GitHub (no need to install it
# manually). This is the C++ equivalent of "npm install --save-dev jest".
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# enable_testing() tells CMake we have tests. This lets us run "ctest"
# from the build directory to execute all registered tests.
enable_testing()

# Auto-discover all test files matching tests/*_test.cpp.
# For each one, we:
#   1. Extract the filename without extension (e.g., memtable_test)
#   2. Create an executable from that test file
#   3. Link it against our library AND GoogleTest
#   4. Register it with ctest so "ctest" runs it
#
# This means adding a new test is as simple as creating a new *_test.cpp
# file — no need to edit CMakeLists.txt.
file(GLOB TEST_SOURCES tests/*_test.cpp)
foreach(test_source ${TEST_SOURCES})
  get_filename_component(test_name ${test_source} NAME_WE)
  add_executable(${test_name} ${test_source})
  target_link_libraries(${test_name} PRIVATE mini_lsm GTest::gtest_main)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
