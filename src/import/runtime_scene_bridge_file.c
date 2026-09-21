#include "import/runtime_scene_bridge.h"

#include "config/config_manager.h"
#include "core_io.h"
#include "import/runtime_curve_asset_loader.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge_json_utils.h"
#include "import/runtime_scene_volume_defaults.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Explicit surface attributes cannot use the size-limited bounds-only preview
 * loader: dropping their mesh also drops the declared chart. */
static bool runtime_scene_bridge_requires_full_surface_meshes(const char *text) {
    json_object *root=json_tokener_parse(text),*objects=NULL,*ext=NULL,*ray=NULL,*authoring=NULL,*rows=NULL;
    bool required=false;
    if(!root)return false;
    json_object_object_get_ex(root,"objects",&objects);
    for(size_t i=0;json_object_is_type(objects,json_type_array)&&i<json_object_array_length(objects);++i){
        json_object *o=json_object_array_get_idx(objects,i),*map=NULL,*sampling=NULL,*method=NULL;
        ext=ray=NULL;json_object_object_get_ex(o,"extensions",&ext);
        if(ext)json_object_object_get_ex(ext,"ray_tracing",&ray);
        if(ray){json_object_object_get_ex(ray,"surface_mapping",&map);json_object_object_get_ex(ray,"surface_sampling",&sampling);}
        if(map)json_object_object_get_ex(map,"method",&method);
        required|=sampling!=NULL||(json_object_is_type(method,json_type_string)&&!strcmp(json_object_get_string(method),"authored_uv"));
    }
    ext=ray=NULL;json_object_object_get_ex(root,"extensions",&ext);
    if(ext)json_object_object_get_ex(ext,"ray_tracing",&ray);
    if(ray)json_object_object_get_ex(ray,"authoring",&authoring);
    if(authoring)json_object_object_get_ex(authoring,"object_materials",&rows);
    for(size_t i=0;json_object_is_type(rows,json_type_array)&&i<json_object_array_length(rows);++i){
        json_object *binding=NULL,*maps=NULL;json_object_object_get_ex(json_object_array_get_idx(rows,i),"surface_material_binding",&binding);
        if(binding)json_object_object_get_ex(binding,"mappings",&maps);
        for(size_t j=0;json_object_is_type(maps,json_type_array)&&j<json_object_array_length(maps);++j){
            json_object *definition=NULL,*method=NULL;json_object_object_get_ex(json_object_array_get_idx(maps,j),"definition",&definition);
            if(definition)json_object_object_get_ex(definition,"method",&method);
            required|=json_object_is_type(method,json_type_string)&&!strcmp(json_object_get_string(method),"authored_uv");
        }
    }
    json_object_put(root);return required;
}

#define RUNTIME_SCENE_BRIDGE_EDITOR_MESH_PREVIEW_MAX_ASSET_BYTES (1024u * 1024u)

bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok;

    if (!runtime_scene_path || !out_preflight) return false;
    runtime_scene_bridge_preflight_reset(out_preflight);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        runtime_scene_bridge_preflight_diag(out_preflight, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        runtime_scene_bridge_preflight_diag(out_preflight, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    ok = runtime_scene_bridge_preflight_json(json_text, out_preflight);
    free(json_text);
    if (ok) {
        RayTracingRuntimeCurveAssetSet *curve_assets =
            calloc(1u, sizeof(*curve_assets));
        if (!curve_assets) {
            runtime_scene_bridge_preflight_diag(
                out_preflight, "out of memory");
            return false;
        }
        ray_tracing_runtime_curve_asset_set_init(curve_assets);
        /*
         * Curve sidecars are digest-bound but intentionally do not yet have the
         * mesh loader's file-stamp cache. Always reload during preflight so a
         * changed sidecar cannot be hidden by a previous scene-path match.
         */
        ok = ray_tracing_runtime_curve_assets_load_scene_file(
            runtime_scene_path,
            curve_assets,
            out_preflight->diagnostics,
            sizeof(out_preflight->diagnostics));
        ray_tracing_runtime_curve_asset_set_free(curve_assets);
        free(curve_assets);
    }
    if (ok) {
        RayTracingRuntimeMeshAssetSet *mesh_assets =
            calloc(1u, sizeof(*mesh_assets));
        if (!mesh_assets) {
            runtime_scene_bridge_preflight_diag(
                out_preflight, "out of memory");
            return false;
        }
        if (!ray_tracing_runtime_mesh_assets_last_matches_scene_file(runtime_scene_path)) {
            ray_tracing_runtime_mesh_asset_set_init(mesh_assets);
            ok = ray_tracing_runtime_mesh_assets_load_scene_file(
                runtime_scene_path,
                mesh_assets,
                out_preflight->diagnostics,
                sizeof(out_preflight->diagnostics));
            ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
        }
        free(mesh_assets);
    }
    return ok;
}

static bool runtime_scene_bridge_apply_file_with_options(const char *runtime_scene_path,
                                                         RuntimeSceneBridgePreflight *out_summary,
                                                         bool load_mesh_assets) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char runtime_scene_path_copy[sizeof(animSettings.runtimeScenePath)];
    char previous_runtime_scene_path[sizeof(animSettings.runtimeScenePath)];
    char *json_text = NULL;
    RayTracingRuntimeMeshAssetSet *mesh_assets = NULL;
    RayTracingRuntimeCurveAssetSet *curve_assets = NULL;
    bool ok;

    if (!runtime_scene_path || !out_summary) return false;
    runtime_scene_bridge_preflight_reset(out_summary);
    mesh_assets = calloc(1u, sizeof(*mesh_assets));
    curve_assets = calloc(1u, sizeof(*curve_assets));
    if (!mesh_assets || !curve_assets) {
        runtime_scene_bridge_preflight_diag(out_summary, "out of memory");
        free(mesh_assets);
        free(curve_assets);
        return false;
    }
    ray_tracing_runtime_mesh_asset_set_init(mesh_assets);
    ray_tracing_runtime_curve_asset_set_init(curve_assets);
    snprintf(previous_runtime_scene_path,
             sizeof(previous_runtime_scene_path),
             "%s",
             animSettings.runtimeScenePath);
    snprintf(runtime_scene_path_copy,
             sizeof(runtime_scene_path_copy),
             "%s",
             runtime_scene_path);

    io_result = core_io_read_all(runtime_scene_path_copy, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        runtime_scene_bridge_preflight_diag(out_summary, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
        ray_tracing_runtime_curve_asset_set_free(curve_assets);
        free(mesh_assets);
        free(curve_assets);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        runtime_scene_bridge_preflight_diag(out_summary, "out of memory");
        core_io_buffer_free(&file_data);
        ray_tracing_runtime_curve_asset_set_free(curve_assets);
        free(mesh_assets);
        free(curve_assets);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    if (!ray_tracing_runtime_curve_assets_load_scene_file(
            runtime_scene_path_copy,
            curve_assets,
            out_summary->diagnostics,
            sizeof(out_summary->diagnostics))) {
        free(json_text);
        ray_tracing_runtime_curve_asset_set_free(curve_assets);
        free(curve_assets);
        ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
        free(mesh_assets);
        return false;
    }

    if (load_mesh_assets || runtime_scene_bridge_requires_full_surface_meshes(json_text)) {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file(
                runtime_scene_path_copy,
                mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            free(json_text);
            ray_tracing_runtime_curve_asset_set_free(curve_assets);
            free(curve_assets);
            ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
            free(mesh_assets);
            return false;
        }
    } else {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
                runtime_scene_path_copy,
                RUNTIME_SCENE_BRIDGE_EDITOR_MESH_PREVIEW_MAX_ASSET_BYTES,
                mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
            ray_tracing_runtime_mesh_asset_set_init(mesh_assets);
        }
    }

    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             runtime_scene_path_copy);
    ok = runtime_scene_bridge_apply_json(json_text, out_summary);
    if (ok) {
        ray_tracing_runtime_mesh_assets_take_last_for_scene(
            runtime_scene_path_copy, mesh_assets);
        ray_tracing_runtime_curve_assets_take_last_for_scene(
            runtime_scene_path_copy, curve_assets);
    }
    if (ok) {
        runtime_scene_volume_defaults_apply_transition(&animSettings,
                                                       previous_runtime_scene_path,
                                                       runtime_scene_path_copy);
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 runtime_scene_path_copy);
    } else {
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 previous_runtime_scene_path);
        ray_tracing_runtime_mesh_asset_set_free(mesh_assets);
        ray_tracing_runtime_curve_asset_set_free(curve_assets);
    }
    free(json_text);
    free(mesh_assets);
    free(curve_assets);
    return ok;
}

bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary) {
    return runtime_scene_bridge_apply_file_with_options(runtime_scene_path, out_summary, true);
}

bool runtime_scene_bridge_apply_file_defer_mesh_assets(const char *runtime_scene_path,
                                                       RuntimeSceneBridgePreflight *out_summary) {
    return runtime_scene_bridge_apply_file_with_options(runtime_scene_path, out_summary, false);
}
