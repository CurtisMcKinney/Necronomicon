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

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# if (CMAKE_COMPILER_IS_GNUCC)
#     set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall")
# endif()
# if (MSVC)
#     set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /W4")
# endif()

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
    source/d_analyzer.c

    source/utility/arena.c
	source/utility/intern.c
	source/utility/utility.c
    source/utility/hash_table.c

    source/runtime/runtime.c
    source/runtime/mouse.c
    source/runtime/slab.c
    source/runtime/vault.c
    source/runtime/region.c

    source/type/type.c
    source/type/kind.c
    source/type/infer.c
    source/type/prim.c
    source/type/type_class.c
    source/type/derive.c

    source/core/core.c
    source/core/closure_conversion.c
    source/core/core_pretty_print.c
    source/core/core_create.c

    source/machine/machine.c
    source/machine/machine_type.c
    source/machine/machine_print.c
    source/machine/machine_prim.c
    source/machine/machine_case.c
    source/machine/machine_build.c
    source/machine/machine_persist.c

    source/codegen/codegen_llvm.c
    )

set(project_HEADERS
    source/necro.h
    source/parser.h
    source/lexer.h
    source/renamer.h
    source/symtable.h
    source/ast.h
    source/driver.h
    source/d_analyzer.h

    source/utility/arena.h
	source/utility/utility.h
	source/utility/intern.h
    source/utility/math.h
    source/utility/hash_table.h
    source/utility/list.h
    source/utility/small_array.h

    source/runtime/runtime.h
    source/runtime/mouse.h
    source/runtime/slab.h
    source/runtime/vault.h
    source/runtime/region.h

    source/type/type.h
    source/type/kind.h
    source/type/infer.h
    source/type/prim.h
    source/type/type_class.h
    source/type/derive.h

    source/core/core.h
    source/core/closure_conversion.h
    source/core/core_pretty_print.h
    source/core/core_create.h

    source/machine/machine.h
    source/machine/machine_type.h
    source/machine/machine_print.h
    source/machine/machine_prim.h
    source/machine/machine_case.h
    source/machine/machine_build.h
    source/machine/machine_persist.h

    source/codegen/codegen_llvm.h
    )

ADD_EXECUTABLE(necro ${project_HEADERS} ${project_SOURCES})

# Find the libraries that correspond to the LLVM components
# that we wish to use
# llvm_map_components_to_libnames(llvm_libs support core irreader analysis target ScalarOpts native passes mcjit)
llvm_map_components_to_libnames(llvm_libs core native passes mcjit)

TARGET_LINK_LIBRARIES(necro ${llvm_libs})
