################################################################################
# Builds the test cases for the libcommunism library.
################################################################################

# Import Catch2 module, for testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    #GIT_TAG        v3.0.0-preview3
    GIT_TAG        v2.13.7
)
FetchContent_MakeAvailable(Catch2)
#list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)

add_executable(tests
    src/main.cpp
    src/basic.cpp
    src/timing.cpp
)
target_link_libraries(tests Catch2::Catch2 libcommunism)

target_compile_definitions(tests PRIVATE -DCATCH_CONFIG_ENABLE_BENCHMARKING)

# auto discover tests
include(CTest)
include(Catch)
catch_discover_tests(tests)
