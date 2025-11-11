#include "timer.h"
#include <time.h>
#include <math.h>
#include <string.h>

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void timer_init(Timer* timer, const char* name) {
    memset(timer, 0, sizeof(Timer));
    timer->name = name;
}

void timer_start(Timer* timer) {
    if (!timer->running) {
        timer->start_time = get_time_ns();
        timer->running = true;
    }
}

void timer_stop(Timer* timer) {
    if (!timer->running) return;

    uint64_t end_time = get_time_ns();
    double duration_ms = (end_time - timer->start_time) / 1e6;

    timer->durations[timer->index] = duration_ms;
    timer->index = (timer->index + 1) % TIMER_HISTORY_SIZE;
    if (timer->count < TIMER_HISTORY_SIZE) {
        timer->count++;
    }

    timer->running = false;
    timer_update_stats(timer);
}

void timer_update_stats(Timer* timer) {
    if (timer->count == 0) return;

    double sum = 0.0;
    double min = timer->durations[0];
    double max = timer->durations[0];

    for (size_t i = 0; i < timer->count; i++) {
        double d = timer->durations[i];
        sum += d;
        if (d < min) min = d;
        if (d > max) max = d;
    }

    timer->avg = sum / timer->count;
    timer->min = min;
    timer->max = max;

    double variance = 0.0;
    for (size_t i = 0; i < timer->count; i++) {
        double diff = timer->durations[i] - timer->avg;
        variance += diff * diff;
    }

    timer->stddev = sqrt(variance / timer->count);
}

