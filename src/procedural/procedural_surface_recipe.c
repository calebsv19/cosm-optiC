#include "procedural/procedural_surface_recipe.h"

#include "app/ray_tracing_sha256.h"

#include <ctype.h>
#include <inttypes.h>
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void report_set(ProceduralSurfaceRecipeReport *report,
                       ProceduralSurfaceRecipeStatus status,
                       const char *field,
                       const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    if (field) snprintf(report->field, sizeof(report->field), "%s", field);
    if (message) {
        snprintf(report->message, sizeof(report->message), "%s", message);
    }
}

static bool fail(ProceduralSurfaceRecipeReport *report,
                 ProceduralSurfaceRecipeStatus status,
                 const char *field,
                 const char *message) {
    report_set(report, status, field, message);
    return false;
}

static bool finite_between(double value, double minimum, double maximum) {
    return isfinite(value) && value >= minimum && value <= maximum;
}

static bool recipe_id_valid(const char *value) {
    size_t length = 0u;
    if (!value || !value[0]) return false;
    length = strlen(value);
    if (length >= PROCEDURAL_SURFACE_RECIPE_ID_CAPACITY) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)value[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static bool object_has_exact_keys(
    struct json_object *object,
    const char *const *keys,
    size_t key_count,
    ProceduralSurfaceRecipeReport *report,
    const char *field) {
    if (!object || !json_object_is_type(object, json_type_object) ||
        (size_t)json_object_object_length(object) != key_count) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    field,
                    "object has missing or unknown fields");
    }
    for (size_t i = 0u; i < key_count; ++i) {
        struct json_object *unused = NULL;
        if (!json_object_object_get_ex(object, keys[i], &unused)) {
            return fail(report,
                        PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                        field,
                        "object has missing or unknown fields");
        }
    }
    return true;
}

static bool json_get_object(struct json_object *parent,
                            const char *key,
                            struct json_object **out_value,
                            ProceduralSurfaceRecipeReport *report) {
    if (!parent || !key || !out_value ||
        !json_object_object_get_ex(parent, key, out_value) ||
        !json_object_is_type(*out_value, json_type_object)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    key,
                    "required object field is missing or has the wrong type");
    }
    return true;
}

static bool json_get_string(struct json_object *parent,
                            const char *key,
                            const char **out_value,
                            ProceduralSurfaceRecipeReport *report) {
    struct json_object *value = NULL;
    if (!parent || !key || !out_value ||
        !json_object_object_get_ex(parent, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    key,
                    "required string field is missing or has the wrong type");
    }
    *out_value = json_object_get_string(value);
    return *out_value != NULL;
}

static bool json_get_u32(struct json_object *parent,
                         const char *key,
                         uint32_t *out_value,
                         ProceduralSurfaceRecipeReport *report) {
    struct json_object *value = NULL;
    int64_t parsed = 0;
    if (!parent || !key || !out_value ||
        !json_object_object_get_ex(parent, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    key,
                    "required integer field is missing or has the wrong type");
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0 || (uint64_t)parsed > UINT32_MAX) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    key,
                    "integer field is outside the unsigned 32-bit range");
    }
    *out_value = (uint32_t)parsed;
    return true;
}

static bool json_get_u64(struct json_object *parent,
                         const char *key,
                         uint64_t *out_value,
                         ProceduralSurfaceRecipeReport *report) {
    struct json_object *value = NULL;
    const char *encoded = NULL;
    if (!parent || !key || !out_value ||
        !json_object_object_get_ex(parent, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    key,
                    "required integer field is missing or has the wrong type");
    }
    encoded = json_object_get_string(value);
    if (!encoded || encoded[0] == '-') {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    key,
                    "integer field must be non-negative");
    }
    *out_value = json_object_get_uint64(value);
    return true;
}

static bool json_get_double(struct json_object *parent,
                            const char *key,
                            double *out_value,
                            ProceduralSurfaceRecipeReport *report) {
    struct json_object *value = NULL;
    if (!parent || !key || !out_value ||
        !json_object_object_get_ex(parent, key, &value) ||
        !(json_object_is_type(value, json_type_double) ||
          json_object_is_type(value, json_type_int))) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
                    key,
                    "required number field is missing or has the wrong type");
    }
    *out_value = json_object_get_double(value);
    if (!isfinite(*out_value)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    key,
                    "number field must be finite");
    }
    return true;
}

void ProceduralSurfaceRecipeV1_InitDefaults(ProceduralSurfaceRecipeV1 *recipe) {
    if (!recipe) return;
    memset(recipe, 0, sizeof(*recipe));
    recipe->schema_version = PROCEDURAL_SURFACE_RECIPE_SCHEMA_VERSION;
    snprintf(recipe->recipe_id,
             sizeof(recipe->recipe_id),
             "%s",
             "procedural_surface_default_v1");
    recipe->seed = 1u;
    recipe->coordinate_space = PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT;
    recipe->base_feature_size_units = 1.0;
    recipe->micro_feature_size_units = 0.125;
    recipe->octave_count = 5u;
    recipe->lacunarity = 2.0;
    recipe->persistence = 0.5;
    recipe->ridge_valley_blend = 0.5;
    recipe->macro_micro_mix = 0.75;
    recipe->target_edge_length_units = 0.25;
    recipe->displacement_amplitude_units = 0.1;
    recipe->edge_lock_width_units = 0.25;
    recipe->output_clamp = PROCEDURAL_SURFACE_OUTPUT_CLAMP_SIGNED_UNIT;
    recipe->snow_elevation_threshold_units = 0.5;
    recipe->snow_slope_threshold = 0.65;
    recipe->quality.preview_max_triangles = 8192u;
    recipe->quality.inspection_max_triangles = 131072u;
    recipe->quality.final_max_triangles = 1000000u;
    recipe->quality.max_field_evaluations = 4000000u;
}

bool ProceduralSurfaceRecipeV1_Validate(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceRecipeReport *report) {
    if (!recipe) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
                    "recipe",
                    "recipe is required");
    }
    if (recipe->schema_version != PROCEDURAL_SURFACE_RECIPE_SCHEMA_VERSION) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_SCHEMA,
                    "schema_version",
                    "only procedural surface recipe schema version 1 is supported");
    }
    if (!recipe_id_valid(recipe->recipe_id)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RECIPE_ID,
                    "recipe_id",
                    "recipe id must use 1-63 alphanumeric, dot, dash, or underscore characters");
    }
    if (recipe->coordinate_space !=
        PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_COORDINATE_SPACE,
                    "coordinate_space",
                    "PSG-0 supports object coordinate space only");
    }
    if (!finite_between(recipe->base_feature_size_units, 1.0e-6, 1.0e6)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "base_feature_size_units",
                    "base feature size must be finite and within [1e-6, 1e6]");
    }
    if (!finite_between(recipe->micro_feature_size_units,
                        1.0e-6,
                        recipe->base_feature_size_units)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "micro_feature_size_units",
                    "micro feature size must be positive and no larger than the base feature size");
    }
    if (recipe->octave_count < 1u || recipe->octave_count > 12u) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "octave_count",
                    "octave count must be within [1, 12]");
    }
    if (!finite_between(recipe->lacunarity, 1.0, 8.0) ||
        !finite_between(recipe->persistence, 0.0, 1.0) ||
        !finite_between(recipe->ridge_valley_blend, 0.0, 1.0) ||
        !finite_between(recipe->macro_micro_mix, 0.0, 1.0)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "field",
                    "field scale and blend values are outside schema-v1 ranges");
    }
    if (!finite_between(recipe->target_edge_length_units, 1.0e-6, 1.0e6)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "target_edge_length_units",
                    "target edge length must be finite and within [1e-6, 1e6]");
    }
    if (!finite_between(recipe->displacement_amplitude_units,
                        0.0,
                        1.0e6)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "displacement_amplitude_units",
                    "displacement amplitude must be finite and within [0, 1e6]");
    }
    if (!finite_between(recipe->edge_lock_width_units,
                        recipe->target_edge_length_units,
                        1.0e6)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "edge_lock_width_units",
                    "edge lock width must cover at least one target edge");
    }
    if (recipe->output_clamp !=
        PROCEDURAL_SURFACE_OUTPUT_CLAMP_SIGNED_UNIT) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "output_clamp",
                    "PSG-0 supports the signed_unit clamp only");
    }
    if (!isfinite(recipe->snow_elevation_threshold_units) ||
        !finite_between(recipe->snow_slope_threshold, 0.0, 1.0)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "material_precursors",
                    "snow precursor thresholds are outside schema-v1 ranges");
    }
    if (recipe->quality.preview_max_triangles < 2u ||
        recipe->quality.inspection_max_triangles <
            recipe->quality.preview_max_triangles ||
        recipe->quality.final_max_triangles <
            recipe->quality.inspection_max_triangles ||
        recipe->quality.final_max_triangles > 2000000u ||
        recipe->quality.max_field_evaluations <
            recipe->quality.final_max_triangles ||
        recipe->quality.max_field_evaluations > 8000000u) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_QUALITY_BUDGET,
                    "quality_budgets",
                    "triangle and evaluation budgets are unordered or outside PSG-0 limits");
    }
    report_set(report,
               PROCEDURAL_SURFACE_RECIPE_STATUS_OK,
               "",
               "recipe is valid");
    return true;
}

static bool parse_recipe(struct json_object *root,
                         ProceduralSurfaceRecipeV1 *out_recipe,
                         ProceduralSurfaceRecipeReport *report) {
    static const char *const root_keys[] = {
        "schema",
        "schema_version",
        "recipe_id",
        "seed",
        "coordinate_space",
        "field",
        "geometry",
        "material_precursors",
        "quality_budgets"
    };
    static const char *const field_keys[] = {
        "base_feature_size_units",
        "micro_feature_size_units",
        "octave_count",
        "lacunarity",
        "persistence",
        "ridge_valley_blend",
        "macro_micro_mix"
    };
    static const char *const geometry_keys[] = {
        "target_edge_length_units",
        "displacement_amplitude_units",
        "edge_lock_width_units",
        "output_clamp"
    };
    static const char *const material_keys[] = {
        "snow_elevation_threshold_units",
        "snow_slope_threshold"
    };
    static const char *const quality_keys[] = {
        "preview_max_triangles",
        "inspection_max_triangles",
        "final_max_triangles",
        "max_field_evaluations"
    };
    ProceduralSurfaceRecipeV1 parsed;
    struct json_object *field = NULL;
    struct json_object *geometry = NULL;
    struct json_object *material = NULL;
    struct json_object *quality = NULL;
    const char *schema = NULL;
    const char *recipe_id = NULL;
    const char *coordinate_space = NULL;
    const char *output_clamp = NULL;

    ProceduralSurfaceRecipeV1_InitDefaults(&parsed);
    if (!object_has_exact_keys(root,
                               root_keys,
                               sizeof(root_keys) / sizeof(root_keys[0]),
                               report,
                               "root") ||
        !json_get_string(root, "schema", &schema, report) ||
        strcmp(schema, PROCEDURAL_SURFACE_RECIPE_SCHEMA) != 0) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_SCHEMA,
                    "schema",
                    "recipe schema identifier is unsupported");
    }
    if (!json_get_u32(root, "schema_version", &parsed.schema_version, report) ||
        !json_get_string(root, "recipe_id", &recipe_id, report) ||
        !json_get_u64(root, "seed", &parsed.seed, report) ||
        !json_get_string(root, "coordinate_space", &coordinate_space, report)) {
        return false;
    }
    if (strlen(recipe_id) >= sizeof(parsed.recipe_id)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RECIPE_ID,
                    "recipe_id",
                    "recipe id exceeds the schema-v1 capacity");
    }
    snprintf(parsed.recipe_id, sizeof(parsed.recipe_id), "%s", recipe_id);
    if (strcmp(coordinate_space, "object") != 0) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_COORDINATE_SPACE,
                    "coordinate_space",
                    "PSG-0 supports object coordinate space only");
    }
    parsed.coordinate_space = PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT;

    if (!json_get_object(root, "field", &field, report) ||
        !object_has_exact_keys(field,
                               field_keys,
                               sizeof(field_keys) / sizeof(field_keys[0]),
                               report,
                               "field") ||
        !json_get_double(field,
                         "base_feature_size_units",
                         &parsed.base_feature_size_units,
                         report) ||
        !json_get_double(field,
                         "micro_feature_size_units",
                         &parsed.micro_feature_size_units,
                         report) ||
        !json_get_u32(field, "octave_count", &parsed.octave_count, report) ||
        !json_get_double(field, "lacunarity", &parsed.lacunarity, report) ||
        !json_get_double(field, "persistence", &parsed.persistence, report) ||
        !json_get_double(field,
                         "ridge_valley_blend",
                         &parsed.ridge_valley_blend,
                         report) ||
        !json_get_double(field,
                         "macro_micro_mix",
                         &parsed.macro_micro_mix,
                         report)) {
        return false;
    }
    if (!json_get_object(root, "geometry", &geometry, report) ||
        !object_has_exact_keys(geometry,
                               geometry_keys,
                               sizeof(geometry_keys) /
                                   sizeof(geometry_keys[0]),
                               report,
                               "geometry") ||
        !json_get_double(geometry,
                         "target_edge_length_units",
                         &parsed.target_edge_length_units,
                         report) ||
        !json_get_double(geometry,
                         "displacement_amplitude_units",
                         &parsed.displacement_amplitude_units,
                         report) ||
        !json_get_double(geometry,
                         "edge_lock_width_units",
                         &parsed.edge_lock_width_units,
                         report) ||
        !json_get_string(geometry, "output_clamp", &output_clamp, report)) {
        return false;
    }
    if (strcmp(output_clamp, "signed_unit") != 0) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                    "output_clamp",
                    "PSG-0 supports the signed_unit clamp only");
    }
    parsed.output_clamp = PROCEDURAL_SURFACE_OUTPUT_CLAMP_SIGNED_UNIT;

    if (!json_get_object(root, "material_precursors", &material, report) ||
        !object_has_exact_keys(material,
                               material_keys,
                               sizeof(material_keys) /
                                   sizeof(material_keys[0]),
                               report,
                               "material_precursors") ||
        !json_get_double(material,
                         "snow_elevation_threshold_units",
                         &parsed.snow_elevation_threshold_units,
                         report) ||
        !json_get_double(material,
                         "snow_slope_threshold",
                         &parsed.snow_slope_threshold,
                         report)) {
        return false;
    }
    if (!json_get_object(root, "quality_budgets", &quality, report) ||
        !object_has_exact_keys(quality,
                               quality_keys,
                               sizeof(quality_keys) /
                                   sizeof(quality_keys[0]),
                               report,
                               "quality_budgets") ||
        !json_get_u32(quality,
                      "preview_max_triangles",
                      &parsed.quality.preview_max_triangles,
                      report) ||
        !json_get_u32(quality,
                      "inspection_max_triangles",
                      &parsed.quality.inspection_max_triangles,
                      report) ||
        !json_get_u32(quality,
                      "final_max_triangles",
                      &parsed.quality.final_max_triangles,
                      report) ||
        !json_get_u32(quality,
                      "max_field_evaluations",
                      &parsed.quality.max_field_evaluations,
                      report)) {
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_Validate(&parsed, report)) return false;
    *out_recipe = parsed;
    return true;
}

bool ProceduralSurfaceRecipeV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceRecipeV1 *out_recipe,
    ProceduralSurfaceRecipeReport *report) {
    struct json_object *root = NULL;
    ProceduralSurfaceRecipeV1 parsed;
    bool ok = false;
    if (!path || !path[0] || !out_recipe) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
                    "path",
                    "input path and output recipe are required");
    }
    root = json_object_from_file(path);
    if (!root) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_IO,
                    "path",
                    "recipe JSON file could not be loaded");
    }
    ok = parse_recipe(root, &parsed, report);
    json_object_put(root);
    if (!ok) return false;
    *out_recipe = parsed;
    return true;
}

bool ProceduralSurfaceRecipeV1_CanonicalJson(
    const ProceduralSurfaceRecipeV1 *recipe,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceRecipeReport *report) {
    int written = 0;
    if (!out_json || out_capacity == 0u) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
                    "out_json",
                    "canonical output buffer is required");
    }
    if (!ProceduralSurfaceRecipeV1_Validate(recipe, report)) return false;
    written = snprintf(
        out_json,
        out_capacity,
        "{\"schema\":\"%s\",\"schema_version\":%u,\"recipe_id\":\"%s\","
        "\"seed\":%" PRIu64 ",\"coordinate_space\":\"object\","
        "\"field\":{\"base_feature_size_units\":%.17g,"
        "\"micro_feature_size_units\":%.17g,\"octave_count\":%u,"
        "\"lacunarity\":%.17g,\"persistence\":%.17g,"
        "\"ridge_valley_blend\":%.17g,\"macro_micro_mix\":%.17g},"
        "\"geometry\":{\"target_edge_length_units\":%.17g,"
        "\"displacement_amplitude_units\":%.17g,"
        "\"edge_lock_width_units\":%.17g,\"output_clamp\":\"signed_unit\"},"
        "\"material_precursors\":{\"snow_elevation_threshold_units\":%.17g,"
        "\"snow_slope_threshold\":%.17g},"
        "\"quality_budgets\":{\"preview_max_triangles\":%u,"
        "\"inspection_max_triangles\":%u,\"final_max_triangles\":%u,"
        "\"max_field_evaluations\":%u}}",
        PROCEDURAL_SURFACE_RECIPE_SCHEMA,
        recipe->schema_version,
        recipe->recipe_id,
        recipe->seed,
        recipe->base_feature_size_units,
        recipe->micro_feature_size_units,
        recipe->octave_count,
        recipe->lacunarity,
        recipe->persistence,
        recipe->ridge_valley_blend,
        recipe->macro_micro_mix,
        recipe->target_edge_length_units,
        recipe->displacement_amplitude_units,
        recipe->edge_lock_width_units,
        recipe->snow_elevation_threshold_units,
        recipe->snow_slope_threshold,
        recipe->quality.preview_max_triangles,
        recipe->quality.inspection_max_triangles,
        recipe->quality.final_max_triangles,
        recipe->quality.max_field_evaluations);
    if (written < 0 || (size_t)written >= out_capacity) {
        if (out_capacity > 0u) out_json[0] = '\0';
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_CANONICALIZATION,
                    "out_json",
                    "canonical output buffer is too small");
    }
    report_set(report,
               PROCEDURAL_SURFACE_RECIPE_STATUS_OK,
               "",
               "recipe canonicalization succeeded");
    return true;
}

bool ProceduralSurfaceRecipeV1_Digest(
    const ProceduralSurfaceRecipeV1 *recipe,
    char out_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY],
    ProceduralSurfaceRecipeReport *report) {
    char canonical[PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY];
    if (!out_digest) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
                    "out_digest",
                    "digest output buffer is required");
    }
    if (!ProceduralSurfaceRecipeV1_CanonicalJson(
            recipe, canonical, sizeof(canonical), report) ||
        !ray_tracing_sha256_bytes(canonical, strlen(canonical), out_digest)) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_CANONICALIZATION,
                    "digest",
                    "recipe digest generation failed");
    }
    report_set(report,
               PROCEDURAL_SURFACE_RECIPE_STATUS_OK,
               "",
               "recipe digest generation succeeded");
    return true;
}

bool ProceduralSurfaceRecipeV1_SaveJsonFile(
    const char *path,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceRecipeReport *report) {
    char canonical[PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY];
    FILE *file = NULL;
    size_t length = 0u;
    if (!path || !path[0]) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
                    "path",
                    "output path is required");
    }
    if (!ProceduralSurfaceRecipeV1_CanonicalJson(
            recipe, canonical, sizeof(canonical), report)) {
        return false;
    }
    file = fopen(path, "wb");
    if (!file) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_IO,
                    "path",
                    "recipe JSON output file could not be opened");
    }
    length = strlen(canonical);
    if (fwrite(canonical, 1u, length, file) != length ||
        fputc('\n', file) == EOF) {
        fclose(file);
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_IO,
                    "path",
                    "recipe JSON output file could not be written");
    }
    if (fclose(file) != 0) {
        return fail(report,
                    PROCEDURAL_SURFACE_RECIPE_STATUS_IO,
                    "path",
                    "recipe JSON output file could not be closed");
    }
    report_set(report,
               PROCEDURAL_SURFACE_RECIPE_STATUS_OK,
               "",
               "recipe JSON save succeeded");
    return true;
}

const char *ProceduralSurfaceRecipeStatus_Name(
    ProceduralSurfaceRecipeStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_RECIPE_STATUS_OK:
            return "ok";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_IO:
            return "io";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_JSON:
            return "json";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_SCHEMA:
            return "schema";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_RECIPE_ID:
            return "recipe_id";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_COORDINATE_SPACE:
            return "coordinate_space";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE:
            return "range";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_QUALITY_BUDGET:
            return "quality_budget";
        case PROCEDURAL_SURFACE_RECIPE_STATUS_CANONICALIZATION:
            return "canonicalization";
        default:
            return "unknown";
    }
}
