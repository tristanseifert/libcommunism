#ifndef COTHREADPRIVATE_H
#define COTHREADPRIVATE_H

#include <libcommunism/Cothread.h>

#include <functional>

/// Implementation details (including architecture/platform specific code) for the library
namespace libcommunism::internal {
extern std::function<void(libcommunism::Cothread *)> gReturnHandler;
};

#endif
