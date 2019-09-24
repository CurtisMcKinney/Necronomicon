# =============================================================================
# NOTE from Curtis:
# Please read ALL instructions before attempting to build Necronomicon
# Step 1: Install Python
#     - NOTE: LLVM seems to want Python 2, not 3!
# Step 2: Build LLVM
#     - Use cmake
#     - To build using cmake on Windows consult this: https://llvm.org/docs/GettingStartedVS.html
# Step 3: Install LLVM
#     - Once LLVM is built you should install it somewhere (In VS build using the INSTALL solution).
#     - Make note of where.
# Step 4:
#     - Generate project files for Necronomicon using cmake-gui
#     - When configuring/generating project files for Necronomicon with cmake-gui set LLVM_DIR to point to where you installed LLVM: <LLVM_Install_Folder>\lib\cmake\llvm
# Necronomicon should now be a functional project you can build
# This cmake file was cobbled together by consulting this: https://llvm.org/docs/CMake.html
# =============================================================================

cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-function -Wno-multistatement-macros")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-function")
endif()

if (MSVC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /W4 /wd4201 /wd4204 /WX")
endif()

add_definitions(${CMAKE_C_FLAGS})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(INCLUDE_DIR
    source
    source/lex
    source/ast
    source/necro
    source/base
    source/type
    source/runtime
    source/utility
    source/symbol
    source/parse
    source/core
    source/mach
    source/codegen
    )

include_directories(${INCLUDE_DIR})

set(project_SOURCES
    source/necro/necro.c
    source/necro/driver.c

    source/lex/lexer.c

    source/ast/symtable.c
    source/ast/ast.c
    source/ast/d_analyzer.c

    source/base/base.c

    source/symbol/intern.c
    source/symbol/ast_symbol.c
    source/symbol/renamer.c

    source/parse/parser.c
    source/parse/parse_test.c
    source/parse/parse_ast.c

    source/utility/arena.c
    source/utility/itoa.c
	source/utility/utility.c
    source/utility/hash_table.c
    source/utility/unicode_properties.c
    source/utility/result.c

    source/runtime/runtime.c

    source/type/type.c
    source/type/kind.c
    source/type/infer.c
    source/type/type_class.c
    source/type/monomorphize.c
    source/type/alias_analysis.c
    source/type/derive.c

    source/core/core_ast.c
    source/core/defunctionalization.c
    source/core/lambda_lift.c
    source/core/state_analysis.c
    source/core/core_infer.c
    source/core/core_simplify.c

    source/mach/mach_ast.c
    source/mach/mach_type.c
    source/mach/mach_print.c
    source/mach/mach_transform.c
    source/mach/mach_case.c

    source/codegen/codegen_llvm.c
    )

set(project_HEADERS
    source/necro/necro.h
    source/necro/driver.h

    source/lex/lexer.h

    source/ast/symtable.h
    source/ast/ast.h
    source/ast/d_analyzer.h

    source/base/base.h

    source/symbol/intern.h
    source/symbol/ast_symbol.h
    source/symbol/renamer.h

    source/parse/parser.h
    source/parse/parse_test.h
    source/parse/parse_ast.h

    source/utility/arena.h
    source/utility/debug_memory.h
    source/utility/utility.h
    source/utility/math.h
    source/utility/hash_table.h
    source/utility/list.h
    source/utility/dequeue.h
    source/utility/small_array.h
    source/utility/unicode_properties.h
    source/utility/result.h

    source/runtime/runtime.h

    source/type/type.h
    source/type/kind.h
    source/type/infer.h
    source/type/type_class.h
    source/type/monomorphize.h
    source/type/alias_analysis.h
    source/type/derive.h

    source/core/core_ast.h
    source/core/defunctionalization.h
    source/core/lambda_lift.h
    source/core/state_analysis.h
    source/core/core_infer.h
    source/core/core_simplify.h

    source/mach/mach_ast.h
    source/mach/mach_type.h
    source/mach/mach_print.h
    source/mach/mach_transform.h
    source/mach/mach_case.h

    source/codegen/codegen_llvm.h
    )

ADD_EXECUTABLE(necro ${project_HEADERS} ${project_SOURCES})

# Find the libraries that correspond to the LLVM components
# that we wish to use
# llvm_map_components_to_libnames(llvm_libs support core irreader analysis target ScalarOpts native passes mcjit)
llvm_map_components_to_libnames(llvm_libs core native passes ScalarOpts analysis mcjit target)

TARGET_LINK_LIBRARIES(necro ${llvm_libs})



execute_process (
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND bash -c "git config core.hooksPath .githooks"
)
