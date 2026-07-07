#include "tools/ray_tracing_render_headless_internal.h"

#include "app/animation.h"
#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "import/water_surface_import.h"
#include "render/runtime_material_payload_3d.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

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
static int ray_tracing_headless_last_requested_frame_index(
    const RayTracingAgentRenderRequest *request) {
    int frame_count = 1;
    int frame_offset = 0;
    if (!request) return 0;
    frame_count = request->frame_count > 0 ? request->frame_count : 1;
    frame_offset = frame_count - 1;
    if (frame_offset <= 0) return request->start_frame;
    if (request->start_frame > INT_MAX - frame_offset) return INT_MAX;
    return request->start_frame + frame_offset;
}

static bool ray_tracing_headless_inspect_volume_frame(
    const RayTracingAgentRenderRequest *request,
    int requested_frame_index,
    char *out_selected_path,
    size_t out_selected_path_size,
    uint64_t *out_loaded_frame_index,
    RuntimeVolumeDebugSummary3D *out_summary,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    RuntimeVolumeAttachment3D attachment = {0};
    RuntimeVolume3DSourceKind source_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[128] = {0};
    bool ok = false;

    if (out_selected_path && out_selected_path_size > 0u) {
        out_selected_path[0] = '\0';
    }
    if (out_loaded_frame_index) {
        *out_loaded_frame_index = 0u;
    }
    if (out_diagnostics && out_diagnostics_size > 0u) {
        snprintf(out_diagnostics, out_diagnostics_size, "invalid input");
    }
    if (!request || !request->volume_enabled || !out_selected_path ||
        out_selected_path_size == 0u) {
        return false;
    }
    source_kind = runtime_volume_source_kind_from_request(request->volume_source_kind);
    if (source_kind == RUNTIME_VOLUME_3D_SOURCE_NONE || !request->volume_source_path[0]) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics, out_diagnostics_size, "volume source missing");
        }
        return false;
    }
    if (!fluid_volume_import_3d_resolve_source_frame_path(request->volume_source_path,
                                                          source_kind,
                                                          requested_frame_index,
                                                          out_selected_path,
                                                          out_selected_path_size,
                                                          diagnostics,
                                                          sizeof(diagnostics))) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "failed to resolve volume frame %d: %s",
                     requested_frame_index,
                     diagnostics);
        }
        return false;
    }
    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = fluid_volume_import_3d_load_source_at_frame(request->volume_source_path,
                                                     source_kind,
                                                     requested_frame_index,
                                                     &attachment,
                                                     diagnostics,
                                                     sizeof(diagnostics));
    if (!ok) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "failed to load volume frame %d: %s",
                     requested_frame_index,
                     diagnostics);
        }
        RuntimeVolumeAttachment3D_Reset(&attachment);
        return false;
    }
    if (out_loaded_frame_index) {
        *out_loaded_frame_index = attachment.grid.frameIndex;
    }
    attachment.affectsLighting = request->volume_affects_lighting;
    attachment.debugOverlayEnabled = request->volume_debug_overlay;
    if (out_summary) {
        ok = RuntimeVolumeDebugSummary3D_Build(&attachment, out_summary);
        if (!ok) {
            if (out_diagnostics && out_diagnostics_size > 0u) {
                snprintf(out_diagnostics,
                         out_diagnostics_size,
                         "failed to build volume summary for frame %d",
                         requested_frame_index);
            }
            RuntimeVolumeAttachment3D_Reset(&attachment);
            return false;
        }
    }
    RuntimeVolumeAttachment3D_Reset(&attachment);
    if (out_diagnostics && out_diagnostics_size > 0u) {
        snprintf(out_diagnostics, out_diagnostics_size, "ok");
    }
    return true;
}

bool ray_tracing_headless_request_has_volume_source(
    const RayTracingAgentRenderRequest *request) {
    return request &&
           request->volume_enabled &&
           request->volume_source_path[0] != '\0' &&
           runtime_volume_source_kind_from_request(request->volume_source_kind) !=
               RUNTIME_VOLUME_3D_SOURCE_NONE;
}

bool ray_tracing_headless_populate_volume_frame_selection(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request) {
    char diagnostics[256] = {0};
    const int first_frame_index = request ? request->start_frame : 0;
    const int last_frame_index = ray_tracing_headless_last_requested_frame_index(request);

    if (!preflight || !request || !request->volume_enabled) return false;
    preflight->volume_requested_first_frame_index = first_frame_index;
    preflight->volume_requested_last_frame_index = last_frame_index;

    if (!ray_tracing_headless_inspect_volume_frame(
            request,
            first_frame_index,
            preflight->volume_selected_first_frame_path,
            sizeof(preflight->volume_selected_first_frame_path),
            &preflight->volume_loaded_first_frame_index,
            &preflight->volume_summary,
            diagnostics,
            sizeof(diagnostics))) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "%s",
                 diagnostics);
        return false;
    }
    preflight->volume_summary_built = true;

    if (last_frame_index == first_frame_index) {
        snprintf(preflight->volume_selected_last_frame_path,
                 sizeof(preflight->volume_selected_last_frame_path),
                 "%s",
                 preflight->volume_selected_first_frame_path);
        preflight->volume_loaded_last_frame_index =
            preflight->volume_loaded_first_frame_index;
    } else if (!ray_tracing_headless_inspect_volume_frame(
                   request,
                   last_frame_index,
                   preflight->volume_selected_last_frame_path,
                   sizeof(preflight->volume_selected_last_frame_path),
                   &preflight->volume_loaded_last_frame_index,
                   NULL,
                   diagnostics,
                   sizeof(diagnostics))) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "%s",
                 diagnostics);
        return false;
    }

    preflight->volume_frame_selection_dynamic =
        strcmp(preflight->volume_selected_first_frame_path,
               preflight->volume_selected_last_frame_path) != 0 ||
        preflight->volume_loaded_first_frame_index !=
            preflight->volume_loaded_last_frame_index;
    preflight->volume_frame_selection_built = true;
    return true;
}

static bool ray_tracing_headless_inspect_water_surface_frame(
    const RayTracingAgentRenderRequest *request,
    int requested_frame_index,
    RuntimeWaterSurfaceFrame *out_frame,
    bool *out_found,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    RuntimeVolume3DSourceKind source_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[256] = {0};

    if (out_found) *out_found = false;
    if (out_diagnostics && out_diagnostics_size > 0u) {
        snprintf(out_diagnostics, out_diagnostics_size, "invalid input");
    }
    if (!ray_tracing_headless_request_has_volume_source(request) || !out_frame) {
        return false;
    }

    source_kind = runtime_volume_source_kind_from_request(request->volume_source_kind);
    if (source_kind != RUNTIME_VOLUME_3D_SOURCE_MANIFEST || !request->volume_source_path[0]) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics, out_diagnostics_size, "water surface source not found");
        }
        return true;
    }

    if (!RuntimeWaterSurfaceImport_LoadSourceAtFrame(request->volume_source_path,
                                                     requested_frame_index,
                                                     out_frame,
                                                     out_found,
                                                     diagnostics,
                                                     sizeof(diagnostics))) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "failed to load water surface frame %d: %s",
                     requested_frame_index,
                     diagnostics);
        }
        return false;
    }
    if (out_diagnostics && out_diagnostics_size > 0u) {
        snprintf(out_diagnostics, out_diagnostics_size, "ok");
    }
    return true;
}

static void ray_tracing_headless_copy_water_surface_first_frame(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeWaterSurfaceFrame *frame) {
    if (!preflight || !frame) return;
    preflight->water_surface_loaded = frame->valid;
    preflight->water_surface_loaded_first_frame_index = frame->frame_index;
    snprintf(preflight->water_surface_manifest_path,
             sizeof(preflight->water_surface_manifest_path),
             "%s",
             frame->source_manifest_path);
    snprintf(preflight->water_surface_selected_first_frame_path,
             sizeof(preflight->water_surface_selected_first_frame_path),
             "%s",
             frame->frame_path);
    snprintf(preflight->water_surface_axis,
             sizeof(preflight->water_surface_axis),
             "%s",
             frame->surface_axis);
    preflight->water_surface_grid_w = frame->grid_w;
    preflight->water_surface_grid_d = frame->grid_d;
    preflight->water_surface_sample_count = frame->sample_count;
    preflight->water_surface_last_grid_w = frame->grid_w;
    preflight->water_surface_last_grid_d = frame->grid_d;
    preflight->water_surface_last_sample_count = frame->sample_count;
    preflight->water_surface_wet_columns = frame->wet_columns;
    preflight->water_surface_dry_columns = frame->dry_columns;
    preflight->water_surface_solid_columns = frame->solid_columns;
    preflight->water_surface_water_cells = frame->water_cells;
    preflight->water_surface_min_y = frame->surface_min_y;
    preflight->water_surface_max_y = frame->surface_max_y;
    preflight->water_surface_avg_y = frame->surface_avg_y;
    preflight->water_surface_max_slope = frame->max_slope;
    preflight->water_surface_finite_normals = frame->finite_normals;
    if (frame->material.valid) {
        preflight->water_surface_material_ior = frame->material.ior;
        preflight->water_surface_absorption_distance_m =
            frame->material.absorption_distance_m;
        preflight->water_surface_absorption_r = frame->material.absorption_rgb[0];
        preflight->water_surface_absorption_g = frame->material.absorption_rgb[1];
        preflight->water_surface_absorption_b = frame->material.absorption_rgb[2];
        preflight->water_surface_material_reflectivity = frame->material.reflectivity;
        preflight->water_surface_material_roughness = frame->material.roughness;
    }
}

bool ray_tracing_headless_populate_water_surface_frame_selection(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request) {
    RuntimeWaterSurfaceFrame first = {0};
    RuntimeWaterSurfaceFrame last = {0};
    char diagnostics[256] = {0};
    bool found = false;
    const int first_frame_index = request ? request->start_frame : 0;
    const int last_frame_index = ray_tracing_headless_last_requested_frame_index(request);

    if (!preflight || !ray_tracing_headless_request_has_volume_source(request)) return false;
    preflight->water_surface_requested_first_frame_index = first_frame_index;
    preflight->water_surface_requested_last_frame_index = last_frame_index;

    RuntimeWaterSurfaceFrame_Init(&first);
    RuntimeWaterSurfaceFrame_Init(&last);
    if (!ray_tracing_headless_inspect_water_surface_frame(request,
                                                          first_frame_index,
                                                          &first,
                                                          &found,
                                                          diagnostics,
                                                          sizeof(diagnostics))) {
        snprintf(preflight->diagnostics, sizeof(preflight->diagnostics), "%s", diagnostics);
        RuntimeWaterSurfaceFrame_Free(&first);
        RuntimeWaterSurfaceFrame_Free(&last);
        return false;
    }
    if (!found) {
        RuntimeWaterSurfaceFrame_Free(&first);
        RuntimeWaterSurfaceFrame_Free(&last);
        return true;
    }

    preflight->water_surface_source_found = true;
    ray_tracing_headless_copy_water_surface_first_frame(preflight, &first);

    if (last_frame_index == first_frame_index) {
        snprintf(preflight->water_surface_selected_last_frame_path,
                 sizeof(preflight->water_surface_selected_last_frame_path),
                 "%s",
                 preflight->water_surface_selected_first_frame_path);
        preflight->water_surface_loaded_last_frame_index =
            preflight->water_surface_loaded_first_frame_index;
        preflight->water_surface_last_grid_w = preflight->water_surface_grid_w;
        preflight->water_surface_last_grid_d = preflight->water_surface_grid_d;
        preflight->water_surface_last_sample_count =
            preflight->water_surface_sample_count;
    } else {
        bool last_found = false;
        if (!ray_tracing_headless_inspect_water_surface_frame(request,
                                                              last_frame_index,
                                                              &last,
                                                              &last_found,
                                                              diagnostics,
                                                              sizeof(diagnostics)) ||
            !last_found) {
            snprintf(preflight->diagnostics,
                     sizeof(preflight->diagnostics),
                     "%s",
                     last_found ? diagnostics : "water surface last frame not found");
            RuntimeWaterSurfaceFrame_Free(&first);
            RuntimeWaterSurfaceFrame_Free(&last);
            return false;
        }
        preflight->water_surface_loaded_last_frame_index = last.frame_index;
        snprintf(preflight->water_surface_selected_last_frame_path,
                 sizeof(preflight->water_surface_selected_last_frame_path),
                 "%s",
                 last.frame_path);
        preflight->water_surface_last_grid_w = last.grid_w;
        preflight->water_surface_last_grid_d = last.grid_d;
        preflight->water_surface_last_sample_count = last.sample_count;
    }

    preflight->water_surface_frame_selection_dynamic =
        strcmp(preflight->water_surface_selected_first_frame_path,
               preflight->water_surface_selected_last_frame_path) != 0 ||
        preflight->water_surface_loaded_first_frame_index !=
            preflight->water_surface_loaded_last_frame_index;
    preflight->water_surface_frame_selection_built = true;
    RuntimeWaterSurfaceFrame_Free(&first);
    RuntimeWaterSurfaceFrame_Free(&last);
    return true;
}

void ray_tracing_headless_note_water_surface_mesh(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame) {
    int triangle_count = 0;
    int payload_scene_object_index = -1;
    if (!preflight || !frame || !preflight->water_surface_source_found) return;
    for (int i = 0; i < frame->scene.triangleMesh.triangleCount; ++i) {
        const int scene_object_index = frame->scene.triangleMesh.triangles[i].sceneObjectIndex;
        if (scene_object_index >= 0 &&
            scene_object_index < sceneSettings.objectCount &&
            strcmp(sceneSettings.sceneObjects[scene_object_index].type, "water_surface") == 0) {
            triangle_count += 1;
            if (payload_scene_object_index < 0) {
                payload_scene_object_index = scene_object_index;
            }
        }
    }
    preflight->water_surface_triangle_count = triangle_count;
    preflight->water_surface_mesh_attached = triangle_count > 0;
    if (payload_scene_object_index >= 0) {
        RuntimeMaterialPayload3D payload = {0};
        if (RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(payload_scene_object_index,
                                                                 &payload) &&
            payload.valid) {
            preflight->water_surface_material_payload_applied = true;
            preflight->water_surface_payload_ior = payload.opticalIor;
            preflight->water_surface_payload_absorption_distance_m =
                payload.absorptionDistance;
            preflight->water_surface_payload_transparency = payload.transparency;
            preflight->water_surface_payload_reflectivity = payload.bsdf.reflectivity;
            preflight->water_surface_payload_roughness = payload.bsdf.roughness;
            preflight->water_surface_payload_tint_r = payload.baseColorR;
            preflight->water_surface_payload_tint_g = payload.baseColorG;
            preflight->water_surface_payload_tint_b = payload.baseColorB;
        }
    }
}
