---
title: "Build Options"
permalink: /build-options.html
---
A few options are available to customize the build processs of the library. Most standard CMake variables for libraries (such as `BUILD_SHARED`) apply, unless otherwise specified.

# Platform Support
Since the library supports many different platforms, it's possible to select specifically which platform is used. With the `ARCH_LIBCOMMUNISM` variable, you can determine what architecture will be built.

## Automatic Detection
By default, this variable is set to `Auto`. With this value, the build script determines which platform specific code based on the target architecture and OS.

## Manual Selection
Any other value will override the automatic detection of the platform. The following values are supported platforms:

- amd64-win: 64-bit Intel x86 with Windows ABI
- amd64-sysv: 64-bit Intel x86 with System V ABI (macOS, BSD, Linux)

# Tests
There are a few test cases that can be built into a test executable to exercise the library. By default, tests are not built; change the `BUILD_LIBCOMMUNISM_TESTS` variable.
