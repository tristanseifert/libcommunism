/*
 * Benchmarking of context switching
 */
#include <catch2/catch.hpp>

#include <libcommunism/Cothread.h>

using namespace libcommunism;

/**
 * Tests the time required for context switching.
 *
 * The resulting timings will actually capture two context switches: first, TO the cothread, then
 * back to the main cothread. So, results must be divided by two.
 */
TEST_CASE("benchmarks") {
    // set up the thread
    static Cothread *t1, *main;

    main = Cothread::Current();
    REQUIRE(!!main);

    REQUIRE_NOTHROW(t1 = new Cothread([]() {
        while(1) {
            main->switchTo();
        }
    }));
    REQUIRE(!!t1);
    t1->setLabel("test cothread");

    BENCHMARK_ADVANCED("context switch")(Catch::Benchmark::Chronometer meter) {
        // perform the benchmarking
        meter.measure([] {
            t1->switchTo();
        });
    };

    // clean up
    REQUIRE_NOTHROW(delete t1);
};
