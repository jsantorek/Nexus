cmake_minimum_required(VERSION 4.0.3...4.3.3)
find_package(Git)

string(TIMESTAMP TIMESTAMP_VERSION "%Y;%m;%d;%H%M" UTC)
set(PROJECT_VERSION "")
foreach(part IN LISTS TIMESTAMP_VERSION)
  string(REGEX REPLACE "^0+([0-9])" "\\1" part "${part}")
  if(part STREQUAL "")
    list(APPEND PROJECT_VERSION "0")
  else()
    list(APPEND PROJECT_VERSION "${part}")
  endif()
endforeach()
string(REPLACE ";" "." PROJECT_VERSION "${PROJECT_VERSION}")
message("Timestamp based versioning: ${PROJECT_VERSION}")


get_filename_component(PROJECT_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
string(REGEX REPLACE "^[^-]+-" "" PROJECT_NAME "${PROJECT_NAME}")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX RC)
file(GLOB_RECURSE RSCs ${CMAKE_CURRENT_SOURCE_DIR}/res/*.rc)
file(GLOB_RECURSE SRCs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE 3PTs ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/*.cpp)
add_library(${PROJECT_NAME}
    SHARED
        ${SRCs}
        ${RSCs}
        ${3PTs}
)

target_link_libraries(${PROJECT_NAME}
  advapi32
  crypt32
  gdi32
  ntdll
  shcore
  dbghelp
  user32
  ws2_32
  libssl64MT
  libcrypto64MT
)
target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl/lib/VC/static
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NEXUS_EXPORTS
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wno-microsoft-cast
        -Wno-nontrivial-memcall
        -Wno-deprecated-literal-operator
        -Wno-switch
        -Wno-format-security
        -Wno-deprecated-declarations
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/generated
)
string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" PROJECT_FILENAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME d3d11)
execute_process(
    COMMAND
        ${GIT_EXECUTABLE} branch --show-current
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
file(WRITE ${CMAKE_BINARY_DIR}/generated/Branch.h "// Auto-generated header. Do not edit manually.
#pragma once
#define BRANCH_NAME \"${GIT_BRANCH_NAME}\"
")
file(WRITE ${CMAKE_BINARY_DIR}/generated/imgui.h "// Auto-generated header. Do not edit manually.
#pragma once
#include <imgui/imgui.h>
")
file(WRITE ${CMAKE_BINARY_DIR}/generated/imgui_internal.h "// Auto-generated header. Do not edit manually.
#pragma once
#include <imgui/imgui_internal.h>
")
file(WRITE ${CMAKE_BINARY_DIR}/generated/src/Version.h "// Auto-generated header. Do not edit manually.
#pragma once
#include <Version.h>
")
file(WRITE ${CMAKE_BINARY_DIR}/generated/Version.h "// Auto-generated header. Do not edit manually.
#pragma once
#define V_MAJOR ${PROJECT_VERSION_MAJOR}
#define V_MINOR ${PROJECT_VERSION_MINOR}
#define V_BUILD ${PROJECT_VERSION_PATCH}
#define V_REVISION ${PROJECT_VERSION_TWEAK}
")
