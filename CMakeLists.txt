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
include(CheckSymbolExists)
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
set_property(CACHE PLATFORM_LIBCOMMUNISM PROPERTY STRINGS Auto amd64-sysv amd64-win aarch64-aapcs
    setjmp ucontext)

if("Auto" STREQUAL ${PLATFORM_LIBCOMMUNISM})
    if("x86_64" STREQUAL ${TARGET_ARCH})
        if(WIN32)
            set(PLATFORM_SOURCES_TYPE "amd64-win")
        else()
            set(PLATFORM_SOURCES_TYPE "amd64-sysv")
        endif()
    elseif("aarch64" STREQUAL ${TARGET_ARCH})
        # TODO: are there aarch64 platforms that don't use AAPCS?
        set(PLATFORM_SOURCES_TYPE "aarch64-aapcs")
    elseif("i386" STREQUAL ${TARGET_ARCH})
        set(PLATFORM_SOURCES_TYPE "x86-fastcall")
    endif()
else()
    set(PLATFORM_SOURCES_TYPE ${PLATFORM_LIBCOMMUNISM})
endif()

### if still unset, select the best generic approach (first sigsetjmp, then ucontext)
if(NOT DEFINED PLATFORM_SOURCES_TYPE)
    message(WARNING "Failed to select an optimized platform; attempting to pick a generic one")

    # check for the methods required by the setjmp approach
    check_symbol_exists(sigsetjmp "setjmp.h" HAVE_SIGSETJMP)
    check_symbol_exists(siglongjmp "setjmp.h" HAVE_SIGLONGJMP)
    check_symbol_exists(sigaltstack "signal.h" HAVE_SIGALTSTACK)
    check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)

    # check for the methods required by the ucontext approach
    set(CMAKE_REQUIRED_DEFINITIONS "-D_XOPEN_SOURCE")
    check_symbol_exists(getcontext "ucontext.h" HAVE_GETCONTEXT)
    check_symbol_exists(swapcontext "ucontext.h" HAVE_SWAPCONTEXT)

    # are all the methods for `setjmp` available?
    if(DEFINED HAVE_SIGSETJMP AND DEFINED HAVE_SIGLONGJMP AND DEFINED HAVE_SIGALTSTACK
            AND DEFINED HAVE_SIGACTION)
        set(PLATFORM_SOURCES_TYPE "setjmp")
    # are all the methods for `ucontext` available?
    elseif(DEFINED HAVE_GETCONTEXT AND DEFINED HAVE_SWAPCONTEXT)
        set(PLATFORM_SOURCES_TYPE "ucontext")
    # no suitable alternate methods
    else()
        message(FATAL_ERROR "Failed to autodetect a suitable generic platform implementation, "
            "please file an issue")
    endif()
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
elseif("aarch64-aapcs" STREQUAL ${PLATFORM_SOURCES_TYPE})
    enable_language(ASM)

    target_sources(libcommunism PRIVATE
        src/arch/aarch64/Common.cpp
        src/arch/aarch64/AAPCS.cpp
        src/arch/aarch64/AAPCS.S
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_AARCH64_AAPCS)
elseif("x86-fastcall" STREQUAL ${PLATFORM_SOURCES_TYPE})
    # compiled sources are always the same as fastcall calling convention is identical
    target_sources(libcommunism PRIVATE
        src/arch/x86/Common.cpp
    )

    # for MSVC toolchain, use the MASM formatted assembler
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        enable_language(ASM_MASM)
        # TODO: define this version of the source
    # otherwise, use the GNU AS style
    else()
        enable_language(ASM)
        target_sources(libcommunism PRIVATE
            src/arch/x86/Fastcall.S
        )
    endif()
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_X86_FASTCALL)
elseif("setjmp" STREQUAL ${PLATFORM_SOURCES_TYPE})
    target_sources(libcommunism PRIVATE
        src/arch/setjmp/SetJmp.cpp
    )
    target_compile_definitions(libcommunism PRIVATE -DPLATFORM_SETJMP)
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
