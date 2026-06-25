#include "import/fluid_volume_import_3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "core_io.h"
#include "core_scene.h"
#include "import/scene_bundle_import.h"

static void fluid_volume_source_import_3d_diag(char* out_diagnostics,
                                               size_t out_diagnostics_size,
                                               const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool fluid_volume_source_import_3d_read_text(const char* path,
                                                    char** out_text) {
    CoreBuffer file_data = {0};
    CoreResult read_result = core_result_ok();
    char* text = NULL;

    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;

    read_result = core_io_read_all(path, &file_data);
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
    *out_text = text;
    return true;
}

static bool fluid_volume_source_import_3d_resolve_manifest_frame_path(
    const char* manifest_path,
    int requested_frame_index,
    char* out_frame_path,
    size_t out_frame_path_size,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    char* manifest_text = NULL;
    cJSON* root = NULL;
    cJSON* frames = NULL;
    cJSON* frame_entry = NULL;
    cJSON* path_item = NULL;
    cJSON* frame_contract = NULL;
    cJSON* space_mode = NULL;
    char base_dir[4096] = {0};
    CoreResult resolve_result = core_result_ok();
    int frame_count = 0;

    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!manifest_path || !manifest_path[0] || !out_frame_path || out_frame_path_size == 0u) {
        return false;
    }

    out_frame_path[0] = '\0';
    if (!fluid_volume_source_import_3d_read_text(manifest_path, &manifest_text)) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "failed to read vf3d manifest");
        return false;
    }

    root = cJSON_Parse(manifest_text);
    free(manifest_text);
    manifest_text = NULL;
    if (!root) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "failed to parse vf3d manifest");
        return false;
    }

    frame_contract = cJSON_GetObjectItem(root, "frame_contract");
    space_mode = cJSON_GetObjectItem(root, "space_mode");
    if (!cJSON_IsString(frame_contract) ||
        strcmp(frame_contract->valuestring, "vf3d") != 0) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frame_contract mismatch");
        return false;
    }
    if (space_mode && (!cJSON_IsString(space_mode) ||
                       strcmp(space_mode->valuestring, "3d") != 0)) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest space_mode mismatch");
        return false;
    }

    frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsArray(frames)) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frames missing");
        return false;
    }

    frame_count = cJSON_GetArraySize(frames);
    if (frame_count <= 0) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frames empty");
        return false;
    }

    if (requested_frame_index >= 0) {
        for (int i = 0; i < frame_count; ++i) {
            cJSON* candidate = cJSON_GetArrayItem(frames, i);
            cJSON* frame_index = NULL;
            if (!cJSON_IsObject(candidate)) continue;
            frame_index = cJSON_GetObjectItem(candidate, "frame_index");
            if (cJSON_IsNumber(frame_index) &&
                (uint64_t)frame_index->valuedouble == (uint64_t)requested_frame_index) {
                frame_entry = candidate;
                break;
            }
        }
    }

    if (!frame_entry) {
        if (requested_frame_index < 0) {
            requested_frame_index = 0;
        } else if (requested_frame_index >= frame_count) {
            requested_frame_index = frame_count - 1;
        }
        frame_entry = cJSON_GetArrayItem(frames, requested_frame_index);
    }
    if (!cJSON_IsObject(frame_entry)) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frame entry invalid");
        return false;
    }

    frame_contract = cJSON_GetObjectItem(frame_entry, "frame_contract");
    if (frame_contract &&
        (!cJSON_IsString(frame_contract) ||
         strcmp(frame_contract->valuestring, "vf3d") != 0)) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frame entry contract mismatch");
        return false;
    }

    path_item = cJSON_GetObjectItem(frame_entry, "path");
    if (!cJSON_IsString(path_item) || !path_item->valuestring || !path_item->valuestring[0]) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frame path missing");
        return false;
    }

    resolve_result = core_scene_dirname(manifest_path, base_dir, sizeof(base_dir));
    if (resolve_result.code != CORE_OK) {
        cJSON_Delete(root);
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest base path invalid");
        return false;
    }

    resolve_result = core_scene_resolve_path(base_dir,
                                             path_item->valuestring,
                                             out_frame_path,
                                             out_frame_path_size);
    cJSON_Delete(root);
    if (resolve_result.code != CORE_OK || !out_frame_path[0]) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d manifest frame path unresolved");
        return false;
    }

    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool fluid_volume_source_import_3d_load_internal(const char* path,
                                                        RuntimeVolume3DSourceKind source_kind,
                                                        int requested_frame_index,
                                                        RuntimeVolumeAttachment3D* out_attachment,
                                                        char* out_diagnostics,
                                                        size_t out_diagnostics_size,
                                                        int indirection_depth) {
    RuntimeVolume3DSourceKind resolved_kind = source_kind;
    char resolved_path[4096] = {0};
    SceneBundleImportResult bundle = {0};

    if (indirection_depth > 4) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d source indirection limit exceeded");
        return false;
    }

    if (resolved_kind == RUNTIME_VOLUME_3D_SOURCE_NONE) {
        if (!fluid_volume_import_3d_classify_path(path, &resolved_kind)) {
            fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                               "unsupported vf3d source path");
            return false;
        }
    }

    switch (resolved_kind) {
        case RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:
            return fluid_volume_import_3d_load_raw(path,
                                                   out_attachment,
                                                   out_diagnostics,
                                                   out_diagnostics_size);

        case RUNTIME_VOLUME_3D_SOURCE_PACK:
            return fluid_volume_import_3d_load_pack(path,
                                                    out_attachment,
                                                    out_diagnostics,
                                                    out_diagnostics_size);

        case RUNTIME_VOLUME_3D_SOURCE_MANIFEST:
            if (core_scene_path_is_scene_bundle(path)) {
                if (!scene_bundle_import_resolve_fluid_source(path, &bundle) ||
                    !bundle.fluid_source_path[0]) {
                    fluid_volume_source_import_3d_diag(out_diagnostics,
                                                       out_diagnostics_size,
                                                       "vf3d scene bundle resolve failed");
                    return false;
                }
                if (strcmp(bundle.fluid_source_path, path) == 0) {
                    fluid_volume_source_import_3d_diag(out_diagnostics,
                                                       out_diagnostics_size,
                                                       "vf3d scene bundle self-reference");
                    return false;
                }
                return fluid_volume_source_import_3d_load_internal(
                    bundle.fluid_source_path,
                    RUNTIME_VOLUME_3D_SOURCE_NONE,
                    requested_frame_index,
                    out_attachment,
                    out_diagnostics,
                    out_diagnostics_size,
                    indirection_depth + 1);
            }

            if (!fluid_volume_source_import_3d_resolve_manifest_frame_path(path,
                                                                           requested_frame_index,
                                                                           resolved_path,
                                                                           sizeof(resolved_path),
                                                                           out_diagnostics,
                                                                           out_diagnostics_size)) {
                return false;
            }
            if (strcmp(resolved_path, path) == 0) {
                fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                                   "vf3d manifest self-reference");
                return false;
            }
            return fluid_volume_source_import_3d_load_internal(resolved_path,
                                                               RUNTIME_VOLUME_3D_SOURCE_NONE,
                                                               requested_frame_index,
                                                               out_attachment,
                                                               out_diagnostics,
                                                               out_diagnostics_size,
                                                               indirection_depth + 1);

        case RUNTIME_VOLUME_3D_SOURCE_NONE:
        default:
            fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                               "unsupported vf3d source kind");
            return false;
    }
}

static bool fluid_volume_source_import_3d_copy_frame_path(const char* path,
                                                          char* out_frame_path,
                                                          size_t out_frame_path_size,
                                                          char* out_diagnostics,
                                                          size_t out_diagnostics_size) {
    if (!path || !path[0] || !out_frame_path || out_frame_path_size == 0u) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "invalid input");
        return false;
    }
    if (snprintf(out_frame_path, out_frame_path_size, "%s", path) >=
        (int)out_frame_path_size) {
        out_frame_path[0] = '\0';
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d source frame path too long");
        return false;
    }
    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool fluid_volume_source_import_3d_resolve_source_frame_path_internal(
    const char* path,
    RuntimeVolume3DSourceKind source_kind,
    int requested_frame_index,
    char* out_frame_path,
    size_t out_frame_path_size,
    char* out_diagnostics,
    size_t out_diagnostics_size,
    int indirection_depth) {
    RuntimeVolume3DSourceKind resolved_kind = source_kind;
    char resolved_path[4096] = {0};
    SceneBundleImportResult bundle = {0};

    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!path || !path[0] || !out_frame_path || out_frame_path_size == 0u) {
        return false;
    }
    out_frame_path[0] = '\0';

    if (indirection_depth > 4) {
        fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                           "vf3d source indirection limit exceeded");
        return false;
    }

    if (resolved_kind == RUNTIME_VOLUME_3D_SOURCE_NONE) {
        if (!fluid_volume_import_3d_classify_path(path, &resolved_kind)) {
            fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                               "unsupported vf3d source path");
            return false;
        }
    }

    switch (resolved_kind) {
        case RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:
        case RUNTIME_VOLUME_3D_SOURCE_PACK:
            return fluid_volume_source_import_3d_copy_frame_path(path,
                                                                 out_frame_path,
                                                                 out_frame_path_size,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size);

        case RUNTIME_VOLUME_3D_SOURCE_MANIFEST:
            if (core_scene_path_is_scene_bundle(path)) {
                if (!scene_bundle_import_resolve_fluid_source(path, &bundle) ||
                    !bundle.fluid_source_path[0]) {
                    fluid_volume_source_import_3d_diag(out_diagnostics,
                                                       out_diagnostics_size,
                                                       "vf3d scene bundle resolve failed");
                    return false;
                }
                if (strcmp(bundle.fluid_source_path, path) == 0) {
                    fluid_volume_source_import_3d_diag(out_diagnostics,
                                                       out_diagnostics_size,
                                                       "vf3d scene bundle self-reference");
                    return false;
                }
                return fluid_volume_source_import_3d_resolve_source_frame_path_internal(
                    bundle.fluid_source_path,
                    RUNTIME_VOLUME_3D_SOURCE_NONE,
                    requested_frame_index,
                    out_frame_path,
                    out_frame_path_size,
                    out_diagnostics,
                    out_diagnostics_size,
                    indirection_depth + 1);
            }

            if (!fluid_volume_source_import_3d_resolve_manifest_frame_path(path,
                                                                           requested_frame_index,
                                                                           resolved_path,
                                                                           sizeof(resolved_path),
                                                                           out_diagnostics,
                                                                           out_diagnostics_size)) {
                return false;
            }
            if (strcmp(resolved_path, path) == 0) {
                fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                                   "vf3d manifest self-reference");
                return false;
            }
            return fluid_volume_source_import_3d_resolve_source_frame_path_internal(
                resolved_path,
                RUNTIME_VOLUME_3D_SOURCE_NONE,
                requested_frame_index,
                out_frame_path,
                out_frame_path_size,
                out_diagnostics,
                out_diagnostics_size,
                indirection_depth + 1);

        case RUNTIME_VOLUME_3D_SOURCE_NONE:
        default:
            fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                               "unsupported vf3d source kind");
            return false;
    }
}

bool fluid_volume_import_3d_load_source(const char* path,
                                        RuntimeVolume3DSourceKind source_kind_hint,
                                        RuntimeVolumeAttachment3D* out_attachment,
                                        char* out_diagnostics,
                                        size_t out_diagnostics_size) {
    return fluid_volume_import_3d_load_source_at_frame(path,
                                                       source_kind_hint,
                                                       0,
                                                       out_attachment,
                                                       out_diagnostics,
                                                       out_diagnostics_size);
}

bool fluid_volume_import_3d_load_source_at_frame(const char* path,
                                                 RuntimeVolume3DSourceKind source_kind_hint,
                                                 int requested_frame_index,
                                                 RuntimeVolumeAttachment3D* out_attachment,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!path || !path[0] || !out_attachment) {
        return false;
    }

    RuntimeVolumeAttachment3D_Reset(out_attachment);
    return fluid_volume_source_import_3d_load_internal(path,
                                                       source_kind_hint,
                                                       requested_frame_index,
                                                       out_attachment,
                                                       out_diagnostics,
                                                       out_diagnostics_size,
                                                       0);
}

bool fluid_volume_import_3d_resolve_source_frame_path(const char* path,
                                                      RuntimeVolume3DSourceKind source_kind_hint,
                                                      int requested_frame_index,
                                                      char* out_frame_path,
                                                      size_t out_frame_path_size,
                                                      char* out_diagnostics,
                                                      size_t out_diagnostics_size) {
    fluid_volume_source_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!path || !path[0] || !out_frame_path || out_frame_path_size == 0u) {
        return false;
    }
    out_frame_path[0] = '\0';
    return fluid_volume_source_import_3d_resolve_source_frame_path_internal(path,
                                                                           source_kind_hint,
                                                                           requested_frame_index,
                                                                           out_frame_path,
                                                                           out_frame_path_size,
                                                                           out_diagnostics,
                                                                           out_diagnostics_size,
                                                                           0);
}
