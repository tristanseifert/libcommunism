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

thread_local std::array<uintptr_t, UContext::kMainStackSize> UContext::gMainStack;

std::unordered_map<int, std::unique_ptr<UContext::Context>> UContext::gContextInfo;
std::mutex UContext::gContextInfoLock;
int UContext::gContextNextId{0};


/**
 * Allocates a cothread including a context region of the specified size.
 *
 * This ensures there's sufficient bonus space allocated to hold the ucontext.
 */
UContext::UContext(const Entry &entry, const size_t stackSize) : CothreadImpl(entry, stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(UContext::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : UContext::kDefaultStackSize;

    // then add space for ucontext
    allocSize += sizeof(ucontext_t);
    if(allocSize % kStackAlignment) {
        allocSize += kStackAlignment - (allocSize % kStackAlignment);
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
    this->ownsStack = true;

    Prepare(this, entry);
}

/**
 * Allocates a cothread with an existing region of memory to back its stack.
 */
UContext::UContext(const Entry &entry, std::span<uintptr_t> stack) : CothreadImpl(entry, stack) {
    Prepare(this, entry);
}

/**
 * Deallocates a cothread. This releases the underlying stack if we allocated it.
 */
UContext::~UContext() {
    if(this->ownsStack) {
#ifdef _WIN32
        _aligned_free(this->stack.data());
#else
        free(this->stack.data());
#endif
    }
}



// XXX: We need to disable deprecation warnings for getcontext() and friends on macOS, BSD
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/**
 * Prepares the `ucontext_t` buffer.
 *
 * @param thread Cothread to initialize
 * @param entry Entry point to execute
 *
 * @throw std::runtime_error If context allocation or initialization failed
 */
void UContext::Prepare(UContext *thread, const UContext::Entry &entry) {
    // build the context structure we pass to our "fake" entry point
    auto info = std::make_unique<Context>(entry);
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
        gContextInfo.emplace(id, std::move(info));
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
 *
 * @param id Key into the gContextInfo map
 */
void UContext::EntryStub(int id) {
    // extract info
    std::unique_ptr<Context> info;
    {
        std::lock_guard<std::mutex> lg(gContextInfoLock);
        info = std::move(gContextInfo.at(id));
        gContextInfo.erase(id);
    }

    // invoke
    info->entry();

    // call the return handler
    info.reset();
    UContext::InvokeCothreadDidReturnHandler(Cothread::Current());
}

/**
 * Performs a context switch to the provided cothread.
 *
 * The state of the caller is stored on the stack of the currently active thread.
 */
void UContext::switchTo(CothreadImpl *from) {
    swapcontext(UContext::ContextFor(from), UContext::ContextFor(this));
}

#pragma clang diagnostic pop

/**
 * Allocates the current physical (kernel) thread's Cothread object.
 *
 * @note This will leak the associated cothread object, unless the caller stores it somewhere and
 *       ensures they deallocate it later when the underlying kernel thread is destroyed.
 */
CothreadImpl *libcommunism::AllocKernelThreadWrapper() {
    return new UContext(UContext::gMainStack);
}

