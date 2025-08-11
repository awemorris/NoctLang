cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(NoctLang LANGUAGES C)
endif()

#set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

#
# Build Configuration
#

option(ENABLE_SHARED
 "Build a shared library"
  OFF
)

option(ENABLE_JIT
  "Enable JIT feature"
  ON
)

option(ENABLE_MULTITHREAD
  "Enable multithread support"
  OFF
)

option(ENABLE_CLI
  "Also build CLI binary"
  ON
)

option(ENABLE_I18N
  "Enable translation feature (increased binary size)"
  OFF
)

# Use "Release" build type by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

# Debug Configuration
if(MSVC)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /Zi /DDEBUG /UNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /DDEBUG /UNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} -O0 -g3 -UNDEBUG")
endif()

# Release Configuration
if(MSVC)
  # MSVC
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # GCC/Clang
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -g0 -DNDEBUG")
endif()

#
# Checks
#

include(CheckIncludeFile)

check_include_file("stdint.h" HAVE_STDINT_H)
if(HAVE_STDINT_H)
  add_definitions(-DHAVE_STDINT_H=1)
endif()

check_include_file("inttypes.h" HAVE_INTTYPES_H)
if(HAVE_INTTYPES_H)
  add_definitions(-DHAVE_INTTYPES_H=1)
endif()

check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
if(HAVE_SYS_TYPES_H)
  add_definitions(-DHAVE_SYS_TYPES_H=1)
endif()

#
# FLEX
#

find_package(FLEX REQUIRED)

FLEX_TARGET(Lexer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c
  COMPILE_FLAGS "--prefix=ast_yy"
)

#
# BISON
#

find_package(BISON REQUIRED)

BISON_TARGET(Parser
  ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
  COMPILE_FLAGS "-p ast_yy"
)

#
# Library Target
#

set(BASE_SOURCE
  ${FLEX_Lexer_OUTPUTS}
  ${BISON_Parser_OUTPUT_SOURCE}
  src/ast.c
  src/hir.c
  src/lir.c
  src/noct.c
  src/runtime.c
  src/interpreter.c
  src/jit.c
  src/execution.c
  src/intrinsics.c
  src/gc.c
)

if(ENABLE_I18N)
  set(I18N_SOURCE
    src/i18n.c
    src/translation.c
  )
endif()

if(ENABLE_SHARED)
  set(LIB_TYPE SHARED)
else()
  set(LIB_TYPE STATIC)
endif()

add_library(
  noctlang
  ${LIB_TYPE}

  ${BASE_SOURCE}
  ${I18N_SOURCE}
)

target_include_directories(noctlang PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(ENABLE_SHARED)
  target_compile_definitions(noctlang PRIVATE USE_DLL DLL_IMPL)
endif()

if(ENABLE_JIT)
  target_compile_definitions(noctlang PRIVATE USE_JIT)
endif()

if(ENABLE_MULTITHREAD)
  target_compile_definitions(noctlang PRIVATE USE_MULTITHREAD)
endif()

if(ENABLE_I18N)
  target_compile_definitions(noctlang PRIVATE USE_TRANSLATION)
endif()  

if(WIN32)
  if(MSVC)
    target_compile_options(noctlang PRIVATE /utf-8)
  else()
    target_compile_options(noctlang PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
  endif()
endif()

set(NOCTLANG_INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(NOCTLANG_INCLUDE_DIRS ${NOCTLANG_INCLUDE_DIR})
set(NOCTLANG_LIBRARY      $<TARGET_FILE:noctlang>)
set(NOCTLANG_LIBRARIES    ${LOCTLANG_LIBRARY})

#
# CLI Target
#

if(ENABLE_CLI)
  add_executable(
    noct

    src/cli.c
    src/cback.c
    src/elback.c
  )

  target_link_libraries(noct PRIVATE
    noctlang
  )

  target_include_directories(noct PRIVATE
     ${NOCTLANG_INCLUDE_DIRS}
     ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  if(ENABLE_SHARED)
    target_compile_definitions(noct PRIVATE USE_DLL)
  endif()

  if(ENABLE_JIT)
    target_compile_definitions(noct PRIVATE USE_JIT)
  endif()

  if(ENABLE_I18N)
    target_compile_definitions(noct PRIVATE USE_TRANSLATION)
  endif()  

  if(WIN32)
    if(MSVC)
      target_compile_options(noct PRIVATE /utf-8)
    else()
      target_compile_options(noct PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
    endif()
  endif()
endif()
