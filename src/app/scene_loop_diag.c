#include "app/scene_loop_diag.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct SceneLoopDiagState {
    bool initialized;
    bool enabled;
    bool json_output;
    uint64_t period_elapsed_ms;
    uint64_t frames;
    uint64_t wait_calls;
    uint64_t blocked_ms;
    uint64_t active_ms;
} SceneLoopDiagState;

static SceneLoopDiagState s_loop_diag = {0};

static bool scene_loop_diag_env_truthy(const char *value) {
    return value && value[0] &&
           (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0);
}

static void scene_loop_diag_init_once(void) {
    if (s_loop_diag.initialized) {
        return;
    }
    if (scene_loop_diag_env_truthy(getenv("RAY_TRACING_LOOP_DIAG_LOG"))) {
        s_loop_diag.enabled = true;
    }
    {
        const char *format_env = getenv("RAY_TRACING_LOOP_DIAG_FORMAT");
        if (format_env && format_env[0] && strcasecmp(format_env, "json") == 0) {
            s_loop_diag.enabled = true;
            s_loop_diag.json_output = true;
        }
    }
    if (scene_loop_diag_env_truthy(getenv("RAY_TRACING_LOOP_DIAG_JSON"))) {
        s_loop_diag.enabled = true;
        s_loop_diag.json_output = true;
    }
    s_loop_diag.initialized = true;
}

static void scene_loop_diag_emit(uint64_t period_ms) {
    uint64_t total_ms = s_loop_diag.blocked_ms + s_loop_diag.active_ms;
    double blocked_pct = (total_ms > 0u)
        ? (100.0 * (double)s_loop_diag.blocked_ms / (double)total_ms)
        : 0.0;
    double active_pct = (total_ms > 0u)
        ? (100.0 * (double)s_loop_diag.active_ms / (double)total_ms)
        : 0.0;

    if (s_loop_diag.json_output) {
        printf("{\"tag\":\"LoopDiag\",\"schema\":1,\"period_ms\":%llu,\"frames\":%llu,"
               "\"wait_calls\":%llu,\"blocked_ms\":%llu,\"blocked_pct\":%.1f,"
               "\"active_ms\":%llu,\"active_pct\":%.1f,\"wakes\":0,\"timers\":0,"
               "\"results_queue\":{\"last\":0,\"peak\":0},"
               "\"jobs\":{\"scheduled\":0,\"coalesced\":0},"
               "\"results\":{\"applied\":0,\"stale_dropped\":0},"
               "\"edit_txn\":{\"starts\":0,\"commits\":0,\"debounce_commits\":0,\"boundary_commits\":0},"
               "\"events\":{\"queue_last\":0,\"queue_peak\":0,\"enqueued\":0,\"processed\":0,\"deferred\":0,\"dropped\":0,"
               "\"emit\":{\"symbols\":0,\"diagnostics\":0,\"analysis_progress\":0,\"analysis_status\":0,\"library_index\":0,\"analysis_finished\":0},"
               "\"dispatch\":{\"symbols\":0,\"diagnostics\":0,\"analysis_progress\":0,\"analysis_status\":0,\"library_index\":0,\"analysis_finished\":0}},"
               "\"stale_by_kind\":{\"symbols\":0,\"diagnostics\":0,\"analysis_progress\":0,\"analysis_status\":0,\"analysis_finished\":0}}\n",
               (unsigned long long)period_ms,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.wait_calls,
               (unsigned long long)s_loop_diag.blocked_ms,
               blocked_pct,
               (unsigned long long)s_loop_diag.active_ms,
               active_pct);
    } else {
        printf("[LoopDiag] period=%llums frames=%llu waits=%llu blocked=%llums(%.1f%%) active=%llums(%.1f%%)\n",
               (unsigned long long)period_ms,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.wait_calls,
               (unsigned long long)s_loop_diag.blocked_ms,
               blocked_pct,
               (unsigned long long)s_loop_diag.active_ms,
               active_pct);
    }
}

void scene_loop_diag_tick(double frame_elapsed_sec,
                          uint32_t wait_blocked_ms,
                          uint32_t wait_call_count) {
    uint64_t frame_elapsed_ms = 0u;
    uint64_t blocked_ms = 0u;
    uint64_t active_ms = 0u;

    scene_loop_diag_init_once();
    if (!s_loop_diag.enabled || frame_elapsed_sec <= 0.0) {
        return;
    }

    frame_elapsed_ms = (uint64_t)(frame_elapsed_sec * 1000.0);
    blocked_ms = wait_blocked_ms;
    if (blocked_ms > frame_elapsed_ms) {
        blocked_ms = frame_elapsed_ms;
    }
    active_ms = frame_elapsed_ms - blocked_ms;

    s_loop_diag.period_elapsed_ms += frame_elapsed_ms;
    s_loop_diag.frames += 1u;
    s_loop_diag.wait_calls += wait_call_count;
    s_loop_diag.blocked_ms += blocked_ms;
    s_loop_diag.active_ms += active_ms;

    if (s_loop_diag.period_elapsed_ms < 1000u) {
        return;
    }

    scene_loop_diag_emit(s_loop_diag.period_elapsed_ms);
    s_loop_diag.period_elapsed_ms = 0u;
    s_loop_diag.frames = 0u;
    s_loop_diag.wait_calls = 0u;
    s_loop_diag.blocked_ms = 0u;
    s_loop_diag.active_ms = 0u;
}
