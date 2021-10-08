#include <libcommunism/Cothread.h>
#include "CothreadPrivate.h"

#include <exception>
#include <iomanip>
#include <iostream>

namespace libcommunism::internal {
static void DefaultCothreadReturnedHandler(Cothread *);
}

using namespace libcommunism;
using namespace libcommunism::internal;

/**
 * Holds a reference to the cothread termination handler. By default, this prints the thread id of
 * the offending cothread and then kills the process.
 */
std::function<void(Cothread *)> internal::gReturnHandler{DefaultCothreadReturnedHandler};



/**
 * Updates the default cothread return handler.
 */
void Cothread::SetReturnHandler(const std::function<void (Cothread *)> &handler) {
    gReturnHandler = handler;
}

/**
 * Default handler for a returned cothread
 */
static void libcommunism::internal::DefaultCothreadReturnedHandler(Cothread *thread) {
    std::cerr << "[libcommunism] Cothread $" << std::hex << thread << std::dec;
    if(thread) {
        const auto &label = thread->getLabel();
        if(!label.empty()) {
            std::cerr << " (" << thread->getLabel() << ")";
        } else {
            std::cerr << " (unnamed cothread)";
        }
    }

    std::cerr << " returned from entry point!" << std::endl;
    std::terminate();
}



/**
 * Initializes the cothread with the provided stack and stack top pointers.
 *
 * This is used internally to create the cothread object for a kernel thread.
 */
Cothread::Cothread(std::span<uintptr_t> _stack, void *_stackTop) : stackTop(_stackTop),
    stack(_stack) {
    // nothing :D
}

