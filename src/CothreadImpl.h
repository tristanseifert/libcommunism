#ifndef LIBCOMMUNISM_COTHREADIMPL_H
#define LIBCOMMUNISM_COTHREADIMPL_H

#include <libcommunism/Cothread.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace libcommunism {
/**
 * @brief Abstract interface for a platform implementation of cothreads
 *
 * Each platform implementation derives from this base class, meaning that the cothread API that we
 * expose to callers is just a thin shim over an instance of this class. The concrete instances of
 * this class will end up holding the actual state of the cothread.
 */
struct CothreadImpl {
    using Entry = Cothread::Entry;

    /**
     * Initialize a cothread that begins execution at the given entry point, allocating a stack
     * for it.
     *
     * @param entry Method to execute on entry to this cothread
     * @param stackSize Size of the stack to be allocated, in bytes. it should be a multiple of
     *        the machine word size, or specify zero to use the platform default.
     */
    CothreadImpl(const Cothread::Entry &entry, const size_t stackSize = 0) {
        (void) entry, (void) stackSize;
    }

    /**
     * Initialize the implementation to start execution at the given point with an already
     * allocated stack.
     *
     * @remark This method should not take ownership of the stack buffer; it's expected that the
     *         caller handles this, and ensures it's valid until the cothread is deallocated.
     *
     * @param entry Method to execute on entry to this cothread
     * @param stack Buffer to use as the stack of the cothread
     */
    CothreadImpl(const Cothread::Entry &entry, std::span<uintptr_t> stack) : stack(stack) {
        (void) entry;
    }

    /**
     * Create a "skeleton" cothread that has only an associated stack.
     *
     * @param stack Buffer to use as the stack of the cothread
     */
    CothreadImpl(std::span<uintptr_t> stack) : stack(stack) {}

    /**
     * Clean up all resources associated with the cothread, such as its stack.
     */
    virtual ~CothreadImpl() = default;

    /**
     * Perform a context switch to this cothread.
     *
     * The currently executing cothread's state is saved to its buffer, then this cothread's state
     * is restored.
     */
    virtual void switchTo(CothreadImpl *from) = 0;

    /**
     * Get the stack size of this cothread
     *
     * @return Stack size in bytes
     */
    virtual size_t getStackSize() const {
        return this->stack.size() * sizeof(uintptr_t);
    }

    /**
     * Get the top (regardless of the direction the stack grows, that is, the first byte allocated
     * to the stack) of the stack.
     *
     * @return Pointer to top of stack
     */
    virtual void *getStack() const {
        return this->stack.data();
    }

    protected:
        /// Stack used by this cothread, if any.
        std::span<uintptr_t> stack;
};

/**
 * Allocate the cothread implementation for the currently executing kernel thread.
 *
 * This is invoked when no cothread is running on the kernel thread, and is only used to hold the
 * state of the kernel thread on entry to the first cothread, so it can "resume" the kernel thread.
 *
 * @remark Only one definition of this method is allowed; it's typically provided by the platform
 *         code selected via build configuration.
 */
CothreadImpl *AllocKernelThreadWrapper();
}

#endif
