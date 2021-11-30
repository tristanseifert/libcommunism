#ifndef ARCH_AMD64_COMMON_H
#define ARCH_AMD64_COMMON_H

#include "CothreadPrivate.h"
#include "CothreadImpl.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace libcommunism::internal {
/**
 * @brief Architecture specific methods for working with cothreads on amd64 based systems.
 */
class Amd64 final: public CothreadImpl {
    friend CothreadImpl *libcommunism::AllocKernelThreadWrapper();

    private:
        /**
         * @brief Information required to make a function call for a cothread's entry point.
         */
        struct CallInfo {
            /// Entry point of the cothread
            Cothread::Entry entry;
        };

    public:
        Amd64(const Entry &entry, const size_t stackSize = 0);
        Amd64(const Entry &entry, std::span<uintptr_t> stack);
        Amd64(std::span<uintptr_t> stack) : CothreadImpl(stack) {}
        ~Amd64();

        void switchTo(CothreadImpl *from) override;

    private:
        /**
         * Ensures the provided stack size is valid.
         *
         * @param size Size of stack, in bytes.
         *
         * @throw std::runtime_error Stack size is invalid (misaligned, too small, etc.)
         */
        static void ValidateStackSize(const size_t size);

        /**
         * Allocates memory for a stack that's the given number of bytes in size.
         *
         * @remark If required (for page alignment, for example) the size may be rounded up.
         *
         * @param bytes Size of the stack memory, in bytes.
         * 
         * @return Pointer to the _top_ of allocated stack
         */
        static void* AllocStack(const size_t bytes);

        /**
         * Releases previously allocated stack memory.
         * 
         * @param stack Pointer to the top of previously allocated stack.
         * 
         * @throw std::runtime_error If deallocating stack fails (invalid pointer)
         */
        static void DeallocStack(void* stack);

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
        static void Prepare(Amd64 *thread, const Entry &entry);

        /**
         * Performs a context switch.
         *
         * @remark The implementation of this method is written in assembly and varies slightly depending
         *         on the calling convention of the platform (System V vs. Windows.)
         *
         * @param from Cothread buffer that will receive the current context
         * @param to Cothread buffer whose context is to be restored
         */
        static void Switch(Amd64 *from, Amd64 *to);

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

    public:
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

    private:
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

    private:
        /// When set, the stack was allocated by us and must be freed on release
        bool ownsStack{false};

        /// Pointer to the top of the stack, where the thread's state is stored
        void *stackTop{nullptr};
};
}

#endif
