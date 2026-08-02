#include "import/water_surface_import.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "core_io.h"
#include "core_scene.h"

static void water_surface_import_diag(char* out_diagnostics,
                                      size_t out_diagnostics_size,
                                      const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool water_surface_import_copy_string(char* dst,
                                             size_t dst_size,
                                             const char* value) {
    if (!dst || dst_size == 0u || !value) return false;
    if (snprintf(dst, dst_size, "%s", value) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

static bool water_surface_import_read_text(const char* path, char** out_text) {
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

static bool water_surface_import_read_json(const char* path,
                                           cJSON** out_root,
                                           char* out_diagnostics,
                                           size_t out_diagnostics_size) {
    char* text = NULL;
    cJSON* root = NULL;

    if (!path || !path[0] || !out_root) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    *out_root = NULL;
    if (!water_surface_import_read_text(path, &text)) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "failed to read water surface json");
        return false;
    }
    root = cJSON_Parse(text);
    free(text);
    if (!root) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "failed to parse water surface json");
        return false;
    }
    *out_root = root;
    return true;
}

static bool water_surface_import_number(const cJSON* root,
                                        const char* key,
                                        double* out_value,
                                        bool required) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsNumber(item)) return !required;
    if (out_value) *out_value = item->valuedouble;
    return true;
}

static bool water_surface_import_uint32(const cJSON* root,
                                        const char* key,
                                        uint32_t* out_value,
                                        bool required) {
    double value = 0.0;
    if (!water_surface_import_number(root, key, &value, required)) return false;
    if (!cJSON_IsNumber(cJSON_GetObjectItem((cJSON*)root, key))) return true;
    if (!isfinite(value) || value < 0.0 || value > 4294967295.0) return false;
    if (out_value) *out_value = (uint32_t)value;
    return true;
}

static bool water_surface_import_uint64(const cJSON* root,
                                        const char* key,
                                        uint64_t* out_value,
                                        bool required) {
    double value = 0.0;
    if (!water_surface_import_number(root, key, &value, required)) return false;
    if (!cJSON_IsNumber(cJSON_GetObjectItem((cJSON*)root, key))) return true;
    if (!isfinite(value) || value < 0.0) return false;
    if (out_value) *out_value = (uint64_t)value;
    return true;
}

static bool water_surface_import_string(const cJSON* root,
                                        const char* key,
                                        char* out_value,
                                        size_t out_value_size,
                                        bool required) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsString(item) || !item->valuestring) return !required;
    return water_surface_import_copy_string(out_value, out_value_size, item->valuestring);
}

void RuntimeWaterSurfaceFrame_Init(RuntimeWaterSurfaceFrame* frame) {
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
}

void RuntimeWaterSurfaceFrame_Reset(RuntimeWaterSurfaceFrame* frame) {
    if (!frame) return;
    free(frame->heights_y);
    free(frame->normals_xyz);
    memset(frame, 0, sizeof(*frame));
}

void RuntimeWaterSurfaceFrame_Free(RuntimeWaterSurfaceFrame* frame) {
    RuntimeWaterSurfaceFrame_Reset(frame);
}

static bool water_surface_import_resolve_bundle_source(
    const char* bundle_path,
    char* out_manifest_path,
    size_t out_manifest_path_size,
    bool* out_found,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    cJSON* root = NULL;
    cJSON* water_source = NULL;
    cJSON* path_item = NULL;
    cJSON* contract = NULL;
    cJSON* representation = NULL;
    char base_dir[RUNTIME_WATER_SURFACE_PATH_MAX] = {0};
    CoreResult result = core_result_ok();

    if (out_found) *out_found = false;
    if (out_manifest_path && out_manifest_path_size > 0u) out_manifest_path[0] = '\0';
    if (!bundle_path || !out_manifest_path || out_manifest_path_size == 0u) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    if (!water_surface_import_read_json(bundle_path, &root, out_diagnostics, out_diagnostics_size)) {
        return false;
    }

    water_source = cJSON_GetObjectItem(root, "water_source");
    if (!cJSON_IsObject(water_source)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "water source not found");
        return true;
    }

    contract = cJSON_GetObjectItem(water_source, "contract");
    if (!cJSON_IsString(contract) || strcmp(contract->valuestring, "water_manifest_v1") != 0) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water source contract mismatch");
        return false;
    }

    representation = cJSON_GetObjectItem(water_source, "surface_representation");
    if (representation &&
        (!cJSON_IsString(representation) ||
         strcmp(representation->valuestring, "heightfield") != 0)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water source representation mismatch");
        return false;
    }

    path_item = cJSON_GetObjectItem(water_source, "path");
    if (!cJSON_IsString(path_item) || !path_item->valuestring || !path_item->valuestring[0]) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water source path missing");
        return false;
    }

    result = core_scene_dirname(bundle_path, base_dir, sizeof(base_dir));
    if (result.code != CORE_OK) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water source base path invalid");
        return false;
    }
    result = core_scene_resolve_path(base_dir,
                                     path_item->valuestring,
                                     out_manifest_path,
                                     out_manifest_path_size);
    cJSON_Delete(root);
    if (result.code != CORE_OK || !out_manifest_path[0]) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water source manifest path unresolved");
        return false;
    }

    if (out_found) *out_found = true;
    water_surface_import_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool water_surface_import_probe_direct_manifest(
    const char* path,
    char* out_manifest_path,
    size_t out_manifest_path_size,
    bool* out_found,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    cJSON* root = NULL;
    cJSON* schema = NULL;
    bool is_manifest = false;

    if (out_found) *out_found = false;
    if (out_manifest_path && out_manifest_path_size > 0u) out_manifest_path[0] = '\0';
    if (!path || !path[0] || !out_manifest_path || out_manifest_path_size == 0u) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    if (!water_surface_import_read_json(path, &root, out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    schema = cJSON_GetObjectItem(root, "schema");
    is_manifest = cJSON_IsString(schema) &&
                  strcmp(schema->valuestring, "physics_sim_water_manifest_v1") == 0;
    cJSON_Delete(root);
    if (!is_manifest) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface source not found");
        return true;
    }
    if (!water_surface_import_copy_string(out_manifest_path, out_manifest_path_size, path)) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest path too long");
        return false;
    }
    if (out_found) *out_found = true;
    water_surface_import_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool water_surface_import_resolve_manifest_path(
    const char* source_path,
    char* out_manifest_path,
    size_t out_manifest_path_size,
    bool* out_found,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    if (out_found) *out_found = false;
    if (!source_path || !source_path[0] || !out_manifest_path || out_manifest_path_size == 0u) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    if (core_scene_path_is_scene_bundle(source_path)) {
        return water_surface_import_resolve_bundle_source(source_path,
                                                         out_manifest_path,
                                                         out_manifest_path_size,
                                                         out_found,
                                                         out_diagnostics,
                                                         out_diagnostics_size);
    }
    return water_surface_import_probe_direct_manifest(source_path,
                                                     out_manifest_path,
                                                     out_manifest_path_size,
                                                     out_found,
                                                     out_diagnostics,
                                                     out_diagnostics_size);
}

static bool water_surface_import_parse_material(const cJSON* root,
                                                RuntimeWaterSurfaceMaterial* out_material) {
    cJSON* material = cJSON_GetObjectItem((cJSON*)root, "material");
    cJSON* roughness = NULL;
    cJSON* absorption = NULL;

    if (!out_material) return true;
    memset(out_material, 0, sizeof(*out_material));
    if (!cJSON_IsObject(material)) return true;
    if (!water_surface_import_number(material, "ior", &out_material->ior, false)) return false;
    if (!water_surface_import_number(material,
                                     "absorption_distance_m",
                                     &out_material->absorption_distance_m,
                                     false)) {
        return false;
    }
    if (!water_surface_import_number(material, "reflectivity", &out_material->reflectivity, false)) {
        return false;
    }
    roughness = cJSON_GetObjectItem(material, "roughness");
    if (!water_surface_import_number(material, "roughness", &out_material->roughness, false)) {
        return false;
    }
    out_material->roughness_authored = cJSON_IsNumber(roughness);
    absorption = cJSON_GetObjectItem(material, "absorption_rgb");
    if (cJSON_IsArray(absorption)) {
        if (cJSON_GetArraySize(absorption) != 3) return false;
        for (int i = 0; i < 3; ++i) {
            cJSON* item = cJSON_GetArrayItem(absorption, i);
            if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return false;
            out_material->absorption_rgb[i] = item->valuedouble;
        }
    }
    out_material->valid = true;
    return true;
}

static bool water_surface_import_parse_boundary_contract(
    const cJSON* root,
    bool* out_closed_volume_boundary,
    bool* out_dynamic_volume_boundary,
    double* out_boundary_height_y,
    double* out_boundary_bottom_height_y,
    char* out_boundary_shell_object_id,
    size_t out_boundary_shell_object_id_size) {
    cJSON* boundary = cJSON_GetObjectItem((cJSON*)root,
                                         "water_body_boundary_v1");
    cJSON* closure_mode = NULL;
    cJSON* dry_sample_policy = NULL;
    cJSON* shell_object_id = NULL;
    double boundary_height_y = 0.0;
    double boundary_bottom_height_y = 0.0;
    bool dynamic_volume_boundary = false;

    if (out_closed_volume_boundary) *out_closed_volume_boundary = false;
    if (out_dynamic_volume_boundary) *out_dynamic_volume_boundary = false;
    if (out_boundary_height_y) *out_boundary_height_y = 0.0;
    if (out_boundary_bottom_height_y) *out_boundary_bottom_height_y = 0.0;
    if (out_boundary_shell_object_id &&
        out_boundary_shell_object_id_size > 0u) {
        out_boundary_shell_object_id[0] = '\0';
    }
    if (!boundary) return true;
    if (!cJSON_IsObject(boundary) || !out_closed_volume_boundary ||
        !out_dynamic_volume_boundary || !out_boundary_height_y ||
        !out_boundary_bottom_height_y || !out_boundary_shell_object_id ||
        out_boundary_shell_object_id_size == 0u) {
        return false;
    }

    closure_mode = cJSON_GetObjectItem(boundary, "closure_mode");
    dry_sample_policy = cJSON_GetObjectItem(boundary, "dry_sample_policy");
    shell_object_id =
        cJSON_GetObjectItem(boundary, "legacy_shell_object_id");
    dynamic_volume_boundary =
        cJSON_IsString(closure_mode) && closure_mode->valuestring &&
        strcmp(closure_mode->valuestring, "dynamic_heightfield_volume") == 0;
    if (!cJSON_IsString(closure_mode) || !closure_mode->valuestring ||
        (strcmp(closure_mode->valuestring, "heightfield_volume") != 0 &&
         !dynamic_volume_boundary) ||
        !cJSON_IsString(dry_sample_policy) ||
        !dry_sample_policy->valuestring ||
        ((!dynamic_volume_boundary &&
          strcmp(dry_sample_policy->valuestring,
                 "surface_min_epsilon_to_base") != 0) ||
         (dynamic_volume_boundary &&
          strcmp(dry_sample_policy->valuestring,
                 "extend_interior_to_boundary") != 0)) ||
        !water_surface_import_number(boundary,
                                     "base_surface_height_m",
                                     &boundary_height_y,
                                     true) ||
        !isfinite(boundary_height_y)) {
        return false;
    }
    if (dynamic_volume_boundary) {
        if (!water_surface_import_number(boundary,
                                         "bottom_height_m",
                                         &boundary_bottom_height_y,
                                         true) ||
            !isfinite(boundary_bottom_height_y) ||
            !(boundary_bottom_height_y < boundary_height_y)) {
            return false;
        }
    } else {
        if (!cJSON_IsString(shell_object_id) || !shell_object_id->valuestring ||
            !shell_object_id->valuestring[0]) {
            return false;
        }
    }
    if (cJSON_IsString(shell_object_id) && shell_object_id->valuestring &&
        shell_object_id->valuestring[0] &&
        !water_surface_import_copy_string(out_boundary_shell_object_id,
                                          out_boundary_shell_object_id_size,
                                          shell_object_id->valuestring)) {
        return false;
    }
    *out_closed_volume_boundary = true;
    *out_dynamic_volume_boundary = dynamic_volume_boundary;
    *out_boundary_height_y = boundary_height_y;
    *out_boundary_bottom_height_y = boundary_bottom_height_y;
    return true;
}

static bool water_surface_import_parse_boundary_string(const cJSON* root,
                                                       const char* key,
                                                       char* out,
                                                       size_t out_size,
                                                       char* diagnostics,
                                                       size_t diagnostics_size) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0] ||
        !water_surface_import_copy_string(out, out_size, item->valuestring)) {
        char message[160];
        snprintf(message, sizeof(message), "water_body_boundary_v1.%s invalid", key);
        water_surface_import_diag(diagnostics, diagnostics_size, message);
        return false;
    }
    return true;
}

static bool water_surface_import_parse_boundary_number(const cJSON* root,
                                                       const char* key,
                                                       double* out,
                                                       char* diagnostics,
                                                       size_t diagnostics_size) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) {
        char message[160];
        snprintf(message, sizeof(message), "water_body_boundary_v1.%s invalid", key);
        water_surface_import_diag(diagnostics, diagnostics_size, message);
        return false;
    }
    *out = item->valuedouble;
    return true;
}

static bool water_surface_import_parse_water_body_boundary(
    const char* manifest_path,
    RuntimeWaterBodyBoundaryV1* out_boundary,
    char* diagnostics,
    size_t diagnostics_size) {
    cJSON* root = NULL;
    cJSON* boundary = NULL;
    cJSON* bounds = NULL;

    if (!out_boundary) return false;
    memset(out_boundary, 0, sizeof(*out_boundary));
    if (!water_surface_import_read_json(manifest_path, &root, diagnostics, diagnostics_size)) {
        return false;
    }
    boundary = cJSON_GetObjectItem(root, "water_body_boundary_v1");
    if (!boundary) {
        cJSON_Delete(root);
        return true;
    }
    if (!cJSON_IsObject(boundary)) {
        cJSON_Delete(root);
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 must be an object");
        return false;
    }
    bounds = cJSON_GetObjectItem(boundary, "container_inner_bounds_m");
    if (!bounds) {
        cJSON_Delete(root);
        return true;
    }
    out_boundary->present = true;
    if (!cJSON_IsObject(bounds) ||
        !water_surface_import_parse_boundary_string(boundary, "closure_mode",
                                                    out_boundary->closure_mode,
                                                    sizeof(out_boundary->closure_mode),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "body_id",
                                                    out_boundary->body_id,
                                                    sizeof(out_boundary->body_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "container_id",
                                                    out_boundary->container_id,
                                                    sizeof(out_boundary->container_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "object_id",
                                                    out_boundary->object_id,
                                                    sizeof(out_boundary->object_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "material_id",
                                                    out_boundary->material_id,
                                                    sizeof(out_boundary->material_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "medium_id",
                                                    out_boundary->medium_id,
                                                    sizeof(out_boundary->medium_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "legacy_shell_object_id",
                                                    out_boundary->legacy_shell_object_id,
                                                    sizeof(out_boundary->legacy_shell_object_id),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "dry_sample_policy",
                                                    out_boundary->dry_sample_policy,
                                                    sizeof(out_boundary->dry_sample_policy),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "solid_occluder_policy",
                                                    out_boundary->solid_occluder_policy,
                                                    sizeof(out_boundary->solid_occluder_policy),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_string(boundary, "classification_metadata",
                                                    out_boundary->classification_metadata,
                                                    sizeof(out_boundary->classification_metadata),
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "min_x", &out_boundary->min_x,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "max_x", &out_boundary->max_x,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "min_y", &out_boundary->min_y,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "max_y", &out_boundary->max_y,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "min_z", &out_boundary->min_z,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(bounds, "max_z", &out_boundary->max_z,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(boundary, "boundary_inset_m",
                                                    &out_boundary->boundary_inset_m,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(boundary, "bottom_height_m",
                                                    &out_boundary->bottom_height_m,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(boundary, "base_surface_height_m",
                                                    &out_boundary->base_surface_height_m,
                                                    diagnostics, diagnostics_size) ||
        !water_surface_import_parse_boundary_number(boundary, "dry_height_epsilon_m",
                                                    &out_boundary->dry_height_epsilon_m,
                                                    diagnostics, diagnostics_size)) {
        if (!cJSON_IsObject(bounds)) {
            water_surface_import_diag(diagnostics, diagnostics_size,
                                      "water_body_boundary_v1.container_inner_bounds_m invalid");
        }
        cJSON_Delete(root);
        return false;
    }
    if (strcmp(out_boundary->closure_mode, "heightfield_volume") != 0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1.closure_mode unsupported");
    } else if (strcmp(out_boundary->body_id, out_boundary->object_id) != 0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 body_id/object_id mismatch");
    } else if (!(out_boundary->min_x < out_boundary->max_x) ||
               !(out_boundary->min_y < out_boundary->max_y) ||
               !(out_boundary->min_z < out_boundary->max_z)) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 container bounds inverted");
    } else if (out_boundary->boundary_inset_m < 0.0 ||
               2.0 * out_boundary->boundary_inset_m >= out_boundary->max_x - out_boundary->min_x ||
               2.0 * out_boundary->boundary_inset_m >= out_boundary->max_z - out_boundary->min_z) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 boundary inset invalid");
    } else if (!(out_boundary->bottom_height_m >= out_boundary->min_y &&
                 out_boundary->bottom_height_m < out_boundary->base_surface_height_m &&
                 out_boundary->base_surface_height_m <= out_boundary->max_y)) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 base/bottom heights invalid");
    } else if (out_boundary->dry_height_epsilon_m < 0.0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 dry height epsilon invalid");
    } else if (strcmp(out_boundary->dry_sample_policy,
                      "surface_min_epsilon_to_base") != 0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 dry sample policy unsupported");
    } else if (strcmp(out_boundary->solid_occluder_policy, "ordinary_geometry_occlusion") != 0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 solid occluder policy unsupported");
    } else if (strcmp(out_boundary->classification_metadata, "legacy_height_sentinel") != 0) {
        water_surface_import_diag(diagnostics, diagnostics_size,
                                  "water_body_boundary_v1 classification metadata unsupported");
    } else {
        cJSON_Delete(root);
        water_surface_import_diag(diagnostics, diagnostics_size, "ok");
        return true;
    }
    cJSON_Delete(root);
    return false;
}

static bool water_surface_import_resolve_frame_path(
    const char* manifest_path,
    int requested_frame_index,
    char* out_frame_path,
    size_t out_frame_path_size,
    RuntimeWaterSurfaceMaterial* out_material,
    uint32_t* out_volume_grid_w,
    uint32_t* out_volume_grid_h,
    uint32_t* out_volume_grid_d,
    double* out_density_threshold,
    bool* out_closed_volume_boundary,
    bool* out_dynamic_volume_boundary,
    double* out_boundary_height_y,
    double* out_boundary_bottom_height_y,
    char* out_boundary_shell_object_id,
    size_t out_boundary_shell_object_id_size,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    cJSON* root = NULL;
    cJSON* schema = NULL;
    cJSON* representation = NULL;
    cJSON* root_contract = NULL;
    cJSON* frames = NULL;
    cJSON* frame_entry = NULL;
    cJSON* path_item = NULL;
    cJSON* frame_contract = NULL;
    char base_dir[RUNTIME_WATER_SURFACE_PATH_MAX] = {0};
    CoreResult result = core_result_ok();
    int frame_count = 0;

    if (!manifest_path || !manifest_path[0] || !out_frame_path || out_frame_path_size == 0u) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    out_frame_path[0] = '\0';
    if (!water_surface_import_read_json(manifest_path, &root, out_diagnostics, out_diagnostics_size)) {
        return false;
    }

    schema = cJSON_GetObjectItem(root, "schema");
    if (!cJSON_IsString(schema) ||
        strcmp(schema->valuestring, "physics_sim_water_manifest_v1") != 0) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest schema mismatch");
        return false;
    }

    representation = cJSON_GetObjectItem(root, "surface_representation");
    if (!cJSON_IsString(representation) ||
        strcmp(representation->valuestring, "heightfield") != 0) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest representation mismatch");
        return false;
    }

    root_contract = cJSON_GetObjectItem(root, "frame_contract");
    if (root_contract &&
        (!cJSON_IsString(root_contract) ||
         strcmp(root_contract->valuestring, "water_surface_heightfield_v1") != 0)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frame_contract mismatch");
        return false;
    }

    if (!water_surface_import_parse_material(root, out_material) ||
        !water_surface_import_parse_boundary_contract(
            root,
            out_closed_volume_boundary,
            out_dynamic_volume_boundary,
            out_boundary_height_y,
            out_boundary_bottom_height_y,
            out_boundary_shell_object_id,
            out_boundary_shell_object_id_size) ||
        !water_surface_import_uint32(root, "volume_grid_w", out_volume_grid_w, false) ||
        !water_surface_import_uint32(root, "volume_grid_h", out_volume_grid_h, false) ||
        !water_surface_import_uint32(root, "volume_grid_d", out_volume_grid_d, false) ||
        !water_surface_import_number(root, "density_threshold", out_density_threshold, false)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest metadata invalid");
        return false;
    }

    frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsArray(frames)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frames missing");
        return false;
    }
    frame_count = cJSON_GetArraySize(frames);
    if (frame_count <= 0) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frames empty");
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
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frame entry invalid");
        return false;
    }
    frame_contract = cJSON_GetObjectItem(frame_entry, "frame_contract");
    if (frame_contract &&
        (!cJSON_IsString(frame_contract) ||
         strcmp(frame_contract->valuestring, "water_surface_heightfield_v1") != 0)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frame entry contract mismatch");
        return false;
    }
    path_item = cJSON_GetObjectItem(frame_entry, "path");
    if (!cJSON_IsString(path_item) || !path_item->valuestring || !path_item->valuestring[0]) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frame path missing");
        return false;
    }

    result = core_scene_dirname(manifest_path, base_dir, sizeof(base_dir));
    if (result.code != CORE_OK) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest base path invalid");
        return false;
    }
    result = core_scene_resolve_path(base_dir,
                                     path_item->valuestring,
                                     out_frame_path,
                                     out_frame_path_size);
    cJSON_Delete(root);
    if (result.code != CORE_OK || !out_frame_path[0]) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest frame path unresolved");
        return false;
    }

    water_surface_import_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool water_surface_import_copy_float_array(cJSON* array,
                                                  uint64_t expected_count,
                                                  float** out_values) {
    float* values = NULL;
    if (!cJSON_IsArray(array) || !out_values) return false;
    if ((uint64_t)cJSON_GetArraySize(array) != expected_count) return false;
    values = (float*)calloc((size_t)expected_count, sizeof(*values));
    if (!values) return false;
    for (uint64_t i = 0u; i < expected_count; ++i) {
        cJSON* item = cJSON_GetArrayItem(array, (int)i);
        if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) {
            free(values);
            return false;
        }
        values[i] = (float)item->valuedouble;
    }
    *out_values = values;
    return true;
}

static bool water_surface_import_parse_summary(cJSON* root,
                                               RuntimeWaterSurfaceFrame* out_frame) {
    cJSON* summary = cJSON_GetObjectItem(root, "summary");
    cJSON* finite_normals = NULL;
    if (!cJSON_IsObject(summary)) return true;
    if (!water_surface_import_uint32(summary, "wet_columns", &out_frame->wet_columns, false) ||
        !water_surface_import_uint32(summary, "dry_columns", &out_frame->dry_columns, false) ||
        !water_surface_import_uint32(summary, "solid_columns", &out_frame->solid_columns, false) ||
        !water_surface_import_uint32(summary, "water_cells", &out_frame->water_cells, false) ||
        !water_surface_import_number(summary, "surface_min_y", &out_frame->surface_min_y, false) ||
        !water_surface_import_number(summary, "surface_max_y", &out_frame->surface_max_y, false) ||
        !water_surface_import_number(summary, "surface_avg_y", &out_frame->surface_avg_y, false) ||
        !water_surface_import_number(summary, "max_slope", &out_frame->max_slope, false)) {
        return false;
    }
    finite_normals = cJSON_GetObjectItem(summary, "finite_normals");
    if (cJSON_IsBool(finite_normals)) {
        out_frame->finite_normals = cJSON_IsTrue(finite_normals);
    }
    return true;
}

static bool water_surface_import_load_frame_json(
    const char* frame_path,
    RuntimeWaterSurfaceFrame* out_frame,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    cJSON* root = NULL;
    cJSON* schema = NULL;
    cJSON* heights = NULL;
    cJSON* normals = NULL;
    uint64_t expected_samples = 0u;
    uint64_t expected_normals = 0u;

    if (!frame_path || !frame_path[0] || !out_frame) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    if (!water_surface_import_read_json(frame_path, &root, out_diagnostics, out_diagnostics_size)) {
        return false;
    }

    schema = cJSON_GetObjectItem(root, "schema");
    if (!cJSON_IsString(schema) ||
        strcmp(schema->valuestring, "physics_sim_water_surface_heightfield_v1") != 0) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface frame schema mismatch");
        return false;
    }

    if (!water_surface_import_copy_string(out_frame->schema,
                                          sizeof(out_frame->schema),
                                          schema->valuestring) ||
        !water_surface_import_string(root,
                                     "frame_contract",
                                     out_frame->frame_contract,
                                     sizeof(out_frame->frame_contract),
                                     false) ||
        !water_surface_import_string(root,
                                     "surface_axis",
                                     out_frame->surface_axis,
                                     sizeof(out_frame->surface_axis),
                                     true) ||
        strcmp(out_frame->surface_axis, "y") != 0 ||
        !water_surface_import_string(root,
                                     "layout",
                                     out_frame->layout,
                                     sizeof(out_frame->layout),
                                     true) ||
        strcmp(out_frame->layout, "row_major_z_x") != 0 ||
        !water_surface_import_uint64(root, "frame_index", &out_frame->frame_index, true) ||
        !water_surface_import_number(root, "time_seconds", &out_frame->time_seconds, true) ||
        !water_surface_import_number(root, "dt_seconds", &out_frame->dt_seconds, true) ||
        !water_surface_import_uint32(root, "grid_w", &out_frame->grid_w, true) ||
        !water_surface_import_uint32(root, "grid_d", &out_frame->grid_d, true) ||
        !water_surface_import_uint64(root, "sample_count", &out_frame->sample_count, true) ||
        !water_surface_import_number(root, "origin_x", &out_frame->origin_x, false) ||
        !water_surface_import_number(root, "origin_y", &out_frame->origin_y, false) ||
        !water_surface_import_number(root, "origin_z", &out_frame->origin_z, false) ||
        !water_surface_import_number(root, "sample_origin_x", &out_frame->sample_origin_x, true) ||
        !water_surface_import_number(root, "sample_origin_z", &out_frame->sample_origin_z, true) ||
        !water_surface_import_number(root, "sample_spacing_x", &out_frame->sample_spacing_x, true) ||
        !water_surface_import_number(root, "sample_spacing_z", &out_frame->sample_spacing_z, true) ||
        !water_surface_import_parse_summary(root, out_frame)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface frame metadata invalid");
        return false;
    }

    expected_samples = (uint64_t)out_frame->grid_w * (uint64_t)out_frame->grid_d;
    expected_normals = expected_samples * 3u;
    if (out_frame->grid_w < 2u ||
        out_frame->grid_d < 2u ||
        out_frame->sample_count != expected_samples ||
        !(out_frame->sample_spacing_x > 0.0) ||
        !(out_frame->sample_spacing_z > 0.0)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface frame grid invalid");
        return false;
    }

    heights = cJSON_GetObjectItem(root, "heights_y");
    normals = cJSON_GetObjectItem(root, "normals_xyz");
    if (!water_surface_import_copy_float_array(heights,
                                               expected_samples,
                                               &out_frame->heights_y) ||
        !water_surface_import_copy_float_array(normals,
                                               expected_normals,
                                               &out_frame->normals_xyz)) {
        cJSON_Delete(root);
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface frame sample arrays invalid");
        return false;
    }

    cJSON_Delete(root);
    if (!water_surface_import_copy_string(out_frame->frame_path,
                                          sizeof(out_frame->frame_path),
                                          frame_path)) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water surface frame path too long");
        return false;
    }
    out_frame->valid = true;
    water_surface_import_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool RuntimeWaterSurfaceImport_LoadSourceAtFrame(const char* source_path,
                                                 int requested_frame_index,
                                                 RuntimeWaterSurfaceFrame* out_frame,
                                                 bool* out_found,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    char manifest_path[RUNTIME_WATER_SURFACE_PATH_MAX] = {0};
    char frame_path[RUNTIME_WATER_SURFACE_PATH_MAX] = {0};
    RuntimeWaterSurfaceMaterial material = {0};
    uint32_t volume_grid_w = 0u;
    uint32_t volume_grid_h = 0u;
    uint32_t volume_grid_d = 0u;
    double density_threshold = 0.0;
    bool found = false;

    if (out_found) *out_found = false;
    water_surface_import_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!source_path || !source_path[0] || !out_frame) {
        return false;
    }

    RuntimeWaterSurfaceFrame_Reset(out_frame);
    if (!water_surface_import_resolve_manifest_path(source_path,
                                                    manifest_path,
                                                    sizeof(manifest_path),
                                                    &found,
                                                    out_diagnostics,
                                                    out_diagnostics_size)) {
        return false;
    }
    if (!found) {
        if (out_found) *out_found = false;
        return true;
    }

    if (!water_surface_import_resolve_frame_path(manifest_path,
                                                 requested_frame_index,
                                                 frame_path,
                                                 sizeof(frame_path),
                                                 &material,
                                                 &volume_grid_w,
                                                 &volume_grid_h,
                                                 &volume_grid_d,
                                                 &density_threshold,
                                                 &out_frame->closed_volume_boundary,
                                                 &out_frame->dynamic_volume_boundary,
                                                 &out_frame->boundary_height_y,
                                                 &out_frame->boundary_bottom_height_y,
                                                 out_frame->boundary_shell_object_id,
                                                 sizeof(out_frame->boundary_shell_object_id),
                                                 out_diagnostics,
                                                 out_diagnostics_size)) {
        return false;
    }

    if (!water_surface_import_copy_string(out_frame->source_manifest_path,
                                          sizeof(out_frame->source_manifest_path),
                                          manifest_path)) {
        water_surface_import_diag(out_diagnostics, out_diagnostics_size,
                                  "water manifest path too long");
        return false;
    }
    out_frame->material = material;
    out_frame->volume_grid_w = volume_grid_w;
    out_frame->volume_grid_h = volume_grid_h;
    out_frame->volume_grid_d = volume_grid_d;
    out_frame->density_threshold = density_threshold;

    if (!water_surface_import_parse_water_body_boundary(
            manifest_path,
            &out_frame->water_body_boundary,
            out_diagnostics,
            out_diagnostics_size)) {
        RuntimeWaterSurfaceFrame_Reset(out_frame);
        return false;
    }

    if (!water_surface_import_load_frame_json(frame_path,
                                              out_frame,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
        RuntimeWaterSurfaceFrame_Reset(out_frame);
        return false;
    }

    if (out_found) *out_found = true;
    return true;
}
