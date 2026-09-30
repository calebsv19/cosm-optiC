#include "app/evaluated_light_route.h"
#include "motion/scene_motion_paths.h"
#include "animation/timeline_property_registry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
TimelineStatus EvaluatedLightRouteSample(const TimelineEvaluationResult *progress,
    const TimelineEvaluationContext *context, TimelineLightMotionSample *out) {
  if (!progress || !context || !out || !progress->valid ||
      progress->status != TIMELINE_STATUS_OK || progress->value.type != TIMELINE_VALUE_SCALAR ||
      strcmp(progress->property_id, MOTION_LIGHT_PROGRESS_PROPERTY)) return TIMELINE_STATUS_INVALID_ARGUMENT;
  TimelineLightMotionSample sample = {0};
  sample.progress = progress->value.as.scalar;
  if (!isfinite(sample.progress) || sample.progress < 0 || sample.progress > 1)
    return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
  if (!MotionPathsRuntimeTargetSample(progress->target_id, sample.progress,
        &sample.position, &sample.path_length_world, &sample.global_path_t))
    return TIMELINE_STATUS_TARGET_NOT_FOUND;
  sample.valid = true;
  sample.progress_per_frame = progress->derivative_per_frame;
  sample.speed_valid = progress->derivative_valid;
  if (sample.speed_valid)
    sample.world_speed_per_second = fabs(sample.path_length_world * sample.progress_per_frame *
        (double)context->rate.frames_per_second_numerator / context->rate.frames_per_second_denominator);
  sample.invalidation_domains = TIMELINE_INVALIDATION_LIGHTING;
  snprintf(sample.target_id, sizeof(sample.target_id), "%s", progress->target_id);
  *out = sample;
  return TIMELINE_STATUS_OK;
}
