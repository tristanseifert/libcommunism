#ifndef ARCH_UCONTEXT_UCONTEXT_H
#define ARCH_UCONTEXT_UCONTEXT_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <ucontext.h>

namespace libcommunism::internal {
/**
 * @brief Implementation of context switching that uses the C library's `setcontext()` methods
 *
 * This is intended mostly to be a "test" platform that can be used to verify that the core
 * library works, without relying on assembly other such fun stuff. It's not particularly fast so
 * other platform backends should always be used in preference.
 *
 * Since we need a place to store the context in addition to the stack, it's stored at the very
 * top of the allocated stack. When allocating the stack internally, this is taken account and some
 * extra space at the top is reserved for it; but this must be kept in mind when using an
 * externally allocated stack, as less (roughly `sizeof(ucontext_t)` and alignment) space than
 * provided will be actually be available as stack.
 *
 * @note Since ucontext has been deprecated since the 2008 revision of POSIX, this may stop
 *       working (or not even be supported to begin with) on any given platform in the future.
 */
struct UContext {
    /**
     * @brief Context structure passed to the entry point of an ucontext
     */
    struct Context {
        /// Entry point of the cothread
        Cothread::Entry entry;

        /// Initializes a context struct with the given entry point.
        Context(const Cothread::Entry &_entry) : entry(_entry) {}
    };

    /**
     * Returns a pointer to the `ucontext_t` structure for a given cooperative thread.
     *
     * It's stored at the top of its stack buffer. The actual stack available to the program will
     * be reduced accordingly, but it is still possible for the program to overflow into this
     * structure and wreak havoc.
     *
     * @param thread Thread whose user context struct to retrieve
     *
     * @return Thread's user context, in the stack allocation.
     */
    static ucontext_t *ContextFor(Cothread *thread) {
        return reinterpret_cast<ucontext_t *>(thread->stack.data());
    }

    static void AllocMainCothread();
    static void Prepare(Cothread *thread, const Cothread::Entry &entry);
    static void EntryStub(int id);
    static void InvokeCothreadDidReturnHandler(Cothread *from);

    /**
     * Requested alignment for stack allocations.
     *
     * 64 bytes is the most stringent alignment requirements we should probably encounter in the
     * real world (one cache line on most systems) and alignment doesn't result in _that_ much
     * overhead so this is fine.
     *
     * @remark This must be a power of 2.
     */
    static constexpr const size_t kStackAlignment{64};

    /**
     * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
     * be large enough to fit the register stack frame. This _must_ be a power of two.
     *
     * It must be sufficiently large to fit an ucontext_t in it.
     */
    static constexpr const size_t kMainStackSize{128};

    /**
     * Default stack size in bytes, if none was requested by the caller. Since this implementation
     * may work on different width architectures, we define this as 64K worth of machine words.
     */
    static constexpr const size_t kDefaultStackSize{sizeof(uintptr_t) * 0x10000};


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



    /**
     * Since `makecontext()` is cursed and only passes parameters of `int` size of the function,
     * this will break passing a pointer on most 64-bit platforms. Instead, we have this here map
     * that stores an int index, which the entry wrapper pulls out and gets the context from.
     *
     * The value going into it is based on some counter we increment.
     */
    static std::unordered_map<int, std::unique_ptr<Context>> gContextInfo;

    /**
     * Mutex protecting access to the context map. This is taken during insertion and removal of
     * entries in the Prepare() and entry stubs.
     */
    static std::mutex gContextInfoLock;

    /**
     * Value of the next integer of the context info map key.
     */
    static int gContextNextId;
};
}


#endif
