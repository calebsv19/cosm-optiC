#include "time_scope.h"

#include <string.h>
#include <stdio.h>

// Centralized initialization
void ts_init(void) {
    if (!ts_load_settings("src/engine/TimerHUD/settings.json")) {
        fprintf(stderr, "[TimeScope] Using default settings.\n");
    }

    if (ts_settings.log_enabled) {
        LogFormat format = LOG_FORMAT_JSON;
        if (strcmp(ts_settings.log_format, "csv") == 0) {
            format = LOG_FORMAT_CSV;
        }
        logger_init(ts_settings.log_filepath, format);
    }

    event_tracker_init();
    tm_init();
    hud_init();
}

// Emit event only if enabled
void ts_emit_event(const char* tag) {
    if (ts_settings.event_tagging_enabled) {
        event_tracker_add(tag);
    }
}

