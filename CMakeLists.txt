###############################################################################
# Primary CMakeLists for libcommunism
###############################################################################
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(libcommunism VERSION 0.1 LANGUAGES C CXX ASM)

### Define the build options
option(BUILD_LIBCOMMUNISM_TESTS "Build libcommunism test cases" OFF)

### Include some modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(ExternalProject)
include(FetchContent)
include(TargetArch)

### Determine the architecture we're compiling for
target_architecture(TARGET_ARCH)
message(STATUS "Target architecture: ${TARGET_ARCH}")

### Force C and C++ standards, warnings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wmissing-declarations -Wformat=2 -Wcast-qual -Wundef
        -fdiagnostics-color=always -Wwrite-strings -Wimplicit-fallthrough
        -Wno-unused-private-field)
    add_compile_options(-Werror)
else()
    message(FATAL_ERROR "We do not know what C/C++ warning flags to use for compiler '${CMAKE_CXX_COMPILER_ID}'!")
endif()


### Build the core library
add_library(libcommunism
    src/Cothread.cpp
)

set_target_properties(libcommunism PROPERTIES OUTPUT_NAME communism)

target_include_directories(libcommunism PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(libcommunism PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

### Add target specific sources
if("x86_64" STREQUAL ${TARGET_ARCH})
    target_sources(libcommunism PRIVATE
        src/arch/Amd64.cpp
    )

    if(WIN32)

    else()
        target_sources(libcommunism PRIVATE
            src/arch/Amd64+SysV.cpp
            src/arch/Amd64+SysV.S
        )
    endif()
else()
    message(SEND_ERROR "don't know what arch specific sources are needed for '${TARGET_ARCH}'!")
endif()

### TODO: define install step

### If tests are desired, include the tests directory
if(BUILD_LIBCOMMUNISM_TESTS)
    add_subdirectory(test)
endif()
