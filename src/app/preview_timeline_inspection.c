#include "app/preview_timeline_inspection.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const TimelineTrack *preview_timeline_selected_track(
    const PreviewTimelineInspection *inspection,
    const PreviewTimelineMarker **out_marker) {
  const PreviewTimelineMarker *marker;
  if (!inspection || !inspection->valid || !inspection->has_selection ||
      inspection->selected_marker_index >= inspection->marker_count ||
      !inspection->document) {
    return NULL;
  }
  marker = &inspection->markers[inspection->selected_marker_index];
  if (marker->track_index >= inspection->document->timeline.track_count) {
    return NULL;
  }
  if (out_marker)
    *out_marker = marker;
  return &inspection->document->timeline.tracks[marker->track_index];
}

TimelineStatus PreviewTimelineInspectionInit(
    PreviewTimelineInspection *inspection,
    const RuntimeSceneLightTimelineDocument *document) {
  PreviewTimelineInspection initialized = {0};
  size_t marker_count = 0u;
  size_t track_index;
  size_t marker_index = 0u;

  if (!inspection || !document || !document->valid ||
      TimelineDocumentValidate(&document->timeline) != TIMELINE_STATUS_OK) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  for (track_index = 0u; track_index < document->timeline.track_count;
       ++track_index) {
    const TimelineTrack *track = &document->timeline.tracks[track_index];
    if (SIZE_MAX - marker_count < track->key_count) {
      return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    marker_count += track->key_count;
  }
  if (marker_count > 0u) {
    initialized.markers =
        (PreviewTimelineMarker *)calloc(marker_count, sizeof(*initialized.markers));
    if (!initialized.markers)
      return TIMELINE_STATUS_CAPACITY_EXCEEDED;
  }
  initialized.document = document;
  initialized.marker_count = marker_count;
  for (track_index = 0u; track_index < document->timeline.track_count;
       ++track_index) {
    const TimelineTrack *track = &document->timeline.tracks[track_index];
    size_t key_index;
    for (key_index = 0u; key_index < track->key_count; ++key_index) {
      PreviewTimelineMarker *marker = &initialized.markers[marker_index++];
      marker->track_index = track_index;
      marker->key_index = key_index;
      marker->frame = track->keys[key_index].frame;
      marker->interpolation_to_next =
          track->keys[key_index].interpolation_to_next;
      marker->source = track->source;
      marker->track_enabled = track->enabled;
    }
  }
  initialized.valid = true;
  *inspection = initialized;
  return TIMELINE_STATUS_OK;
}

void PreviewTimelineInspectionReset(PreviewTimelineInspection *inspection) {
  if (!inspection)
    return;
  free(inspection->markers);
  memset(inspection, 0, sizeof(*inspection));
}

int PreviewTimelineInspectionMarkerX(const PreviewTimelineInspection *inspection,
                                     size_t marker_index,
                                     PreviewWorkspaceRect slider_track) {
  TimelineEvaluationContext context = {0};
  TimelineSample sample = {0};
  double normalized;
  if (!inspection || !inspection->valid ||
      marker_index >= inspection->marker_count || slider_track.w <= 0 ||
      !inspection->document) {
    return slider_track.x;
  }
  sample.absolute_frame = inspection->markers[marker_index].frame;
  sample.subframe_denominator = 1u;
  if (TimelineEvaluationContextBuild(inspection->document->timeline.rate,
                                     inspection->document->timeline.range,
                                     sample, &context) != TIMELINE_STATUS_OK) {
    return slider_track.x;
  }
  normalized = context.normalized_t;
  if (normalized < 0.0)
    normalized = 0.0;
  if (normalized > 1.0)
    normalized = 1.0;
  return slider_track.x + (int)llround(normalized * (double)slider_track.w);
}

bool PreviewTimelineInspectionSelectAt(PreviewTimelineInspection *inspection,
                                       PreviewWorkspaceRect slider_track, int x,
                                       int y, int tolerance_pixels,
                                       TimelineSample *out_sample) {
  size_t marker_index;
  size_t best_index = 0u;
  int best_distance = INT_MAX;
  if (!inspection || !inspection->valid || tolerance_pixels < 0 ||
      y < slider_track.y - tolerance_pixels ||
      y > slider_track.y + slider_track.h + tolerance_pixels) {
    return false;
  }
  for (marker_index = 0u; marker_index < inspection->marker_count;
       ++marker_index) {
    int marker_x =
        PreviewTimelineInspectionMarkerX(inspection, marker_index, slider_track);
    int distance = abs(x - marker_x);
    if (distance <= tolerance_pixels && distance < best_distance) {
      best_distance = distance;
      best_index = marker_index;
    }
  }
  if (best_distance == INT_MAX)
    return false;
  inspection->selected_marker_index = best_index;
  inspection->has_selection = true;
  if (out_sample) {
    memset(out_sample, 0, sizeof(*out_sample));
    out_sample->absolute_frame = inspection->markers[best_index].frame;
    out_sample->subframe_denominator = 1u;
  }
  return true;
}

void PreviewTimelineInspectionClearSelection(
    PreviewTimelineInspection *inspection) {
  if (!inspection)
    return;
  inspection->has_selection = false;
  inspection->selected_marker_index = 0u;
  inspection->frame_line[0] = '\0';
  inspection->channel_line[0] = '\0';
  inspection->interpolation_line[0] = '\0';
  inspection->provenance_line[0] = '\0';
}

void PreviewTimelineInspectionUpdateReadout(
    PreviewTimelineInspection *inspection,
    const RayEvaluatedSceneSnapshot *snapshot) {
  const PreviewTimelineMarker *marker = NULL;
  const TimelineTrack *track =
      preview_timeline_selected_track(inspection, &marker);
  const TimelineEvaluationResult *provenance;
  bool matching_provenance;
  if (!track || !marker)
    return;

  snprintf(inspection->frame_line, sizeof(inspection->frame_line),
           "Selected key: frame %lld", (long long)marker->frame);
  snprintf(inspection->channel_line, sizeof(inspection->channel_line),
           "Target %s  /  Property %s  /  Track %s", track->target_id,
           track->property_id, track->track_id);
  snprintf(inspection->interpolation_line,
           sizeof(inspection->interpolation_line),
           "Interpolation to next: %s  /  Source: %s%s",
           TimelineInterpolationLabel(marker->interpolation_to_next),
           TimelineChannelSourceLabel(marker->source),
           marker->track_enabled ? "" : "  /  disabled");

  if (!snapshot || !snapshot->valid) {
    snprintf(inspection->provenance_line,
             sizeof(inspection->provenance_line),
             "Evaluated provenance: unavailable at this Preview sample");
    return;
  }
  provenance = &snapshot->light.property_provenance;
  matching_provenance =
      provenance->valid &&
      strcmp(provenance->target_id, track->target_id) == 0 &&
      strcmp(provenance->property_id, track->property_id) == 0;
  if (matching_provenance) {
    snprintf(inspection->provenance_line,
             sizeof(inspection->provenance_line),
             "Evaluated provenance: %s, keys %lld -> %lld, alpha %.3f",
             provenance->exact_key
                 ? "exact key"
                 : (provenance->interpolated ? "interpolated" : "held"),
             (long long)provenance->left_frame,
             (long long)provenance->right_frame, provenance->alpha);
  } else {
    snprintf(inspection->provenance_line,
             sizeof(inspection->provenance_line),
             "Evaluated provenance: selected channel is not in the light-first snapshot");
  }
}
