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
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()

if (MSVC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /W4 /wd4201 /wd4204 /WX")
endif()

add_definitions(${CMAKE_C_FLAGS})

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
    source/machine
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
	source/utility/utility.c
    source/utility/hash_table.c
    source/utility/unicode_properties.c
    source/utility/result.c

    source/runtime/runtime.c
    source/runtime/mouse.c

    source/type/type.c
    source/type/kind.c
    source/type/infer.c
    source/type/prim.c
    source/type/type_class.c
    source/type/monomorphize.c
    source/type/derive.c

    source/core/core.c
    source/core/defunctionalization.c
    source/core/closure_conversion.c
    source/core/lambda_lift.c
    source/core/state_analysis.c
    source/core/core_pretty_print.c
    source/core/core_create.c

    source/machine/machine.c
    source/machine/machine_type.c
    source/machine/machine_print.c
    source/machine/machine_prim.c
    source/machine/machine_case.c
    source/machine/machine_build.c
    source/machine/machine_copy.c
    source/machine/machine_closure.c

    source/mach/mach_ast.c

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
    source/utility/small_array.h
    source/utility/unicode_properties.h
    source/utility/result.h

    source/runtime/runtime.h
    source/runtime/mouse.h

    source/type/type.h
    source/type/kind.h
    source/type/infer.h
    source/type/prim.h
    source/type/type_class.h
    source/type/monomorphize.h
    source/type/derive.h

    source/core/core.h
    source/core/defunctionalization.h
    source/core/closure_conversion.h
    source/core/lambda_lift.h
    source/core/state_analysis.h
    source/core/core_pretty_print.h
    source/core/core_create.h

    source/machine/machine.h
    source/machine/machine_type.h
    source/machine/machine_print.h
    source/machine/machine_prim.h
    source/machine/machine_case.h
    source/machine/machine_build.h
    source/machine/machine_copy.h
    source/machine/machine_closure.h

    source/mach/mach_ast.h

    source/codegen/codegen_llvm.h
    )

ADD_EXECUTABLE(necro ${project_HEADERS} ${project_SOURCES})

# Find the libraries that correspond to the LLVM components
# that we wish to use
# llvm_map_components_to_libnames(llvm_libs support core irreader analysis target ScalarOpts native passes mcjit)
llvm_map_components_to_libnames(llvm_libs core native passes mcjit target)

TARGET_LINK_LIBRARIES(necro ${llvm_libs})



execute_process (
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND bash -c "git config core.hooksPath .githooks"
)
