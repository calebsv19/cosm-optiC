#include "app/preview_transport.h"

#include <math.h>
#include <string.h>

static bool preview_transport_mode_valid(PreviewTransportMode mode) {
  return mode == PREVIEW_TRANSPORT_MODE_LOOP ||
         mode == PREVIEW_TRANSPORT_MODE_BOUNCE;
}

static bool
preview_transport_direction_valid(PreviewTransportDirection direction) {
  return direction == PREVIEW_TRANSPORT_DIRECTION_FORWARD ||
         direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE;
}

static double preview_transport_fps(const PreviewTransport *transport) {
  return (double)transport->rate.frames_per_second_numerator /
         (double)transport->rate.frames_per_second_denominator;
}

static TimelineStatus preview_transport_phase_for_sample(
    const PreviewTransport *transport, TimelineSample sample,
    PreviewTransportDirection direction, double *out_phase_seconds) {
  TimelineEvaluationContext context;
  TimelineStatus status;
  double local_position;
  double max_local;
  double phase_frames;

  if (!transport || !out_phase_seconds ||
      !preview_transport_direction_valid(direction)) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  status = TimelineEvaluationContextBuild(transport->rate, transport->range,
                                          sample, &context);
  if (status != TIMELINE_STATUS_OK)
    return status;

  local_position = context.local_frame_position;
  max_local = (double)(transport->range.frame_count - 1u);
  if (transport->mode == PREVIEW_TRANSPORT_MODE_BOUNCE &&
      local_position > max_local) {
    return TIMELINE_STATUS_FRAME_OUT_OF_RANGE;
  }
  phase_frames = local_position;
  if (transport->mode == PREVIEW_TRANSPORT_MODE_BOUNCE &&
      direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE && max_local > 0.0) {
    phase_frames = (max_local * 2.0) - local_position;
  }
  *out_phase_seconds = phase_frames / preview_transport_fps(transport);
  return TIMELINE_STATUS_OK;
}

static double preview_transport_positive_mod(double value, double period) {
  double result;
  if (!(period > 0.0))
    return 0.0;
  result = fmod(value, period);
  if (result < 0.0)
    result += period;
  return result;
}

TimelineStatus PreviewTransportInit(PreviewTransport *transport,
                                    TimelineRate rate, TimelineRange range) {
  PreviewTransport initialized;
  if (!transport)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  if (!TimelineRateIsValid(rate))
    return TIMELINE_STATUS_INVALID_RATE;
  if (!TimelineRangeIsValid(range))
    return TIMELINE_STATUS_INVALID_RANGE;

  memset(&initialized, 0, sizeof(initialized));
  initialized.valid = true;
  initialized.playing = false;
  initialized.mode = PREVIEW_TRANSPORT_MODE_LOOP;
  initialized.direction = PREVIEW_TRANSPORT_DIRECTION_FORWARD;
  initialized.rate = rate;
  initialized.range = range;
  initialized.sample.absolute_frame = range.start_frame;
  initialized.sample.subframe_denominator = 1u;
  *transport = initialized;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewTransportPlay(PreviewTransport *transport) {
  if (!transport || !transport->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  transport->playing = true;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewTransportPause(PreviewTransport *transport) {
  if (!transport || !transport->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  transport->playing = false;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewTransportSetMode(PreviewTransport *transport,
                                       PreviewTransportMode mode) {
  PreviewTransport candidate;
  TimelineStatus status;
  if (!transport || !transport->valid || !preview_transport_mode_valid(mode)) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  candidate = *transport;
  candidate.mode = mode;
  status = preview_transport_phase_for_sample(&candidate, candidate.sample,
                                              candidate.direction,
                                              &candidate.phase_seconds);
  if (status != TIMELINE_STATUS_OK)
    return status;
  *transport = candidate;
  return TIMELINE_STATUS_OK;
}

TimelineStatus
PreviewTransportSetDirection(PreviewTransport *transport,
                             PreviewTransportDirection direction) {
  PreviewTransport candidate;
  TimelineStatus status;
  if (!transport || !transport->valid ||
      !preview_transport_direction_valid(direction)) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  candidate = *transport;
  candidate.direction = direction;
  status = preview_transport_phase_for_sample(
      &candidate, candidate.sample, direction, &candidate.phase_seconds);
  if (status != TIMELINE_STATUS_OK)
    return status;
  *transport = candidate;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewTransportSeek(PreviewTransport *transport,
                                    TimelineSample sample,
                                    PreviewTransportDirection direction) {
  PreviewTransport candidate;
  TimelineStatus status;
  if (!transport || !transport->valid ||
      !preview_transport_direction_valid(direction)) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  candidate = *transport;
  status = preview_transport_phase_for_sample(&candidate, sample, direction,
                                              &candidate.phase_seconds);
  if (status != TIMELINE_STATUS_OK)
    return status;
  candidate.sample = sample;
  candidate.direction = direction;
  *transport = candidate;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewTransportAdvance(PreviewTransport *transport,
                                       double elapsed_seconds) {
  RayEvaluatedPlaybackMode evaluated_mode;
  TimelineSample sample;
  bool reverse_direction = false;
  double fps;
  double period_seconds;
  double phase_frames;
  double max_local;
  TimelineStatus status;

  if (!transport || !transport->valid || !isfinite(elapsed_seconds) ||
      elapsed_seconds < 0.0) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  if (!transport->playing || elapsed_seconds == 0.0) {
    return TIMELINE_STATUS_OK;
  }

  fps = preview_transport_fps(transport);
  max_local = (double)(transport->range.frame_count - 1u);
  if (transport->mode == PREVIEW_TRANSPORT_MODE_LOOP) {
    period_seconds = (double)transport->range.frame_count / fps;
    if (transport->direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE) {
      transport->phase_seconds -= elapsed_seconds;
    } else {
      transport->phase_seconds += elapsed_seconds;
    }
  } else {
    period_seconds = max_local > 0.0 ? (max_local * 2.0) / fps : 0.0;
    transport->phase_seconds += elapsed_seconds;
  }
  transport->phase_seconds =
      preview_transport_positive_mod(transport->phase_seconds, period_seconds);

  evaluated_mode = PreviewTransportEvaluatedPlaybackMode(transport);
  status = RayEvaluatedTimelineSampleFromElapsed(
      transport->rate, transport->range, transport->phase_seconds,
      evaluated_mode, &sample, &reverse_direction, NULL);
  if (status != TIMELINE_STATUS_OK)
    return status;

  transport->sample = sample;
  if (transport->mode == PREVIEW_TRANSPORT_MODE_BOUNCE) {
    phase_frames = transport->phase_seconds * fps;
    if (max_local <= 0.0 || phase_frames == 0.0) {
      transport->direction = PREVIEW_TRANSPORT_DIRECTION_FORWARD;
    } else if (phase_frames >= max_local) {
      transport->direction = PREVIEW_TRANSPORT_DIRECTION_REVERSE;
    } else {
      transport->direction = reverse_direction
                                 ? PREVIEW_TRANSPORT_DIRECTION_REVERSE
                                 : PREVIEW_TRANSPORT_DIRECTION_FORWARD;
    }
  }
  return TIMELINE_STATUS_OK;
}

TimelineStatus
PreviewTransportCurrentSample(const PreviewTransport *transport,
                              TimelineSample *out_sample,
                              PreviewTransportDirection *out_direction) {
  if (!transport || !transport->valid || !out_sample) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  *out_sample = transport->sample;
  if (out_direction)
    *out_direction = transport->direction;
  return TIMELINE_STATUS_OK;
}

RayEvaluatedPlaybackMode
PreviewTransportEvaluatedPlaybackMode(const PreviewTransport *transport) {
  if (transport && transport->valid &&
      transport->mode == PREVIEW_TRANSPORT_MODE_BOUNCE) {
    return RAY_EVALUATED_PLAYBACK_BOUNCE;
  }
  return RAY_EVALUATED_PLAYBACK_LOOP;
}
