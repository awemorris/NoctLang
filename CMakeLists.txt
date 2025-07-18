cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(NoctLang LANGUAGES C)
endif()

set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

#
# Configuration
#

option(USE_SHARED      "Enable shared libraries" OFF)
option(USE_JIT         "Enable JIT"              ON)
option(USE_TRANSLATION "Enable translation"      ON)

#
# FLEX
#

find_package(FLEX REQUIRED)

FLEX_TARGET(Lexer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.yy.c
  COMPILE_FLAGS "--prefix=ast_yy"
)

if(USE_TRANSLATION)
  set_source_files_properties(${FLEX_Lexer_OUTPUTS}
    PROPERTIES COMPILE_DEFINITIONS "USE_GETTEXT_COMPAT"
  )
endif()

#
# BISON
#

find_package(BISON REQUIRED)

BISON_TARGET(Parser
  ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
  ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.tab.c
  DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.tab.h
  COMPILE_FLAGS "-p ast_yy"
)

if(USE_TRANSLATION)
  set_source_files_properties(${BISON_Parser_OUTPUT_SOURCE}
    PROPERTIES COMPILE_DEFINITIONS "USE_GETTEXT_COMPAT"
  )
endif()

#
# Source Files
#

set(LIB_SOURCES
  ${FLEX_Lexer_OUTPUTS}
  ${BISON_Parser_OUTPUT_SOURCE}
  src/ast.c
  src/hir.c
  src/lir.c
  src/runtime.c
  src/interpreter.c
  src/execution.c
  src/intrinsics.c
  src/jit-common.c
  src/jit-x86_64.c
  src/jit-x86.c
  src/jit-arm64.c
  src/jit-arm32.c
  src/jit-ppc64.c
  src/jit-ppc32.c
  src/jit-mips64.c
  src/jit-mips32.c
  src/cback.c
  src/elback.c
)

set(CLI_SOURCES
  ${LANG_SOURCES}
  src/cli.c
)

if(USE_TRANSLATION)
  set(I18N_SOURCES
    src/translation.c
  )
endif()

# Library
if(USE_SHARED)
  set(LIB_TYPE SHARED)
else()
  set(LIB_TYPE STATIC)
endif()
add_library(noctlang STATIC ${LIB_SOURCES} ${I18N_SOURCES})

# CLI
add_executable(noct ${CLI_SOURCES} ${I18N_SOURCES})
target_link_libraries(noct PRIVATE noctlang)

#
# CPPFLAGS
#

target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noctlang PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noct PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(noct PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(USE_JIT)
target_compile_definitions(noctlang PRIVATE USE_JIT)
target_compile_definitions(noct PRIVATE USE_JIT)
endif()

if(USE_TRANSLATION)
target_compile_definitions(noctlang PRIVATE USE_GETTEXT_COMPAT)
target_compile_definitions(noct PRIVATE USE_GETTEXT_COMPAT)
endif()  

#
# CFLAGS
#

# Use "Release" build type by default.
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

# Debug Configuration.
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_RELEASE}   -O0 -g3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -O0 -g3")

# Release Configuration.
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   -O3 -g0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0")

if(NOT MSVC)
  target_compile_options(noctlang PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
  target_compile_options(noct PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
else()
  target_compile_options(noctlang PRIVATE /utf-8)
  target_compile_options(noct PRIVATE /utf-8)
endif()
