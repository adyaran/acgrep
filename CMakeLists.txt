cmake_minimum_required(VERSION 3.20)

project(acgrep)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(acgrep)

add_executable(acgrep_tests
    tests/test_aho_corasick.cpp
    acgrep/aho_corasick.cpp
)

enable_testing()
add_test(NAME acgrep_tests COMMAND acgrep_tests)