#ifndef RAY_TRACING_PREVIEW_TRANSPORT_H
#define RAY_TRACING_PREVIEW_TRANSPORT_H

#include <stdbool.h>

#include "animation/evaluated_scene_snapshot.h"

typedef enum PreviewTransportMode {
  PREVIEW_TRANSPORT_MODE_LOOP = 0,
  PREVIEW_TRANSPORT_MODE_BOUNCE
} PreviewTransportMode;

typedef enum PreviewTransportDirection {
  PREVIEW_TRANSPORT_DIRECTION_FORWARD = 0,
  PREVIEW_TRANSPORT_DIRECTION_REVERSE
} PreviewTransportDirection;

typedef struct PreviewTransport {
  bool valid;
  bool playing;
  PreviewTransportMode mode;
  PreviewTransportDirection direction;
  TimelineRate rate;
  TimelineRange range;
  TimelineSample sample;
  double phase_seconds;
} PreviewTransport;

TimelineStatus PreviewTransportInit(PreviewTransport *transport,
                                    TimelineRate rate, TimelineRange range);
TimelineStatus PreviewTransportPlay(PreviewTransport *transport);
TimelineStatus PreviewTransportPause(PreviewTransport *transport);
TimelineStatus PreviewTransportSetMode(PreviewTransport *transport,
                                       PreviewTransportMode mode);
TimelineStatus
PreviewTransportSetDirection(PreviewTransport *transport,
                             PreviewTransportDirection direction);
TimelineStatus PreviewTransportSeek(PreviewTransport *transport,
                                    TimelineSample sample,
                                    PreviewTransportDirection direction);
TimelineStatus PreviewTransportAdvance(PreviewTransport *transport,
                                       double elapsed_seconds);
TimelineStatus
PreviewTransportCurrentSample(const PreviewTransport *transport,
                              TimelineSample *out_sample,
                              PreviewTransportDirection *out_direction);
RayEvaluatedPlaybackMode
PreviewTransportEvaluatedPlaybackMode(const PreviewTransport *transport);

#endif
