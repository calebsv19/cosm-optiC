#ifndef TIMESCOPE_SESSION_H
#define TIMESCOPE_SESSION_H

#include "session_fwd.h"
#include "../events/event_tracker.h"
#include "../logging/logger.h"
#include "../core/timer_manager.h"
#include "timer_hud/timer_hud_config.h"
#include "timer_hud/timer_hud_backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct TimerHUDSession {
    const TimerHUDBackend* backend;
    TimerManager timer_manager;

    FILE* log_file;
    LogFormat log_format;
    bool wrote_csv_header;

    char settings_path[PATH_MAX];
    char output_root[PATH_MAX];
    char program_name[64];
    TimerHUDSettings settings;

    double display_max_ms[MAX_TIMERS];

    char event_buffer[MAX_EVENTS_PER_FRAME][MAX_EVENT_NAME_LENGTH];
    const char* event_ptrs[MAX_EVENTS_PER_FRAME];
    size_t event_count;
};

TimerHUDSession* ts_session_create_internal(void);
void ts_session_destroy_internal(TimerHUDSession* session);
TimerHUDSession* ts_default_session_internal(void);

#endif // TIMESCOPE_SESSION_H
