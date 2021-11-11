#ifndef ALLOCIMPL_H
#define ALLOCIMPL_H

#include <cstddef>
#include <cstdint>
#include <span>

#if defined(PLATFORM_AMD64_SYSV) || defined(PLATFORM_AMD64_WINDOWS)
#include "arch/amd64/Common.h"
#elif defined(PLATFORM_SETJMP)
#include "arch/setjmp/SetJmp.h"
#elif defined(PLATFORM_UCONTEXT)
#include "arch/ucontext/UContext.h"
#endif

template <class ImplClass, typename ...Args>
static constexpr auto AllocImplHelper(std::span<uintptr_t> buffer, bool &bufferUsed, Args && ...args) {
    const auto bytes{buffer.size() * sizeof(uintptr_t)};

    if(sizeof(ImplClass) <= bytes && alignof(ImplClass) <= sizeof(uintptr_t)) {
        bufferUsed = true;
        auto mem = reinterpret_cast<ImplClass *>(buffer.data());
        return new(mem) ImplClass(std::forward<Args>(args)...);
    } else {
        bufferUsed = false;
        return new ImplClass(std::forward<Args>(args)...);
    }
}

/**
 * Helper method to construct a new implementation.
 */
template <typename ...Args>
static constexpr auto AllocImpl(std::span<uintptr_t> buffer, bool &bufferUsed, Args && ...args) {
    using namespace libcommunism::internal;

#if defined(PLATFORM_AMD64_SYSV) || defined(PLATFORM_AMD64_WINDOWS)
    return AllocImplHelper<Amd64>(buffer, bufferUsed, std::forward<Args>(args)...);
#elif defined(PLATFORM_SETJMP)
    return AllocImplHelper<SetJmp>(buffer, bufferUsed, std::forward<Args>(args)...);
#elif defined(PLATFORM_UCONTEXT)
    return AllocImplHelper<UContext>(buffer, bufferUsed, std::forward<Args>(args)...);
#else
#error Do not know how to allocate implementation for current platform!
#endif
}

#endif
