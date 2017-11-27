cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

set(project_SOURCES
    source/necro.c
    source/arena.c
    source/parser.c
    source/lexer.c
	source/intern.c
	source/utility.c
    source/runtime.c
    source/slab.c
    source/vault.c
    source/region.c
    )
set(project_HEADERS
    source/necro.h
    source/arena.h
    source/parser.h
    source/lexer.h
    source/math.h
	source/intern.h
	source/utility.h
    source/runtime.h
    source/slab.h
    source/vault.h
    source/region.h
    )

ADD_EXECUTABLE(necro ${project_SOURCES})

TARGET_LINK_LIBRARIES(necro)
