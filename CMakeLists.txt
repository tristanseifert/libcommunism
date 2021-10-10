###############################################################################
# Primary CMakeLists for libcommunism
###############################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(libcommunism VERSION 0.1 LANGUAGES C CXX)

### Define the build options
option(BUILD_LIBCOMMUNISM_TESTS "Build libcommunism test cases" OFF)

### Include some modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(ExternalProject)
include(FetchContent)
include(TargetArch)

### Force C and C++ standards, warnings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wmissing-declarations -Wformat=2 -Wcast-qual -Wundef
        -fdiagnostics-color=always -Wwrite-strings -Wimplicit-fallthrough
        -Wno-unused-private-field)
    add_compile_options(-Werror)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wmissing-declarations -Wformat=2 -Wcast-qual -Wundef
        -fdiagnostics-color=always -Wwrite-strings -Wimplicit-fallthrough)
    add_compile_options(-Werror)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # default warnings, treat warnings as errors
    # this breaks the MASM :(
    #add_compile_options(/W4 /WX)
else()
    message(FATAL_ERROR "We do not know what C/C++ warning flags to use for compiler '${CMAKE_CXX_COMPILER_ID}'!")
endif()

### Determine the architecture we're compiling for
target_architecture(TARGET_ARCH)
message(STATUS "Target architecture: ${TARGET_ARCH}")

### Use the manually specified arch code override, or autodetect it
set(PLATFORM_LIBCOMMUNISM "Auto" CACHE STRING "Architecture specific code to be built")
set_property(CACHE PLATFORM_LIBCOMMUNISM PROPERTY STRINGS Auto amd64-sysv amd64-win setjmp ucontext)

if("Auto" STREQUAL ${PLATFORM_LIBCOMMUNISM})
    if("x86_64" STREQUAL ${TARGET_ARCH})
        if(WIN32)
            set(PLATFORM_SOURCES_TYPE "amd64-win")
        else()
            set(PLATFORM_SOURCES_TYPE "amd64-sysv")
        endif()
    else()
        message(SEND_ERROR "failed to autodetect architecture variant for '${TARGET_ARCH}'!")
    endif()
else()
    set(PLATFORM_SOURCES_TYPE ${PLATFORM_LIBCOMMUNISM})
endif()

message(STATUS "Platform support variant: ${PLATFORM_SOURCES_TYPE}")


### Build the core library
add_library(libcommunism
    src/Cothread.cpp
)

set_target_properties(libcommunism PROPERTIES OUTPUT_NAME communism)

target_include_directories(libcommunism PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(libcommunism PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

### Add target specific sources
if("amd64-win" STREQUAL ${PLATFORM_SOURCES_TYPE})
    enable_language(ASM_MASM)

    target_sources(libcommunism PRIVATE
        src/arch/amd64/Common.cpp
        src/arch/Amd64/Windows.cpp
        src/arch/Amd64/Windows.asm
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_AMD64_WINDOWS)
elseif("amd64-sysv" STREQUAL ${PLATFORM_SOURCES_TYPE})
    enable_language(ASM)

    target_sources(libcommunism PRIVATE
        src/arch/amd64/Common.cpp
        src/arch/amd64/SysV.cpp
        src/arch/amd64/SysV.S
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_AMD64_SYSV)
elseif("setjmp" STREQUAL ${PLATFORM_SOURCES_TYPE})
    target_sources(libcommunism PRIVATE
        src/arch/setjmp/SetJmp.cpp
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_UCONTEXT)
elseif("ucontext" STREQUAL ${PLATFORM_SOURCES_TYPE})
    target_sources(libcommunism PRIVATE
        src/arch/ucontext/UContext.cpp
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_UCONTEXT)
else()
    message(SEND_ERROR "don't know what arch specific sources are needed for '${PLATFORM_SOURCES_TYPE}'!")
endif()

### TODO: define install step

### If tests are desired, include the tests directory
if(BUILD_LIBCOMMUNISM_TESTS)
    add_subdirectory(test)
endif()
