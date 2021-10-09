/**
 * Context switching using the `ucontext` methods from the C library.
 *
 * This is very very slow.
 */
#include "UContext.h"
#include "CothreadPrivate.h"

#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <stdexcept>

// required to get the "deprecated" ucontext sources
#define _XOPEN_SOURCE
#include <ucontext.h>

using namespace libcommunism;
using namespace libcommunism::internal;

// Validate an ucontext fits in the buffer
static_assert(sizeof(ucontext_t) < (UContext::kMainStackSize * sizeof(uintptr_t)),
        "main stack size is too small for ucontext!");

thread_local Cothread *UContext::gCurrentHandle{nullptr};
thread_local std::array<uintptr_t, UContext::kMainStackSize> UContext::gMainStack;

std::unordered_map<int, UContext::Context *> UContext::gContextInfo;
std::mutex UContext::gContextInfoLock;
int UContext::gContextNextId{0};

/**
 * Returns the handle to the currently executing cothread.
 *
 * If no cothread has been lanched yet, the "fake" initial cothread handle is returned.
 */
Cothread *Cothread::Current() {
    if(!UContext::gCurrentHandle) UContext::AllocMainCothread();
    return UContext::gCurrentHandle;
}


/**
 * Allocates a cothread including a context region of the specified size.
 *
 * This ensures there's sufficient bonus space allocated to hold the ucontext.
 */
Cothread::Cothread(const Entry &entry, const size_t stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(UContext::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : UContext::kDefaultStackSize;

    // then add space for ucontext
    allocSize += sizeof(ucontext_t);
    if(allocSize % UContext::kStackAlignment) {
        allocSize += UContext::kStackAlignment - (allocSize % UContext::kStackAlignment);
    }

    // and allocate it
#ifdef _WIN32
    buf = _aligned_malloc(allocSize, UContext::kStackAlignment);
#else
    int err{0};
    err = posix_memalign(&buf, UContext::kStackAlignment, allocSize);
    if(err) {
        throw std::runtime_error("posix_memalign() failed");
    }
#endif
    if(!buf) {
        throw std::runtime_error("failed to allocate stack");
    }

    // create it as if we had provided the memory in the first place
    this->stack = {reinterpret_cast<uintptr_t *>(buf), allocSize / sizeof(uintptr_t)};
    this->flags = Cothread::Flags::OwnsStack;// | Cothread::Flags::PartialReserved;

    UContext::Prepare(this, entry);
}

/**
 * Allocates a cothread with an existing region of memory to back its stack.
 */
Cothread::Cothread(const Entry &entry, std::span<uintptr_t> _stack) : stack(_stack) {
    UContext::Prepare(this, entry);
}

/**
 * Deallocates a cothread. This releases the underlying stack if we allocated it.
 */
Cothread::~Cothread() {
    if(static_cast<uintptr_t>(this->flags) & static_cast<uintptr_t>(Flags::OwnsStack)) {
#ifdef _WIN32
        _aligned_free(this->stack.data());
#else
        free(this->stack.data());
#endif
    }
}



/**
 * Allocates the Cothread instance for the current kernel thread.
 */
void UContext::AllocMainCothread() {
    auto main = new Cothread(gMainStack, gMainStack.data() + UContext::kMainStackSize);
    gCurrentHandle = main;
}

// XXX: We need to disable deprecation warnings for getcontext() and friends on macOS, BSD
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/**
 * Prepares the `ucontext_t` buffer.
 */
void UContext::Prepare(Cothread *thread, const Cothread::Entry &entry) {
    // ensure current handle is valid
    if(!gCurrentHandle) UContext::AllocMainCothread();

    // build the context structure we pass to our "fake" entry point
    auto info = new Context{entry};
    if(!info) throw std::runtime_error("Failed to allocate context");

    // get its ucontext_t and prepare it
    auto uctx = ContextFor(thread);
    memset(uctx, 0, sizeof(*uctx));

    if(getcontext(uctx)) {
        throw std::runtime_error("getcontext() failed");
    }

    // set its stack
    auto offset = sizeof(ucontext_t);
    if(offset % kStackAlignment) {
        offset += kStackAlignment - (offset % kStackAlignment);
    }

    uctx->uc_stack.ss_sp = reinterpret_cast<std::byte *>(thread->stack.data()) + offset;
    uctx->uc_stack.ss_size = (thread->stack.size() * sizeof(uintptr_t)) - offset;

    // store the context in the spicy map
    int id{0};
    {
        std::lock_guard<std::mutex> lg(gContextInfoLock);
        do{
            id = ++gContextNextId;
        } while(!id);
        gContextInfo[id] = info;
    }

    // fill in the context to invoke the helper method
    // this is disgusting but it's C. lol
    makecontext(uctx, reinterpret_cast<void(*)()>(&EntryStub), 1, id);
}

/**
 * Invoke the return handler. This is put in a separate function so it shows up on the stack
 * trace explicitly.
 *
 * @param from Cothread that returned
 */
void UContext::InvokeCothreadDidReturnHandler(Cothread *from) {
    gReturnHandler(from);
    std::terminate();
}

/**
 * Looks up the context for the cothread.
 */
void UContext::EntryStub(int id) {
    // extract info
    Context *info{nullptr};
    {
        std::lock_guard<std::mutex> lg(gContextInfoLock);
        info = gContextInfo.at(id);
        gContextInfo.erase(id);
    }

    // invoke
    info->entry();

    // call the return handler
    delete info;
    UContext::InvokeCothreadDidReturnHandler(gCurrentHandle);
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
 */
void Cothread::switchTo() {
    auto from = UContext::gCurrentHandle;
    UContext::gCurrentHandle = this;

    swapcontext(UContext::ContextFor(from), UContext::ContextFor(this));
}

#pragma clang diagnostic pop
