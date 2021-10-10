#ifndef ARCH_SETJMP_SETJMP_H
#define ARCH_SETJMP_SETJMP_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>
#include <mutex>

#include <csetjmp>

namespace libcommunism::internal {
/**
 * @brief Context switching utilizing the C library `setjmp()` and `longjmp()` methods
 */
struct SetJmp {
    struct EntryContext {
        /// Cothread that is being created
        Cothread *thread{nullptr};
        /// Entry point of the cothread
        Cothread::Entry entry;
        /// Initializes a context struct with the given entry point.
        EntryContext(Cothread *_thread, const Cothread::Entry &_entry) : thread(_thread),
            entry(_entry) {}
    };

    static auto JmpBufFor(Cothread *thread) {
        return reinterpret_cast<sigjmp_buf *>(thread->stack.data());
    }

    static void AllocMainCothread();
    static void Prepare(Cothread *thread, const Cothread::Entry &entry);
    static void InvokeCothreadDidReturnHandler(Cothread *from);
    static void SignalHandlerSetupThunk(int);


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
     * It must be sufficiently large to fit an sigjmp_buf it.
     */
    static constexpr const size_t kMainStackSize{512};

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
     * system already, and this "stack" only holds the `sigjmp_buf`.
     */
    static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;


    static EntryContext *gCurrentlyPreparing;
    static std::mutex gSignalLock;
};
}

#endif
