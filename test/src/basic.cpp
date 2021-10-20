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
    Cothread* t1{nullptr};
    REQUIRE_NOTHROW(t1 = new Cothread([]() {}));
    REQUIRE(!!t1);
    REQUIRE_NOTHROW(delete t1);
}

/**
 * Ensure the stack size and stack top getters work as expected.
 */
TEST_CASE("stack accessors") {
    // size of the temp stack; doesn't matter as we won't execute it
    constexpr static const size_t kStackSize{1024 * 128};

    Cothread* t1{nullptr}, * t2{nullptr};
    REQUIRE_NOTHROW(t1 = new Cothread([]() {}, kStackSize));
    REQUIRE(!!t1);

    /*
     * Stack size must be what we specified, or slightly more, in case the platform allocated more
     * memory than we requested so it can hold its own data.
     */
    REQUIRE(t1->getStackSize() <= kStackSize + (1024));
    REQUIRE(!!t1->getStack());

    REQUIRE_NOTHROW(delete t1);

    /*
     * Now, preallocate a stack of fixed size and pass it to a cothread.
     */
    std::vector<uintptr_t> stack;
    stack.resize(kStackSize / sizeof(uintptr_t));

    REQUIRE_NOTHROW(t2 = new Cothread([]() {}, stack));

    /*
     * Ensure the stack size is (roughly) the same as what we originally specified. We allow some
     * wiggle room (in the downwards direction, to allow for platforms to reserve some stack for
     * context; and upwards, to ensure alignments) here.
     *
     * We also check that the top pointer is exactly at or beyond the start of the region we
     * allocated, again to allow for platform code to reserve some space at the start.
     */
    REQUIRE(t2->getStackSize() >= kStackSize - (1024 * 4));
    REQUIRE(t2->getStackSize() <= kStackSize + (1024));

    REQUIRE(!!t2->getStack());
    REQUIRE(t2->getStack() >= stack.data());

    REQUIRE_NOTHROW(delete t2);
}

/**
 * This test creates a cothread, switches to it, then back to the main cotherad. It ensures that
 * the context switch takes place and a counter is incremented from the cothread.
 */
TEST_CASE("context switch between main, cothread, and back") {
    static Cothread *t1{nullptr}, *main{nullptr};
    static size_t counter{0};

    main = Cothread::Current();
    REQUIRE(!!main);

    REQUIRE_NOTHROW(t1 = new Cothread([]() {
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

/**
 * This test creates a cothread, then returns from its main routine. We should catch the return in
 * our error handler.
 */
TEST_CASE("test return handler") {
    Cothread* t1{ nullptr }, * main{ nullptr };
    size_t counter1{ 0 }, counter2{ 0 };

    main = Cothread::Current();
    REQUIRE(!!main);

    REQUIRE_NOTHROW(t1 = new Cothread([&]() {
        counter1 = 69;
    }));
    REQUIRE(!!t1);

    // install the handler
    Cothread::SetReturnHandler([&](auto) {
        counter2 = 420;
        main->switchTo();
    });

    // switch to it
    REQUIRE(counter1 == 0);
    REQUIRE(counter2 == 0);
    t1->switchTo();

    // when we get back here, we _should_ have executed it once
    REQUIRE(counter1 == 69);
    REQUIRE(counter2 == 420);

    // clean up
    REQUIRE_NOTHROW(delete t1);
    Cothread::ResetReturnHandler();
}
