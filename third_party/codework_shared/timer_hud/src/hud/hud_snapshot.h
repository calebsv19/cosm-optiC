#ifndef TIMESCOPE_HUD_SNAPSHOT_H
#define TIMESCOPE_HUD_SNAPSHOT_H

#include "timer_hud/timer_hud_config.h"
#include "../core/timer.h"
#include "../core/session_fwd.h"
#include "../core/timer_manager.h"

#include <stdbool.h>
#include <stddef.h>

#define TIMER_HUD_ROW_TEXT_MAX 256

typedef enum TimerHUDAnchor {
    TIMER_HUD_ANCHOR_TOP_LEFT = 0,
    TIMER_HUD_ANCHOR_TOP_RIGHT = 1,
    TIMER_HUD_ANCHOR_BOTTOM_LEFT = 2,
    TIMER_HUD_ANCHOR_BOTTOM_RIGHT = 3,
} TimerHUDAnchor;

typedef struct TimerHUDGraphSnapshot {
    size_t sample_count;
    double scale_max_ms;
    double samples[TIMER_HISTORY_SIZE];
} TimerHUDGraphSnapshot;

typedef struct TimerHUDRowSnapshot {
    char text[TIMER_HUD_ROW_TEXT_MAX];
    bool has_graph;
    TimerHUDGraphSnapshot graph;
} TimerHUDRowSnapshot;

typedef struct TimerHUDRenderSnapshot {
    bool hud_enabled;
    TimerHUDVisualMode visual_mode;
    TimerHUDAnchor anchor;
    int offset_x;
    int offset_y;
    int graph_width;
    int graph_height;
    int row_count;
    TimerHUDRowSnapshot rows[MAX_TIMERS];
} TimerHUDRenderSnapshot;

bool hud_snapshot_build(TimerHUDSession* session, TimerHUDRenderSnapshot* out_snapshot);

#endif // TIMESCOPE_HUD_SNAPSHOT_H
