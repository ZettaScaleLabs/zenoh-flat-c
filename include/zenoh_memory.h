//
// zenoh-c-compatible memory + sleep helpers.
//
#ifndef ZENOH_COMPAT_MEMORY_H
#define ZENOH_COMPAT_MEMORY_H

#include <stdlib.h>
#include <unistd.h>

static inline void* z_malloc(size_t size) { return malloc(size); }
static inline void* z_realloc(void* ptr, size_t size) { return realloc(ptr, size); }
static inline void z_free(void* ptr) { free(ptr); }

static inline void z_sleep_s(size_t time) { sleep((unsigned int)time); }
static inline void z_sleep_ms(size_t time) { usleep((useconds_t)time * 1000); }
static inline void z_sleep_us(size_t time) { usleep((useconds_t)time); }

#endif  // ZENOH_COMPAT_MEMORY_H
