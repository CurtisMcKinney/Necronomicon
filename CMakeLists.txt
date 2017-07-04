cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

set(CMAKE_C_STANDARD 99)

set(project_SOURCES
    source/necro.c
    source/arena.c
    )
set(project_HEADERS
    source/necro.h
    source/arena.h
    )

ADD_EXECUTABLE(necro ${project_SOURCES})

TARGET_LINK_LIBRARIES(necro)
