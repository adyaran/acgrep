cmake_minimum_required(VERSION 3.20)

project(acgrep)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(acgrep
    acgrep.cpp
    aho_corasick.cpp
)