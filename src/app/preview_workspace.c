#include "app/preview_workspace.h"

#include <math.h>
#include <string.h>

static bool preview_workspace_contains(PreviewWorkspaceRect rect, int x,
                                       int y) {
  return x >= rect.x && x <= rect.x + rect.w && y >= rect.y &&
         y <= rect.y + rect.h;
}

static TimelineStatus
preview_workspace_seek_pointer(PreviewWorkspace *workspace, int x) {
  const PreviewWorkspaceRect *track;
  PreviewTransportDirection direction;
  TimelineSample sample = {0};
  double normalized;
  uint64_t local_frame;

  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  track = &workspace->layout.slider_track;
  normalized = track->w > 0 ? (double)(x - track->x) / (double)track->w : 0.0;
  if (normalized < 0.0)
    normalized = 0.0;
  if (normalized > 1.0)
    normalized = 1.0;
  local_frame = workspace->transport.range.frame_count > 1u
                    ? (uint64_t)llround(
                          normalized *
                          (double)(workspace->transport.range.frame_count - 1u))
                    : 0u;
  sample.absolute_frame =
      workspace->transport.range.start_frame + (int64_t)local_frame;
  sample.subframe_denominator = 1u;
  direction = workspace->transport.direction;
  return PreviewTransportSeek(&workspace->transport, sample, direction);
}

TimelineStatus PreviewWorkspaceResize(PreviewWorkspace *workspace, int width,
                                      int height) {
  PreviewWorkspaceLayout layout = {0};
  int slider_x;
  int slider_right;

  if (!workspace || width <= 0 || height <= 0) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  layout.panel = (PreviewWorkspaceRect){0, height - 88, width, 88};
  layout.play_pause_button = (PreviewWorkspaceRect){12, height - 68, 76, 34};
  layout.loop_button = (PreviewWorkspaceRect){98, height - 68, 64, 34};
  layout.bounce_button = (PreviewWorkspaceRect){170, height - 68, 76, 34};
  slider_x = 266;
  slider_right = width - 144;
  if (slider_right < slider_x + 40)
    slider_right = slider_x + 40;
  layout.slider_track = (PreviewWorkspaceRect){slider_x, height - 60,
                                               slider_right - slider_x, 18};
  layout.frame_label =
      (PreviewWorkspaceRect){slider_right + 12, height - 68, 120, 34};
  workspace->layout = layout;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewWorkspaceInit(PreviewWorkspace *workspace,
                                    TimelineRate rate, TimelineRange range,
                                    int width, int height) {
  PreviewWorkspace initialized;
  TimelineStatus status;

  if (!workspace)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  memset(&initialized, 0, sizeof(initialized));
  status = PreviewTransportInit(&initialized.transport, rate, range);
  if (status != TIMELINE_STATUS_OK)
    return status;
  status = PreviewWorkspaceResize(&initialized, width, height);
  if (status != TIMELINE_STATUS_OK)
    return status;
  status = PreviewTransportPlay(&initialized.transport);
  if (status != TIMELINE_STATUS_OK)
    return status;
  initialized.valid = true;
  *workspace = initialized;
  return TIMELINE_STATUS_OK;
}

TimelineStatus PreviewWorkspaceAdvance(PreviewWorkspace *workspace,
                                       double elapsed_seconds) {
  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  return PreviewTransportAdvance(&workspace->transport, elapsed_seconds);
}

TimelineStatus PreviewWorkspaceTogglePlayback(PreviewWorkspace *workspace) {
  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  return workspace->transport.playing
             ? PreviewTransportPause(&workspace->transport)
             : PreviewTransportPlay(&workspace->transport);
}

TimelineStatus PreviewWorkspaceSetMode(PreviewWorkspace *workspace,
                                       PreviewTransportMode mode) {
  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  return PreviewTransportSetMode(&workspace->transport, mode);
}

TimelineStatus PreviewWorkspaceSeekFrameDelta(PreviewWorkspace *workspace,
                                              int frame_delta) {
  TimelineSample sample = {0};
  int64_t first_frame;
  int64_t last_frame;
  int64_t target_frame;

  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  first_frame = workspace->transport.range.start_frame;
  last_frame =
      first_frame + (int64_t)workspace->transport.range.frame_count - 1;
  target_frame = workspace->transport.sample.absolute_frame + frame_delta;
  if (target_frame < first_frame)
    target_frame = first_frame;
  if (target_frame > last_frame)
    target_frame = last_frame;
  sample.absolute_frame = target_frame;
  sample.subframe_denominator = 1u;
  return PreviewTransportSeek(&workspace->transport, sample,
                              workspace->transport.direction);
}

TimelineStatus PreviewWorkspacePointerDown(PreviewWorkspace *workspace, int x,
                                           int y) {
  TimelineStatus status;

  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  if (preview_workspace_contains(workspace->layout.play_pause_button, x, y)) {
    return PreviewWorkspaceTogglePlayback(workspace);
  }
  if (preview_workspace_contains(workspace->layout.loop_button, x, y)) {
    return PreviewWorkspaceSetMode(workspace, PREVIEW_TRANSPORT_MODE_LOOP);
  }
  if (preview_workspace_contains(workspace->layout.bounce_button, x, y)) {
    return PreviewWorkspaceSetMode(workspace, PREVIEW_TRANSPORT_MODE_BOUNCE);
  }
  if (!preview_workspace_contains(workspace->layout.slider_track, x, y)) {
    return TIMELINE_STATUS_OK;
  }
  workspace->resume_after_scrub = workspace->transport.playing;
  status = PreviewTransportPause(&workspace->transport);
  if (status != TIMELINE_STATUS_OK)
    return status;
  workspace->scrubbing = true;
  status = preview_workspace_seek_pointer(workspace, x);
  if (status != TIMELINE_STATUS_OK) {
    workspace->scrubbing = false;
    workspace->resume_after_scrub = false;
  }
  return status;
}

TimelineStatus PreviewWorkspacePointerMotion(PreviewWorkspace *workspace, int x,
                                             int y) {
  (void)y;
  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  if (!workspace->scrubbing)
    return TIMELINE_STATUS_OK;
  return preview_workspace_seek_pointer(workspace, x);
}

TimelineStatus PreviewWorkspacePointerUp(PreviewWorkspace *workspace, int x,
                                         int y) {
  bool should_resume;
  TimelineStatus status;
  (void)y;

  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  if (!workspace->scrubbing)
    return TIMELINE_STATUS_OK;
  status = preview_workspace_seek_pointer(workspace, x);
  should_resume = workspace->resume_after_scrub;
  workspace->scrubbing = false;
  workspace->resume_after_scrub = false;
  if (status != TIMELINE_STATUS_OK)
    return status;
  return should_resume ? PreviewTransportPlay(&workspace->transport)
                       : TIMELINE_STATUS_OK;
}

TimelineStatus
PreviewWorkspaceCurrentSample(const PreviewWorkspace *workspace,
                              TimelineSample *out_sample,
                              PreviewTransportDirection *out_direction) {
  if (!workspace || !workspace->valid) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  return PreviewTransportCurrentSample(&workspace->transport, out_sample,
                                       out_direction);
}

double PreviewWorkspaceNormalizedPosition(const PreviewWorkspace *workspace) {
  TimelineEvaluationContext context = {0};
  if (!workspace || !workspace->valid ||
      TimelineEvaluationContextBuild(
          workspace->transport.rate, workspace->transport.range,
          workspace->transport.sample, &context) != TIMELINE_STATUS_OK) {
    return 0.0;
  }
  return context.normalized_t;
}
