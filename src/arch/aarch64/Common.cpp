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

thread_local Cothread *Aarch64::gCurrentHandle{nullptr};
thread_local std::array<uintptr_t, Aarch64::kMainStackSize> Aarch64::gMainStack;



Cothread *Cothread::Current() {
    if(!Aarch64::gCurrentHandle) Aarch64::AllocMainCothread();
    return Aarch64::gCurrentHandle;
}

Cothread::Cothread(const Entry &entry, const size_t stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(Aarch64::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : Aarch64::kDefaultStackSize;
    allocSize += Aarch64::kContextSaveAreaSize;

    buf = Aarch64::AllocStack(allocSize);

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->flags = Cothread::Flags::OwnsStack;

    Aarch64::Prepare(this, entry);
}

Cothread::Cothread(const Entry &entry, std::span<uintptr_t> _stack) : stack(_stack) {
    Aarch64::ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    Aarch64::Prepare(this, entry);
}

/**
 * Deallocates a cothread. This releases the underlying stack if we allocated it.
 */
Cothread::~Cothread() {
    if(static_cast<uintptr_t>(this->flags) & static_cast<uintptr_t>(Flags::OwnsStack)) {
        Aarch64::DeallocStack(this->stack.data());
    }
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
 */
void Cothread::switchTo() {
    auto from = Aarch64::gCurrentHandle;
    Aarch64::gCurrentHandle = this;
    Aarch64::Switch(from, this);
}

/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
void Aarch64::AllocMainCothread() {
    auto main = new Cothread(gMainStack, gMainStack.data() + Aarch64::kMainStackSize);
    gCurrentHandle = main;
}

/**
 * The currently running cothread returned from its main function. This is very naughty behavior.
 */
void Aarch64::CothreadReturned() {
    gReturnHandler(gCurrentHandle);
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

    // if the return handler returns, we will crash. so abort to make debugging easier
    std::terminate();
}

