#ifndef ARCH_AMD64_H
#define ARCH_AMD64_H

#include "CothreadPrivate.h"

#include <cstddef>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on amd64 based systems.
 */
struct Amd64 {
    /**
     * Allocates the current physical (kernel) thread's Cothread object.
     */
    static void AllocMainCothread();

    /**
     * Ensures the provided stack size is valid.
     *
     * @param size Size of stack, in bytes.
     *
     * @throw std::runtime_error Stack size is invalid (misaligned, too small, etc.)
     */
    static void ValidateStackSize(const size_t size);

    /**
     * Given a wrapper structure and initial function to invoke, prepares the context of the
     * cothread such that it will return to the start of this method.
     *
     * @param thread Cothread whose stack frame is to be prepared
     * @param entry Function to return control to when switching to this cothread
     * @param ctx Arbitrary pointer to pass to the entry point
     */
    static void Prepare(Cothread *thread, void (*entry)(void *), void *ctx = nullptr);

    /**
     * Performs a context switch.
     *
     * @remark The implementation of this method is written in assembly and varies slightly depending
     *         on the calling convention of the platform (SystemV vs. Windows.)
     *
     * @param from Cothread buffer that will receive the current context
     * @param to Cothread buffer whose context is to be restored
     */
    static void Switch(Cothread *from, Cothread *to);

    /**
     * Pops two arguments off the stack (the entry point and its context argument) and invokes the
     * entry point.
     *
     * @remark The implementation of this method is written in assembly and varies slightly depending
     *         on the calling convention of the platform (SystemV vs. Windows.)
     *
     * @remark The arguments are implicit; they reside on the stack when this method is invoked.
     */
    static void JumpToEntry(/* void (*func)(void *), void *ctx */);

    /**
     * Stub method that fixes the stack alignment before invoking the error handler for a cothread
     * that returned from its main method.
     */
    static void EntryReturnedStub();

    /**
     * Invoked when the main method of a cothread returns.
     */
    static void CothreadReturned();
};
}

#endif
