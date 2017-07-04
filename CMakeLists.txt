cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

set(CMAKE_C_STANDARD 99)

set(project_SOURCES
    Source/necro.c
    )
set(project_HEADERS
    Source/necro.h
    )

ADD_EXECUTABLE(necro ${project_SOURCES})

TARGET_LINK_LIBRARIES(necro)
