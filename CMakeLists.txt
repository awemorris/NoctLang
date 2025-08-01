cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(NoctLang LANGUAGES C)
endif()

#set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

#
# Configuration
#

option(ENABLE_SHARED "Enable shared library" OFF)
option(ENABLE_JIT    "Enable JIT"            ON)
option(ENABLE_I18N   "Enable translation"    OFF)

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
  src/execution.c
  src/intrinsics.c
  src/gc.c
)

if(ENABLE_JIT)
  set(JIT_SOURCE
    src/jit-common.c
    src/jit-x86_64.c
    src/jit-x86.c
    src/jit-arm64.c
    src/jit-arm32.c
    src/jit-ppc64.c
    src/jit-ppc32.c
    src/jit-mips64.c
    src/jit-mips32.c
  )
endif()

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
  ${JIT_SOURCE}
  ${I18N_SOURCE}
)

#
# CLI Target
#

add_executable(
  noct
  src/cli.c
  src/cback.c
  src/elback.c
)

target_link_libraries(noct PRIVATE noctlang)

#
# CPPFLAGS
#

# -Iinclude
target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noctlang PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(noct     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# -Isrc
target_include_directories(noctlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(noct     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# -DUSE_DLL
if(ENABLE_SHARED)
  target_compile_definitions(noctlang PRIVATE USE_DLL DLL_IMPL)
  target_compile_definitions(noct     PRIVATE USE_DLL)
endif()

# -DUSE_JIT
if(ENABLE_JIT)
  target_compile_definitions(noctlang PRIVATE USE_JIT)
  target_compile_definitions(noct     PRIVATE USE_JIT)
endif()

# -DUSE_GETTEXT_COMPAT
if(ENABLE_I18N)
  target_compile_definitions(noctlang PRIVATE USE_GETTEXT_COMPAT)
  target_compile_definitions(noct     PRIVATE USE_GETTEXT_COMPAT)
endif()  

#
# CFLAGS
#

# Use "Release" build type by default.
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

# Debug Configuration
if(MSVC)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /Zi /DDEBUG /UNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} -O0 -g3 -UNDEBUG")
endif()

# Release Configuration
if(MSVC)
  # MSVC
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /DNDEBUG")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  # Clang
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Oz -g0 -DNDEBUG")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # GCC
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -g0 -DNDEBUG")
endif()

# Unicode options.
if(WIN32)
  if(MSVC)
    target_compile_options(noctlang PRIVATE /utf-8)
    target_compile_options(noct     PRIVATE /utf-8)
  else()
    target_compile_options(noctlang PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
    target_compile_options(noct     PRIVATE -municode -finput-charset=utf-8 -fexec-charset=utf-8)
  endif()
endif()
