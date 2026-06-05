#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/agent_render_request.h"
#include "config/config_file_io.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_volume_3d_debug.h"
#include "render/runtime_volume_3d_scatter.h"
#include "tools/make_video.h"
#include "tools/ray_tracing_render_headless_internal.h"

static RayTracingHeadlessObjectAuditEntry *ray_tracing_headless_object_audit_ensure_entry(
    RayTracingHeadlessPreflight *preflight,
    int scene_object_index) {
    RayTracingHeadlessObjectAuditEntry *free_entry = NULL;
    char object_id[64] = {0};
    if (!preflight || scene_object_index < 0) return NULL;
    for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
        RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
        if (entry->used && entry->scene_object_index == scene_object_index) {
            return entry;
        }
        if (!entry->used && !free_entry) {
            free_entry = entry;
        }
    }
    if (!free_entry) return NULL;
    memset(free_entry, 0, sizeof(*free_entry));
    free_entry->used = true;
    free_entry->scene_object_index = scene_object_index;
    if (runtime_scene_bridge_get_last_object_id_for_scene_index(scene_object_index,
                                                                object_id,
                                                                sizeof(object_id))) {
        snprintf(free_entry->object_id, sizeof(free_entry->object_id), "%s", object_id);
    }
    if (scene_object_index >= 0 && scene_object_index < sceneSettings.objectCount) {
        snprintf(free_entry->object_type,
                 sizeof(free_entry->object_type),
                 "%s",
                 sceneSettings.sceneObjects[scene_object_index].type);
        free_entry->material_id = sceneSettings.sceneObjects[scene_object_index].material_id;
        free_entry->alpha = sceneSettings.sceneObjects[scene_object_index].alpha;
        free_entry->reflectivity = sceneSettings.sceneObjects[scene_object_index].reflectivity;
        free_entry->roughness = sceneSettings.sceneObjects[scene_object_index].roughness;
        free_entry->emissive_strength =
            sceneSettings.sceneObjects[scene_object_index].emissiveStrength;
        free_entry->texture_id = sceneSettings.sceneObjects[scene_object_index].textureId;
        free_entry->texture_strength =
            sceneSettings.sceneObjects[scene_object_index].textureStrength;
        free_entry->texture_scale = sceneSettings.sceneObjects[scene_object_index].textureScale;
        free_entry->texture_offset_u =
            sceneSettings.sceneObjects[scene_object_index].textureOffsetU;
        free_entry->texture_offset_v =
            sceneSettings.sceneObjects[scene_object_index].textureOffsetV;
        free_entry->texture_seed = sceneSettings.sceneObjects[scene_object_index].textureSeed;
        free_entry->texture_pattern_mode =
            sceneSettings.sceneObjects[scene_object_index].texturePatternMode;
        free_entry->texture_coverage =
            sceneSettings.sceneObjects[scene_object_index].textureCoverage;
        free_entry->texture_grain =
            sceneSettings.sceneObjects[scene_object_index].textureGrain;
        free_entry->texture_edge_softness =
            sceneSettings.sceneObjects[scene_object_index].textureEdgeSoftness;
        free_entry->texture_contrast =
            sceneSettings.sceneObjects[scene_object_index].textureContrast;
        free_entry->texture_flow =
            sceneSettings.sceneObjects[scene_object_index].textureFlow;
        free_entry->texture_color_depth =
            sceneSettings.sceneObjects[scene_object_index].textureColorDepth;
        free_entry->texture_surface_damage =
            sceneSettings.sceneObjects[scene_object_index].textureSurfaceDamage;
        free_entry->packed_color = sceneSettings.sceneObjects[scene_object_index].color;
    } else {
        free_entry->material_id = -1;
    }
    preflight->object_audit_count += 1;
    return free_entry;
}

static void ray_tracing_headless_object_audit_note_primitive_center(
    RayTracingHeadlessObjectAuditEntry *entry,
    const RuntimePrimitive3D *primitive) {
    Vec3 center = vec3(0.0, 0.0, 0.0);
    if (!entry || !primitive) return;
    switch (primitive->kind) {
        case RUNTIME_PRIMITIVE_3D_KIND_PLANE:
            center = primitive->shape.plane.origin;
            break;
        case RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM:
            center = primitive->shape.rectPrism.origin;
            break;
        default:
            return;
    }
    entry->center_x += center.x;
    entry->center_y += center.y;
    entry->center_z += center.z;
}

static void ray_tracing_headless_audit_prepared_frame(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame) {
    if (!preflight || !frame || !frame->valid) return;

    for (int i = 0; i < sceneSettings.objectCount; ++i) {
        (void)ray_tracing_headless_object_audit_ensure_entry(preflight, i);
    }

    for (int i = 0; i < frame->scene.primitiveCount; ++i) {
        const RuntimePrimitive3D *primitive = &frame->scene.primitives[i];
        RayTracingHeadlessObjectAuditEntry *entry =
            ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                           primitive->source.sceneObjectIndex);
        if (!entry) continue;
        entry->primitive_count += 1;
        ray_tracing_headless_object_audit_note_primitive_center(entry, primitive);
    }

    for (int i = 0; i < frame->scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &frame->scene.triangleMesh.triangles[i];
        RayTracingHeadlessObjectAuditEntry *entry =
            ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                           triangle->sceneObjectIndex);
        if (!entry) continue;
        entry->triangle_count += 1;
    }

    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) {
            HitInfo3D hit = {0};
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector,
                                                                (double)x,
                                                                (double)y);
            if (!RuntimeRay3D_TraceSceneFirstHit(&frame->scene, &ray, 1e-6, 1.0e9, &hit)) {
                continue;
            }
            if (hit.sceneObjectIndex < 0) continue;
            RayTracingHeadlessObjectAuditEntry *entry =
                ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                               hit.sceneObjectIndex);
            if (!entry) continue;
            entry->primary_hit_pixels += 1;
        }
    }

    for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
        RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
        Vec3 center = vec3(0.0, 0.0, 0.0);
        if (!entry->used || entry->primitive_count <= 0) continue;
        center.x = entry->center_x / (double)entry->primitive_count;
        center.y = entry->center_y / (double)entry->primitive_count;
        center.z = entry->center_z / (double)entry->primitive_count;
        entry->center_x = center.x;
        entry->center_y = center.y;
        entry->center_z = center.z;
        entry->center_projectable =
            RuntimeCameraProjector3D_ProjectPoint(&frame->projector,
                                                  center,
                                                  &entry->center_screen_x,
                                                  &entry->center_screen_y,
                                                  &entry->center_camera_depth,
                                                  &entry->center_inside_viewport);
    }
}

static size_t count_nonzero_pixels(const uint8_t *pixels,
                                   int width,
                                   int height,
                                   uint8_t *out_max_r,
                                   uint8_t *out_max_g,
                                   uint8_t *out_max_b) {
    size_t nonzero = 0u;
    uint8_t max_r = 0u;
    uint8_t max_g = 0u;
    uint8_t max_b = 0u;

    if (!pixels || width <= 0 || height <= 0) {
        if (out_max_r) *out_max_r = 0u;
        if (out_max_g) *out_max_g = 0u;
        if (out_max_b) *out_max_b = 0u;
        return 0u;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t base =
                ((size_t)y * (size_t)width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const uint8_t r = pixels[base];
            const uint8_t g = pixels[base + 1u];
            const uint8_t b = pixels[base + 2u];
            if (r > max_r) max_r = r;
            if (g > max_g) max_g = g;
            if (b > max_b) max_b = b;
            if (r > 0u || g > 0u || b > 0u) {
                nonzero += 1u;
            }
        }
    }

    if (out_max_r) *out_max_r = max_r;
    if (out_max_g) *out_max_g = max_g;
    if (out_max_b) *out_max_b = max_b;
    return nonzero;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s --request <request.json> [--preflight|--render] [--summary <summary.json>] [--job-id <id>] [--job-status <job_status.json>]\n",
            argv0 ? argv0 : "ray_tracing_render_headless");
}

static RuntimeVolume3DSourceKind runtime_volume_source_kind_from_request(int kind) {
    switch (animation_config_volume_source_kind_clamp(kind)) {
        case VOLUME_SOURCE_MANIFEST:
            return RUNTIME_VOLUME_3D_SOURCE_MANIFEST;
        case VOLUME_SOURCE_RAW_VF3D:
            return RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
        case VOLUME_SOURCE_PACK:
            return RUNTIME_VOLUME_3D_SOURCE_PACK;
        case VOLUME_SOURCE_NONE:
        default:
            return RUNTIME_VOLUME_3D_SOURCE_NONE;
    }
}

static bool build_volume_debug_summary(const RayTracingAgentRenderRequest *request,
                                       RuntimeVolumeDebugSummary3D *out_summary) {
    RuntimeVolumeAttachment3D attachment = {0};
    RuntimeVolume3DSourceKind source_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[128] = {0};
    bool ok = false;

    if (!request || !out_summary || !request->volume_enabled) return false;
    source_kind = runtime_volume_source_kind_from_request(request->volume_source_kind);
    if (source_kind == RUNTIME_VOLUME_3D_SOURCE_NONE || !request->volume_source_path[0]) {
        return false;
    }
    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = fluid_volume_import_3d_load_source(request->volume_source_path,
                                            source_kind,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics));
    if (ok) {
        ok = RuntimeVolumeDebugSummary3D_Build(&attachment, out_summary);
    }
    RuntimeVolumeAttachment3D_Reset(&attachment);
    return ok;
}

static void apply_inspection_overrides(const RayTracingAgentRenderRequest *request) {
    if (!request) return;

    RuntimeNative3DRender_ResetInspectionCameraOverrides();
    RuntimeVolume3DScatter_ResetTuning();
    if (request->has_camera_zoom_override) {
        sceneSettings.camera.zoom = request->camera_zoom_override;
    }
    if (request->has_camera_position_override) {
        RuntimeNative3DRender_SetInspectionCameraPosition(
            vec3(request->camera_position_x,
                 request->camera_position_y,
                 request->camera_position_z));
    }
    if (request->has_camera_look_at_override) {
        RuntimeNative3DRender_SetInspectionCameraLookAt(
            vec3(request->camera_look_at_x,
                 request->camera_look_at_y,
                 request->camera_look_at_z));
    }
    if (request->has_environment_brightness_override) {
        animSettings.environmentBrightness = request->environment_brightness_override;
    }
    if (request->has_ambient_strength_override) {
        animSettings.environmentBrightness = request->ambient_strength_override * 255.0;
    }
    if (request->has_environment_light_mode_override) {
        animSettings.environmentLightMode =
            animation_config_environment_light_mode_clamp(
                request->environment_light_mode_override);
    }
    if (request->has_top_fill_strength_override) {
        animSettings.topFillStrength = request->top_fill_strength_override;
    }
    if (request->has_light_intensity_override) {
        animSettings.lightIntensity = request->light_intensity_override;
    }
    if (request->has_light_radius_override) {
        animSettings.lightRadius = request->light_radius_override;
    }
    if (request->has_forward_decay_override) {
        animSettings.forwardDecay = request->forward_decay_override;
    }
    if (request->has_volume_scatter_gain_override) {
        RuntimeVolume3DScatter_SetStrengthGain(request->volume_scatter_gain_override);
    }
    if (request->has_volume_step_scale_override) {
        RuntimeVolume3DScatter_SetStepScale(request->volume_step_scale_override);
    }
    if (request->has_secondary_diffuse_samples_3d_override) {
        animSettings.secondaryDiffuseSamples3D = request->secondary_diffuse_samples_3d_override;
    }
    if (request->has_transmission_samples_3d_override) {
        animSettings.transmissionSamples3D = request->transmission_samples_3d_override;
    }
    if (request->has_volume_tint_override) {
        RuntimeVolume3DScatter_SetTint(request->volume_tint_r,
                                       request->volume_tint_g,
                                       request->volume_tint_b);
    }
}

static int run_preflight(const RayTracingAgentRenderRequest *request,
                         RayTracingHeadlessPreflight *out_preflight,
                         const char *job_status_path,
                         const char *job_id,
                         const char *request_path) {
    RuntimeNative3DPreparedFrame frame = {0};
    Point light_point = {0.0, 0.0};
    RayTracingHeadlessPreflight preflight = {0};

    if (!request || !out_preflight) return 2;
    preflight.request_loaded = true;
    snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "ok");
    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "loading_scene",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  "running",
                                  "starting preflight",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);

    LoadAllSettings();
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.interactiveMode = false;
    animSettings.integratorMode3D = (int)request->integrator_3d;
    animSettings.temporalFrames3D = request->temporal_frames;
    sceneSettings.windowWidth = request->width;
    sceneSettings.windowHeight = request->height;

    preflight.scene_applied =
        AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                   request->runtime_scene_path,
                                   true);
    runtime_scene_bridge_preflight_file(request->runtime_scene_path, &preflight.scene_summary);
    if (!preflight.scene_applied) {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "failed to apply runtime scene");
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      3);
        *out_preflight = preflight;
        return 3;
    }
    apply_inspection_overrides(request);

    if (request->volume_enabled) {
        animSettings.volumeAffectsLighting = request->volume_affects_lighting;
        animSettings.volumeDebugOverlayEnabled = request->volume_debug_overlay;
        preflight.volume_attached =
            AnimationSelectVolumeSource(request->volume_source_kind,
                                        request->volume_source_path,
                                        true);
        if (!preflight.volume_attached) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "failed to attach volume source");
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          request->start_frame,
                                          0,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          4);
            *out_preflight = preflight;
            return 4;
        }
        preflight.volume_summary_built =
            build_volume_debug_summary(request, &preflight.volume_summary);
    } else {
        AnimationClearVolumeSource();
        preflight.volume_attached = false;
    }

    preflight.route = RayTracingModeBackend_ResolveRoute();
    preflight.route_native_3d = RayTracingModeBackend_IsNative3D(&preflight.route);
    if (!preflight.route_native_3d) {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "native 3D route not ready");
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      5);
        *out_preflight = preflight;
        return 5;
    }

    if (sceneSettings.bezierPath.numPoints >= 1) {
        light_point = sceneSettings.bezierPath.points[0];
    }
    preflight.prepared_frame =
        RuntimeNative3DPrepareFrame(&frame,
                                    request->width,
                                    request->height,
                                    request->normalized_t,
                                    light_point.x,
                                    light_point.y);
    if (preflight.prepared_frame) {
        RuntimeTriangleMesh3D_BVHBuildStats(&frame.scene.triangleMesh,
                                            &preflight.bvh_build_stats);
        ray_tracing_headless_audit_prepared_frame(&preflight, &frame);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DPreparedSceneCacheStatsSnapshot(
            &preflight.prepared_scene_cache_stats);
    } else {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "failed to prepare native 3D frame: %s",
                 RuntimeNative3DPrepareFrameLastDiagnostics());
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      6);
        *out_preflight = preflight;
        return 6;
    }

    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "preflight_ready",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  "running",
                                  "preflight ready",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    *out_preflight = preflight;
    return 0;
}

static double frame_normalized_t(const RayTracingAgentRenderRequest *request, int local_frame) {
    int sampling_offset = 0;
    int sampling_count = 0;
    if (!request) return 0.0;
    sampling_offset = request->has_sampling_window ? request->sampling_frame_offset : 0;
    sampling_count = request->has_sampling_window ? request->sampling_frame_count : request->frame_count;
    if (sampling_count <= 1) return request->normalized_t;
    return (double)(sampling_offset + local_frame) / (double)(sampling_count - 1);
}

typedef struct {
    const RayTracingAgentRenderRequest *request;
    const char *job_status_path;
    const char *job_id;
    const char *request_path;
    int frame_index;
    int frames_completed;
} RayTracingTemporalProgressContext;

static void ray_tracing_temporal_progress_callback(int started_subpasses,
                                                   int completed_subpasses,
                                                   int total_subpasses,
                                                   void *user_data) {
    RayTracingTemporalProgressContext *context =
        (RayTracingTemporalProgressContext *)user_data;
    char diagnostics[128];
    if (!context || !context->request) return;
    if (completed_subpasses < started_subpasses) {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d active)",
                 started_subpasses,
                 total_subpasses);
    } else {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d committed)",
                 completed_subpasses,
                 total_subpasses);
    }
    (void)ray_tracing_render_headless_write_progress_and_job_status(context->request->progress_path,
                                        context->request,
                                        "rendering_frame",
                                        context->frame_index,
                                        context->frames_completed,
                                        started_subpasses,
                                        completed_subpasses,
                                        total_subpasses,
                                        "running",
                                        diagnostics,
                                        context->job_status_path,
                                        context->job_id,
                                        context->request_path,
                                        -1);
}

static int run_render(const RayTracingAgentRenderRequest *request,
                      RayTracingHeadlessPreflight *out_preflight,
                      const char *job_status_path,
                      const char *job_id,
                      const char *request_path) {
    RayTracingHeadlessPreflight preflight = {0};
    uint8_t *pixels = NULL;
    Point light_point = {0.0, 0.0};
    int preflight_code = 0;

    if (!request || !out_preflight) return 2;
    if (!request->output_root[0]) {
        memset(&preflight, 0, sizeof(preflight));
        preflight.request_loaded = true;
        snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "output.root required for render");
        *out_preflight = preflight;
        return 8;
    }

    preflight_code = run_preflight(request, &preflight, job_status_path, job_id, request_path);
    if (preflight_code != 0) {
        *out_preflight = preflight;
        return preflight_code;
    }

    if (snprintf(preflight.frame_dir,
                 sizeof(preflight.frame_dir),
                 "%s/frames",
                 request->output_root) >= (int)sizeof(preflight.frame_dir)) {
        snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "frame directory path too long");
        *out_preflight = preflight;
        return 8;
    }
    if (!config_io_ensure_directory_exists(preflight.frame_dir)) {
        snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "failed to create frame directory");
        *out_preflight = preflight;
        return 8;
    }

    pixels = (uint8_t *)calloc((size_t)request->width * (size_t)request->height,
                               (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    if (!pixels) {
        snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "failed to allocate frame buffer");
        *out_preflight = preflight;
        return 8;
    }

    if (sceneSettings.bezierPath.numPoints >= 1) {
        light_point = sceneSettings.bezierPath.points[0];
    }
    RuntimeTriangleBVH3D_ResetTraceStats();

    for (int i = 0; i < request->frame_count; ++i) {
        char frame_path[PATH_MAX];
        RuntimeNative3DRenderStats stats = {0};
        uint8_t frame_max_r = 0u;
        uint8_t frame_max_g = 0u;
        uint8_t frame_max_b = 0u;
        size_t frame_nonzero_pixels = 0u;
        RayTracingTemporalProgressContext temporal_progress = {0};
        const int frame_index = request->start_frame + i;
        const double t = frame_normalized_t(request, i);

        if (snprintf(frame_path,
                     sizeof(frame_path),
                     "%s/frame_%04d.bmp",
                     preflight.frame_dir,
                     frame_index) >= (int)sizeof(frame_path)) {
            snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "frame path too long");
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          frame_index,
                                          preflight.frames_rendered,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          8);
            free(pixels);
            *out_preflight = preflight;
            return 8;
        }
        if (!request->overwrite && access(frame_path, F_OK) == 0) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "frame exists; set output.overwrite=true");
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          frame_index,
                                          preflight.frames_rendered,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          8);
            free(pixels);
            *out_preflight = preflight;
            return 8;
        }
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "rendering_frame",
                                      frame_index,
                                      preflight.frames_rendered,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      "running",
                                      "rendering frame",
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);

        temporal_progress.request = request;
        temporal_progress.job_status_path = job_status_path;
        temporal_progress.job_id = job_id;
        temporal_progress.request_path = request_path;
        temporal_progress.frame_index = frame_index;
        temporal_progress.frames_completed = preflight.frames_rendered;

        if (!RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
                pixels,
                preflight.route.integratorMode3D,
                request->width,
                request->height,
                t,
                frame_index,
                light_point.x,
                light_point.y,
                NULL,
                request->temporal_frames,
                ray_tracing_temporal_progress_callback,
                &temporal_progress,
                &stats)) {
            snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "failed to render frame");
            RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight.bvh_trace_stats);
            RuntimeNative3DPreparedSceneCacheStatsSnapshot(
                &preflight.prepared_scene_cache_stats);
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          frame_index,
                                          preflight.frames_rendered,
                                          request->temporal_frames,
                                          0,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          9);
            free(pixels);
            *out_preflight = preflight;
            return 9;
        }
        RuntimeNative3DRenderStats_Accumulate(&preflight.stats, &stats);
        frame_nonzero_pixels = count_nonzero_pixels(pixels,
                                                    request->width,
                                                    request->height,
                                                    &frame_max_r,
                                                    &frame_max_g,
                                                    &frame_max_b);
        preflight.nonzero_pixels += frame_nonzero_pixels;
        if (frame_max_r > preflight.max_r) preflight.max_r = frame_max_r;
        if (frame_max_g > preflight.max_g) preflight.max_g = frame_max_g;
        if (frame_max_b > preflight.max_b) preflight.max_b = frame_max_b;
        if (!RayTracing2Native3DOverlay_ExportFrameBMP(frame_path,
                                                       request->width,
                                                       request->height,
                                                       pixels,
                                                       NULL)) {
            snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "failed to write frame bmp");
            RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight.bvh_trace_stats);
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          frame_index,
                                          preflight.frames_rendered,
                                          request->temporal_frames,
                                          request->temporal_frames,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          10);
            free(pixels);
            *out_preflight = preflight;
            return 10;
        }
        if (i == 0) {
            snprintf(preflight.first_frame_path,
                     sizeof(preflight.first_frame_path),
                     "%s",
                     frame_path);
        }
        snprintf(preflight.last_frame_path,
                 sizeof(preflight.last_frame_path),
                 "%s",
                 frame_path);
        preflight.frames_rendered += 1;
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "writing_frame",
                                      frame_index,
                                      preflight.frames_rendered,
                                      request->temporal_frames,
                                      request->temporal_frames,
                                      request->temporal_frames,
                                      "running",
                                      frame_path,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);
    }

    free(pixels);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight.bvh_trace_stats);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&preflight.prepared_scene_cache_stats);
    preflight.rendered_frames = preflight.frames_rendered == request->frame_count;
    if (preflight.rendered_frames && request->video_enabled) {
        int video_code = 0;
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "encoding_video",
                                      request->start_frame + request->frame_count - 1,
                                      preflight.frames_rendered,
                                      request->temporal_frames,
                                      request->temporal_frames,
                                      request->temporal_frames,
                                      "running",
                                      request->video_path,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);
        video_code = MakeVideoFromFrames(preflight.frame_dir,
                                         request->video_path,
                                         request->video_fps);
        if (video_code != 0) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "failed to encode video: %s",
                     request->video_path);
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          request->start_frame + request->frame_count - 1,
                                          preflight.frames_rendered,
                                          request->temporal_frames,
                                          request->temporal_frames,
                                          request->temporal_frames,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          11);
            *out_preflight = preflight;
            return 11;
        }
    }
    snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "ok");
    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "completed",
                                  request->start_frame + request->frame_count - 1,
                                  preflight.frames_rendered,
                                  request->temporal_frames,
                                  request->temporal_frames,
                                  request->temporal_frames,
                                  "completed",
                                  "render completed",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  0);
    *out_preflight = preflight;
    return preflight.rendered_frames ? 0 : 9;
}

int main(int argc, char **argv) {
    const char *request_path = NULL;
    const char *summary_override = NULL;
    const char *job_id = NULL;
    const char *job_status_path = NULL;
    bool render_mode = false;
    RayTracingAgentRenderRequest request;
    RayTracingHeadlessPreflight preflight = {0};
    char diagnostics[256] = {0};
    int run_code = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0 && i + 1 < argc) {
            request_path = argv[++i];
        } else if (strcmp(argv[i], "--summary") == 0 && i + 1 < argc) {
            summary_override = argv[++i];
        } else if (strcmp(argv[i], "--job-id") == 0 && i + 1 < argc) {
            job_id = argv[++i];
        } else if (strcmp(argv[i], "--job-status") == 0 && i + 1 < argc) {
            job_status_path = argv[++i];
        } else if (strcmp(argv[i], "--preflight") == 0) {
            render_mode = false;
        } else if (strcmp(argv[i], "--render") == 0) {
            render_mode = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!request_path) {
        usage(argv[0]);
        return 2;
    }
    if (!ray_tracing_agent_render_request_load_file(request_path,
                                                    &request,
                                                    diagnostics,
                                                    sizeof(diagnostics))) {
        fprintf(stderr, "ray_tracing_render_headless: %s\n", diagnostics);
        return 2;
    }
    if (summary_override && summary_override[0]) {
        snprintf(request.summary_path, sizeof(request.summary_path), "%s", summary_override);
    }
    if (job_status_path && job_status_path[0]) {
        ray_tracing_render_headless_write_job_status_file(job_status_path,
                              job_id ? job_id : request.run_id,
                              request_path,
                              &request,
                              render_mode ? "running" : "preflight",
                              render_mode ? "loading_request" : "preflight",
                              -1,
                              request.start_frame,
                              0,
                              0,
                              0,
                              request.temporal_frames > 0 ? request.temporal_frames : 1,
                              "render process started");
    }

    run_code = render_mode
                   ? run_render(&request,
                                &preflight,
                                job_status_path,
                                job_id,
                                request_path)
                   : run_preflight(&request,
                                   &preflight,
                                   job_status_path,
                                   job_id,
                                   request_path);
    ray_tracing_render_headless_write_summary(stdout, &request, &preflight);
    if (job_status_path && job_status_path[0]) {
        ray_tracing_render_headless_write_job_status_file(job_status_path,
                              job_id ? job_id : request.run_id,
                              request_path,
                              &request,
                              run_code == 0 ? "completed" : "failed",
                              run_code == 0 ? "completed" : "failed",
                              run_code,
                              run_code == 0 ? (request.start_frame + request.frame_count - 1) : request.start_frame,
                              run_code == 0 ? request.frame_count : 0,
                              run_code == 0 ? (request.temporal_frames > 0 ? request.temporal_frames : 1) : 0,
                              run_code == 0 ? (request.temporal_frames > 0 ? request.temporal_frames : 1) : 0,
                              request.temporal_frames > 0 ? request.temporal_frames : 1,
                              preflight.diagnostics);
    }
    if (!ray_tracing_render_headless_write_summary_file(request.summary_path, &request, &preflight)) {
        fprintf(stderr,
                "ray_tracing_render_headless: failed to write summary %s\n",
                request.summary_path);
        return 7;
    }
    return run_code;
}
