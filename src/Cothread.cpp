#include <libcommunism/Cothread.h>
#include "CothreadPrivate.h"

#include <exception>
#include <iomanip>
#include <iostream>

/**
 * \mainpage libcommunism Documentation
 *
 * This is the autogenerated documentation for the libcommunism library.
 *
 * \section intro_sec Introduction
 *
 * libcommunism is intended to be a really, really fast, portable, and small implementation of
 * cooperative multithreading in userspace.
 *
 * \subsection license License
 *
 * The library is available under the terms of the ISC License.
 *
 * Copyright (c) 2021 Tristan Seifert
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * \section install-sec Installation
 *
 * The library is typically built as a static library, as there are C++ ABI instability problems
 * with C++ shared libraries on many platforms. However, if you really want to build it as a shared
 * library, set the `BUILD_SHARED` flag when compiling.
 *
 * In all cases, the library is built using CMake. Built it as with any other project; i.e. to get
 * started, from the root directory of the source, execute `mkdir build && cmake ..` for most
 * UNIX-like platforms.
 *
 * \section porting Porting
 *
 * Supporting other architectures and platforms should be relatively trivial. See the
 * \link libcommunism::internal::Amd64 amd64\endlink port for an example of the required interaces.
 *
 * \section more More Information
 *
 * For more information, check out the [project site.](https://libcommunism.blraaz.me/)
 */

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
 * Installs the default cothread return handler.
 */
void Cothread::ResetReturnHandler() {
    gReturnHandler = DefaultCothreadReturnedHandler;
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

