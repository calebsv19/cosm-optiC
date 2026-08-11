#include "procedural/procedural_surface_binding.h"

#include "app/ray_tracing_sha256.h"

#include <json-c/json.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void report_set(
    ProceduralSurfaceBindingReport *report,
    ProceduralSurfaceBindingStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static double clamp01(double value) {
    return fmin(1.0, fmax(0.0, value));
}

static double smoothstep01(double value) {
    const double t = clamp01(value);
    return t * t * (3.0 - (2.0 * t));
}

static double point_dot(
    ProceduralSurfaceFieldPoint3D a,
    ProceduralSurfaceFieldPoint3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static double point_length(ProceduralSurfaceFieldPoint3D value) {
    return sqrt(point_dot(value, value));
}

static bool point_normalize(
    ProceduralSurfaceFieldPoint3D value,
    ProceduralSurfaceFieldPoint3D *out) {
    const double length = point_length(value);
    if (!out || !(length > 1.0e-15) || !isfinite(length)) return false;
    *out = (ProceduralSurfaceFieldPoint3D){
        value.x / length, value.y / length, value.z / length};
    return true;
}

static bool id_valid(const char *id, size_t capacity) {
    size_t length;
    if (!id || !id[0]) return false;
    length = strlen(id);
    if (length >= capacity) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)id[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

const char *ProceduralSurfaceSelectorKind_Name(
    ProceduralSurfaceSelectorKind selector) {
    switch (selector) {
        case PROCEDURAL_SURFACE_SELECTOR_ALL: return "all";
        case PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP:
            return "surface_group";
        case PROCEDURAL_SURFACE_SELECTOR_UPWARD_FACING:
            return "upward_facing";
        case PROCEDURAL_SURFACE_SELECTOR_INVALID: break;
    }
    return "invalid";
}

const char *ProceduralSurfaceProjectionKind_Name(
    ProceduralSurfaceProjectionKind projection) {
    switch (projection) {
        case PROCEDURAL_SURFACE_PROJECTION_OBJECT_3D: return "object_3d";
        case PROCEDURAL_SURFACE_PROJECTION_PLANAR_XY: return "planar_xy";
        case PROCEDURAL_SURFACE_PROJECTION_PLANAR_XZ: return "planar_xz";
        case PROCEDURAL_SURFACE_PROJECTION_PLANAR_YZ: return "planar_yz";
        case PROCEDURAL_SURFACE_PROJECTION_INVALID: break;
    }
    return "invalid";
}

const char *ProceduralSurfaceDisplacementDirection_Name(
    ProceduralSurfaceDisplacementDirection direction) {
    switch (direction) {
        case PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL:
            return "source_normal";
        case PROCEDURAL_SURFACE_DISPLACEMENT_SMOOTH_NORMAL:
            return "smooth_normal";
        case PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP:
            return "world_up";
        case PROCEDURAL_SURFACE_DISPLACEMENT_INVALID:
            break;
    }
    return "invalid";
}

const char *ProceduralSurfaceBindingStatus_Name(
    ProceduralSurfaceBindingStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_BINDING_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_BINDING_STATUS_IO: return "io";
        case PROCEDURAL_SURFACE_BINDING_STATUS_JSON: return "json";
        case PROCEDURAL_SURFACE_BINDING_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SURFACE_BINDING_STATUS_SELECTOR: return "selector";
        case PROCEDURAL_SURFACE_BINDING_STATUS_PROJECTION:
            return "projection";
        case PROCEDURAL_SURFACE_BINDING_STATUS_GRAPH: return "graph";
        case PROCEDURAL_SURFACE_BINDING_STATUS_EVALUATION:
            return "evaluation";
        case PROCEDURAL_SURFACE_BINDING_STATUS_CANONICALIZATION:
            return "canonicalization";
    }
    return "unknown";
}

void ProceduralSurfaceBindingV1_Init(ProceduralSurfaceBindingV1 *binding) {
    if (!binding) return;
    memset(binding, 0, sizeof(*binding));
    binding->schema_version = PROCEDURAL_SURFACE_BINDING_SCHEMA_VERSION;
    binding->selector = PROCEDURAL_SURFACE_SELECTOR_ALL;
    binding->up_axis = (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0};
    binding->selector_min_dot = 0.5;
    binding->selector_feather = 0.1;
    binding->projection = PROCEDURAL_SURFACE_PROJECTION_OBJECT_3D;
    binding->projection_scale = 1.0;
    binding->displacement_direction =
        PROCEDURAL_SURFACE_DISPLACEMENT_SMOOTH_NORMAL;
    binding->displacement_scale = 1.0;
    binding->fallback_color_r = 0.4;
    binding->fallback_color_g = 0.4;
    binding->fallback_color_b = 0.4;
    binding->fallback_roughness = 0.8;
}

bool ProceduralSurfaceBindingV1_Validate(
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceBindingReport *report) {
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceFieldPoint3D up;
    report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_OK, "", "ok");
    if (!binding || !graph) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
                   "arguments", "binding and graph are required");
        return false;
    }
    if (binding->schema_version != PROCEDURAL_SURFACE_BINDING_SCHEMA_VERSION ||
        !id_valid(binding->binding_id, sizeof(binding->binding_id)) ||
        !id_valid(binding->graph_program_id,
                  sizeof(binding->graph_program_id)) ||
        strcmp(binding->graph_program_id, graph->program_id) != 0) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_SCHEMA,
                   "binding", "binding schema or graph identity is invalid");
        return false;
    }
    if (binding->selector != PROCEDURAL_SURFACE_SELECTOR_ALL &&
        binding->selector != PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP &&
        binding->selector != PROCEDURAL_SURFACE_SELECTOR_UPWARD_FACING) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_SELECTOR,
                   "selector", "selector kind is unsupported");
        return false;
    }
    if (binding->selector == PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP &&
        !id_valid(binding->surface_group_id,
                  sizeof(binding->surface_group_id))) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_SELECTOR,
                   "surface_group_id", "surface-group selector needs an id");
        return false;
    }
    if (!point_normalize(binding->up_axis, &up) ||
        !isfinite(binding->selector_min_dot) ||
        binding->selector_min_dot < -1.0 ||
        binding->selector_min_dot > 1.0 ||
        !isfinite(binding->selector_feather) ||
        binding->selector_feather < 0.0 ||
        binding->selector_feather > 2.0) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_SELECTOR,
                   "selector", "selector orientation parameters are invalid");
        return false;
    }
    if (binding->projection < PROCEDURAL_SURFACE_PROJECTION_OBJECT_3D ||
        binding->projection > PROCEDURAL_SURFACE_PROJECTION_PLANAR_YZ ||
        !isfinite(binding->projection_scale) ||
        !(binding->projection_scale > 0.0)) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_PROJECTION,
                   "projection", "projection parameters are invalid");
        return false;
    }
    if (binding->displacement_direction <
            PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL ||
        binding->displacement_direction >
            PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP ||
        !isfinite(binding->displacement_scale) ||
        binding->displacement_scale < 0.0 ||
        !isfinite(binding->fallback_color_r) ||
        !isfinite(binding->fallback_color_g) ||
        !isfinite(binding->fallback_color_b) ||
        !isfinite(binding->fallback_roughness) ||
        binding->fallback_color_r < 0.0 ||
        binding->fallback_color_r > 1.0 ||
        binding->fallback_color_g < 0.0 ||
        binding->fallback_color_g > 1.0 ||
        binding->fallback_color_b < 0.0 ||
        binding->fallback_color_b > 1.0 ||
        binding->fallback_roughness < 0.0 ||
        binding->fallback_roughness > 1.0) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_SCHEMA,
                   "material", "displacement or fallback values are invalid");
        return false;
    }
    if (!ProceduralSurfaceFieldGraphV1_Validate(graph, &graph_report)) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_GRAPH,
                   graph_report.field, graph_report.message);
        return false;
    }
    return true;
}

static ProceduralSurfaceSelectorKind parse_selector(const char *text) {
    if (!text) return PROCEDURAL_SURFACE_SELECTOR_INVALID;
    if (strcmp(text, "all") == 0) return PROCEDURAL_SURFACE_SELECTOR_ALL;
    if (strcmp(text, "surface_group") == 0) {
        return PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP;
    }
    if (strcmp(text, "upward_facing") == 0) {
        return PROCEDURAL_SURFACE_SELECTOR_UPWARD_FACING;
    }
    return PROCEDURAL_SURFACE_SELECTOR_INVALID;
}

static ProceduralSurfaceProjectionKind parse_projection(const char *text) {
    if (!text) return PROCEDURAL_SURFACE_PROJECTION_INVALID;
    if (strcmp(text, "object_3d") == 0) {
        return PROCEDURAL_SURFACE_PROJECTION_OBJECT_3D;
    }
    if (strcmp(text, "planar_xy") == 0) {
        return PROCEDURAL_SURFACE_PROJECTION_PLANAR_XY;
    }
    if (strcmp(text, "planar_xz") == 0) {
        return PROCEDURAL_SURFACE_PROJECTION_PLANAR_XZ;
    }
    if (strcmp(text, "planar_yz") == 0) {
        return PROCEDURAL_SURFACE_PROJECTION_PLANAR_YZ;
    }
    return PROCEDURAL_SURFACE_PROJECTION_INVALID;
}

static ProceduralSurfaceDisplacementDirection parse_direction(
    const char *text) {
    if (!text) return PROCEDURAL_SURFACE_DISPLACEMENT_INVALID;
    if (strcmp(text, "source_normal") == 0) {
        return PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL;
    }
    if (strcmp(text, "smooth_normal") == 0) {
        return PROCEDURAL_SURFACE_DISPLACEMENT_SMOOTH_NORMAL;
    }
    if (strcmp(text, "world_up") == 0) {
        return PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP;
    }
    return PROCEDURAL_SURFACE_DISPLACEMENT_INVALID;
}

static bool json_text(
    struct json_object *object,
    const char *key,
    char *out,
    size_t capacity) {
    struct json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool json_number(
    struct json_object *object,
    const char *key,
    double *out) {
    struct json_object *value = NULL;
    enum json_type type;
    if (!json_object_object_get_ex(object, key, &value)) return false;
    type = json_object_get_type(value);
    if (type != json_type_double && type != json_type_int) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static bool json_vec3(
    struct json_object *object,
    const char *key,
    ProceduralSurfaceFieldPoint3D *out) {
    struct json_object *array = NULL;
    if (!json_object_object_get_ex(object, key, &array) ||
        json_object_get_type(array) != json_type_array ||
        json_object_array_length(array) != 3u) return false;
    for (size_t i = 0u; i < 3u; ++i) {
        struct json_object *value = json_object_array_get_idx(array, i);
        enum json_type type;
        double parsed;
        if (!value) return false;
        type = json_object_get_type(value);
        if (type != json_type_double && type != json_type_int) return false;
        parsed = json_object_get_double(value);
        if (!isfinite(parsed)) return false;
        if (i == 0u) out->x = parsed;
        if (i == 1u) out->y = parsed;
        if (i == 2u) out->z = parsed;
    }
    return true;
}

bool ProceduralSurfaceBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceBindingV1 *out_binding,
    ProceduralSurfaceBindingReport *report) {
    struct json_object *root = NULL;
    struct json_object *value = NULL;
    ProceduralSurfaceBindingV1 binding;
    bool result = false;
    report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_OK, "", "ok");
    if (!path || !out_binding) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
                   "path", "binding path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_IO,
                   "path", "unable to parse binding JSON");
        return false;
    }
    ProceduralSurfaceBindingV1_Init(&binding);
    if (!json_object_object_get_ex(root, "schema", &value) ||
        json_object_get_type(value) != json_type_string ||
        strcmp(json_object_get_string(value),
               PROCEDURAL_SURFACE_BINDING_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_type(value) != json_type_int ||
        json_object_get_int64(value) !=
            PROCEDURAL_SURFACE_BINDING_SCHEMA_VERSION ||
        !json_text(root, "binding_id", binding.binding_id,
                   sizeof(binding.binding_id)) ||
        !json_text(root, "graph_program_id", binding.graph_program_id,
                   sizeof(binding.graph_program_id)) ||
        !json_object_object_get_ex(root, "selector", &value) ||
        json_object_get_type(value) != json_type_string ||
        (binding.selector =
             parse_selector(json_object_get_string(value))) ==
            PROCEDURAL_SURFACE_SELECTOR_INVALID ||
        !json_text(root, "surface_group_id", binding.surface_group_id,
                   sizeof(binding.surface_group_id)) ||
        !json_vec3(root, "up_axis", &binding.up_axis) ||
        !json_number(root, "selector_min_dot",
                     &binding.selector_min_dot) ||
        !json_number(root, "selector_feather",
                     &binding.selector_feather) ||
        !json_object_object_get_ex(root, "projection", &value) ||
        json_object_get_type(value) != json_type_string ||
        (binding.projection =
             parse_projection(json_object_get_string(value))) ==
            PROCEDURAL_SURFACE_PROJECTION_INVALID ||
        !json_number(root, "projection_scale",
                     &binding.projection_scale) ||
        !json_object_object_get_ex(root, "displacement_direction", &value) ||
        json_object_get_type(value) != json_type_string ||
        (binding.displacement_direction =
             parse_direction(json_object_get_string(value))) ==
            PROCEDURAL_SURFACE_DISPLACEMENT_INVALID ||
        !json_number(root, "displacement_scale",
                     &binding.displacement_scale) ||
        !json_number(root, "fallback_color_r",
                     &binding.fallback_color_r) ||
        !json_number(root, "fallback_color_g",
                     &binding.fallback_color_g) ||
        !json_number(root, "fallback_color_b",
                     &binding.fallback_color_b) ||
        !json_number(root, "fallback_roughness",
                     &binding.fallback_roughness)) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_JSON,
                   "root", "binding JSON is invalid");
        goto cleanup;
    }
    *out_binding = binding;
    result = true;
cleanup:
    json_object_put(root);
    return result;
}

bool ProceduralSurfaceBindingV1_CanonicalJson(
    const ProceduralSurfaceBindingV1 *binding,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceBindingReport *report) {
    int written;
    report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_OK, "", "ok");
    if (!binding || !out_json || out_capacity == 0u) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
                   "canonical", "binding and output buffer are required");
        return false;
    }
    written = snprintf(
        out_json, out_capacity,
        "{\"schema\":\"%s\",\"schema_version\":%u,"
        "\"binding_id\":\"%s\",\"graph_program_id\":\"%s\","
        "\"selector\":\"%s\",\"surface_group_id\":\"%s\","
        "\"up_axis\":[%.17g,%.17g,%.17g],"
        "\"selector_min_dot\":%.17g,\"selector_feather\":%.17g,"
        "\"projection\":\"%s\",\"projection_scale\":%.17g,"
        "\"displacement_direction\":\"%s\","
        "\"displacement_scale\":%.17g,"
        "\"fallback_color_r\":%.17g,\"fallback_color_g\":%.17g,"
        "\"fallback_color_b\":%.17g,\"fallback_roughness\":%.17g}",
        PROCEDURAL_SURFACE_BINDING_SCHEMA, binding->schema_version,
        binding->binding_id, binding->graph_program_id,
        ProceduralSurfaceSelectorKind_Name(binding->selector),
        binding->surface_group_id, binding->up_axis.x, binding->up_axis.y,
        binding->up_axis.z, binding->selector_min_dot,
        binding->selector_feather,
        ProceduralSurfaceProjectionKind_Name(binding->projection),
        binding->projection_scale,
        ProceduralSurfaceDisplacementDirection_Name(
            binding->displacement_direction),
        binding->displacement_scale, binding->fallback_color_r,
        binding->fallback_color_g, binding->fallback_color_b,
        binding->fallback_roughness);
    if (written < 0 || (size_t)written >= out_capacity) {
        report_set(report,
                   PROCEDURAL_SURFACE_BINDING_STATUS_CANONICALIZATION,
                   "canonical", "canonical binding exceeds output capacity");
        return false;
    }
    return true;
}

bool ProceduralSurfaceBindingV1_Digest(
    const ProceduralSurfaceBindingV1 *binding,
    char out_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY],
    ProceduralSurfaceBindingReport *report) {
    char canonical[4096];
    if (!out_digest ||
        !ProceduralSurfaceBindingV1_CanonicalJson(
            binding, canonical, sizeof(canonical), report)) return false;
    if (!ray_tracing_sha256_bytes(
            canonical, strlen(canonical), out_digest)) {
        report_set(report,
                   PROCEDURAL_SURFACE_BINDING_STATUS_CANONICALIZATION,
                   "digest", "unable to digest canonical binding");
        return false;
    }
    return true;
}

static double selector_weight(
    const ProceduralSurfaceBindingV1 *binding,
    ProceduralSurfaceFieldPoint3D source_normal,
    const char *surface_group_id) {
    ProceduralSurfaceFieldPoint3D normal;
    ProceduralSurfaceFieldPoint3D up;
    if (binding->selector == PROCEDURAL_SURFACE_SELECTOR_ALL) return 1.0;
    if (binding->selector == PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP) {
        return surface_group_id &&
                       strcmp(binding->surface_group_id, surface_group_id) == 0
                   ? 1.0
                   : 0.0;
    }
    if (!point_normalize(source_normal, &normal) ||
        !point_normalize(binding->up_axis, &up)) return 0.0;
    {
        const double dot = point_dot(normal, up);
        if (binding->selector_feather <= 1.0e-15) {
            return dot >= binding->selector_min_dot ? 1.0 : 0.0;
        }
        return smoothstep01(
            (dot - (binding->selector_min_dot -
                    binding->selector_feather)) /
            binding->selector_feather);
    }
}

static ProceduralSurfaceFieldPoint3D map_point(
    const ProceduralSurfaceBindingV1 *binding,
    ProceduralSurfaceFieldPoint3D point) {
    ProceduralSurfaceFieldPoint3D mapped = {
        point.x / binding->projection_scale,
        point.y / binding->projection_scale,
        point.z / binding->projection_scale};
    if (binding->projection == PROCEDURAL_SURFACE_PROJECTION_PLANAR_XY) {
        mapped.z = 0.0;
    } else if (binding->projection ==
               PROCEDURAL_SURFACE_PROJECTION_PLANAR_XZ) {
        mapped.y = 0.0;
    } else if (binding->projection ==
               PROCEDURAL_SURFACE_PROJECTION_PLANAR_YZ) {
        mapped.x = 0.0;
    }
    return mapped;
}

bool ProceduralSurfaceBinding_Evaluate(
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D source_position,
    ProceduralSurfaceFieldPoint3D source_normal,
    const char *surface_group_id,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceBoundSample *out_sample,
    ProceduralSurfaceBindingReport *report) {
    ProceduralSurfaceBoundSample sample;
    ProceduralSurfaceFieldGraphReport graph_report;
    double weight;
    report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_OK, "", "ok");
    if (!binding || !graph || !sample_budget || !out_sample) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
                   "arguments", "binding evaluation arguments are required");
        return false;
    }
    if (!ProceduralSurfaceBindingV1_Validate(binding, graph, report)) {
        return false;
    }
    memset(&sample, 0, sizeof(sample));
    sample.evaluation_point = map_point(binding, source_position);
    weight = selector_weight(binding, source_normal, surface_group_id);
    if (!ProceduralSurfaceFieldGraphV1_Evaluate(
            graph, sample.evaluation_point, sample_budget,
            &sample.graph_sample, &graph_report)) {
        report_set(report, PROCEDURAL_SURFACE_BINDING_STATUS_EVALUATION,
                   graph_report.field, graph_report.message);
        return false;
    }
    sample.application_weight = weight;
    sample.graph_sample.height *= weight * binding->displacement_scale;
    sample.graph_sample.macro_variation *= weight;
    sample.graph_sample.micro_variation *= weight;
    sample.graph_sample.cavity *= weight;
    sample.graph_sample.mask *= weight;
    sample.graph_sample.color_r =
        binding->fallback_color_r +
        ((sample.graph_sample.color_r - binding->fallback_color_r) * weight);
    sample.graph_sample.color_g =
        binding->fallback_color_g +
        ((sample.graph_sample.color_g - binding->fallback_color_g) * weight);
    sample.graph_sample.color_b =
        binding->fallback_color_b +
        ((sample.graph_sample.color_b - binding->fallback_color_b) * weight);
    sample.graph_sample.roughness =
        binding->fallback_roughness +
        ((sample.graph_sample.roughness - binding->fallback_roughness) * weight);
    sample.legacy_field.height = sample.graph_sample.height;
    sample.legacy_field.macro_variation =
        sample.graph_sample.macro_variation;
    sample.legacy_field.micro_variation =
        sample.graph_sample.micro_variation;
    sample.legacy_field.rock_mask = sample.graph_sample.mask;
    sample.legacy_field.roughness = sample.graph_sample.roughness;
    sample.legacy_field.snow_precursor = sample.graph_sample.mask;
    *out_sample = sample;
    return true;
}

ProceduralSurfaceFieldPoint3D ProceduralSurfaceBinding_DisplacementDirection(
    const ProceduralSurfaceBindingV1 *binding,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D smooth_normal) {
    ProceduralSurfaceFieldPoint3D result = smooth_normal;
    if (!binding) return result;
    if (binding->displacement_direction ==
        PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL) {
        result = source_normal;
    } else if (binding->displacement_direction ==
               PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP) {
        result = binding->up_axis;
    }
    if (!point_normalize(result, &result)) {
        return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0};
    }
    return result;
}
