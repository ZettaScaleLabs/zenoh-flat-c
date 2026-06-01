//
// zenoh.h — a zenoh-c-compatible umbrella header implemented over zenoh-flat-c.
//
// Drop-in for the subset of the zenoh-c API that zenoh-flat covers (closure-based
// pub/sub/query, scouting, info). Include this and link `libzenoh_flat_c`.
//
// NOTE: `Z_FEATURE_UNSTABLE_API` is defined by `zenoh_flat_configure.h` only when
// the library was built with the `unstable` Cargo feature; it gates the
// `reliability` QoS surface (and the unstable branches of the zenoh-c examples).
//
#ifndef ZENOH_H
#define ZENOH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "zenoh_flat_configure.h"
#include "zenoh_flat.h"
#include "zenoh_constants.h"
#include "zenoh_commons.h"

#ifdef __cplusplus
}
#endif

#include "zenoh_macros.h"
#include "zenoh_memory.h"

#endif  // ZENOH_H
