#ifndef RAY_TRACING_PREVIEW_WORKSPACE_H
#define RAY_TRACING_PREVIEW_WORKSPACE_H

#include <stdbool.h>

#include "app/preview_transport.h"

typedef struct PreviewWorkspaceRect {
  int x;
  int y;
  int w;
  int h;
} PreviewWorkspaceRect;

typedef struct PreviewWorkspaceLayout {
  PreviewWorkspaceRect panel;
  PreviewWorkspaceRect play_pause_button;
  PreviewWorkspaceRect loop_button;
  PreviewWorkspaceRect bounce_button;
  PreviewWorkspaceRect slider_track;
  PreviewWorkspaceRect frame_label;
} PreviewWorkspaceLayout;

typedef struct PreviewWorkspace {
  bool valid;
  bool scrubbing;
  bool resume_after_scrub;
  PreviewTransport transport;
  PreviewWorkspaceLayout layout;
} PreviewWorkspace;

TimelineStatus PreviewWorkspaceInit(PreviewWorkspace *workspace,
                                    TimelineRate rate, TimelineRange range,
                                    int width, int height);
TimelineStatus PreviewWorkspaceResize(PreviewWorkspace *workspace, int width,
                                      int height);
TimelineStatus PreviewWorkspaceAdvance(PreviewWorkspace *workspace,
                                       double elapsed_seconds);
TimelineStatus PreviewWorkspaceTogglePlayback(PreviewWorkspace *workspace);
TimelineStatus PreviewWorkspaceSetMode(PreviewWorkspace *workspace,
                                       PreviewTransportMode mode);
TimelineStatus PreviewWorkspaceSeekFrameDelta(PreviewWorkspace *workspace,
                                              int frame_delta);
TimelineStatus PreviewWorkspaceInspectSample(PreviewWorkspace *workspace,
                                             TimelineSample sample);
TimelineStatus PreviewWorkspacePointerDown(PreviewWorkspace *workspace, int x,
                                           int y);
TimelineStatus PreviewWorkspacePointerMotion(PreviewWorkspace *workspace, int x,
                                             int y);
TimelineStatus PreviewWorkspacePointerUp(PreviewWorkspace *workspace, int x,
                                         int y);
TimelineStatus
PreviewWorkspaceCurrentSample(const PreviewWorkspace *workspace,
                              TimelineSample *out_sample,
                              PreviewTransportDirection *out_direction);
double PreviewWorkspaceNormalizedPosition(const PreviewWorkspace *workspace);

#endif
