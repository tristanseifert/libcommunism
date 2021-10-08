/*
 * Basic tests that ensure cothreads can pass control between one another properly.
 */
#include <catch2/catch.hpp>

#include <libcommunism/Cothread.h>

using namespace libcommunism;

/**
 * Allocates a cothread and ensures this succeeded, then deallocates it again.
 */
TEST_CASE("initialization of cothreads") {
    Cothread *t1;
    REQUIRE_NOTHROW(t1 = new Cothread([](auto) {}));
    REQUIRE(!!t1);
    REQUIRE_NOTHROW(delete t1);
}

/**
 * This test creates a cothread, switches to it, then back to the main cotherad. It ensures that
 * the context switch takes place and a counter is incremented from the cothread.
 */
TEST_CASE("context switch between main, cothread, and back") {
    static libcommunism::Cothread *t1{nullptr}, *main{nullptr};
    static size_t counter{0};

    main = Cothread::Current();
    REQUIRE(!!main);

    REQUIRE_NOTHROW(t1 = new Cothread([](auto) {
        counter++;
        main->switchTo();
    }));
    REQUIRE(!!t1);

    // switch to it
    counter = 0;
    REQUIRE(counter == 0);
    t1->switchTo();

    // when we get back here, we _should_ have executed it once
    REQUIRE(counter == 1);

    // clean up
    REQUIRE_NOTHROW(delete t1);
}
