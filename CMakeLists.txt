cmake_minimum_required(VERSION 4.0)
project(miniShell C)

set(CMAKE_C_STANDARD 11)

include_directories(Main)

add_executable(miniShell
        Main/myshell.c
        Main/myshell.h
        Main/parser.h
        Main/test.c)
