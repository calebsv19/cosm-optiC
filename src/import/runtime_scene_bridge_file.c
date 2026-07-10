#include "import/runtime_scene_bridge.h"

#include "config/config_manager.h"
#include "core_io.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge_json_utils.h"
#include "import/runtime_scene_volume_defaults.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        RayTracingRuntimeMeshAssetSet mesh_assets;
        if (!ray_tracing_runtime_mesh_assets_last_matches_scene_file(runtime_scene_path)) {
            ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
            ok = ray_tracing_runtime_mesh_assets_load_scene_file(
                runtime_scene_path,
                &mesh_assets,
                out_preflight->diagnostics,
                sizeof(out_preflight->diagnostics));
            ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
        }
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
    RayTracingRuntimeMeshAssetSet mesh_assets;
    bool ok;

    if (!runtime_scene_path || !out_summary) return false;
    runtime_scene_bridge_preflight_reset(out_summary);
    ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
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
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        runtime_scene_bridge_preflight_diag(out_summary, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    if (load_mesh_assets) {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file(
                runtime_scene_path_copy,
                &mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            free(json_text);
            return false;
        }
    } else {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
                runtime_scene_path_copy,
                RUNTIME_SCENE_BRIDGE_EDITOR_MESH_PREVIEW_MAX_ASSET_BYTES,
                &mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
            ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
        }
    }

    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             runtime_scene_path_copy);
    ok = runtime_scene_bridge_apply_json(json_text, out_summary);
    if (ok) {
        ray_tracing_runtime_mesh_assets_take_last_for_scene(runtime_scene_path_copy, &mesh_assets);
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
        ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
    }
    free(json_text);
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
