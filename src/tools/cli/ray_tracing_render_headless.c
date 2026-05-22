#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/agent_render_request.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_volume_3d_debug.h"
#include "render/runtime_volume_3d_scatter.h"

#define RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX 64

typedef struct RayTracingHeadlessObjectAuditEntry {
    bool used;
    int scene_object_index;
    char object_id[64];
    char object_type[20];
    int material_id;
    double alpha;
    double reflectivity;
    double roughness;
    double emissive_strength;
    int texture_id;
    double texture_strength;
    double texture_scale;
    double texture_offset_u;
    double texture_offset_v;
    int texture_seed;
    int texture_pattern_mode;
    double texture_coverage;
    double texture_grain;
    double texture_edge_softness;
    double texture_contrast;
    double texture_flow;
    double texture_color_depth;
    double texture_surface_damage;
    int packed_color;
    int primitive_count;
    int triangle_count;
    int primary_hit_pixels;
    double center_x;
    double center_y;
    double center_z;
    bool center_projectable;
    bool center_inside_viewport;
    double center_screen_x;
    double center_screen_y;
    double center_camera_depth;
} RayTracingHeadlessObjectAuditEntry;

typedef struct RayTracingHeadlessPreflight {
    bool request_loaded;
    bool scene_applied;
    bool volume_attached;
    bool volume_summary_built;
    bool route_native_3d;
    bool prepared_frame;
    bool rendered_frames;
    int frames_rendered;
    RayTracingRuntimeRoute route;
    RuntimeSceneBridgePreflight scene_summary;
    RuntimeVolumeDebugSummary3D volume_summary;
    RuntimeNative3DRenderStats stats;
    size_t nonzero_pixels;
    uint8_t max_r;
    uint8_t max_g;
    uint8_t max_b;
    char frame_dir[PATH_MAX];
    char first_frame_path[PATH_MAX];
    char last_frame_path[PATH_MAX];
    int object_audit_count;
    RayTracingHeadlessObjectAuditEntry
        object_audit[RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX];
    char diagnostics[1024];
} RayTracingHeadlessPreflight;

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

static const char *route_family_label(RayTracingRouteFamily family) {
    switch (family) {
        case RAY_TRACING_ROUTE_NATIVE_3D:
            return "native_3d";
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            return "compat_3d_fallback";
        case RAY_TRACING_ROUTE_CANONICAL_2D:
        default:
            return "canonical_2d";
    }
}

static void json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static void write_summary(FILE *file,
                          const RayTracingAgentRenderRequest *request,
                          const RayTracingHeadlessPreflight *preflight) {
    if (!file || !request || !preflight) return;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_headless_summary_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    json_write_string(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"request_loaded\": %s,\n", preflight->request_loaded ? "true" : "false");
    fprintf(file, "  \"scene_applied\": %s,\n", preflight->scene_applied ? "true" : "false");
    fprintf(file, "  \"volume_attached\": %s,\n", preflight->volume_attached ? "true" : "false");
    fprintf(file, "  \"volume_summary_built\": %s,\n",
            preflight->volume_summary_built ? "true" : "false");
    fprintf(file, "  \"route_family\": ");
    json_write_string(file, route_family_label(preflight->route.routeFamily));
    fprintf(file, ",\n");
    fprintf(file, "  \"route_native_3d\": %s,\n", preflight->route_native_3d ? "true" : "false");
    fprintf(file, "  \"prepared_frame\": %s,\n", preflight->prepared_frame ? "true" : "false");
    fprintf(file, "  \"rendered_frames\": %s,\n", preflight->rendered_frames ? "true" : "false");
    fprintf(file, "  \"frames_rendered\": %d,\n", preflight->frames_rendered);
    fprintf(file, "  \"runtime_scene_path\": ");
    json_write_string(file, request->runtime_scene_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_path\": ");
    json_write_string(file, request->volume_source_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_kind\": ");
    json_write_string(file,
                      ray_tracing_agent_render_request_volume_kind_label(
                          request->volume_source_kind));
    fprintf(file, ",\n");
    fprintf(file, "  \"integrator_3d\": ");
    json_write_string(file,
                      ray_tracing_agent_render_request_integrator_label(
                          request->integrator_3d));
    fprintf(file, ",\n");
    fprintf(file, "  \"render\": {\n");
    fprintf(file, "    \"start_frame\": %d,\n", request->start_frame);
    fprintf(file, "    \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "    \"width\": %d,\n", request->width);
    fprintf(file, "    \"height\": %d,\n", request->height);
    fprintf(file, "    \"normalized_t\": %.9f,\n", request->normalized_t);
    fprintf(file, "    \"temporal_frames\": %d\n", request->temporal_frames);
    fprintf(file, "  },\n");
    fprintf(file, "  \"inspection\": {\n");
    fprintf(file, "    \"preset\": ");
    json_write_string(file,
                      ray_tracing_agent_render_request_inspection_preset_label(
                          request->inspection_preset));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_camera_zoom_override\": %s,\n",
            request->has_camera_zoom_override ? "true" : "false");
    fprintf(file, "    \"camera_zoom\": %.9f,\n", request->camera_zoom_override);
    fprintf(file, "    \"has_camera_position_override\": %s,\n",
            request->has_camera_position_override ? "true" : "false");
    fprintf(file, "    \"camera_position\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_position_x,
            request->camera_position_y,
            request->camera_position_z);
    fprintf(file, "    \"has_camera_look_at_override\": %s,\n",
            request->has_camera_look_at_override ? "true" : "false");
    fprintf(file, "    \"camera_look_at\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_look_at_x,
            request->camera_look_at_y,
            request->camera_look_at_z);
    fprintf(file, "    \"has_environment_brightness_override\": %s,\n",
            request->has_environment_brightness_override ? "true" : "false");
    fprintf(file, "    \"environment_brightness\": %.9f,\n",
            request->environment_brightness_override);
    fprintf(file, "    \"has_light_intensity_override\": %s,\n",
            request->has_light_intensity_override ? "true" : "false");
    fprintf(file, "    \"light_intensity\": %.9f,\n", request->light_intensity_override);
    fprintf(file, "    \"has_light_radius_override\": %s,\n",
            request->has_light_radius_override ? "true" : "false");
    fprintf(file, "    \"light_radius\": %.9f,\n", request->light_radius_override);
    fprintf(file, "    \"has_forward_decay_override\": %s,\n",
            request->has_forward_decay_override ? "true" : "false");
    fprintf(file, "    \"forward_decay\": %.9f,\n", request->forward_decay_override);
    fprintf(file, "    \"has_volume_scatter_gain_override\": %s,\n",
            request->has_volume_scatter_gain_override ? "true" : "false");
    fprintf(file, "    \"volume_scatter_gain\": %.9f,\n",
            request->volume_scatter_gain_override);
    fprintf(file, "    \"has_volume_step_scale_override\": %s,\n",
            request->has_volume_step_scale_override ? "true" : "false");
    fprintf(file, "    \"volume_step_scale\": %.9f,\n",
            request->volume_step_scale_override);
    fprintf(file, "    \"has_secondary_diffuse_samples_3d_override\": %s,\n",
            request->has_secondary_diffuse_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"secondary_diffuse_samples_3d\": %d,\n",
            request->secondary_diffuse_samples_3d_override);
    fprintf(file, "    \"has_transmission_samples_3d_override\": %s,\n",
            request->has_transmission_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"transmission_samples_3d\": %d,\n",
            request->transmission_samples_3d_override);
    fprintf(file, "    \"has_volume_tint_override\": %s,\n",
            request->has_volume_tint_override ? "true" : "false");
    fprintf(file, "    \"volume_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f }\n",
            request->volume_tint_r,
            request->volume_tint_g,
            request->volume_tint_b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    json_write_string(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "    \"frame_dir\": ");
    json_write_string(file, preflight->frame_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"first_frame_path\": ");
    json_write_string(file, preflight->first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"last_frame_path\": ");
    json_write_string(file, preflight->last_frame_path);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"scene_summary\": {\n");
    fprintf(file, "    \"valid_contract\": %s,\n",
            preflight->scene_summary.valid_contract ? "true" : "false");
    fprintf(file, "    \"scene_id\": ");
    json_write_string(file, preflight->scene_summary.scene_id);
    fprintf(file, ",\n");
    fprintf(file, "    \"object_count\": %d,\n", preflight->scene_summary.object_count);
    fprintf(file, "    \"material_count\": %d,\n", preflight->scene_summary.material_count);
    fprintf(file, "    \"light_count\": %d,\n", preflight->scene_summary.light_count);
    fprintf(file, "    \"camera_count\": %d\n", preflight->scene_summary.camera_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"volume_summary\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", preflight->volume_summary.enabled ? "true" : "false");
    fprintf(file, "    \"has_data\": %s,\n", preflight->volume_summary.hasData ? "true" : "false");
    fprintf(file, "    \"layout_valid\": %s,\n",
            preflight->volume_summary.layoutValid ? "true" : "false");
    fprintf(file, "    \"has_density\": %s,\n",
            preflight->volume_summary.hasDensity ? "true" : "false");
    fprintf(file, "    \"has_velocity\": %s,\n",
            preflight->volume_summary.hasVelocity ? "true" : "false");
    fprintf(file, "    \"has_pressure\": %s,\n",
            preflight->volume_summary.hasPressure ? "true" : "false");
    fprintf(file, "    \"has_solid_mask\": %s,\n",
            preflight->volume_summary.hasSolidMask ? "true" : "false");
    fprintf(file, "    \"grid_w\": %u,\n", preflight->volume_summary.gridW);
    fprintf(file, "    \"grid_h\": %u,\n", preflight->volume_summary.gridH);
    fprintf(file, "    \"grid_d\": %u,\n", preflight->volume_summary.gridD);
    fprintf(file, "    \"cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.cellCount);
    fprintf(file, "    \"density_non_zero_cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.densityNonZeroCellCount);
    fprintf(file, "    \"density_min\": %.9f,\n", preflight->volume_summary.densityMin);
    fprintf(file, "    \"density_max\": %.9f\n", preflight->volume_summary.densityMax);
    fprintf(file, "  },\n");
    fprintf(file, "  \"render_stats\": {\n");
    fprintf(file, "    \"hit_pixels\": %d,\n", preflight->stats.hitPixelCount);
    fprintf(file, "    \"visible_pixels\": %d,\n", preflight->stats.visiblePixelCount);
    fprintf(file, "    \"secondary_rays\": %d,\n", preflight->stats.secondaryRayCount);
    fprintf(file, "    \"secondary_hits\": %d,\n", preflight->stats.secondaryHitCount);
    fprintf(file, "    \"temporal_committed_subpasses\": %d,\n",
            preflight->stats.temporalCommittedSubpasses);
    fprintf(file, "    \"temporal_pixels_rendered\": %d,\n",
            preflight->stats.temporalPixelsRendered);
    fprintf(file, "    \"temporal_pixels_skipped\": %d,\n",
            preflight->stats.temporalPixelsSkipped);
    fprintf(file, "    \"temporal_active_pixels\": %d,\n",
            preflight->stats.temporalActivePixelCount);
    fprintf(file, "    \"temporal_active_tiles\": %d,\n",
            preflight->stats.temporalActiveTileCount);
    fprintf(file, "    \"temporal_inactive_tiles\": %d,\n",
            preflight->stats.temporalInactiveTileCount);
    fprintf(file, "    \"max_radiance\": %.9f,\n", preflight->stats.maxRadiance);
    fprintf(file, "    \"max_bounce_radiance\": %.9f,\n",
            preflight->stats.maxBounceRadiance);
    fprintf(file, "    \"total_bounce_radiance\": %.9f,\n",
            preflight->stats.totalBounceRadiance);
    fprintf(file, "    \"nonzero_pixels\": %llu,\n",
            (unsigned long long)preflight->nonzero_pixels);
    fprintf(file, "    \"max_rgb\": [%u, %u, %u]\n",
            (unsigned)preflight->max_r,
            (unsigned)preflight->max_g,
            (unsigned)preflight->max_b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"object_audit\": [\n");
    {
        int emitted = 0;
        for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
            const RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
            if (!entry->used) continue;
            if (emitted > 0) {
                fprintf(file, ",\n");
            }
            fprintf(file, "    {\n");
            fprintf(file, "      \"scene_object_index\": %d,\n", entry->scene_object_index);
            fprintf(file, "      \"object_id\": ");
            json_write_string(file, entry->object_id);
            fprintf(file, ",\n");
            fprintf(file, "      \"object_type\": ");
            json_write_string(file, entry->object_type);
            fprintf(file, ",\n");
            fprintf(file, "      \"material_id\": %d,\n", entry->material_id);
            fprintf(file, "      \"alpha\": %.6f,\n", entry->alpha);
            fprintf(file, "      \"reflectivity\": %.6f,\n", entry->reflectivity);
            fprintf(file, "      \"roughness\": %.6f,\n", entry->roughness);
            fprintf(file, "      \"emissive_strength\": %.6f,\n", entry->emissive_strength);
            fprintf(file, "      \"texture_id\": %d,\n", entry->texture_id);
            fprintf(file, "      \"texture_strength\": %.6f,\n", entry->texture_strength);
            fprintf(file, "      \"texture_scale\": %.6f,\n", entry->texture_scale);
            fprintf(file, "      \"texture_offset_u\": %.6f,\n", entry->texture_offset_u);
            fprintf(file, "      \"texture_offset_v\": %.6f,\n", entry->texture_offset_v);
            fprintf(file, "      \"texture_seed\": %d,\n", entry->texture_seed);
            fprintf(file, "      \"texture_pattern_mode\": %d,\n", entry->texture_pattern_mode);
            fprintf(file, "      \"texture_coverage\": %.6f,\n", entry->texture_coverage);
            fprintf(file, "      \"texture_grain\": %.6f,\n", entry->texture_grain);
            fprintf(file, "      \"texture_edge_softness\": %.6f,\n", entry->texture_edge_softness);
            fprintf(file, "      \"texture_contrast\": %.6f,\n", entry->texture_contrast);
            fprintf(file, "      \"texture_flow\": %.6f,\n", entry->texture_flow);
            fprintf(file, "      \"texture_color_depth\": %.6f,\n", entry->texture_color_depth);
            fprintf(file, "      \"texture_surface_damage\": %.6f,\n", entry->texture_surface_damage);
            fprintf(file, "      \"packed_color\": %d,\n", entry->packed_color);
            fprintf(file, "      \"primitive_count\": %d,\n", entry->primitive_count);
            fprintf(file, "      \"triangle_count\": %d,\n", entry->triangle_count);
            fprintf(file, "      \"primary_hit_pixels\": %d,\n", entry->primary_hit_pixels);
            fprintf(file, "      \"center\": { \"x\": %.6f, \"y\": %.6f, \"z\": %.6f },\n",
                    entry->center_x,
                    entry->center_y,
                    entry->center_z);
            fprintf(file, "      \"center_projectable\": %s,\n",
                    entry->center_projectable ? "true" : "false");
            fprintf(file, "      \"center_inside_viewport\": %s,\n",
                    entry->center_inside_viewport ? "true" : "false");
            fprintf(file,
                    "      \"center_screen\": { \"x\": %.6f, \"y\": %.6f, \"camera_depth\": %.6f }\n",
                    entry->center_screen_x,
                    entry->center_screen_y,
                    entry->center_camera_depth);
            fprintf(file, "    }");
            emitted += 1;
        }
        if (emitted > 0) {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "  ],\n");
    fprintf(file, "  \"diagnostics\": ");
    json_write_string(file, preflight->diagnostics);
    fprintf(file, "\n");
    fprintf(file, "}\n");
}

static bool write_summary_file(const char *path,
                               const RayTracingAgentRenderRequest *request,
                               const RayTracingHeadlessPreflight *preflight) {
    FILE *file = NULL;
    if (!path || !path[0]) return true;
    file = fopen(path, "wb");
    if (!file) return false;
    write_summary(file, request, preflight);
    fclose(file);
    return true;
}

static bool write_job_status_file(const char *path,
                                  const char *job_id,
                                  const char *request_path,
                                  const RayTracingAgentRenderRequest *request,
                                  const char *state,
                                  const char *stage,
                                  int exit_code,
                                  int frame_index,
                                  int frames_completed,
                                  int temporal_subpasses_started,
                                  int temporal_subpasses_completed,
                                  int temporal_subpasses_total,
                                  const char *diagnostics);

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    const char *slash = NULL;
    char dir[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    slash = strrchr(path, '/');
    if (!slash) return true;
    len = (size_t)(slash - path);
    if (len == 0u) {
        return true;
    }
    if (len >= sizeof(dir)) return false;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return ensure_directory_exists(dir);
}

static bool write_progress_file(const char *path,
                                const RayTracingAgentRenderRequest *request,
                                const char *stage,
                                int frame_index,
                                int frames_completed,
                                int temporal_subpasses_started,
                                int temporal_subpasses_completed,
                                int temporal_subpasses_total,
                                const char *state,
                                const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    if (!path || !path[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_render_progress_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    json_write_string(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    json_write_string(file, stage ? stage : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    json_write_string(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"progress_ratio\": %.6f,\n",
            (temporal_subpasses_total > 0)
                ? ((double)temporal_subpasses_completed / (double)temporal_subpasses_total)
                : ((request->frame_count > 0)
                       ? ((double)frames_completed / (double)request->frame_count)
                       : 0.0));
    fprintf(file, "  \"updated_at_utc\": ");
    json_write_string(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    json_write_string(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

static bool write_progress_and_job_status(const char *progress_path,
                                          const RayTracingAgentRenderRequest *request,
                                          const char *stage,
                                          int frame_index,
                                          int frames_completed,
                                          int temporal_subpasses_started,
                                          int temporal_subpasses_completed,
                                          int temporal_subpasses_total,
                                          const char *state,
                                          const char *diagnostics,
                                          const char *job_status_path,
                                          const char *job_id,
                                          const char *request_path,
                                          int exit_code) {
    if (!write_progress_file(progress_path,
                             request,
                             stage,
                             frame_index,
                             frames_completed,
                             temporal_subpasses_started,
                             temporal_subpasses_completed,
                             temporal_subpasses_total,
                             state,
                             diagnostics)) {
        return false;
    }
    if (job_status_path && job_status_path[0] && job_id && job_id[0]) {
        if (!write_job_status_file(job_status_path,
                                   job_id,
                                   request_path,
                                   request,
                                   state,
                                   stage,
                                   exit_code,
                                   frame_index,
                                   frames_completed,
                                   temporal_subpasses_started,
                                   temporal_subpasses_completed,
                                   temporal_subpasses_total,
                                   diagnostics)) {
            return false;
        }
    }
    return true;
}

static void load_existing_job_status_times(const char *path,
                                           char *out_submitted_at_utc,
                                           size_t submitted_size,
                                           char *out_started_at_utc,
                                           size_t started_size) {
    json_object *root = NULL;
    json_object *value = NULL;
    const char *text_value = NULL;
    if (out_submitted_at_utc && submitted_size > 0u) out_submitted_at_utc[0] = '\0';
    if (out_started_at_utc && started_size > 0u) out_started_at_utc[0] = '\0';
    if (!path || !path[0]) return;
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }
    if (out_submitted_at_utc && submitted_size > 0u &&
        json_object_object_get_ex(root, "submitted_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_submitted_at_utc, submitted_size, "%s", text_value);
    }
    if (out_started_at_utc && started_size > 0u &&
        json_object_object_get_ex(root, "started_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_started_at_utc, started_size, "%s", text_value);
    }
    json_object_put(root);
}

static bool write_job_status_file(const char *path,
                                  const char *job_id,
                                  const char *request_path,
                                  const RayTracingAgentRenderRequest *request,
                                  const char *state,
                                  const char *stage,
                                  int exit_code,
                                  int frame_index,
                                  int frames_completed,
                                  int temporal_subpasses_started,
                                  int temporal_subpasses_completed,
                                  int temporal_subpasses_total,
                                  const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    char started_at_utc[32] = {0};
    char finished_at_utc[32] = {0};
    char submitted_at_utc[32] = {0};
    char stdout_path[PATH_MAX] = {0};
    char stderr_path[PATH_MAX] = {0};
    char pid_path[PATH_MAX] = {0};
    char job_root[PATH_MAX] = {0};
    char overwrite_policy[32] = {0};
    const char *slash = NULL;
    size_t job_root_len = 0u;
    if (!path || !path[0] || !job_id || !job_id[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    load_existing_job_status_times(path,
                                   submitted_at_utc,
                                   sizeof(submitted_at_utc),
                                   started_at_utc,
                                   sizeof(started_at_utc));
    slash = strrchr(path, '/');
    if (slash) {
        job_root_len = (size_t)(slash - path);
        if (job_root_len > 0u && job_root_len < sizeof(job_root)) {
            memcpy(job_root, path, job_root_len);
            job_root[job_root_len] = '\0';
            snprintf(stdout_path, sizeof(stdout_path), "%s/stdout.log", job_root);
            snprintf(stderr_path, sizeof(stderr_path), "%s/stderr.log", job_root);
            snprintf(pid_path, sizeof(pid_path), "%s/pid.txt", job_root);
        }
    }
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    if ((strcmp(state ? state : "", "running") == 0 ||
         strcmp(state ? state : "", "completed") == 0 ||
         strcmp(state ? state : "", "failed") == 0) &&
        started_at_utc[0] == '\0') {
        snprintf(started_at_utc, sizeof(started_at_utc), "%s", updated_at_utc);
    }
    if (strcmp(state ? state : "", "completed") == 0 ||
        strcmp(state ? state : "", "failed") == 0 ||
        strcmp(state ? state : "", "cancelled") == 0) {
        snprintf(finished_at_utc, sizeof(finished_at_utc), "%s", updated_at_utc);
    }
    if (submitted_at_utc[0] == '\0') {
        snprintf(submitted_at_utc, sizeof(submitted_at_utc), "%s", updated_at_utc);
    }
    if (request->overwrite) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "overwrite");
    } else if (request->has_sampling_window && request->sampling_frame_offset > 0) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "resume");
    } else {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "fail_if_exists");
    }
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_detached_job_status_v1\",\n");
    fprintf(file, "  \"program\": \"ray_tracing\",\n");
    fprintf(file, "  \"tool\": \"ray_tracing_render_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    json_write_string(file, job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    json_write_string(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    json_write_string(file, stage ? stage : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    json_write_string(file, request_path ? request_path : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    json_write_string(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    json_write_string(file, request->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    json_write_string(file, request->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    json_write_string(file, stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    json_write_string(file, stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid_path\": ");
    json_write_string(file, pid_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)getpid());
    fprintf(file, "  \"exit_code\": %d,\n", exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    json_write_string(file, overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"requested_start_frame\": %d,\n",
            request->start_frame - (request->has_sampling_window ? request->sampling_frame_offset : 0));
    fprintf(file, "  \"requested_frame_count\": %d,\n",
            request->has_sampling_window ? request->sampling_frame_count : request->frame_count);
    fprintf(file, "  \"effective_start_frame\": %d,\n", request->start_frame);
    fprintf(file, "  \"effective_frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"progress_ratio\": %.6f,\n",
            (temporal_subpasses_total > 0)
                ? ((double)temporal_subpasses_completed / (double)temporal_subpasses_total)
                : ((request->frame_count > 0)
                       ? ((double)frames_completed / (double)request->frame_count)
                       : 0.0));
    fprintf(file, "  \"submitted_at_utc\": ");
    json_write_string(file, submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    json_write_string(file, started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    json_write_string(file, finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    json_write_string(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    json_write_string(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
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
    write_progress_and_job_status(request->progress_path,
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
        write_progress_and_job_status(request->progress_path,
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
            write_progress_and_job_status(request->progress_path,
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
        write_progress_and_job_status(request->progress_path,
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
        ray_tracing_headless_audit_prepared_frame(&preflight, &frame);
        RuntimeNative3DPreparedFrame_Free(&frame);
    } else {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "failed to prepare native 3D frame: %s",
                 RuntimeNative3DPrepareFrameLastDiagnostics());
        write_progress_and_job_status(request->progress_path,
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

    write_progress_and_job_status(request->progress_path,
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
    (void)write_progress_and_job_status(context->request->progress_path,
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
    if (!ensure_directory_exists(preflight.frame_dir)) {
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
            write_progress_and_job_status(request->progress_path,
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
            write_progress_and_job_status(request->progress_path,
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
        write_progress_and_job_status(request->progress_path,
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
            write_progress_and_job_status(request->progress_path,
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
            write_progress_and_job_status(request->progress_path,
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
        write_progress_and_job_status(request->progress_path,
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
    preflight.rendered_frames = preflight.frames_rendered == request->frame_count;
    snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "ok");
    write_progress_and_job_status(request->progress_path,
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
        write_job_status_file(job_status_path,
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
    write_summary(stdout, &request, &preflight);
    if (job_status_path && job_status_path[0]) {
        write_job_status_file(job_status_path,
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
    if (!write_summary_file(request.summary_path, &request, &preflight)) {
        fprintf(stderr,
                "ray_tracing_render_headless: failed to write summary %s\n",
                request.summary_path);
        return 7;
    }
    return run_code;
}
