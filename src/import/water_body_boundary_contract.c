#include "water_body_boundary_contract.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void boundary_diag(char* out, size_t out_size, const char* message) {
    if (out && out_size > 0u) {
        snprintf(out, out_size, "%s", message ? message : "unknown");
    }
}

static bool boundary_copy_string(char* out, size_t out_size, const char* value) {
    if (!out || out_size == 0u || !value || !value[0]) return false;
    if (snprintf(out, out_size, "%s", value) >= (int)out_size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

static bool boundary_parse_string(const cJSON* root,
                                  const char* key,
                                  char* out,
                                  size_t out_size,
                                  bool required,
                                  char* diagnostics,
                                  size_t diagnostics_size) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0]) {
        if (!required) {
            if (out && out_size > 0u) out[0] = '\0';
            return true;
        }
        char message[160];
        snprintf(message, sizeof(message), "water_body_boundary_v1.%s invalid", key);
        boundary_diag(diagnostics, diagnostics_size, message);
        return false;
    }
    if (!boundary_copy_string(out, out_size, item->valuestring)) {
        char message[160];
        snprintf(message, sizeof(message), "water_body_boundary_v1.%s invalid", key);
        boundary_diag(diagnostics, diagnostics_size, message);
        return false;
    }
    return true;
}

static bool boundary_parse_number(const cJSON* root,
                                  const char* key,
                                  double* out,
                                  char* diagnostics,
                                  size_t diagnostics_size) {
    cJSON* item = cJSON_GetObjectItem((cJSON*)root, key);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) {
        char message[160];
        snprintf(message, sizeof(message), "water_body_boundary_v1.%s invalid", key);
        boundary_diag(diagnostics, diagnostics_size, message);
        return false;
    }
    *out = item->valuedouble;
    return true;
}

static bool boundary_policy_valid(const RuntimeWaterBodyBoundaryV1* boundary,
                                  bool dynamic,
                                  char* diagnostics,
                                  size_t diagnostics_size) {
    const char* expected_dry_policy = dynamic
                                          ? "extend_interior_to_boundary"
                                          : "surface_min_epsilon_to_base";
    const char* expected_classification = dynamic
                                              ? "dynamic_perimeter_inherits_interior_height"
                                              : "legacy_height_sentinel";
    if (strcmp(boundary->dry_sample_policy, expected_dry_policy) != 0) {
        boundary_diag(diagnostics, diagnostics_size,
                      "water_body_boundary_v1 dry sample policy unsupported");
        return false;
    }
    if (strcmp(boundary->solid_occluder_policy, "ordinary_geometry_occlusion") != 0) {
        boundary_diag(diagnostics, diagnostics_size,
                      "water_body_boundary_v1 solid occluder policy unsupported");
        return false;
    }
    if (strcmp(boundary->classification_metadata, expected_classification) != 0) {
        boundary_diag(diagnostics, diagnostics_size,
                      "water_body_boundary_v1 classification metadata unsupported");
        return false;
    }
    return true;
}

bool WaterBodyBoundaryContract_Parse(
    const cJSON* manifest_root,
    RuntimeWaterBodyBoundaryV1* out_boundary,
    bool* out_closed_volume_boundary,
    bool* out_dynamic_volume_boundary,
    double* out_boundary_height_y,
    double* out_boundary_bottom_height_y,
    char* out_boundary_shell_object_id,
    size_t out_boundary_shell_object_id_size,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    cJSON* boundary = NULL;
    cJSON* bounds = NULL;
    bool dynamic = false;
    bool has_full_contract = false;

    if (!manifest_root || !out_boundary || !out_closed_volume_boundary ||
        !out_dynamic_volume_boundary || !out_boundary_height_y ||
        !out_boundary_bottom_height_y || !out_boundary_shell_object_id ||
        out_boundary_shell_object_id_size == 0u) {
        boundary_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    memset(out_boundary, 0, sizeof(*out_boundary));
    *out_closed_volume_boundary = false;
    *out_dynamic_volume_boundary = false;
    *out_boundary_height_y = 0.0;
    *out_boundary_bottom_height_y = 0.0;
    out_boundary_shell_object_id[0] = '\0';

    boundary = cJSON_GetObjectItem((cJSON*)manifest_root, "water_body_boundary_v1");
    if (!boundary) return true;
    if (!cJSON_IsObject(boundary)) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 must be an object");
        return false;
    }
    if (!boundary_parse_string(boundary, "closure_mode",
                               out_boundary->closure_mode,
                               sizeof(out_boundary->closure_mode), true,
                               out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    dynamic = strcmp(out_boundary->closure_mode, "dynamic_heightfield_volume") == 0;
    if (!dynamic && strcmp(out_boundary->closure_mode, "heightfield_volume") != 0) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1.closure_mode unsupported");
        return false;
    }
    if (!boundary_parse_string(boundary, "dry_sample_policy",
                               out_boundary->dry_sample_policy,
                               sizeof(out_boundary->dry_sample_policy), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(boundary, "base_surface_height_m",
                               &out_boundary->base_surface_height_m,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "legacy_shell_object_id",
                               out_boundary->legacy_shell_object_id,
                               sizeof(out_boundary->legacy_shell_object_id), !dynamic,
                               out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    if (dynamic &&
        !boundary_parse_number(boundary, "bottom_height_m",
                               &out_boundary->bottom_height_m,
                               out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    if (!dynamic) out_boundary->bottom_height_m = 0.0;
    if (!boundary_copy_string(out_boundary_shell_object_id,
                              out_boundary_shell_object_id_size,
                              out_boundary->legacy_shell_object_id) &&
        out_boundary->legacy_shell_object_id[0]) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1.legacy_shell_object_id invalid");
        return false;
    }
    *out_closed_volume_boundary = true;
    *out_dynamic_volume_boundary = dynamic;
    *out_boundary_height_y = out_boundary->base_surface_height_m;
    *out_boundary_bottom_height_y = out_boundary->bottom_height_m;

    bounds = cJSON_GetObjectItem(boundary, "container_inner_bounds_m");
    if (!bounds) return true;
    has_full_contract = cJSON_IsObject(bounds);
    if (!has_full_contract) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1.container_inner_bounds_m invalid");
        return false;
    }
    out_boundary->present = true;
    if (!boundary_parse_string(boundary, "body_id", out_boundary->body_id,
                               sizeof(out_boundary->body_id), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "container_id", out_boundary->container_id,
                               sizeof(out_boundary->container_id), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "object_id", out_boundary->object_id,
                               sizeof(out_boundary->object_id), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "material_id", out_boundary->material_id,
                               sizeof(out_boundary->material_id), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "medium_id", out_boundary->medium_id,
                               sizeof(out_boundary->medium_id), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "solid_occluder_policy",
                               out_boundary->solid_occluder_policy,
                               sizeof(out_boundary->solid_occluder_policy), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_string(boundary, "classification_metadata",
                               out_boundary->classification_metadata,
                               sizeof(out_boundary->classification_metadata), true,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "min_x", &out_boundary->min_x,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "max_x", &out_boundary->max_x,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "min_y", &out_boundary->min_y,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "max_y", &out_boundary->max_y,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "min_z", &out_boundary->min_z,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(bounds, "max_z", &out_boundary->max_z,
                               out_diagnostics, out_diagnostics_size) ||
        !boundary_parse_number(boundary, "boundary_inset_m",
                               &out_boundary->boundary_inset_m,
                               out_diagnostics, out_diagnostics_size) ||
        (!dynamic &&
         !boundary_parse_number(boundary, "bottom_height_m",
                                &out_boundary->bottom_height_m,
                                out_diagnostics, out_diagnostics_size)) ||
        !boundary_parse_number(boundary, "dry_height_epsilon_m",
                               &out_boundary->dry_height_epsilon_m,
                               out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    *out_boundary_bottom_height_y = out_boundary->bottom_height_m;
    if (strcmp(out_boundary->body_id, out_boundary->object_id) != 0) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 body_id/object_id mismatch");
    } else if (!(out_boundary->min_x < out_boundary->max_x) ||
               !(out_boundary->min_y < out_boundary->max_y) ||
               !(out_boundary->min_z < out_boundary->max_z)) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 container bounds inverted");
    } else if (out_boundary->boundary_inset_m < 0.0 ||
               2.0 * out_boundary->boundary_inset_m >= out_boundary->max_x - out_boundary->min_x ||
               2.0 * out_boundary->boundary_inset_m >= out_boundary->max_z - out_boundary->min_z) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 boundary inset invalid");
    } else if (!(out_boundary->bottom_height_m >= out_boundary->min_y &&
                 out_boundary->bottom_height_m < out_boundary->base_surface_height_m &&
                 out_boundary->base_surface_height_m <= out_boundary->max_y)) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 base/bottom heights invalid");
    } else if (out_boundary->dry_height_epsilon_m < 0.0) {
        boundary_diag(out_diagnostics, out_diagnostics_size,
                      "water_body_boundary_v1 dry height epsilon invalid");
    } else if (!boundary_policy_valid(out_boundary, dynamic,
                                      out_diagnostics, out_diagnostics_size)) {
        return false;
    } else {
        boundary_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }
    return false;
}
