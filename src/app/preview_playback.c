#include "app/preview_playback.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static double preview_playback_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool preview_playback_loop_requested(const char* loop_mode) {
    return loop_mode && strcmp(loop_mode, "loop") == 0;
}

bool PreviewPlaybackEvaluate(double elapsed_seconds,
                             double duration_seconds,
                             bool bounce_mode,
                             const char* loop_mode,
                             PreviewPlaybackSample* out_sample) {
    PreviewPlaybackSample sample;
    double safe_duration = duration_seconds;
    double phase = 0.0;
    double cycle = 0.0;

    if (!out_sample) return false;
    memset(&sample, 0, sizeof(sample));

    if (safe_duration <= 0.1) {
        safe_duration = PREVIEW_PLAYBACK_DEFAULT_DURATION_SECONDS;
    }
    if (elapsed_seconds < 0.0) {
        elapsed_seconds = 0.0;
    }

    sample.valid = true;
    sample.duration_seconds = safe_duration;
    sample.elapsed_seconds = elapsed_seconds;

    if (bounce_mode) {
        sample.mode = PREVIEW_PLAYBACK_MODE_BOUNCE;
        strcpy(sample.mode_label, "Bounce");
        cycle = elapsed_seconds / safe_duration;
        phase = fmod(cycle, 2.0);
        if (phase < 0.0) phase += 2.0;
        if (phase <= 1.0) {
            sample.normalized_t = phase;
            sample.reverse_direction = false;
        } else {
            sample.normalized_t = 2.0 - phase;
            sample.reverse_direction = true;
        }
    } else if (preview_playback_loop_requested(loop_mode)) {
        sample.mode = PREVIEW_PLAYBACK_MODE_LOOP;
        strcpy(sample.mode_label, "Loop");
        cycle = elapsed_seconds / safe_duration;
        phase = fmod(cycle, 1.0);
        if (phase < 0.0) phase += 1.0;
        sample.normalized_t = phase;
        sample.reverse_direction = false;
    } else {
        sample.mode = PREVIEW_PLAYBACK_MODE_STOP;
        strcpy(sample.mode_label, "Stop");
        sample.normalized_t = preview_playback_clamp01(elapsed_seconds / safe_duration);
        sample.reverse_direction = false;
        sample.clamped = (elapsed_seconds >= safe_duration);
    }

    sample.normalized_t = preview_playback_clamp01(sample.normalized_t);
    snprintf(sample.status_line,
             sizeof(sample.status_line),
             "Playback %s t=%.3f dir=%s duration=%.2fs%s",
             sample.mode_label,
             sample.normalized_t,
             sample.reverse_direction ? "reverse" : "forward",
             sample.duration_seconds,
             sample.clamped ? " clamped" : "");

    *out_sample = sample;
    return true;
}
