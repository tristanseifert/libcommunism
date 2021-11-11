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

/**
 * Buffer to store the state of the kernel thread when switching to the first cothread. This only
 * has to be large enough to hold the register context frame, as the stack has been allocated by
 * the system already.
 */
thread_local std::array<uintptr_t, x86::kMainStackSize> x86::gMainStack;



/**
 * Allocate an x86 cothread instance, allocating the stack as part of this.
 *
 * @param entry Method to execute on entry to this cothread
 * @param stackSize Size of the stack to be allocated, in bytes. it should be a multiple of the
 *        machine word size, or specify zero to use the platform default.
 *
 * @throw std::runtime_error If the memory for the cothread could not be allocated.
 * @throw std::runtime_error If the provided stack size is invalid
 */
x86::x86(const Entry &entry, const size_t stackSize) : CothreadImpl(entry, stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(kStackAlignment - 1);
    allocSize = allocSize ? allocSize : kDefaultStackSize;

    buf = AllocStack(allocSize);

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->ownsStack = true;

    Prepare(this, entry);
}

/**
 * Allocates an x86 cothread with an already provided stack.
 *
 * @param entry Method to execute on entry to this cothread
 * @param stack Buffer to use as the stack of the cothread
 *
 * @throw std::runtime_error If the provided stack is invalid
 */
x86::x86(const Entry &entry, std::span<uintptr_t> stack) : CothreadImpl(entry, stack) {
    x86::ValidateStackSize(stack.size() * sizeof(uintptr_t));
    Prepare(this, entry);
}

/**
 * Deallocate the stack.
 */
x86::~x86() {
    if(this->ownsStack) {
        x86::DeallocStack(this->stack.data());
    }
}

void x86::switchTo(CothreadImpl *from) {
    x86::Switch(static_cast<x86 *>(from), this);
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
    buf = _aligned_malloc(bytes, kStackAlignment);
#else
    int err{0};
    err = posix_memalign(&buf, kStackAlignment, bytes);
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
    gReturnHandler(Cothread::Current());
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
void x86::Prepare(x86 *wrap, const Entry &entry) {
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    static_assert(offsetof(x86, stackTop) == COTHREAD_OFF_CONTEXT_TOP, "cothread stack top is invalid");
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

    // build the context structure we pass to our entry point stub
    auto info = new CallInfo{entry};
    if(!info) throw std::runtime_error("Failed to allocate call info");

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    // prepare some space for a stack frame
    auto &stackBuf = wrap->stack;
    auto stackFrame = reinterpret_cast<std::byte *>(stackBuf.data());
    stackFrame += ((stackBuf.size()*sizeof(uintptr_t)) & ~(0x10-1))
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


/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
CothreadImpl *libcommunism::AllocKernelThreadWrapper() {
    return new x86(x86::gMainStack);
}
