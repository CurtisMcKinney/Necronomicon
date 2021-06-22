# =============================================================================
# NOTE from Curtis:
# Step 0: READ ALL instructions before attempting to build Necronomicon
# Step 1: Install Python 2
#     - NOTE: LLVM seems to want Python 2, not 3!
# Step 2: Build LLVM
#     - Use cmake
#     - To build using cmake on Windows consult this: https://llvm.org/docs/GettingStartedVS.html
# Step 3: Install LLVM
#     - Once LLVM is built you should install it somewhere (In VS build using the INSTALL solution).
#     - Make note of where.
# Step 4: Build Port Audio
#     - Consult: http://www.portaudio.com/
#     - Tested with PortAudio 19
#     - Make note of where the include directory and static library is.
# Step 5: Build Port Midi
#     - Consult: http://portmedia.sourceforge.net/portmidi/doxygen/
#     - Make note of where the include directory and static library is.
# Step 6: Build libsndfile
#     - Consult: https://github.com/erikd/libsndfile/
#     - Tested with libsndfile version 1.0.29
#     - Make note of where the include directory and static library is.
# Step 7:
#     - Generate project files for Necronomicon using cmake-gui
#     - When configuring/generating project files for Necronomicon with cmake-gui set LLVM_DIR to point to where you installed LLVM: <LLVM_Install_Folder>\lib\cmake\llvm
#     - Configure the portaudio_include and portaudio_lib variables.
#     - Configure the portmidi and portmidi variables.
# Necronomicon should now be a functional project you can build
# This cmake file was cobbled together by consulting this: https://llvm.org/docs/CMake.html
# =============================================================================

cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(necro)

include(CheckFunctionExists)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if (MSVC)
    set(PORTAUDIO_INCLUDE "./portaudio/include/" CACHE PATH "portaudio include files")
    set(PORTAUDIO_LIB "./portaudio/build/Debug/portaudio_static.lib" CACHE FILEPATH "portaudio static library")
    set(PORTMIDI_INCLUDE "./portmidi/include/" CACHE PATH "portmidi include files")
    set(PORTMIDI_LIB "./portmidi/build/Debug/portmidi_static.lib" CACHE FILEPATH "portmidi static library")
	set(SNDFILE_INCLUDE "./libsndfile/include/" CACHE PATH "libsndfile include files")
    set(SNDFILE_LIB "./libsndfile/build/Debug/sndfile.lib" CACHE FILEPATH "libsndfile static library")
else()
    find_package(portaudio)
    find_package(portmidi)
    find_package(libsndfile)
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-function -Wno-multistatement-macros")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-function")
endif()

if (MSVC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /W4 /we4061 /we4062 /wd4201 /wd4204 /WX")
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
	${PORTAUDIO_INCLUDE}
  ${PORTMIDI_INCLUDE}
	${SNDFILE_INCLUDE}
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
    source/runtime/runtime_audio.c

    source/type/type.c
    source/type/kind.c
    source/type/infer.c
    source/type/type_class.c
    source/type/monomorphize.c
    source/type/alias_analysis.c

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
    source/utility/math_utility.h
    source/utility/hash_table.h
    source/utility/list.h
    source/utility/dequeue.h
    source/utility/small_array.h
    source/utility/unicode_properties.h
    source/utility/result.h

    source/runtime/runtime_common.h
    source/runtime/runtime.h
    source/runtime/runtime_audio.h

    source/type/type.h
    source/type/kind.h
    source/type/infer.h
    source/type/type_class.h
    source/type/monomorphize.h
    source/type/alias_analysis.h

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
TARGET_LINK_LIBRARIES(necro ${llvm_libs} ${PORTAUDIO_LIB} ${PORTMIDI_LIB} ${SNDFILE_LIB})

execute_process (
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND bash -c "git config core.hooksPath .githooks"
)
