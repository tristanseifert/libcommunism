#ifndef ARCH_x86_COMMON_H
#define ARCH_x86_COMMON_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>

#if defined(_MSC_VER) && !__INTEL_COMPILER
#define FASTCALL_TAG __fastcall
#else
#define FASTCALL_TAG __attribute__((fastcall))
#endif

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on x86 systems
 *
 * This implementation exploits the fact that clang and GCC both support Microsoft's [fastcall]
 * (https://docs.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-160) calling convention, so we
 * can get away with one implementation for both System V and Windows platforms, albeit with
 * differing assembly syntaxes.
 *
 * In operation, this is identical to the Amd64 implementation; thread state is stored on the
 * stack and the cothread's `stackTop` pointer actually points to the stack pointer of the cothread
 * when it is switched out.
 */
struct x86 {
    /**
     * @brief Information required to make a function call for a cothread's entry point.
     */
    struct CallInfo {
        /// Entry point of the cothread
        Cothread::Entry entry;
    };

    static void AllocMainCothread();
    static void ValidateStackSize(const size_t size);
    static void* AllocStack(const size_t bytes);
    static void DeallocStack(void* stack);
    static void CothreadReturned();
    static void FASTCALL_TAG DereferenceCallInfo(CallInfo *info);
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
    static void FASTCALL_TAG Switch(Cothread *from, Cothread *to);

    /**
     * Pops two words off the stack (for the address of the entry function, and its first register
     * argument) and sets up for a `fastcall` to that method.
     *
     * @remark This is necessary because we can't make a fastcall directly on return from switching
     *         as these registers are used by the arguments to the context switch call.
     */
    static void JumpToEntry();


    /**
     * Number of registers saved by the cothread swap code. This is used to correctly build the stack
     * frames during initialization.
     */
    static const size_t kNumSavedRegisters{4};

    /**
     * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
     * be large enough to fit the register stack frame. This _must_ be a power of two.
     *
     * It must be sufficiently large to store the callee-saved general purpose registers
     */
    static constexpr const size_t kMainStackSize{64};

    /**
     * Requested alignment for stack allocations.
     *
     * This is set to only 16 byte alignment as that's the most stringent of any x86 platform.
     */
    static constexpr const size_t kStackAlignment{16};

    /**
     * Platform default size to use for the stack, in bytes, if no size is requested by the caller. We
     * default to 256K.
     */
    static constexpr const size_t kDefaultStackSize{0x40000};

    static thread_local Cothread *gCurrentHandle;
    static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;
};
}

#endif
