/**
 * This implements some abuses of setjmp and longjmp to do platform independent context switching
 * in a somewhat performant manner. It relies on the existence of signals, so it is limited to
 * UNIX-like systems.
 */
#include "SetJmp.h"
#include "CothreadPrivate.h"

#include <atomic>
#include <csetjmp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <system_error>

#include <signal.h>

using namespace libcommunism;
using namespace libcommunism::internal;

static_assert(sizeof(sigjmp_buf) < (SetJmp::kMainStackSize * sizeof(uintptr_t)),
        "main stack size is too small for sigjmp_buf!");

thread_local Cothread *SetJmp::gCurrentHandle{nullptr};
thread_local std::array<uintptr_t, SetJmp::kMainStackSize> SetJmp::gMainStack;

SetJmp::EntryContext *SetJmp::gCurrentlyPreparing{nullptr};
std::mutex SetJmp::gSignalLock;

/**
 * Returns the handle to the currently executing cothread.
 *
 * If no cothread has been lanched yet, the "fake" initial cothread handle is returned.
 */
Cothread *Cothread::Current() {
    if(!SetJmp::gCurrentHandle) SetJmp::AllocMainCothread();
    return SetJmp::gCurrentHandle;
}

/**
 * Allocates a cothread including a context region of the specified size.
 *
 * This ensures there's sufficient bonus space allocated to hold the sigjmp_buf.
 */
Cothread::Cothread(const Entry &entry, const size_t stackSize) {
    void *buf{nullptr};

    // round down stack size to ensure it's aligned before allocating it
    auto allocSize = stackSize & ~(SetJmp::kStackAlignment - 1);
    allocSize = allocSize ? allocSize : SetJmp::kDefaultStackSize;

    // then add space for ucontext
    allocSize += sizeof(sigjmp_buf);
    if(allocSize % SetJmp::kStackAlignment) {
        allocSize += SetJmp::kStackAlignment - (allocSize % SetJmp::kStackAlignment);
    }

    // and allocate it
#ifdef _WIN32
    buf = _aligned_malloc(allocSize, SetJmp::kStackAlignment);
#else
    int err{0};
    err = posix_memalign(&buf, SetJmp::kStackAlignment, allocSize);
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

    SetJmp::Prepare(this, entry);
}

/**
 * Allocates a cothread with an existing region of memory to back its stack and jump buffer.
 */
Cothread::Cothread(const Entry &entry, std::span<uintptr_t> _stack) : stack(_stack) {
    SetJmp::Prepare(this, entry);
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
 * Performs a context switch to the provided cothread.
 */
void Cothread::switchTo() {
    if(!sigsetjmp(*SetJmp::JmpBufFor(SetJmp::gCurrentHandle), 0)) {
        SetJmp::gCurrentHandle = this;
        std::atomic_thread_fence(std::memory_order_release);
        siglongjmp(*SetJmp::JmpBufFor(this), 1);
    }
}



/**
 * Allocates the Cothread instance for the current kernel thread.
 */
void SetJmp::AllocMainCothread() {
    auto main = new Cothread(gMainStack, gMainStack.data() + SetJmp::kMainStackSize);
    gCurrentHandle = main;
}

/**
 * Invoke the return handler. This is put in a separate function so it shows up on the stack
 * trace explicitly.
 *
 * @param from Cothread that returned
 */
void SetJmp::InvokeCothreadDidReturnHandler(Cothread *from) {
    gReturnHandler(from);
    std::terminate();
}

/**
 * Prepares the `sigjmp_buf` buffer for the cothread, so that when it is switched to, it will
 * begin executing its entry point.
 *
 * This abuses signal handling to set up the return stack in a platform independent way. The
 * algorithm is very well described in Engelschall's Portable Multithreading.
 *
 * @param thread Cothread to initialize
 * @param entry Entry point to execute
 *
 * @throw std::runtime_error If context allocation or initialization failed
 */
void SetJmp::Prepare(Cothread *thread, const Cothread::Entry &entry) {
    int err{0};
    struct sigaction handler, oldHandler;
    stack_t stack{}, oldStack{};

    if(!gCurrentHandle) SetJmp::AllocMainCothread();

    auto jbuf = JmpBufFor(thread);
    memset(jbuf, 0, sizeof(*jbuf));

    // prepare the signal handling stack
    auto offset = sizeof(sigjmp_buf);
    if(offset % kStackAlignment) {
        offset += kStackAlignment - (offset % kStackAlignment);
    }

    stack.ss_sp = reinterpret_cast<std::byte *>(thread->stack.data()) + offset;
    stack.ss_size = (thread->stack.size() * sizeof(uintptr_t)) - offset;

    auto info = new EntryContext(thread, entry);
    if(!info) throw std::runtime_error("Failed to allocate context");

    // listen man you're just gonna have to trust me on this one
    try {
        std::lock_guard<std::mutex> lock(gSignalLock);

        err = sigaltstack(&stack, &oldStack);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "sigaltstack");
        }

        gCurrentlyPreparing = info;
        std::atomic_thread_fence(std::memory_order_release);

        handler.sa_handler = SignalHandlerSetupThunk;
        handler.sa_flags = SA_ONSTACK;
        sigemptyset(&handler.sa_mask);

        err = sigaction(SIGUSR1, &handler, &oldHandler);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "sigaction");
        }
        err = raise(SIGUSR1);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "raise");
        }
        sigaltstack(&oldStack, nullptr);
        sigaction(SIGUSR1, &oldHandler, nullptr);
    } catch(const std::exception &) {
        delete info;
        throw;
    }
}

/**
 * Helper method that's registered as a signal handler when initializing a cothread.
 */
void SetJmp::SignalHandlerSetupThunk(int signal) {
    (void) signal;

    auto ctx = gCurrentlyPreparing;
    if(sigsetjmp(*JmpBufFor(ctx->thread), 0)) {
        ctx->entry();
        InvokeCothreadDidReturnHandler(ctx->thread);
    }
}
