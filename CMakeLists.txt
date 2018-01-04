cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

set(INCLUDE_DIR
    source
    source/type
    source/runtime
    source/utility
    )

include_directories(${INCLUDE_DIR})

set(project_SOURCES
    source/necro.c
    source/parser.c
    source/lexer.c
    source/renamer.c
    source/symtable.c
    source/ast.c
    source/driver.c

    source/utility/arena.c
	source/utility/intern.c
	source/utility/utility.c
    source/utility/hash_table.c

    source/runtime/runtime.c
    source/runtime/slab.c
    source/runtime/vault.c
    source/runtime/region.c

    source/type/type.c
    source/type/infer.c
    source/type/prim.c
    source/type/type_class.c

    source/core/core.c
    )
set(project_HEADERS
    source/necro.h
    source/parser.h
    source/lexer.h
    source/renamer.h
    source/symtable.h
    source/ast.h
    source/driver.h

    source/utility/arena.h
	source/utility/utility.h
	source/utility/intern.h
    source/utility/math.h
    source/utility/hash_table.h

    source/runtime/runtime.h
    source/runtime/slab.h
    source/runtime/vault.h
    source/runtime/region.h

    source/type/type.h
    source/type/infer.h
    source/type/prim.h
    source/type/type_class.h
    
    source/core/core.h
    )

ADD_EXECUTABLE(necro ${project_HEADERS} ${project_SOURCES})

TARGET_LINK_LIBRARIES(necro)
