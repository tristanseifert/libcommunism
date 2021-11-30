#ifndef ARCH_AARCH64_COMMON_H
#define ARCH_AARCH64_COMMON_H

#include "CothreadPrivate.h"
#include "CothreadImpl.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on 64 bit ARM machines.
 *
 * @remark The context of threads is stored at the top of the allocated stack. Therefore, roughly
 *         0x100 bytes fewer than provided are available as actual program stack.
 */
class Aarch64 final: public CothreadImpl {
    friend CothreadImpl *libcommunism::AllocKernelThreadWrapper();

    /**
     * @brief Information required to make a function call for a cothread's entry point.
     */
    struct CallInfo {
        /// Entry point of the cothread
        Cothread::Entry entry;
    };

    public:
        Aarch64(const Entry &entry, const size_t stackSize = 0);
        Aarch64(const Entry &entry, std::span<uintptr_t> stack);
        Aarch64(std::span<uintptr_t> stack) : CothreadImpl(stack) {}
        ~Aarch64();

        void switchTo(CothreadImpl *from) override;

    private:
        static void AllocMainCothread();
        static void ValidateStackSize(const size_t size);
        static void *AllocStack(const size_t bytes);
        static void DeallocStack(void* stack);
        static void CothreadReturned();
        static void DereferenceCallInfo(CallInfo *info);

        /**
         * Given a wrapper structure and initial function to invoke, prepares the context of the
         * cothread such that it will return to the start of this method.
         *
         * @param thread Cothread whose stack frame is to be prepared
         * @param entry Function to return control to when switching to this cothread
         */
        static void Prepare(Aarch64 *thread, const Entry &entry);

        /**
         * Performs a context switch.
         *
         * @remark This is implemented in platform-specific assembly.
         *
         * @param from Cothread buffer that will receive the current context
         * @param to Cothread buffer whose context is to be restored
         */
        static void Switch(Aarch64 *from, Aarch64 *to);

    public:
        /**
         * Size of the reserved region, at the top of the stack, which is reserved for saving the
         * context of a thread. This is in bytes.
         */
        static constexpr const size_t kContextSaveAreaSize{0x100};

        /**
         * Size of the stack buffer for the "fake" initial cothread, in machine words. This only needs to
         * be large enough to fit the register frame. This _must_ be a power of two.
         */
        static constexpr const size_t kMainStackSize{(kContextSaveAreaSize * 2) / sizeof(uintptr_t)};

        /**
         * Requested alignment for stack allocations, in bytes.
         */
        static constexpr const size_t kStackAlignment{64};

        /**
         * Platform default size to use for the stack, in bytes, if no size is requested by the caller. We
         * default to 512K.
         */
        static constexpr const size_t kDefaultStackSize{0x80000};

    private:
        /**
         * Buffer to hold the state of the kernel thread that executed the first context switch to
         * a cothread. (In other words, this buffer represents the state of the thread immediately
         * before starting the first cothread.)
         *
         * It does not have to be particularly large, since the stack is actually allocated by the
         * system already, and this "stack" only holds the register state.
         */
        static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;

    private:
        /// When set, the stack was allocated by us and must be freed on release
        bool ownsStack{false};

        /// Pointer to the top of the stack, where the thread's state is stored
        void *stackTop{nullptr};
};
}

#endif
