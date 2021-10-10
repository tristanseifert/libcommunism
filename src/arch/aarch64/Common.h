#ifndef ARCH_AARCH64_COMMON_H
#define ARCH_AARCH64_COMMON_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on 64 bit ARM machines.
 *
 * @remark The context of threads is stored at the top of the allocated stack. Therefore, roughly
 *         0x100 bytes fewer than provided are available as actual program stack.
 */
struct Aarch64 {
    /**
     * @brief Information required to make a function call for a cothread's entry point.
     */
    struct CallInfo {
        /// Entry point of the cothread
        Cothread::Entry entry;
    };

    /**
     * Performs a context switch.
     *
     * @remark The implementation of this method is written in assembly.
     *
     * @param from Cothread buffer that will receive the current context
     * @param to Cothread buffer whose context is to be restored
     */
    static void Switch(Cothread *from, Cothread *to);

    static void AllocMainCothread();
    static void ValidateStackSize(const size_t size);
    static void *AllocStack(const size_t bytes);
    static void DeallocStack(void* stack);
    static void CothreadReturned();
    static void DereferenceCallInfo(CallInfo *info);
    static void Prepare(Cothread *thread, const Cothread::Entry &entry);


    /**
     * Size of the reserved region, at the top of the stack, which is reserved for saving the
     * context of a thread.
     */
    static constexpr const size_t kContextSaveAreaSize{0x100};

    /**
     * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
     * be large enough to fit the register frame. This _must_ be a power of two.
     */
    static constexpr const size_t kMainStackSize{128};

    /**
     * Requested alignment for stack allocations.
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
