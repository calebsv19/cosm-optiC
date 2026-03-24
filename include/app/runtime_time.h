#ifndef RAY_TRACING_RUNTIME_TIME_H
#define RAY_TRACING_RUNTIME_TIME_H

#include <stdint.h>

#include "core_time.h"

static inline uint64_t runtime_time_now_ns(void) {
    return core_time_now_ns();
}

static inline double runtime_time_diff_seconds(uint64_t now_ns, uint64_t prev_ns) {
    return core_time_ns_to_seconds(core_time_diff_ns(now_ns, prev_ns));
}

#endif
