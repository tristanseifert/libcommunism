# libcommunism
libcommunism is a simple cooperative multithreading library. It implements the concept of CoThreads, which are lightweight streams of execution context switched in user space. Each CoThread has its own stack, which allows them to maintain implicit state between invocations. Control is passed cooperatively between threads, rather than preemptively as kernel threads are scheduled, hence the name.

The library has no external dependencies, besides a C++20 compiler and CMake 3.18. (Earlier versions of CMake will likely work, but were not tested. C++20 is required for native `std::span` support.)

More information is available on [the project website.](https://libcommunism.blraaz.me)

## Supported Platforms
Due to the nature of the library, some architecture specific code is required to implement context switching. Currently supported architectures and platforms are:

- aarch64 ([AAPCS](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst))
- amd64 (System V ABI)
- amd64 (Windows ABI)
- x86 (fastcall)
- setjmp
- ucontext

Support for additional platforms is relatively trivial to add, with only a few required methods.

Out of the box, the build system will autodetect the best platform implementation for the architecture and OS it is being built for. To override this behavior, you can set the `PLATFORM_LIBCOMMUNISM` variable.

## Documentation
Documentation on the library can be autogenerated from the sources using Doxygen and is [available here.](https://libcommunism.blraaz.me/docs/doxygen)

## Tests
Some basic tests are implemented via Catch2 and can be built by setting the `BUILD_LIBCOMMUNISM_TESTS` option in the CMake configuration. Of particular interest may be the `benchmark` tests, which test the context switching speed.
