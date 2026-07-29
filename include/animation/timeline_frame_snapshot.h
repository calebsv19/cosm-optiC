#ifndef RAY_TRACING_TIMELINE_FRAME_SNAPSHOT_H
#define RAY_TRACING_TIMELINE_FRAME_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "animation/timeline_property_registry.h"

#define TIMELINE_FRAME_SNAPSHOT_PROPERTY_CAPACITY                              \
  TIMELINE_DOCUMENT_TRACK_CAPACITY

typedef struct TimelineFrameSnapshot {
  TimelineEvaluationContext context;
  size_t property_count;
  TimelinePropertyEvaluationResult
      properties[TIMELINE_FRAME_SNAPSHOT_PROPERTY_CAPACITY];
  uint32_t invalidation_domains;
} TimelineFrameSnapshot;

typedef TimelineStatus (*TimelineFrameApplyPropertyFn)(
    void *copied_scene, const TimelinePropertyEvaluationResult *property,
    void *user_data);

typedef TimelineStatus (*TimelineFrameValidateSceneFn)(const void *copied_scene,
                                                       void *user_data);

typedef struct TimelineFrameApplicationAdapter {
  TimelineFrameApplyPropertyFn apply_property;
  TimelineFrameValidateSceneFn validate_scene;
} TimelineFrameApplicationAdapter;

typedef struct TimelineFrameApplicationReport {
  TimelineEvaluationContext context;
  size_t applied_property_count;
  uint32_t invalidation_domains;
  bool scene_changed;
} TimelineFrameApplicationReport;

TimelineStatus
TimelineFrameSnapshotBuild(const TimelinePropertyRegistry *registry,
                           const TimelineDocument *document,
                           const TimelineEvaluationContext *context,
                           TimelineFrameSnapshot *out_snapshot);

TimelineStatus
TimelineFrameSnapshotValidate(const TimelinePropertyRegistry *registry,
                              const TimelineFrameSnapshot *snapshot);

TimelineStatus TimelineFrameSnapshotApplyToCopy(
    const TimelinePropertyRegistry *registry,
    const TimelineFrameSnapshot *snapshot, const void *base_scene,
    void *scratch_scene, size_t scene_size,
    const TimelineFrameApplicationAdapter *adapter, void *user_data,
    void *out_scene, TimelineFrameApplicationReport *out_report);

/*
 * base_scene, scratch_scene, and out_scene must describe the same trivially
 * byte-copyable scene representation. Pointer-owning live scenes require a
 * later clone/application adapter rather than this foundation copy seam.
 *
 * scratch_scene is caller-owned working storage and must not alias base_scene
 * or out_scene. base_scene and out_scene may alias for transactional in-place
 * commit. On refusal, scratch contents are unspecified while base_scene,
 * out_scene, and out_report remain unchanged.
 */

#endif
