/**
 * x86 implementation of cothreads for all ABIs
 */
#include "Common.h"
#include "CothreadPrivate.h"

#include "Fastcall.S"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace libcommunism;
using namespace libcommunism::internal;

/// Pointer to the currently executing cothread (or `nullptr` til first request for this)
thread_local Cothread *x86::gCurrentHandle{nullptr};
/**
 * Buffer to store the state of the kernel thread when switching to the first cothread. This only
 * has to be large enough to hold the register context frame, as the stack has been allocated by
 * the system already.
 */
thread_local std::array<uintptr_t, x86::kMainStackSize> x86::gMainStack;



Cothread *Cothread::Current() {
    if(!x86::gCurrentHandle) x86::AllocMainCothread();
    return x86::gCurrentHandle;
}

Cothread::Cothread(const Entry &entry, const size_t stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(x86::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : x86::kDefaultStackSize;

    buf = x86::AllocStack(allocSize);

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->flags = Cothread::Flags::OwnsStack;

    x86::Prepare(this, entry);
}

Cothread::Cothread(const Entry &entry, std::span<uintptr_t> _stack) : stack(_stack) {
    x86::ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    x86::Prepare(this, entry);
}

Cothread::~Cothread() {
    if(static_cast<uintptr_t>(this->flags) & static_cast<uintptr_t>(Flags::OwnsStack)) {
        x86::DeallocStack(this->stack.data());
    }
}

void Cothread::switchTo() {
    auto from = x86::gCurrentHandle;
    x86::gCurrentHandle = this;
    x86::Switch(from, this);
}

/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
void x86::AllocMainCothread() {
    auto main = new Cothread(gMainStack, gMainStack.data() + x86::kMainStackSize);
    gCurrentHandle = main;
}

/**
 * Ensures the provided stack size is valid.
 *
 * @param size Size of stack, in bytes.
 *
 * @throw std::runtime_error Stack size is invalid (misaligned, too small, etc.)
 */
void x86::ValidateStackSize(const size_t size) {
    if(!size) throw std::runtime_error("Size may not be nil");
    if(size % kStackAlignment) throw std::runtime_error("Stack is misaligned");
}

/**
 * Allocates memory for a stack that's the given number of bytes in size.
 * 
 * @remark If required (for page alignment, for example) the size may be rounded up.
 *
 * @param bytes Size of the stack memory, in bytes.
 *
 * @throw std::runtime_error If memory allocation failed
 * @return Pointer to the _top_ of allocated stack
 */
void* x86::AllocStack(const size_t bytes) {
    void *buf{nullptr};
#ifdef _WIN32
    buf = _aligned_malloc(bytes, x86::kStackAlignment);
#else
    int err{0};
    err = posix_memalign(&buf, x86::kStackAlignment, bytes);
    if(err) {
        throw std::runtime_error("posix_memalign() failed");
    }
#endif
    if(!buf) {
        throw std::runtime_error("failed to allocate stack");
    }
    return buf;
}

/**
 * Releases previously allocated stack memory.
 *
 * @param stack Pointer to the top of previously allocated stack.
 *
 * @throw std::runtime_error If deallocating stack fails (invalid pointer)
 */
void x86::DeallocStack(void* stack) {
#ifdef _WIN32
    _aligned_free(stack);
#else
    free(stack);
#endif
}

/**
 * The currently running cothread returned from its main function. This is a separate function so
 * that it shows up clearly on stack traces if this causes a crash.
 */
void x86::CothreadReturned() {
    gReturnHandler(gCurrentHandle);
}

/**
 * Performs the call described inside a call info structure.
 *
 * @param info Pointer to the call info structure; it's deleted once the call returns.
 */
void x86::DereferenceCallInfo(CallInfo *info) {
    info->entry();
    delete info;

    // invoke the return handler; this shouldn't return
    CothreadReturned();
    std::terminate();
}

/**
 * Builds the initial stack frame for a cothread, such that it will return to the entry stub,
 * which then in turn jumps to the context dereferencing handler.
 *
 * @param wrap Wrapper structure defining the cothread
 * @param main Entry point for the cothread
 */
void x86::Prepare(Cothread *wrap, const Cothread::Entry &entry) {
    static_assert(offsetof(Cothread, stackTop) == COTHREAD_OFF_CONTEXT_TOP, "cothread stack top is invalid");

    // ensure current handle is valid
    if(!gCurrentHandle) x86::AllocMainCothread();

    // build the context structure we pass to our "fake" entry point
    auto info = new CallInfo{entry};
    if(!info) throw std::runtime_error("Failed to allocate call info");

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    // prepare some space for a stack frame (zeroed registers; four function call params)
    auto &stackBuf = wrap->stack;
    auto stackFrame = reinterpret_cast<std::byte *>(stackBuf.data());
    stackFrame += ((stackBuf.size()*sizeof(typeof(*stackBuf.data()))) & ~(0x10-1))
        - (sizeof(uintptr_t) * (4 + kNumSavedRegisters));
    auto stack = reinterpret_cast<uintptr_t *>(stackFrame);

    // if main returns (it shouldn't, we call std:;terminate) just crash
    *--stack = 0;

    // and then jump to the stub that calls the entry point
    *--stack = reinterpret_cast<uintptr_t>(info);
    *--stack = reinterpret_cast<uintptr_t>(&x86::DereferenceCallInfo);
    *--stack = reinterpret_cast<uintptr_t>(&x86::JumpToEntry);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    // clear the region that registers are written into (so they're all zeroed)
    for(size_t i = 0; i < kNumSavedRegisters; i++) {
        *--stack = 0;
    }

    // restore the stack pointer to the correct point
    wrap->stackTop = stack;
}

