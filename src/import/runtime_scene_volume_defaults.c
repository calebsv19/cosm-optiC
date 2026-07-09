#include "import/runtime_scene_volume_defaults.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "core_io.h"
#include "core_scene.h"
#include "import/scene_bundle_import.h"

static bool runtime_scene_volume_defaults_join_candidate(const char* runtime_scene_path,
                                                         const char* relative_name,
                                                         char* out_path,
                                                         size_t out_path_size) {
    char base_dir[4096] = {0};
    if (!runtime_scene_path || !runtime_scene_path[0] || !relative_name || !relative_name[0] ||
        !out_path || out_path_size == 0u) {
        return false;
    }
    out_path[0] = '\0';
    if (core_scene_dirname(runtime_scene_path, base_dir, sizeof(base_dir)).code != CORE_OK) {
        return false;
    }
    if (core_scene_resolve_path(base_dir, relative_name, out_path, out_path_size).code != CORE_OK) {
        out_path[0] = '\0';
        return false;
    }
    return out_path[0] != '\0';
}

static bool runtime_scene_volume_defaults_manifest_is_vf3d(const char* manifest_path) {
    CoreBuffer file_data = {0};
    CoreResult read_result = core_result_ok();
    char* text = NULL;
    cJSON* root = NULL;
    cJSON* frame_contract = NULL;
    cJSON* space_mode = NULL;
    cJSON* frames = NULL;
    bool ok = false;

    if (!manifest_path || !manifest_path[0]) return false;
    read_result = core_io_read_all(manifest_path, &file_data);
    if (read_result.code != CORE_OK || !file_data.data || file_data.size == 0u) {
        core_io_buffer_free(&file_data);
        return false;
    }

    text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    root = cJSON_Parse(text);
    free(text);
    if (!root) {
        return false;
    }

    frame_contract = cJSON_GetObjectItem(root, "frame_contract");
    space_mode = cJSON_GetObjectItem(root, "space_mode");
    frames = cJSON_GetObjectItem(root, "frames");
    ok = cJSON_IsString(frame_contract) &&
         strcmp(frame_contract->valuestring, "vf3d") == 0 &&
         (!space_mode ||
          (cJSON_IsString(space_mode) && strcmp(space_mode->valuestring, "3d") == 0)) &&
         cJSON_IsArray(frames) &&
         cJSON_GetArraySize(frames) > 0;

    cJSON_Delete(root);
    return ok;
}

static bool runtime_scene_volume_defaults_match(int current_kind,
                                                const char* current_path,
                                                int candidate_kind,
                                                const char* candidate_path) {
    if (animation_config_volume_source_kind_clamp(current_kind) !=
        animation_config_volume_source_kind_clamp(candidate_kind)) {
        return false;
    }
    if (!current_path || !candidate_path) return false;
    return strcmp(current_path, candidate_path) == 0;
}

bool runtime_scene_volume_defaults_resolve(const char* runtime_scene_path,
                                           int* out_volume_source_kind,
                                           char* out_volume_source_path,
                                           size_t out_volume_source_path_size) {
    char candidate_path[4096] = {0};
    SceneBundleImportResult bundle = {0};

    if (out_volume_source_kind) {
        *out_volume_source_kind = VOLUME_SOURCE_NONE;
    }
    if (out_volume_source_path && out_volume_source_path_size > 0u) {
        out_volume_source_path[0] = '\0';
    }
    if (!runtime_scene_path || !runtime_scene_path[0] || !out_volume_source_path ||
        out_volume_source_path_size == 0u) {
        return false;
    }

    if (runtime_scene_volume_defaults_join_candidate(runtime_scene_path,
                                                     "scene_bundle.json",
                                                     candidate_path,
                                                     sizeof(candidate_path)) &&
        core_io_path_exists(candidate_path) &&
        scene_bundle_import_resolve_fluid_source(candidate_path, &bundle) &&
        bundle.fluid_source_path[0]) {
        if (out_volume_source_kind) {
            *out_volume_source_kind = VOLUME_SOURCE_MANIFEST;
        }
        snprintf(out_volume_source_path, out_volume_source_path_size, "%s", candidate_path);
        return true;
    }

    if (runtime_scene_volume_defaults_join_candidate(runtime_scene_path,
                                                     "manifest.json",
                                                     candidate_path,
                                                     sizeof(candidate_path)) &&
        core_io_path_exists(candidate_path) &&
        runtime_scene_volume_defaults_manifest_is_vf3d(candidate_path)) {
        if (out_volume_source_kind) {
            *out_volume_source_kind = VOLUME_SOURCE_MANIFEST;
        }
        snprintf(out_volume_source_path, out_volume_source_path_size, "%s", candidate_path);
        return true;
    }

    return false;
}

void runtime_scene_volume_defaults_apply_transition(AnimationConfig* cfg,
                                                    const char* previous_runtime_scene_path,
                                                    const char* next_runtime_scene_path) {
    int previous_auto_kind = VOLUME_SOURCE_NONE;
    int next_auto_kind = VOLUME_SOURCE_NONE;
    char previous_auto_path[4096] = {0};
    char next_auto_path[4096] = {0};
    const int current_kind = cfg ? animation_config_volume_source_kind_clamp(cfg->volumeSourceKind)
                                 : VOLUME_SOURCE_NONE;
    const bool has_current_source =
        cfg && current_kind != VOLUME_SOURCE_NONE && cfg->volumeSourcePath[0] != '\0';
    bool matches_previous_auto = false;
    bool next_auto_valid = false;

    if (!cfg || !next_runtime_scene_path || !next_runtime_scene_path[0]) return;

    if (previous_runtime_scene_path && previous_runtime_scene_path[0]) {
        runtime_scene_volume_defaults_resolve(previous_runtime_scene_path,
                                              &previous_auto_kind,
                                              previous_auto_path,
                                              sizeof(previous_auto_path));
    }
    next_auto_valid = runtime_scene_volume_defaults_resolve(next_runtime_scene_path,
                                                            &next_auto_kind,
                                                            next_auto_path,
                                                            sizeof(next_auto_path));
    matches_previous_auto =
        has_current_source &&
        previous_auto_kind != VOLUME_SOURCE_NONE &&
        previous_auto_path[0] &&
        runtime_scene_volume_defaults_match(current_kind,
                                            cfg->volumeSourcePath,
                                            previous_auto_kind,
                                            previous_auto_path);

    if (has_current_source && !matches_previous_auto) {
        return;
    }

    if (!next_auto_valid) {
        cfg->volumeInteractionEnabled = false;
        cfg->volumeSourceKind = VOLUME_SOURCE_NONE;
        cfg->volumeSourcePath[0] = '\0';
        return;
    }

    cfg->volumeSourceKind = animation_config_volume_source_kind_clamp(next_auto_kind);
    snprintf(cfg->volumeSourcePath, sizeof(cfg->volumeSourcePath), "%s", next_auto_path);
    if (!matches_previous_auto) {
        cfg->volumeInteractionEnabled = true;
    }
}
