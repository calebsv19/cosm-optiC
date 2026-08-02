#ifndef RAY_TRACING_PREVIEW_TIMELINE_INSPECTION_H
#define RAY_TRACING_PREVIEW_TIMELINE_INSPECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "animation/evaluated_scene_snapshot.h"
#include "app/preview_workspace.h"
#include "import/runtime_scene_light_timeline_io.h"

typedef struct PreviewTimelineMarker {
  size_t track_index;
  size_t key_index;
  int64_t frame;
  TimelineInterpolation interpolation_to_next;
  TimelineChannelSource source;
  bool track_enabled;
} PreviewTimelineMarker;

typedef struct PreviewTimelineInspection {
  bool valid;
  const RuntimeSceneLightTimelineDocument *document;
  PreviewTimelineMarker *markers;
  size_t marker_count;
  size_t selected_marker_index;
  bool has_selection;
  char frame_line[128];
  char channel_line[256];
  char interpolation_line[192];
  char provenance_line[256];
} PreviewTimelineInspection;

TimelineStatus PreviewTimelineInspectionInit(
    PreviewTimelineInspection *inspection,
    const RuntimeSceneLightTimelineDocument *document);
void PreviewTimelineInspectionReset(PreviewTimelineInspection *inspection);
int PreviewTimelineInspectionMarkerX(const PreviewTimelineInspection *inspection,
                                     size_t marker_index,
                                     PreviewWorkspaceRect slider_track);
bool PreviewTimelineInspectionSelectAt(PreviewTimelineInspection *inspection,
                                       PreviewWorkspaceRect slider_track, int x,
                                       int y, int tolerance_pixels,
                                       TimelineSample *out_sample);
void PreviewTimelineInspectionClearSelection(
    PreviewTimelineInspection *inspection);
void PreviewTimelineInspectionUpdateReadout(
    PreviewTimelineInspection *inspection,
    const RayEvaluatedSceneSnapshot *snapshot);

#endif
