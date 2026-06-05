#include "tools/ray_tracing_render_headless_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

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

void ray_tracing_render_headless_write_summary(
    FILE *file,
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
    fprintf(file, "    \"has_ambient_strength_override\": %s,\n",
            request->has_ambient_strength_override ? "true" : "false");
    fprintf(file, "    \"ambient_strength\": %.9f,\n", request->ambient_strength_override);
    fprintf(file, "    \"has_environment_light_mode_override\": %s,\n",
            request->has_environment_light_mode_override ? "true" : "false");
    fprintf(file, "    \"environment_light_mode\": ");
    json_write_string(file,
                      request->environment_light_mode_override == ENVIRONMENT_LIGHT_MODE_TOP_FILL
                          ? "top_fill"
                          : (request->environment_light_mode_override ==
                                     ENVIRONMENT_LIGHT_MODE_AMBIENT
                                 ? "ambient"
                                 : "off"));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_top_fill_strength_override\": %s,\n",
            request->has_top_fill_strength_override ? "true" : "false");
    fprintf(file, "    \"top_fill_strength\": %.9f,\n", request->top_fill_strength_override);
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
    fprintf(file, ",\n");
    fprintf(file, "    \"video_enabled\": %s,\n", request->video_enabled ? "true" : "false");
    fprintf(file, "    \"video_path\": ");
    json_write_string(file, request->video_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"video_fps\": %d", request->video_fps);
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
    fprintf(file, "  \"prepared_scene_cache\": {\n");
    fprintf(file, "    \"valid\": %s,\n",
            preflight->prepared_scene_cache_stats.valid ? "true" : "false");
    fprintf(file, "    \"generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.generation);
    fprintf(file, "    \"cached_generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.cachedGeneration);
    fprintf(file, "    \"hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.hits);
    fprintf(file, "    \"misses\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.misses);
    fprintf(file, "    \"stores\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.stores);
    fprintf(file, "    \"invalidations\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.invalidations);
    fprintf(file, "    \"static_geometry_reuse_enabled\": %s,\n",
            preflight->prepared_scene_cache_stats.staticGeometryReuseEnabled ? "true"
                                                                             : "false");
    fprintf(file, "    \"time_independent_hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.timeIndependentHits);
    fprintf(file, "    \"cached_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.cachedNormalizedT);
    fprintf(file, "    \"last_requested_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.lastRequestedNormalizedT);
    fprintf(file, "    \"cached_primitive_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedPrimitiveCount);
    fprintf(file, "    \"cached_triangle_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedTriangleCount);
    fprintf(file, "    \"cached_bvh_node_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedBVHNodeCount);
    fprintf(file, "    \"cached_bvh_leaf_count\": %d\n",
            preflight->prepared_scene_cache_stats.cachedBVHLeafCount);
    fprintf(file, "  },\n");
    fprintf(file, "  \"bvh_summary\": {\n");
    fprintf(file, "    \"ready\": %s,\n",
            preflight->bvh_build_stats.ready ? "true" : "false");
    fprintf(file, "    \"triangle_count\": %d,\n",
            preflight->bvh_build_stats.triangleCount);
    fprintf(file, "    \"node_count\": %d,\n", preflight->bvh_build_stats.nodeCount);
    fprintf(file, "    \"leaf_count\": %d,\n", preflight->bvh_build_stats.leafCount);
    fprintf(file, "    \"max_depth\": %d,\n", preflight->bvh_build_stats.maxDepth);
    fprintf(file, "    \"leaf_size\": %d,\n", preflight->bvh_build_stats.leafSize);
    fprintf(file, "    \"max_leaf_triangle_count\": %d,\n",
            preflight->bvh_build_stats.maxLeafTriangleCount);
    fprintf(file, "    \"build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.buildCpuMs);
    fprintf(file, "    \"node_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.nodeBytes);
    fprintf(file, "    \"index_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.indexBytes);
    fprintf(file, "    \"centroid_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.centroidBytes);
    fprintf(file, "    \"sort_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.sortScratchBytes);
    fprintf(file, "    \"build_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.buildScratchBytes);
    fprintf(file, "    \"total_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.totalBytes);
    fprintf(file, "    \"trace_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceCalls);
    fprintf(file, "    \"trace_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceHits);
    fprintf(file, "    \"trace_misses\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceMisses);
    fprintf(file, "    \"trace_overflows\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceOverflows);
    fprintf(file, "    \"flat_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.flatFallbackCalls);
    fprintf(file, "    \"overflow_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.overflowFallbackCalls);
    fprintf(file, "    \"node_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.nodeVisits);
    fprintf(file, "    \"leaf_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.leafVisits);
    fprintf(file, "    \"aabb_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbTests);
    fprintf(file, "    \"aabb_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbHits);
    fprintf(file, "    \"triangle_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleTests);
    fprintf(file, "    \"triangle_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleHits);
    fprintf(file, "    \"max_stack_depth\": %llu\n",
            (unsigned long long)preflight->bvh_trace_stats.maxStackDepth);
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

bool ray_tracing_render_headless_write_summary_file(
    const char *path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    FILE *file = NULL;
    if (!path || !path[0]) return true;
    file = fopen(path, "wb");
    if (!file) return false;
    ray_tracing_render_headless_write_summary(file, request, preflight);
    fclose(file);
    return true;
}

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

bool ray_tracing_render_headless_write_progress_and_job_status(
    const char *progress_path,
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
        if (!ray_tracing_render_headless_write_job_status_file(job_status_path,
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

bool ray_tracing_render_headless_write_job_status_file(
    const char *path,
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
