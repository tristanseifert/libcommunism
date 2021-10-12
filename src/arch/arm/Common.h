#ifndef ARCH_ARM_COMMON_H
#define ARCH_ARM_COMMON_H

#include "CothreadPrivate.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on 32 bit ARM machines.
 *
 * @remark The context of threads is stored at the top of the allocated stack. Therefore, roughly
 *         0x100 bytes fewer than provided are available as actual program stack.
 */
struct Arm {
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
     * @remark The implementation of this method is written in assembly as it depends on the
     *         actual calling convention used.
     *
     * @param from Cothread context that will receive the current context
     * @param to Cothread context buffer whose context is to be restored
     */
    static void Switch(void *from, void *to);

    static void AllocMainCothread();
    static void ValidateStackSize(const size_t size);
    static void *AllocStack(const size_t bytes);
    static void DeallocStack(void* stack);
    static void CothreadReturned();
    static void DereferenceCallInfo(CallInfo *info);
    static void Prepare(Cothread *thread, const Cothread::Entry &entry);


    /**
     * Size of the reserved region, at the top of the stack, which is reserved for saving the
     * context of a thread. This is in bytes.
     */
    static constexpr const size_t kContextSaveAreaSize{0x80};
    /**
     * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
     * be large enough to fit the register frame. This _must_ be a power of two.
     */
    static constexpr const size_t kMainStackSize{(kContextSaveAreaSize * 2) / sizeof(uintptr_t)};
    /// Requested stack alignment, in bytes
    static constexpr const size_t kStackAlignment{16};
    /// Default stack size, in bytes, if none is specified
    static constexpr const size_t kDefaultStackSize{0x40000};

    static thread_local Cothread *gCurrentHandle;
    static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;
};
}

#endif
