#ifndef LIBCOMMUNISM_COTHREAD_H
#define LIBCOMMUNISM_COTHREAD_H

#include <cstddef>
#include <functional>
#include <span>
#include <string>

/**
 * @brief Main namespace for the libcommunism library.
 */
namespace libcommunism {
namespace internal{
struct Amd64;
}

/**
 * Cooperative threads are threads that perform context switching in userspace, rather than relying
 * on the kernel to do this. This has distinct performance advantages as the context switch is
 * avoided, which costs a significant amount of clock cycles.
 *
 * @brief Instance of a single cooperative thread
 */
class Cothread {
    friend struct internal::Amd64;

    private:
        /**
         * Flags that change the behavior of a cothread. These are used internally by the
         * architecture specific code.
         */
        enum class Flags: uintptr_t {
            /// Context memory was owned and allocated by the cothread and should be deallocated
            OwnsStack                   = (1 << 0),
        };

    public:
        /**
         * Returns the cothread currently executing on the calling "physical" thread.
         *
         * @remark If the calling thread is not executing a cothread, a special handle is returned
         *         that points to a static, per thread buffer. Its sole purpose is to store the
         *         register state of the caller that invoked the first cothread on this physical
         *         thread. Currently, that buffer is not directly accessible, aside from storing
         *         this handle before any cothreads are executed.
         *
         * @return Handle to the current cothread
         */
        [[nodiscard]] static Cothread *Current();

        /**
         * Sets the method that's invoked when a cothread returns from its entry point. The
         * deafult action is to terminate the program when this occurs.
         *
         * @remark It's suggested that cothreads returning from entry is treated as a fatal
         *         programming error. The state of the cothread's stack is not well defined after
         *         it returns (w.r.t. alignment;) instead design your code so that it switches to
         *         another cothread when it's done, at which point it can be deallocated.
         */
        static void SetReturnHandler(const std::function<void(Cothread *)> &handler);

        /**
         * Allocates a new cothread without explicitly allocating its state buffer.
         *
         * @remark The backing storage for the cothread is allocated internally by the platform
         *         implementation and not directly accessible to clients of this interface.
         *         Depending on the platform, it may be allocated in a special way to match how
         *         normal stacks are allocated on the platform, rather than by using a mechanism
         *         like `malloc()`. That is to say, you should not have any expectations on how
         *         or where the stack is allocated.
         *
         * @note If the entry point returns, the the cothread return handler is invoked; its
         *       default action is to terminate the program, as the state of the stack after return
         *       from the main thread is undefined and may result in undefined behavior.
         *
         * @param entry Method to execute on entry to this cothread
         * @param ctx Optional context parameter passed to the cothread entry
         * @param stackSize Size of the stack to be allocated, in bytes. it should be a multiple of
         *        the machine word size, or specify zero to use the platform default.
         *
         * @throw std::runtime_error If the memory for the cothread could not be allocated.
         * @throw std::runtime_error If the provided stack size is invalid
         *
         * @return An initialized cothread object
         */
        Cothread(void (*entry)(void *), void *ctx = nullptr, const size_t stackSize = 0);

        /**
         * Allocates a new cothread, using an existing buffer to store its context.
         *
         * @remark You are responsible for managing the buffer memory, i.e. freeing it after the
         *         cothread has been deallocated. See notes for important information.
         *
         * @note If the entry point returns, the the cothread return handler is invoked; its
         *       default action is to terminate the program, as the state of the stack after return
         *       from the main thread is undefined and may result in undefined behavior.
         *
         * @note The provided buffer must remain valid for the duration of the cothread's life. If
         *       it is deallocated or otherwise reused during the lifetime of the cothread,
         *       undefined behavior results. You must not attempt to manually modify the contents
         *       of the buffer. Additionally, it must meet alignment requirements for stacks on
         *       the underlying platform. (A 64 byte alignment should be safe for most platforms.)
         *
         * @param stack Buffer to use as the stack of the cothread
         * @param entry Method to execute on entry to this cothread
         * @param ctx Optional context passed to the cothread entry
         *
         * @throw std::runtime_error If the provided stack is invalid
         *
         * @return An initialized cothread object
         */
        Cothread(std::span<uintptr_t> stack, void (*entry)(void *), void *ctx = nullptr);

        /**
         * Destroys a previously allocated cothread, and deallocates any underlying buffer memory
         * used by it.
         *
         * @note Destroying the currently executing cothread results in undefined behavior, as it
         *       will cause its stack to be deallocated.
         */
        ~Cothread();

        /**
         * Performs a context switch to this cothread.
         *
         * This method saves the context of the current cothread (registers, including the stack
         * pointer) in its context buffer; then restores registers, stack and returns control to
         * the destination cothread.
         *
         * @note Do not attempt to switch to a currently executing cothread, whether it is on the
         *       same physical thread or not. This will corrupt both cothreads' stacks and result
         *       in undefined behavior.
         */
        void switchTo();

        /**
         * Gets the debug label (name) associated with this cothread.
         *
         * @return A string containing the thread's debug label
         */
        constexpr auto &getLabel() const {
            return this->label;
        }

        /**
         * Changes the debug label (name) associated with this cothread.
         *
         * @param newLabel New string value to set as the cothread's label
         */
        void setLabel(const std::string &newLabel) {
            this->label = newLabel;
        }

    private:
        Cothread(std::span<uintptr_t> stack, void *stackTop);

    /*
     * These fields are accessed directly by the platform specific code. It is absolutely
     * essential that these are not reordered or otherwise changed, as this changes their offsets
     * in the class.
     */
    private:
        /// Last saved stack pointer to restore when switching to this thread
        void *stackTop;
        /// Range encompassing the entire stack allocated to this cothread
        std::span<uintptr_t> stack;

        /// Flags that define the behavior of the cothread
        Flags flags{};
        /// Optional label attached to the cothread (for debugging purposes only)
        std::string label{""};
};
}

#endif
