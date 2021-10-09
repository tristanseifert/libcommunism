#ifndef ARCH_AMD64_H
#define ARCH_AMD64_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on amd64 based systems.
 */
struct Amd64 {
    /**
     * @brief Information required to make a function call for a cothread's entry point.
     */
    struct CallInfo {
        Cothread::Entry entry;
    };

    /**
     * Allocates the current physical (kernel) thread's Cothread object.
     *
     * @note This will leak the associated cothread object, unless the caller stores it somewhere
     *       and ensures they deallocate it later when the underlying kernel thread is destroyed.
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
     * Invoked when the main method of a cothread returns.
     */
    static void CothreadReturned();

    /**
     * Performs the call described inside a call info structure.
     *
     * @param info Pointer to the call info structure; it's deleted once the call returns.
     */
    static void DereferenceCallInfo(CallInfo *info);

    /**
     * Given a wrapper structure and initial function to invoke, prepares the context of the
     * cothread such that it will return to the start of this method.
     *
     * @param thread Cothread whose stack frame is to be prepared
     * @param entry Function to return control to when switching to this cothread
     */
    //static void Prepare(Cothread *thread, void (*entry)(void *), void *ctx = nullptr);
    static void Prepare(Cothread *thread, const Cothread::Entry &entry);

    /**
     * Performs a context switch.
     *
     * @remark The implementation of this method is written in assembly and varies slightly depending
     *         on the calling convention of the platform (System V vs. Windows.)
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
     *         on the calling convention of the platform (System V vs. Windows.)
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
     * Number of registers saved by the cothread swap code. This is used to correctly build the stack
     * frames during initialization.
     */
    static const size_t kNumSavedRegisters;

    /**
     * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
     * be large enough to fit the register stack frame. This _must_ be a power of two.
     *
     * It must be sufficiently large to store the callee-saved general purpose registers, as well as
     * all of the SSE registers on Windows.
     */
    static constexpr const size_t kMainStackSize{64};

    /**
     * Requested alignment for stack allocations.
     *
     * This is set to 64 bytes for cache line alignment. On amd64, the stack should always be at least
     * 16 byte aligned (for SSE quantities).
     */
    static constexpr const size_t kStackAlignment{64};

    /**
     * Platform default size to use for the stack, in bytes, if no size is requested by the caller. We
     * default to 512K.
     */
    static constexpr const size_t kDefaultStackSize{0x80000};

    /**
     * Handle for the currently executing cothread in the calling physical thread. This is updated when
     * switching cothreads, but will be `nullptr` until the first call to `SwitchTo()`.
     */
    static thread_local Cothread *gCurrentHandle;

    /**
     * Pseudo-stack to use for the "main" cothread, i.e. the native kernel thread executing before
     * a cothread is ever switched to it.
     *
     * This buffer receives the stack frame of the context of the thread on the first invocation to
     * SwitchTo(). When invoking the Current() method before executing a real cothread, the
     * returned Cothread will correspond to this buffer.
     *
     * It does not have to be particularly large, since the stack is actually allocated by the
     * system already, and this "stack" only holds the register state.
     */
    static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;
};
}

#endif
