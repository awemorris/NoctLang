cmake_minimum_required(VERSION 3.22)
project(remacs C)

#
# Noct VM.
#
# remacs lives in samples/remacs/ inside the NoctLang tree, so the VM
# sources are two directories up. Build them as a sub-project (giving
# an explicit binary directory, since the source is an ancestor path).
#
set(NOCT_ENABLE_STATIC       ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_JIT          ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_MULTITHREAD  ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API          ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_SYSTEM   ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_CONSOLE  ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_FILE     ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_THREAD   ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_TERM     ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_PROCESS  ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_API_HTTPSERVER ON CACHE BOOL "" FORCE)
set(NOCT_ENABLE_ELBACKEND    ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_BCBACKEND    ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_CLI          ON  CACHE BOOL "" FORCE)
set(NOCT_ENABLE_INSTALL      OFF CACHE BOOL "" FORCE)
get_filename_component(NOCT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${NOCT_ROOT} ${CMAKE_BINARY_DIR}/noctlang)

#
# Generated FFI tables (from src/napi.def).
#
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(NAPI_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_custom_command(
  OUTPUT
    ${NAPI_GEN_DIR}/napi-init.noct
    ${NAPI_GEN_DIR}/lisp-bridge.noct
    ${NAPI_GEN_DIR}/elisp-ns-map.txt
    ${NAPI_GEN_DIR}/noct-shim.el
    ${NAPI_GEN_DIR}/napi-scope.txt
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen-napi.py
    ${CMAKE_CURRENT_SOURCE_DIR}/src/napi.def
    ${NAPI_GEN_DIR}
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen-napi.py
    ${CMAKE_CURRENT_SOURCE_DIR}/src/napi.def
  COMMENT "Generating FFI tables from napi.def"
)

#
# remacs development launcher.
#
# The shipped form is remacs.nap run by an unmodified noct (see the
# Makefile); this C launcher is only for development and the test
# suite, loading the editor sources from the tree.
#
add_executable(
  remacs
  src/main.c
  src/api-util.c
  ${NAPI_GEN_DIR}/napi-init.noct
)
set_source_files_properties(${NAPI_GEN_DIR}/napi-init.noct
                            PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(remacs PRIVATE src)
target_link_libraries(remacs PRIVATE noct noctapi)
target_compile_definitions(remacs PRIVATE
  REMACS_EDITOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/editor"
  REMACS_GEN_DIR="${NAPI_GEN_DIR}"
)
if(NOT MSVC)
  target_compile_options(remacs PRIVATE -Wall -Wextra)
endif()
