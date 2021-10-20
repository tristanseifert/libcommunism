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

thread_local Cothread *Amd64::gCurrentHandle{nullptr};
thread_local std::array<uintptr_t, Amd64::kMainStackSize> Amd64::gMainStack;



Cothread *Cothread::Current() {
    if(!Amd64::gCurrentHandle) Amd64::AllocMainCothread();
    return Amd64::gCurrentHandle;
}

/**
 * Allocates a cothread including a context region of the specified size.
 */
Cothread::Cothread(const Entry &entry, const size_t stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(Amd64::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : Amd64::kDefaultStackSize;

    buf = Amd64::AllocStack(allocSize);

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->flags = Cothread::Flags::OwnsStack;

    Amd64::Prepare(this, entry);
}

/**
 * Allocates a cothread with an existing region of memory to back its stack.
 */
Cothread::Cothread(const Entry &entry, std::span<uintptr_t> _stack) : stack(_stack) {
    Amd64::ValidateStackSize(_stack.size() * sizeof(uintptr_t));
    Amd64::Prepare(this, entry);
}

/**
 * Deallocates a cothread. This releases the underlying stack if we allocated it.
 */
Cothread::~Cothread() {
    if(static_cast<uintptr_t>(this->flags) & static_cast<uintptr_t>(Flags::OwnsStack)) {
        Amd64::DeallocStack(this->stack.data());
    }
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
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
 * The currently running cothread returned from its main function. This is very naughty behavior.
 */
void Amd64::CothreadReturned() {
    gReturnHandler(gCurrentHandle);
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

