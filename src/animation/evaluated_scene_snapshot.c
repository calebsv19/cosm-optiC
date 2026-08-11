#include "animation/evaluated_scene_snapshot.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAY_EVALUATED_SUBFRAME_DENOMINATOR 1000000u

static uint64_t ray_hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    size_t i = 0u;
    for (i = 0u; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t ray_hash_string(uint64_t hash, const char* value, size_t capacity) {
    size_t length = value ? strnlen(value, capacity) : 0u;
    hash = ray_hash_bytes(hash, &length, sizeof(length));
    return value ? ray_hash_bytes(hash, value, length) : hash;
}

static uint64_t ray_hash_timeline_value(uint64_t hash, TimelineValue value) {
    hash = ray_hash_bytes(hash, &value.type, sizeof(value.type));
    if (value.type == TIMELINE_VALUE_SCALAR) {
        return ray_hash_bytes(hash, &value.as.scalar, sizeof(value.as.scalar));
    }
    if (value.type == TIMELINE_VALUE_VEC3) {
        hash = ray_hash_bytes(hash, &value.as.vec3.x, sizeof(value.as.vec3.x));
        hash = ray_hash_bytes(hash, &value.as.vec3.y, sizeof(value.as.vec3.y));
        hash = ray_hash_bytes(hash, &value.as.vec3.z, sizeof(value.as.vec3.z));
    }
    return hash;
}

static bool ray_evaluated_vec3_finite(TimelineVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool ray_evaluated_id_valid(const char* value, size_t capacity) {
    return value && value[0] && strnlen(value, capacity) < capacity;
}

const char* RayEvaluatedSceneSourceLabel(RayEvaluatedSceneSource source) {
    switch (source) {
        case RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE: return "authored";
        case RAY_EVALUATED_SCENE_SOURCE_LEGACY_PREVIEW_FALLBACK: return "legacy-fallback";
        case RAY_EVALUATED_SCENE_SOURCE_NONE:
        default: return "none";
    }
}

const char* RayEvaluatedPlaybackModeLabel(RayEvaluatedPlaybackMode mode) {
    switch (mode) {
        case RAY_EVALUATED_PLAYBACK_LOOP: return "Loop";
        case RAY_EVALUATED_PLAYBACK_BOUNCE: return "Bounce";
        case RAY_EVALUATED_PLAYBACK_STOP:
        default: return "Stop";
    }
}

TimelineStatus RayEvaluatedTimelineSampleFromElapsed(
    TimelineRate rate,
    TimelineRange range,
    double elapsed_seconds,
    RayEvaluatedPlaybackMode mode,
    TimelineSample* out_sample,
    bool* out_reverse_direction,
    bool* out_clamped) {
    double fps = 0.0;
    double local_position = 0.0;
    double max_local = 0.0;
    double phase = 0.0;
    double fractional = 0.0;
    uint64_t local_frame = 0u;
    TimelineSample sample = {0};
    bool reverse_direction = false;
    bool clamped = false;

    if (!out_sample || !TimelineRateIsValid(rate) || !TimelineRangeIsValid(range) ||
        !isfinite(elapsed_seconds)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (elapsed_seconds < 0.0) elapsed_seconds = 0.0;
    fps = (double)rate.frames_per_second_numerator /
          (double)rate.frames_per_second_denominator;
    max_local = (double)(range.frame_count - 1u);
    local_position = elapsed_seconds * fps;

    if (mode == RAY_EVALUATED_PLAYBACK_BOUNCE && max_local > 0.0) {
        phase = fmod(local_position, max_local * 2.0);
        if (phase < 0.0) phase += max_local * 2.0;
        if (phase > max_local) {
            local_position = (max_local * 2.0) - phase;
            reverse_direction = true;
        } else {
            local_position = phase;
        }
    } else if (mode == RAY_EVALUATED_PLAYBACK_LOOP && range.frame_count > 0u) {
        local_position = fmod(local_position, (double)range.frame_count);
        if (local_position < 0.0) local_position += (double)range.frame_count;
    } else {
        if (local_position > max_local) {
            local_position = max_local;
            clamped = true;
        }
    }

    local_frame = (uint64_t)floor(local_position);
    fractional = local_position - (double)local_frame;
    sample.absolute_frame = range.start_frame + (int64_t)local_frame;
    sample.subframe_denominator = RAY_EVALUATED_SUBFRAME_DENOMINATOR;
    sample.subframe_numerator =
        (uint32_t)llround(fractional * (double)RAY_EVALUATED_SUBFRAME_DENOMINATOR);
    if (sample.subframe_numerator >= sample.subframe_denominator) {
        sample.subframe_numerator = 0u;
        if (local_frame + 1u < range.frame_count) {
            sample.absolute_frame += 1;
        }
    }
    *out_sample = sample;
    if (out_reverse_direction) *out_reverse_direction = reverse_direction;
    if (out_clamped) *out_clamped = clamped;
    return TIMELINE_STATUS_OK;
}

TimelineStatus RayEvaluatedSceneSnapshotValidate(
    const RayEvaluatedSceneSnapshot* snapshot) {
    TimelineEvaluationContext rebuilt;
    TimelineStatus status;
    size_t i = 0u;
    if (!snapshot || !snapshot->valid ||
        snapshot->schema_version != RAY_EVALUATED_SCENE_SNAPSHOT_SCHEMA_VERSION ||
        snapshot->source == RAY_EVALUATED_SCENE_SOURCE_NONE ||
        !snapshot->light.valid || !snapshot->camera.valid ||
        !ray_evaluated_vec3_finite(snapshot->light.position) ||
        !isfinite(snapshot->light.intensity) ||
        snapshot->light.intensity < 0.0 ||
        !ray_evaluated_vec3_finite(snapshot->camera.position) ||
        snapshot->object_transform_count >
            RAY_EVALUATED_OBJECT_TRANSFORM_CAPACITY) {
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    }
    status = TimelineEvaluationContextBuild(snapshot->frame.rate,
                                            snapshot->frame.range,
                                            snapshot->frame.sample,
                                            &rebuilt);
    if (status != TIMELINE_STATUS_OK ||
        !TimelineEvaluationContextsReferToSameSample(&snapshot->frame, &rebuilt)) {
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    }
    if (snapshot->source == RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE &&
        (!snapshot->light.path_progress_provenance.valid ||
         !snapshot->light.property_provenance.valid ||
         strcmp(snapshot->light.path_progress_provenance.property_id,
                "light/path_progress") != 0 ||
         strcmp(snapshot->light.property_provenance.property_id,
                "light/path_progress") != 0 ||
         (snapshot->light.intensity_authored &&
          (!snapshot->light.intensity_provenance.valid ||
           strcmp(snapshot->light.intensity_provenance.property_id,
                  "light/intensity") != 0)))) {
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    }
    if (snapshot->simulation.source == RAY_EVALUATED_SIMULATION_NONE &&
        snapshot->simulation.valid) {
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    }
    for (i = 0u; i < snapshot->object_transform_count; ++i) {
        const RayEvaluatedObjectTransform* transform =
            &snapshot->object_transforms[i];
        size_t prior = 0u;
        if (!transform->valid ||
            transform->source == RAY_EVALUATED_OBJECT_TRANSFORM_NONE ||
            !ray_evaluated_id_valid(transform->target_id,
                                    sizeof(transform->target_id)) ||
            (!transform->has_position && !transform->has_rotation) ||
            !ray_evaluated_vec3_finite(transform->position) ||
            !ray_evaluated_vec3_finite(transform->rotation_radians) ||
            !TimelineEvaluationContextsReferToSameSample(
                &snapshot->frame, &transform->frame)) {
            return TIMELINE_STATUS_INVALID_SNAPSHOT;
        }
        for (prior = 0u; prior < i; ++prior) {
            if (strcmp(snapshot->object_transforms[prior].target_id,
                       transform->target_id) == 0) {
                return TIMELINE_STATUS_INVALID_SNAPSHOT;
            }
        }
    }
    if (snapshot->simulation.source == RAY_EVALUATED_SIMULATION_CACHE &&
        (!snapshot->simulation.valid ||
         !ray_evaluated_id_valid(snapshot->simulation.cache_id,
                                 sizeof(snapshot->simulation.cache_id)) ||
         snapshot->simulation.frame_stride == 0u ||
         !TimelineRateIsValid(snapshot->simulation.source_rate) ||
         snapshot->simulation.subframe_denominator == 0u ||
         snapshot->simulation.subframe_numerator >=
             snapshot->simulation.subframe_denominator ||
         !ray_evaluated_id_valid(snapshot->simulation.content_digest,
                                 sizeof(snapshot->simulation.content_digest)))) {
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus RayEvaluatedSceneSnapshotBuild(
    const RayEvaluatedSceneSnapshotInputs* inputs,
    RayEvaluatedSceneSnapshot* out_snapshot) {
    RayEvaluatedSceneSnapshot snapshot;
    TimelineStatus status;
    if (!inputs || !out_snapshot) return TIMELINE_STATUS_INVALID_ARGUMENT;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.schema_version = RAY_EVALUATED_SCENE_SNAPSHOT_SCHEMA_VERSION;
    snapshot.valid = true;
    snapshot.source = inputs->source;
    snapshot.playback_mode = inputs->playback_mode;
    snapshot.reverse_direction = inputs->reverse_direction;
    snapshot.clamped = inputs->clamped;
    snapshot.frame = inputs->frame;
    snapshot.identity = inputs->identity;
    snapshot.light = inputs->light;
    if (!snapshot.light.path_progress_provenance.valid &&
        snapshot.light.property_provenance.valid) {
        snapshot.light.path_progress_provenance =
            snapshot.light.property_provenance;
    }
    if (!snapshot.light.property_provenance.valid &&
        snapshot.light.path_progress_provenance.valid) {
        snapshot.light.property_provenance =
            snapshot.light.path_progress_provenance;
    }
    if (snapshot.light.path_progress_provenance.valid &&
        !snapshot.light.path_progress_provenance.property_id[0]) {
        snprintf(snapshot.light.path_progress_provenance.property_id,
                 sizeof(snapshot.light.path_progress_provenance.property_id),
                 "light/path_progress");
    }
    if (snapshot.light.property_provenance.valid &&
        !snapshot.light.property_provenance.property_id[0]) {
        snprintf(snapshot.light.property_provenance.property_id,
                 sizeof(snapshot.light.property_provenance.property_id),
                 "light/path_progress");
    }
    if (snapshot.light.intensity_authored &&
        snapshot.light.intensity_provenance.valid &&
        !snapshot.light.intensity_provenance.property_id[0]) {
        snprintf(snapshot.light.intensity_provenance.property_id,
                 sizeof(snapshot.light.intensity_provenance.property_id),
                 "light/intensity");
    }
    snapshot.camera = inputs->camera;
    if (inputs->object_transform_count >
            RAY_EVALUATED_OBJECT_TRANSFORM_CAPACITY ||
        (inputs->object_transform_count > 0u && !inputs->object_transforms)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    snapshot.object_transform_count = inputs->object_transform_count;
    if (snapshot.object_transform_count > 0u) {
        memcpy(snapshot.object_transforms, inputs->object_transforms,
               snapshot.object_transform_count *
                   sizeof(snapshot.object_transforms[0]));
    }
    snapshot.simulation = inputs->simulation;
    snapshot.invalidation_domains = inputs->invalidation_domains;
    snprintf(snapshot.diagnostics, sizeof(snapshot.diagnostics), "%s",
             inputs->diagnostics ? inputs->diagnostics : "");
    status = RayEvaluatedSceneSnapshotValidate(&snapshot);
    if (status != TIMELINE_STATUS_OK) return status;
    *out_snapshot = snapshot;
    return TIMELINE_STATUS_OK;
}

uint64_t RayEvaluatedTimelineFingerprint(const TimelineDocument* timeline,
                                         const Path* spatial_path,
                                         const CameraPath3D* spatial_path_3d) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i = 0u;
    if (!timeline) return 0u;
    hash = ray_hash_bytes(hash, &timeline->rate, sizeof(timeline->rate));
    hash = ray_hash_bytes(hash, &timeline->range, sizeof(timeline->range));
    hash = ray_hash_bytes(hash, &timeline->track_count, sizeof(timeline->track_count));
    for (i = 0u; i < timeline->track_count; ++i) {
        const TimelineTrack* track = &timeline->tracks[i];
        size_t key_index = 0u;
        hash = ray_hash_string(hash, track->track_id, sizeof(track->track_id));
        hash = ray_hash_string(hash, track->target_id, sizeof(track->target_id));
        hash = ray_hash_string(hash, track->property_id, sizeof(track->property_id));
        hash = ray_hash_bytes(hash, &track->value_type, sizeof(track->value_type));
        hash = ray_hash_bytes(hash, &track->unit, sizeof(track->unit));
        hash = ray_hash_bytes(hash, &track->source, sizeof(track->source));
        hash = ray_hash_bytes(hash, &track->enabled, sizeof(track->enabled));
        hash = ray_hash_bytes(hash, &track->key_count, sizeof(track->key_count));
        for (key_index = 0u; key_index < track->key_count; ++key_index) {
            const TimelineKeyframe* key = &track->keys[key_index];
            hash = ray_hash_bytes(hash, &key->frame, sizeof(key->frame));
            hash = ray_hash_timeline_value(hash, key->value);
            hash = ray_hash_bytes(hash, &key->interpolation_to_next,
                                  sizeof(key->interpolation_to_next));
            hash = ray_hash_bytes(hash, &key->incoming_frame_offset,
                                  sizeof(key->incoming_frame_offset));
            hash = ray_hash_bytes(hash, &key->incoming_value_offset,
                                  sizeof(key->incoming_value_offset));
            hash = ray_hash_bytes(hash, &key->outgoing_frame_offset,
                                  sizeof(key->outgoing_frame_offset));
            hash = ray_hash_bytes(hash, &key->outgoing_value_offset,
                                  sizeof(key->outgoing_value_offset));
        }
    }
    if (spatial_path) {
        hash = ray_hash_bytes(hash, &spatial_path->mode,
                              sizeof(spatial_path->mode));
        hash = ray_hash_bytes(hash, &spatial_path->numPoints,
                              sizeof(spatial_path->numPoints));
        for (i = 0u; i < (size_t)spatial_path->numPoints; ++i) {
            hash = ray_hash_bytes(hash, &spatial_path->points[i].x,
                                  sizeof(spatial_path->points[i].x));
            hash = ray_hash_bytes(hash, &spatial_path->points[i].y,
                                  sizeof(spatial_path->points[i].y));
            if (i + 1u < (size_t)spatial_path->numPoints) {
                hash = ray_hash_bytes(hash, &spatial_path->handles[i][0].vx,
                                      sizeof(spatial_path->handles[i][0].vx));
                hash = ray_hash_bytes(hash, &spatial_path->handles[i][0].vy,
                                      sizeof(spatial_path->handles[i][0].vy));
                hash = ray_hash_bytes(hash, &spatial_path->handles[i][1].vx,
                                      sizeof(spatial_path->handles[i][1].vx));
                hash = ray_hash_bytes(hash, &spatial_path->handles[i][1].vy,
                                      sizeof(spatial_path->handles[i][1].vy));
            }
        }
    }
    if (spatial_path_3d && spatial_path) {
        for (i = 0u; i < (size_t)spatial_path->numPoints; ++i) {
            hash = ray_hash_bytes(hash, &spatial_path_3d->point_z[i],
                                  sizeof(spatial_path_3d->point_z[i]));
            if (i + 1u < (size_t)spatial_path->numPoints) {
                hash = ray_hash_bytes(hash, &spatial_path_3d->handles_vz[i][0],
                                      sizeof(spatial_path_3d->handles_vz[i][0]));
                hash = ray_hash_bytes(hash, &spatial_path_3d->handles_vz[i][1],
                                      sizeof(spatial_path_3d->handles_vz[i][1]));
            }
        }
    }
    return hash;
}
