#ifndef COTHREADPRIVATE_H
#define COTHREADPRIVATE_H

#include <libcommunism/Cothread.h>

#include <functional>

/**
 * Namespace containing implementation details of the library.
 */
namespace libcommunism::internal {
extern std::function<void(libcommunism::Cothread *)> gReturnHandler;
};

#endif
