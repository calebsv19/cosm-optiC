#include "app/evaluated_scene_service.h"

#include "animation/timeline_property_registry.h"
#include "app/preview_camera_sample.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "render/runtime_native_3d_prepare_cache.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static RayEvaluatedPlaybackMode ray_evaluated_playback_mode(void) {
    if (animSettings.bounceMode) return RAY_EVALUATED_PLAYBACK_BOUNCE;
    if (strcmp(animSettings.loopMode, "loop") == 0) {
        return RAY_EVALUATED_PLAYBACK_LOOP;
    }
    return RAY_EVALUATED_PLAYBACK_STOP;
}

static TimelineVec3 ray_evaluated_vec3(Vec3 value) {
    TimelineVec3 result = {value.x, value.y, value.z};
    return result;
}

static void ray_evaluated_copy_light_base(
    const RuntimeLightSource3D* source,
    const char* target_id,
    RayEvaluatedLight* out_light) {
    if (!source || !out_light) return;
    memset(out_light, 0, sizeof(*out_light));
    out_light->valid = true;
    out_light->enabled = source->enabled;
    snprintf(out_light->target_id, sizeof(out_light->target_id), "%s",
             target_id ? target_id : "");
    snprintf(out_light->runtime_light_id, sizeof(out_light->runtime_light_id),
             "%s", source->id);
    out_light->kind = (int)source->kind;
    out_light->origin = (int)source->origin;
    out_light->emission_profile = (int)source->emissionProfile;
    out_light->position = ray_evaluated_vec3(source->position);
    out_light->axis_u = ray_evaluated_vec3(source->axisU);
    out_light->axis_v = ray_evaluated_vec3(source->axisV);
    out_light->normal = ray_evaluated_vec3(source->normal);
    out_light->radius = source->radius;
    out_light->width = source->width;
    out_light->height = source->height;
    out_light->color = ray_evaluated_vec3(source->color);
    out_light->intensity = source->intensity;
    out_light->radiometry_mode = (int)source->radiometryMode;
    out_light->radiance = source->radiance;
    out_light->falloff_distance = source->falloffDistance;
    out_light->falloff_mode = (int)source->falloffMode;
}

static bool ray_evaluated_capture_camera(double normalized_t,
                                         RayEvaluatedCamera* out_camera) {
    PreviewCameraSample sample = {0};
    if (!out_camera ||
        !PreviewCameraSampleEvaluate(&sceneSettings.camera,
                                     sceneSettings.cameraZ,
                                     &sceneSettings.cameraPath,
                                     &sceneSettings.cameraPath3D,
                                     normalized_t,
                                     sceneSettings.windowWidth,
                                     sceneSettings.windowHeight,
                                     &sample)) {
        return false;
    }
    memset(out_camera, 0, sizeof(*out_camera));
    out_camera->valid = sample.valid;
    out_camera->uses_authored_path = sample.uses_authored_path;
    out_camera->position.x = sample.position_x;
    out_camera->position.y = sample.position_y;
    out_camera->position.z = sample.position_z;
    out_camera->yaw_radians = sample.yaw_radians;
    out_camera->pitch_radians = sample.pitch_radians;
    out_camera->fov_y_degrees = sample.fov_y_degrees;
    out_camera->aspect_ratio = sample.aspect_ratio;
    out_camera->zoom = sceneSettings.camera.zoom;
    return out_camera->valid;
}

static void ray_evaluated_fail(RayEvaluatedSceneServiceResult* result,
                               TimelineStatus status,
                               const char* message) {
    memset(result, 0, sizeof(*result));
    result->status = status;
    snprintf(result->status_line, sizeof(result->status_line),
             "Evaluated scene unavailable: %s (%s)",
             message ? message : "unknown error", TimelineStatusLabel(status));
}

static TimelineStatus ray_evaluated_authored_context(
    const RuntimeSceneLightTimelineDocument* document,
    TimelineSample sample,
    TimelineEvaluationContext* out_context) {
    if (!document || !document->valid || !out_context) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    return TimelineEvaluationContextBuild(document->timeline.rate,
                                          document->timeline.range,
                                          sample,
                                          out_context);
}

static bool ray_evaluated_build_authored(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineEvaluationContext* context,
    RayEvaluatedPlaybackMode playback_mode,
    bool reverse_direction,
    bool clamped,
    RayEvaluatedSceneServiceResult* out_result) {
    RuntimeSceneBridge3DLightSeedState light_state = {0};
    RuntimeSceneLightTimelineTarget target = {0};
    RuntimeNative3DPreparedSceneCacheStats cache_stats = {0};
    TimelineLightMotionSample motion = {0};
    TimelineEvaluationResult provenance = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    const TimelineTrack* progress_track = NULL;
    TimelineStatus status;

    if (!document || !context || !out_result ||
        document->progress_track_index >= document->timeline.track_count) {
        return false;
    }
    progress_track = &document->timeline.tracks[
        document->progress_track_index];
    status = TimelineLightMotionEvaluate(progress_track,
                                         &document->spatial_path,
                                         &document->spatial_path_3d,
                                         context,
                                         &motion);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "light motion evaluation failed");
        return false;
    }
    status = TimelineTrackEvaluate(progress_track, context, &provenance);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "property provenance evaluation failed");
        return false;
    }
    runtime_scene_bridge_get_last_3d_light_seed_state(&light_state);
    if (!light_state.valid || light_state.light_count <= 0) {
        ray_evaluated_fail(out_result, TIMELINE_STATUS_TARGET_NOT_FOUND,
                           "runtime light seed state is unavailable");
        return false;
    }
    status = RuntimeSceneLightTimelineResolveTarget(
        light_state.lights, (size_t)light_state.light_count, motion.target_id, &target);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "authored light target resolution failed");
        return false;
    }
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&cache_stats);
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.playback_mode = playback_mode;
    inputs.reverse_direction = reverse_direction;
    inputs.clamped = clamped;
    inputs.frame = *context;
    inputs.identity.scene_revision = cache_stats.generation;
    inputs.identity.timeline_revision = RayEvaluatedTimelineFingerprint(
        &document->timeline, &document->spatial_path, &document->spatial_path_3d);
    ray_evaluated_copy_light_base(&light_state.lights[target.light_index],
                                  motion.target_id, &inputs.light);
    inputs.light.position = motion.position;
    inputs.light.progress = motion.progress;
    inputs.light.progress_per_frame = motion.progress_per_frame;
    inputs.light.path_length_world = motion.path_length_world;
    inputs.light.world_speed_per_second = motion.world_speed_per_second;
    inputs.light.global_path_t = motion.global_path_t;
    inputs.light.speed_valid = motion.speed_valid;
    inputs.light.property_provenance = provenance;
    if (!ray_evaluated_capture_camera(context->normalized_t, &inputs.camera)) {
        ray_evaluated_fail(out_result, TIMELINE_STATUS_INVALID_SNAPSHOT,
                           "camera evaluation failed");
        return false;
    }
    inputs.simulation.source = RAY_EVALUATED_SIMULATION_NONE;
    inputs.simulation.valid = false;
    inputs.invalidation_domains = motion.invalidation_domains;
    inputs.diagnostics = "immutable authored evaluated-scene snapshot";
    status = RayEvaluatedSceneSnapshotBuild(&inputs, &out_result->snapshot);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "snapshot validation failed");
        return false;
    }
    out_result->valid = true;
    out_result->status = TIMELINE_STATUS_OK;
    snprintf(out_result->status_line, sizeof(out_result->status_line),
             "Frame %lld+%u/%u %s %s traversal=authored progress=%.3f speed=%.3f world/s",
             (long long)context->sample.absolute_frame,
             context->sample.subframe_numerator,
             context->sample.subframe_denominator,
             RayEvaluatedPlaybackModeLabel(playback_mode),
             RayEvaluatedSceneSourceLabel(inputs.source),
             motion.progress,
             motion.speed_valid ? motion.world_speed_per_second : 0.0);
    return true;
}

static bool ray_evaluated_build_legacy(
    const TimelineEvaluationContext* context,
    RayEvaluatedPlaybackMode playback_mode,
    bool reverse_direction,
    bool clamped,
    RayEvaluatedSceneServiceResult* out_result) {
    RuntimeSceneBridge3DLightSeedState light_state = {0};
    RuntimeNative3DPreparedSceneCacheStats cache_stats = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    Point light_point = {0};
    double light_z = 0.0;
    TimelineStatus status;

    if (!context || !out_result) return false;
    runtime_scene_bridge_get_last_3d_light_seed_state(&light_state);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&cache_stats);
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_LEGACY_PREVIEW_FALLBACK;
    inputs.playback_mode = playback_mode;
    inputs.reverse_direction = reverse_direction;
    inputs.clamped = clamped;
    inputs.frame = *context;
    inputs.identity.scene_revision = cache_stats.generation;
    if (light_state.valid && light_state.light_count > 0) {
        ray_evaluated_copy_light_base(&light_state.lights[0],
                                      "legacy/first-light", &inputs.light);
    } else {
        inputs.light.valid = true;
        inputs.light.enabled = true;
        snprintf(inputs.light.target_id, sizeof(inputs.light.target_id),
                 "legacy/compatibility-light");
        snprintf(inputs.light.runtime_light_id,
                 sizeof(inputs.light.runtime_light_id), "compatibility-light");
        inputs.light.color.x = 1.0;
        inputs.light.color.y = 1.0;
        inputs.light.color.z = 1.0;
        inputs.light.intensity = animSettings.lightIntensity;
        inputs.light.radius = animSettings.lightRadius;
        inputs.light.falloff_distance = animSettings.forwardDecay;
        inputs.light.falloff_mode = (int)animSettings.forwardFalloffMode;
    }
    light_point = (sceneSettings.bezierPath.numPoints >= 2)
                      ? GetPositionAlongPathNormalized(&sceneSettings.bezierPath,
                                                       context->normalized_t)
                      : sceneSettings.bezierPath.points[0];
    light_z = (sceneSettings.bezierPath.numPoints >= 2)
                  ? CameraPath3D_GetPositionZNormalized(
                        &sceneSettings.bezierPath, &sceneSettings.bezierPath3D,
                        context->normalized_t)
                  : sceneSettings.bezierPath3D.point_z[0];
    inputs.light.position.x = light_point.x;
    inputs.light.position.y = light_point.y;
    inputs.light.position.z = light_z;
    inputs.light.progress = context->normalized_t;
    inputs.light.global_path_t = context->normalized_t;
    if (!ray_evaluated_capture_camera(context->normalized_t, &inputs.camera)) {
        ray_evaluated_fail(out_result, TIMELINE_STATUS_INVALID_SNAPSHOT,
                           "legacy camera evaluation failed");
        return false;
    }
    inputs.simulation.source = RAY_EVALUATED_SIMULATION_NONE;
    inputs.simulation.valid = false;
    inputs.invalidation_domains = TIMELINE_INVALIDATION_LIGHTING |
                                  TIMELINE_INVALIDATION_CAMERA;
    inputs.diagnostics =
        "explicit legacy Preview fallback; no authored light timeline present";
    status = RayEvaluatedSceneSnapshotBuild(&inputs, &out_result->snapshot);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "legacy snapshot validation failed");
        return false;
    }
    out_result->valid = true;
    out_result->status = TIMELINE_STATUS_OK;
    snprintf(out_result->status_line, sizeof(out_result->status_line),
             "Frame %lld+%u/%u %s legacy-fallback traversal=legacy-normalized progress=%.3f",
             (long long)context->sample.absolute_frame,
             context->sample.subframe_numerator,
             context->sample.subframe_denominator,
             RayEvaluatedPlaybackModeLabel(playback_mode),
             context->normalized_t);
    return true;
}

bool RayEvaluatedSceneCaptureAuthoredSample(
    TimelineSample sample,
    RayEvaluatedSceneServiceResult* out_result) {
    RuntimeSceneLightTimelineDocument document = {0};
    TimelineEvaluationContext context = {0};
    TimelineStatus status;
    if (!out_result) return false;
    memset(out_result, 0, sizeof(*out_result));
    if (!RuntimeSceneLightTimelineGetLast(&document)) {
        ray_evaluated_fail(out_result, TIMELINE_STATUS_TARGET_NOT_FOUND,
                           "no authored light timeline is active");
        return false;
    }
    status = ray_evaluated_authored_context(&document, sample, &context);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "authored frame context is invalid");
        return false;
    }
    return ray_evaluated_build_authored(&document, &context,
                                        RAY_EVALUATED_PLAYBACK_STOP,
                                        false, false, out_result);
}

bool RayEvaluatedSceneCaptureForElapsed(
    double elapsed_seconds,
    RayEvaluatedSceneServiceResult* out_result) {
    RuntimeSceneLightTimelineDocument document = {0};
    TimelineRate rate = {0};
    TimelineRange range = {0};
    TimelineSample sample = {0};
    TimelineEvaluationContext context = {0};
    RayEvaluatedPlaybackMode mode = ray_evaluated_playback_mode();
    bool reverse_direction = false;
    bool clamped = false;
    TimelineStatus status;

    if (!out_result) return false;
    memset(out_result, 0, sizeof(*out_result));
    if (RuntimeSceneLightTimelineGetLast(&document)) {
        rate = document.timeline.rate;
        range = document.timeline.range;
    } else {
        rate.frames_per_second_numerator =
            (uint32_t)(animSettings.fps > 0 ? animSettings.fps : 30);
        rate.frames_per_second_denominator = 1u;
        range.start_frame = animSettings.startFrameIndex;
        range.frame_count =
            (uint64_t)(animSettings.framesForTravel > 0
                           ? animSettings.framesForTravel
                           : 1);
    }
    status = RayEvaluatedTimelineSampleFromElapsed(
        rate, range, elapsed_seconds, mode, &sample,
        &reverse_direction, &clamped);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "elapsed time mapping failed");
        return false;
    }
    status = TimelineEvaluationContextBuild(rate, range, sample, &context);
    if (status != TIMELINE_STATUS_OK) {
        ray_evaluated_fail(out_result, status, "frame context construction failed");
        return false;
    }
    if (document.valid) {
        return ray_evaluated_build_authored(&document, &context, mode,
                                            reverse_direction, clamped, out_result);
    }
    return ray_evaluated_build_legacy(&context, mode, reverse_direction,
                                      clamped, out_result);
}
