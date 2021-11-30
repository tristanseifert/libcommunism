---
layout: default
title: "Build Options"
description: "Customizing the resulting library"
permalink: /build-options/
---
A few options are available to customize the build processs of the library. Most standard CMake variables for libraries (such as `BUILD_SHARED`) apply, unless otherwise specified.

# Platform Support
Since the library supports many different platforms, it's possible to select specifically which platform is used. With the `PLATFORM_LIBCOMMUNISM` variable, you can determine what platform will be built.

## Automatic Detection
By default, this variable is set to `Auto`. With this value, the build script determines which platform specific code based on the target architecture and OS.

## Manual Selection
Any other value will override the automatic detection of the platform. The following values are supported platforms:

- amd64-win: 64-bit Intel x86 with Windows ABI
- amd64-sysv: 64-bit Intel x86 with System V ABI (macOS, BSD, Linux)
- aarch64-aapcs: 64-bit ARM using the standard [Procedure Call Standard](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)
- x86-fastcall: 32-bit Intel x86 with [fastcall](https://docs.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-160) calling convention. This is also supported by GCC and Clang, so it allows supporting all 32-bit x86 platforms using the same code.
- setjmp: Generic, fully portable [setjmp()](https://linux.die.net/man/3/sigsetjmp) context switching. Its performance is significantly better than the ucontext implementation, but still several times slower than fully native implementations.
- ucontext: Generic [setcontext()](https://en.wikipedia.org/wiki/Setcontext) based implementation. (_Note:_ This is egregiously slow and mainly intended as a proof of concept; the setjmp implementation blows it out of the water.)

# Tests
There are a few test cases that can be built into a test executable to exercise the library. By default, tests are not built; change the `BUILD_LIBCOMMUNISM_TESTS` variable.
