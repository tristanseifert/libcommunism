/**
 * AMD64 implementation of cothreads
 */
#include "Amd64.h"
#include "CothreadPrivate.h"

#include "Amd64+SysV.S"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <sys/_types/_uintptr_t.h>
#include <thread>

using namespace libcommunism;
using namespace libcommunism::internal;

thread_local Cothread *Amd64::gCurrentHandle{nullptr};
thread_local std::array<uintptr_t, Amd64::kMainStackSize> Amd64::gMainStack;



/**
 * Returns the handle to the currently executing cothread.
 *
 * If no cothread has been lanched yet, the "fake" initial cothread handle is returned.
 */
Cothread *Cothread::Current() {
    if(!Amd64::gCurrentHandle) Amd64::AllocMainCothread();
    return Amd64::gCurrentHandle;
}

/**
 * Allocates a cothread including a context region of the specified size.
 */
Cothread::Cothread(void (*entry)(void *), void *ctx, const size_t contextSize) {
    void *buf{nullptr};
    int err{-1};

    // round down context size to ensure it's aligned
    auto allocSize = contextSize & ~(Amd64::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : Amd64::kDefaultStackSize;

    // allocate buffer
    err = posix_memalign(&buf, Amd64::kStackAlignment, allocSize);
    if(err) {
        throw std::runtime_error("posix_memalign() failed");
    }

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->flags = Cothread::Flags::OwnsStack;

    Amd64::Prepare(this, entry, ctx);
}

/**
 * Allocates a cothread with an existing region of memory to back its stack.
 */
Cothread::Cothread(std::span<uintptr_t> _stack, void (*entry)(void *), void *ctx) : stack(_stack) {
    Amd64::ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    Amd64::Prepare(this, entry, ctx);
}

/**
 * Deallocates a cothread. This releases the underlying stack if we allocated it.
 */
Cothread::~Cothread() {
    if(static_cast<uintptr_t>(this->flags) & static_cast<uintptr_t>(Flags::OwnsStack)) {
        free(this->stack.data());
    }
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored into the context structure of the currently active thread.
 */
void Cothread::switchTo() {
    auto from = Amd64::gCurrentHandle;
    Amd64::gCurrentHandle = this;
    Amd64::Switch(from, this);
}



/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
void Amd64::AllocMainCothread() {
    auto main = new Cothread(gMainStack, gMainStack.data() + Amd64::kMainStackSize);
    gCurrentHandle = main;
}

/**
 * Ensures the provided stack size is valid.
 *
 * @param size Size of stack, in bytes.
 *
 * @throw std::runtime_error Stack size is invalid (misaligned, too small, etc.)
 */
void Amd64::ValidateStackSize(const size_t size) {
    if(!size) throw std::runtime_error("Size may not be nil");
    if(size % kStackAlignment) throw std::runtime_error("Stack is misaligned");
}

/**
 * Builds the initial stack frame and updates the wrapper fields so that it is correctly restored.
 *
 * The stack frame will return first to the main function; and if that returns, it will cause the
 * program to terminate. The stack frame is set up in such a way that on entry to the main
 * function, the stack is 8 byte aligned; this is what functions expect since they'd normally be
 * invoked by a `call` instruction which leaves an aligned stuck 8 byte aligned because of the
 * return address.
 *
 * When returning from the main function, the stack will be misaligned; the assembly stub there
 * will fix that.
 *
 * @param wrap Wrapper structure defining the cothread
 * @param main Entry point for the cothread
 */
void Amd64::Prepare(Cothread *wrap, void (*entry)(void *), void *ctx) {
    static_assert(offsetof(Cothread, stackTop) == COTHREAD_OFF_CONTEXT_TOP, "cothread stack top is invalid");

    // ensure current handle is valid
    if(!gCurrentHandle) Amd64::AllocMainCothread();

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    // prepare some space for a stack frame (zeroed registers; four function call params)
    auto &stackBuf = wrap->stack;
    auto stackFrame = reinterpret_cast<std::byte *>(stackBuf.data());
    stackFrame += ((stackBuf.size()*sizeof(typeof(*stackBuf.data()))) & ~(0x10-1))
        - (sizeof(uintptr_t) * (4 + kNumSavedRegisters));
    auto stack = reinterpret_cast<uintptr_t *>(stackFrame);

    // method to execute if main method returns
    *--stack = reinterpret_cast<uintptr_t>(&Amd64::EntryReturnedStub);

    // and then jump to the stub that calls the entry point
    *--stack = reinterpret_cast<uintptr_t>(ctx);
    *--stack = reinterpret_cast<uintptr_t>(entry);
    *--stack = reinterpret_cast<uintptr_t>(&Amd64::JumpToEntry);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    // clear the region that registers are written into (so they're all zeroed)
    for(size_t i = 0; i < kNumSavedRegisters; i++) {
        *--stack = 0;
    }

    // restore the stack pointer to the correct point
    wrap->stackTop = stack;
}

/**
 * The currently running cothread returned from its main function. This is very naughty behavior.
 */
void Amd64::CothreadReturned() {
    // invokle handler
    gReturnHandler(gCurrentHandle);
}
