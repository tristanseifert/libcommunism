/**
 * AMD64 implementation of cothreads for all ABIs
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

thread_local std::array<uintptr_t, Amd64::kMainStackSize> Amd64::gMainStack;

/**
 * Allocates an amd64 thread, allocating its stack.
 *
 *
 * @param entry Method to execute on entry to this cothread
 * @param stackSize Size of the stack to be allocated, in bytes. it should be a multiple of the
 *        machine word size, or specify zero to use the platform default.
 *
 * @throw std::runtime_error If the memory for the cothread could not be allocated.
 * @throw std::runtime_error If the provided stack size is invalid
 */
Amd64::Amd64(const Entry &entry, const size_t stackSize) : CothreadImpl(entry, stackSize) {
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
 * Allocates and amd64 cothread with an already provided stack.
 *
 * @param entry Method to execute on entry to this cothread
 * @param stack Buffer to use as the stack of the cothread
 *
 * @throw std::runtime_error If the provided stack is invalid
 */
Amd64::Amd64(const Entry &entry, std::span<uintptr_t> _stack) : CothreadImpl(entry, _stack) {
    ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    Prepare(this, entry);
}

/**
 * Release the stack if we allocated it.
 */
Amd64::~Amd64() {
    if(this->ownsStack) {
        DeallocStack(this->stack.data());
    }
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
 */
void Amd64::switchTo(CothreadImpl *from) {
    Switch(static_cast<Amd64 *>(from), this);
}

/**
 * The currently running cothread returned from its main function. This is very naughty behavior.
 */
void Amd64::CothreadReturned() {
    gReturnHandler(Cothread::Current());
}

/**
 * Performs the call described inside a call info structure.
 *
 * @param info Pointer to the call info structure; it's deleted once the call returns.
 */
void Amd64::DereferenceCallInfo(CallInfo *info) {
    info->entry();
    delete info;
}



/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
CothreadImpl *libcommunism::AllocKernelThreadWrapper() {
    return new Amd64(Amd64::gMainStack);
}
