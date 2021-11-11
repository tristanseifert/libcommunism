#ifndef ARCH_SETJMP_SETJMP_H
#define ARCH_SETJMP_SETJMP_H

#include "CothreadPrivate.h"
#include "CothreadImpl.h"

#include <array>
#include <cstddef>
#include <mutex>

#include <csetjmp>

namespace libcommunism::internal {
/**
 * @brief Context switching utilizing the C library `setjmp()` and `longjmp()` methods
 *
 * The means by which this is works is based on ideas by Ralf S. Engelschall, from the 2000 paper
 * titled [Portable Multithreading.](http://www.xmailserver.org/rse-pmt.pdf) Thread stacks are set
 * up in a portable way by making use of signal handlers, so this should be supported on basically
 * all targets that have a functional C library and are UNIX-y.
 *
 * @remark Since signals are a per-process resource, allocation of cothreads effectively becomes
 *         serialized to ensure safety.
 */
class SetJmp final: public CothreadImpl {
    friend CothreadImpl *libcommunism::AllocKernelThreadWrapper();

    public:
        SetJmp(const Entry &entry, const size_t stackSize = 0);
        SetJmp(const Entry &entry, std::span<uintptr_t> stack);
        SetJmp(std::span<uintptr_t> stack) : CothreadImpl(stack) {}
        ~SetJmp();

        void switchTo(CothreadImpl *from) override;

    private:
        /**
         * @brief Context structure passed to the entry point of a setjmp based cothread
         */
        struct EntryContext {
            /// Implementation for this cothread
            SetJmp *impl{nullptr};

            /// Entry point of the cothread
            Cothread::Entry entry;
            /// Initializes a context struct with the given entry point.
            EntryContext(SetJmp *_impl, const Cothread::Entry &_entry) : impl(_impl),
                entry(_entry) {}
        };

        /**
         * Returns a pointer to the `sigjmp_buf` structure for a given cooperative thread.
         *
         * It's stored at the top of its stack buffer. The actual stack available to the program will
         * be reduced accordingly, but it is still possible for the program to overflow into this
         * structure and wreak havoc.
         *
         * @param thread Thread whose setjmp buffer to retrieve
         *
         * @return Thread's setjmp, in the stack allocation.
         */
        static auto JmpBufFor(CothreadImpl *thread) {
            return reinterpret_cast<sigjmp_buf *>(static_cast<SetJmp *>(thread)->stack.data());
        }

        static void AllocMainCothread();
        static void InvokeCothreadDidReturnHandler(Cothread *from);
        static void SignalHandlerSetupThunk(int);

        void Prepare(SetJmp *thread, const Cothread::Entry &entry);

    public:
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
         * system already, and this "stack" only holds the `sigjmp_buf`.
         */
        static thread_local std::array<uintptr_t, kMainStackSize> gMainStack;

        /**
         * Global variable indicating the context of the current cothread whose state buffer is to be
         * initialized. This is consulted in the signal handler to find the thread's actual entry
         * point.
         */
        static EntryContext *gCurrentlyPreparing;

        /**
         * Because the signals are shared between all threads in a process, including the associated
         * signal stacks, it's possible that multiple threads attempting to be prepared simultaneously
         * would cause issues.
         *
         * Therefore, this lock is taken for the duration of signal based operations (that is, the
         * entire time between saving the current signal handler, installing our custom ones, raising
         * the signal, and then restoring the old h andlers) needed to initialize the context buffer.
         */
        static std::mutex gSignalLock;

    private:
        /// When set, the stack was allocated by us and must be freed on release
        bool ownsStack{false};
};
}

#endif
