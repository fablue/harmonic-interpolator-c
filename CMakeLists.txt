cmake_minimum_required(VERSION 3.8)
project(playground)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c)
add_executable(playground ${SOURCE_FILES})