/**
 * Common code for implementing context switching on aarch64
 */
#include "Common.h"
#include "CothreadPrivate.h"

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

thread_local std::array<uintptr_t, Aarch64::kMainStackSize> Aarch64::gMainStack;



/**
 * Allocate a cothread with a private stack.
 *
 * @param entry Method to execute on entry to this cothread
 * @param stackSize Size of the stack to be allocated, in bytes. it should be a multiple of the
 *        machine word size, or specify zero to use the platform default.
 *
 * @throw std::runtime_error If the memory for the cothread could not be allocated.
 * @throw std::runtime_error If the provided stack size is invalid
 */
Aarch64::Aarch64(const Entry &entry, const size_t stackSize) : CothreadImpl(entry, stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(Aarch64::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : Aarch64::kDefaultStackSize;
    allocSize += Aarch64::kContextSaveAreaSize;

    buf = Aarch64::AllocStack(allocSize);

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->ownsStack = true;

    Aarch64::Prepare(this, entry);
}

/**
 * Allocates a cothread with an already provided stack.
 *
 * @param entry Method to execute on entry to this cothread
 * @param stack Buffer to use as the stack of the cothread
 *
 * @throw std::runtime_error If the provided stack is invalid
 */
Aarch64::Aarch64(const Entry &entry, std::span<uintptr_t> _stack) : CothreadImpl(entry, _stack) {
    Aarch64::ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    Aarch64::Prepare(this, entry);
}

/**
 * Allocate a cothread placeholder for a kernel thread. This uses a preallocated "stack" to
 * store the kernel thread's context at the time we switched to the cothread.
 */
Aarch64::Aarch64(std::span<uintptr_t> stack) : CothreadImpl(stack), stackTop(stack.data()) {
}


/**
 * Release the stack if we allocated it.
 */
Aarch64::~Aarch64() {
    if(this->ownsStack) {
        DeallocStack(this->stack.data());
    }
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
 */
void Aarch64::switchTo(CothreadImpl *from) {
    Switch(static_cast<Aarch64 *>(from), this);
}

/**
 * Ensures the provided stack size is valid.
 *
 * @param size Size of stack, in bytes.
 *
 * @throw std::runtime_error Stack size is invalid (misaligned, too small, etc.)
 */
void Aarch64::ValidateStackSize(const size_t size) {
    if(!size) throw std::runtime_error("Size may not be nil");
    if(size % kStackAlignment) throw std::runtime_error("Stack is misaligned");
}

/**
 * The currently running cothread returned from its main function. This is a separate function such
 * that it will show up in stack traces.
 */
void Aarch64::CothreadReturned() {
    gReturnHandler(Cothread::Current());
}

/**
 * Performs the call described inside a call info structure, then invokes the return handler if it
 * returns.
 *
 * @param info Pointer to the call info structure; it's deleted once the call returns.
 */
void Aarch64::DereferenceCallInfo(CallInfo *info) {
    info->entry();
    delete info;

    CothreadReturned();
    gReturnHandler(Cothread::Current());

    // if the return handler returns, we will crash. so abort to make debugging easier
    std::terminate();
}



/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
CothreadImpl *libcommunism::AllocKernelThreadWrapper() {
    return new Aarch64(Aarch64::gMainStack);
}

