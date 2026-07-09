#ifndef PREVIEW_PLAYBACK_H
#define PREVIEW_PLAYBACK_H

#include <stdbool.h>

#define PREVIEW_PLAYBACK_DEFAULT_DURATION_SECONDS 5.0

typedef enum PreviewPlaybackMode {
    PREVIEW_PLAYBACK_MODE_STOP = 0,
    PREVIEW_PLAYBACK_MODE_LOOP = 1,
    PREVIEW_PLAYBACK_MODE_BOUNCE = 2
} PreviewPlaybackMode;

typedef struct PreviewPlaybackSample {
    bool valid;
    PreviewPlaybackMode mode;
    double duration_seconds;
    double elapsed_seconds;
    double normalized_t;
    bool reverse_direction;
    bool clamped;
    char mode_label[16];
    char status_line[128];
} PreviewPlaybackSample;

bool PreviewPlaybackEvaluate(double elapsed_seconds,
                             double duration_seconds,
                             bool bounce_mode,
                             const char* loop_mode,
                             PreviewPlaybackSample* out_sample);

#endif
