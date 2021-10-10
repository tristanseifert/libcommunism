/**
 * Aarch64 context switching code using the standard ARM procedure calling standard, [defined here.]
 * (https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)
 */
#include "Common.h"
#include "CothreadPrivate.h"

#include "AAPCS.S"

#include <cstddef>
#include <stdexcept>

using namespace libcommunism;
using namespace libcommunism::internal;

extern "C" void Aarch64AapcsEntryStub();

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
 * Allocates memory for a stack that's the given number of bytes in size. We simply take it from
 * the system heap.
 *
 * @remark If required (for page alignment, for example) the size may be rounded up.
 *
 * @param bytes Size of the stack memory, in bytes.
 *
 * @throw std::runtime_error If memory allocation failed
 * @return Pointer to the _top_ of allocated stack
 */
void* Aarch64::AllocStack(const size_t bytes) {
    void* buf{ nullptr };
    int err{ -1 };

    err = posix_memalign(&buf, kStackAlignment, bytes);
    if (err) {
        throw std::runtime_error("posix_memalign() failed");
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
void Aarch64::DeallocStack(void* stack) {
    free(stack);
}

/**
 * Sets up the state area of the given cothread with a register frame that will return it to the
 * entry handler method, which in turn will invoke the entry point. It also invokes the return
 * handler if the entry pointer returns.
 *
 * For aarch64, the register state is always written at the top of the context buffer.
 *
 * @param thread Thread whose stack to prepare
 * @param main Entry point for the cothread
 */
void Aarch64::Prepare(Cothread *thread, const Cothread::Entry &entry) {
    static_assert(offsetof(Cothread, stackTop) == COTHREAD_OFF_CONTEXT_TOP, "cothread stack top is invalid");

    // ensure current handle is valid
    if(!gCurrentHandle) Aarch64::AllocMainCothread();

    // build the context structure we pass to our "fake" entry point
    auto info = new CallInfo{entry};
    if(!info) throw std::runtime_error("Failed to allocate call info");

    // build up the stack frame
    auto &stack = thread->stack;
    const auto stackBottom = reinterpret_cast<uintptr_t>(stack.data()) +
        (stack.size() * sizeof(uintptr_t));
    auto context = reinterpret_cast<uintptr_t *>(stack.data());

    context[0]  = stackBottom; // sp
    context[1]  = reinterpret_cast<uintptr_t>(Aarch64AapcsEntryStub); // x30/lr
    context[2]  = reinterpret_cast<uintptr_t>(info); // x19
    context[12] = stackBottom; //x29/fp

    // we use the `stackTop` ptr to point to the context area
    thread->stackTop = context;
}
