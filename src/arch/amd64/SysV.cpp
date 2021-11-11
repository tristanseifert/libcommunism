/**
 * AMD64 implementation of cothreads for SysV ABI
 */
#include "Common.h"
#include "CothreadPrivate.h"

#include "SysV.S"

#include <cstddef>
#include <stdexcept>

using namespace libcommunism;
using namespace libcommunism::internal;

/**
 * For System V ABI, the only saved registers are %rbp, %rbx, and %r12-%r15.
 */
const size_t Amd64::kNumSavedRegisters{6};

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
 * Allocates memory for a stack that's the given number of bytes in size.
 *
 * @remark If required (for page alignment, for example) the size may be rounded up.
 *
 * @param bytes Size of the stack memory, in bytes.
 *
 * @throw std::runtime_error If memory allocation failed
 * @return Pointer to the _top_ of allocated stack
 */
void* Amd64::AllocStack(const size_t bytes) {
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
void Amd64::DeallocStack(void* stack) {
    free(stack);
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
void Amd64::Prepare(Amd64 *wrap, const Entry &entry) {
    static_assert(offsetof(Amd64, stackTop) == COTHREAD_OFF_CONTEXT_TOP, "cothread stack top is invalid");

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

    // method to execute if main method returns
    *--stack = reinterpret_cast<uintptr_t>(&Amd64::EntryReturnedStub);

    // and then jump to the stub that calls the entry point
    *--stack = reinterpret_cast<uintptr_t>(info);
    *--stack = reinterpret_cast<uintptr_t>(&Amd64::DereferenceCallInfo);
    *--stack = reinterpret_cast<uintptr_t>(&Amd64::JumpToEntry);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    // clear the region that registers are written into (so they're all zeroed)
    for(size_t i = 0; i < kNumSavedRegisters; i++) {
        *--stack = 0;
    }

    // restore the stack pointer to the correct point
    wrap->stackTop = stack;
}

