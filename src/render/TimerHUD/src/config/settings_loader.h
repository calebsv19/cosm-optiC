#ifndef TIMESCOPE_SETTINGS_LOADER_H
#define TIMESCOPE_SETTINGS_LOADER_H

#include <stdbool.h>

typedef struct TimeScopeSettings {
    bool hud_enabled;
    bool log_enabled;
    bool event_tagging_enabled;

    int timer_buffer_size;

    char log_filepath[256];
    char log_format[16]; // "csv" or "json"

    char render_mode[16];     // "always" or "throttled"
    float render_threshold;   // seconds


    char hud_position[32];    // e.g., "top-left"
    int hud_offset_x;         // e.g., 10
    int hud_offset_y;         // e.g., 10
} TimeScopeSettings;



// Global access
extern TimeScopeSettings ts_settings;

bool ts_load_settings(const char* filepath);
void save_settings_to_file(const char* path);


#endif // TIMESCOPE_SETTINGS_LOADER_H

