cmake_minimum_required(VERSION 4.3)
project(OC C)

set(CMAKE_C_STANDARD 11)

add_executable(OC
        parent.c
        child.c)
