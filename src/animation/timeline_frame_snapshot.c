#include "animation/timeline_frame_snapshot.h"

#include <string.h>

static bool timeline_frame_id_is_valid(const char *id) {
  size_t length = 0u;
  if (!id || id[0] == '\0')
    return false;
  while (length < TIMELINE_ID_CAPACITY && id[length] != '\0')
    length += 1u;
  return length > 0u && length < TIMELINE_ID_CAPACITY;
}

static const char *
timeline_frame_target_prefix(TimelinePropertyTargetKind kind) {
  switch (kind) {
  case TIMELINE_PROPERTY_TARGET_OBJECT:
    return "object/";
  case TIMELINE_PROPERTY_TARGET_LIGHT:
    return "light/";
  case TIMELINE_PROPERTY_TARGET_MATERIAL:
    return "material/";
  case TIMELINE_PROPERTY_TARGET_CAMERA_RESERVED:
    return "camera/";
  case TIMELINE_PROPERTY_TARGET_VOLUME_RESERVED:
    return "volume/";
  default:
    return NULL;
  }
}

static bool timeline_frame_target_matches(TimelinePropertyTargetKind kind,
                                          const char *target_id) {
  const char *prefix = timeline_frame_target_prefix(kind);
  size_t prefix_length;
  if (!prefix || !timeline_frame_id_is_valid(target_id))
    return false;
  prefix_length = strlen(prefix);
  return strncmp(target_id, prefix, prefix_length) == 0 &&
         target_id[prefix_length] != '\0';
}

static bool timeline_frame_context_matches_canonical(
    const TimelineEvaluationContext *context) {
  TimelineEvaluationContext canonical;
  if (!context || TimelineEvaluationContextBuild(context->rate, context->range,
                                                 context->sample, &canonical) !=
                      TIMELINE_STATUS_OK) {
    return false;
  }
  return context->local_frame == canonical.local_frame &&
         context->subframe == canonical.subframe &&
         context->absolute_frame_position ==
             canonical.absolute_frame_position &&
         context->local_frame_position == canonical.local_frame_position &&
         context->absolute_time_seconds == canonical.absolute_time_seconds &&
         context->local_time_seconds == canonical.local_time_seconds &&
         context->duration_seconds == canonical.duration_seconds &&
         context->normalized_t == canonical.normalized_t;
}

static TimelineStatus timeline_frame_property_validate(
    const TimelinePropertyRegistry *registry,
    const TimelinePropertyEvaluationResult *property) {
  const TimelinePropertyDescriptor *descriptor = NULL;
  TimelineStatus status;
  if (!registry || !property)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  if (!property->track.valid || property->track.status != TIMELINE_STATUS_OK ||
      !timeline_frame_id_is_valid(property->track.track_id) ||
      !timeline_frame_id_is_valid(property->track.property_id) ||
      !timeline_frame_target_matches(property->target_kind,
                                     property->track.target_id)) {
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  }
  status = TimelinePropertyRegistryFind(registry, property->track.property_id,
                                        &descriptor);
  if (status != TIMELINE_STATUS_OK)
    return status;
  if (property->target_kind != descriptor->target_kind ||
      property->unit != descriptor->unit ||
      property->access != descriptor->access ||
      property->invalidation_domains != descriptor->invalidation_domains ||
      property->track.source != TIMELINE_CHANNEL_SOURCE_AUTHORED) {
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  }
  return TimelinePropertyDescriptorValidateValue(descriptor,
                                                 property->track.value);
}

TimelineStatus
TimelineFrameSnapshotValidate(const TimelinePropertyRegistry *registry,
                              const TimelineFrameSnapshot *snapshot) {
  uint32_t invalidation_domains = TIMELINE_INVALIDATION_NONE;
  TimelineStatus status;
  if (!registry || !snapshot)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  if (!timeline_frame_context_matches_canonical(&snapshot->context) ||
      snapshot->property_count > TIMELINE_FRAME_SNAPSHOT_PROPERTY_CAPACITY) {
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  }
  for (size_t i = 0u; i < snapshot->property_count; ++i) {
    status =
        timeline_frame_property_validate(registry, &snapshot->properties[i]);
    if (status != TIMELINE_STATUS_OK)
      return status;
    invalidation_domains |= snapshot->properties[i].invalidation_domains;
    for (size_t j = i + 1u; j < snapshot->property_count; ++j) {
      if (strcmp(snapshot->properties[i].track.target_id,
                 snapshot->properties[j].track.target_id) == 0 &&
          strcmp(snapshot->properties[i].track.property_id,
                 snapshot->properties[j].track.property_id) == 0) {
        return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
      }
    }
  }
  if (invalidation_domains != snapshot->invalidation_domains) {
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  }
  return TIMELINE_STATUS_OK;
}

TimelineStatus
TimelineFrameSnapshotBuild(const TimelinePropertyRegistry *registry,
                           const TimelineDocument *document,
                           const TimelineEvaluationContext *context,
                           TimelineFrameSnapshot *out_snapshot) {
  TimelineFrameSnapshot candidate;
  size_t property_count = 0u;
  TimelineStatus status;
  if (!registry || !document || !context || !out_snapshot) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  memset(&candidate, 0, sizeof(candidate));
  status = TimelinePropertyRegistryEvaluateDocument(
      registry, document, context, candidate.properties,
      TIMELINE_FRAME_SNAPSHOT_PROPERTY_CAPACITY, &property_count);
  if (status != TIMELINE_STATUS_OK)
    return status;
  candidate.context = *context;
  candidate.property_count = property_count;
  for (size_t i = 0u; i < property_count; ++i) {
    candidate.invalidation_domains |=
        candidate.properties[i].invalidation_domains;
  }
  status = TimelineFrameSnapshotValidate(registry, &candidate);
  if (status != TIMELINE_STATUS_OK)
    return status;
  *out_snapshot = candidate;
  return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineFrameSnapshotApplyToCopy(
    const TimelinePropertyRegistry *registry,
    const TimelineFrameSnapshot *snapshot, const void *base_scene,
    void *scratch_scene, size_t scene_size,
    const TimelineFrameApplicationAdapter *adapter, void *user_data,
    void *out_scene, TimelineFrameApplicationReport *out_report) {
  TimelineFrameApplicationReport report;
  TimelineStatus status;
  if (!registry || !snapshot || !base_scene || !scratch_scene ||
      scene_size == 0u || scratch_scene == base_scene ||
      scratch_scene == out_scene || !adapter || !adapter->apply_property ||
      !adapter->validate_scene || !out_scene || !out_report) {
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  }
  status = TimelineFrameSnapshotValidate(registry, snapshot);
  if (status != TIMELINE_STATUS_OK)
    return status;
  memcpy(scratch_scene, base_scene, scene_size);
  for (size_t i = 0u; i < snapshot->property_count; ++i) {
    status = adapter->apply_property(scratch_scene, &snapshot->properties[i],
                                     user_data);
    if (status != TIMELINE_STATUS_OK)
      return status;
  }
  status = adapter->validate_scene(scratch_scene, user_data);
  if (status != TIMELINE_STATUS_OK)
    return status;
  memset(&report, 0, sizeof(report));
  report.context = snapshot->context;
  report.applied_property_count = snapshot->property_count;
  report.invalidation_domains = snapshot->invalidation_domains;
  report.scene_changed = snapshot->property_count > 0u;
  memcpy(out_scene, scratch_scene, scene_size);
  *out_report = report;
  return TIMELINE_STATUS_OK;
}
