#include "procedural/procedural_surface_derived_asset.h"

#include "app/ray_tracing_sha256.h"

#include <json-c/json.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void derived_asset_report(
    ProceduralSurfaceDerivedAssetReport *report,
    ProceduralSurfaceDerivedAssetStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool derived_asset_copy(char *dst,
                               size_t capacity,
                               const char *src) {
    size_t length = src ? strlen(src) : 0u;
    if (!dst || capacity == 0u || !src || length == 0u ||
        length >= capacity) {
        return false;
    }
    memcpy(dst, src, length + 1u);
    return true;
}

static bool derived_asset_digest(const char *value) {
    return value && ray_tracing_sha256_is_valid_hex(value);
}

static bool derived_asset_positive(double value) {
    return isfinite(value) && value > 0.0;
}

const char *ProceduralSurfaceDerivedAssetQuality_Name(
    ProceduralSurfacePlaneQuality quality) {
    switch (quality) {
        case PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW:
            return "preview";
        case PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION:
            return "inspection";
        case PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL:
            return "final";
        default:
            return "invalid";
    }
}

static ProceduralSurfacePlaneQuality derived_asset_quality_from_name(
    const char *name) {
    if (name && strcmp(name, "preview") == 0) {
        return PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW;
    }
    if (name && strcmp(name, "inspection") == 0) {
        return PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION;
    }
    if (name && strcmp(name, "final") == 0) {
        return PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL;
    }
    return (ProceduralSurfacePlaneQuality)0;
}

const char *ProceduralSurfaceDerivedAssetStatus_Name(
    ProceduralSurfaceDerivedAssetStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK:
            return "ok";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA:
            return "schema";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY:
            return "identity";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_PATH:
            return "path";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IO:
            return "io";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_JSON:
            return "json";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE:
            return "stale";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_ALLOCATION:
            return "allocation";
        case PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL:
            return "material";
        default:
            return "unknown";
    }
}

void ProceduralSurfaceDerivedAssetMaterial_Init(
    ProceduralSurfaceDerivedAssetMaterial *material) {
    if (!material) return;
    memset(material, 0, sizeof(*material));
}

void ProceduralSurfaceDerivedAssetMaterial_Free(
    ProceduralSurfaceDerivedAssetMaterial *material) {
    if (!material) return;
    free(material->vertex_samples);
    free(material->triangle_indices);
    memset(material, 0, sizeof(*material));
}

bool ProceduralSurfaceDerivedAsset_CageDigest(
    ProceduralSurfaceCageKind kind,
    double width_units,
    double height_units,
    double depth_units,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report) {
    char canonical[384];
    int length;
    if (!out_digest) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "out_digest", "cage digest output is required");
        return false;
    }
    out_digest[0] = '\0';
    if (kind != PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM ||
        !derived_asset_positive(width_units) ||
        !derived_asset_positive(height_units) ||
        !derived_asset_positive(depth_units)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "semantic_cage", "semantic cage dimensions are invalid");
        return false;
    }
    length = snprintf(
        canonical, sizeof(canonical),
        "{\"schema_version\":1,\"kind\":\"rectangular_prism\","
        "\"width_units\":%.17g,\"height_units\":%.17g,"
        "\"depth_units\":%.17g}",
        width_units, height_units, depth_units);
    if (length < 0 || (size_t)length >= sizeof(canonical) ||
        !ray_tracing_sha256_bytes(canonical, (size_t)length, out_digest)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "semantic_cage", "unable to create semantic cage digest");
        return false;
    }
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;
}

bool ProceduralSurfaceDerivedAsset_CacheIdentity(
    const char *recipe_digest,
    const char *cage_digest,
    ProceduralSurfacePlaneQuality quality,
    const char *shell_digest,
    const char *material_digest,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report) {
    char canonical[640];
    int length;
    const char *quality_name = ProceduralSurfaceDerivedAssetQuality_Name(quality);
    if (!out_digest) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "out_digest", "cache identity output is required");
        return false;
    }
    out_digest[0] = '\0';
    if (!derived_asset_digest(recipe_digest) ||
        !derived_asset_digest(cage_digest) ||
        !derived_asset_digest(shell_digest) ||
        !derived_asset_digest(material_digest) ||
        strcmp(quality_name, "invalid") == 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "cache_identity", "cache identity inputs are invalid");
        return false;
    }
    length = snprintf(
        canonical, sizeof(canonical),
        "{\"schema_version\":1,\"recipe_digest_sha256\":\"%s\","
        "\"cage_digest_sha256\":\"%s\",\"quality\":\"%s\","
        "\"shell_digest_sha256\":\"%s\","
        "\"material_digest_sha256\":\"%s\"}",
        recipe_digest, cage_digest, quality_name, shell_digest, material_digest);
    if (length < 0 || (size_t)length >= sizeof(canonical) ||
        !ray_tracing_sha256_bytes(canonical, (size_t)length, out_digest)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "cache_identity", "unable to create cache identity");
        return false;
    }
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;
}

bool ProceduralSurfaceDerivedAsset_CacheIdentityV2(
    const char *recipe_digest,
    const char *field_graph_digest,
    const char *binding_digest,
    const char *cage_digest,
    ProceduralSurfacePlaneQuality quality,
    const char *shell_digest,
    const char *material_digest,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report) {
    char canonical[1024];
    int length;
    const char *quality_name = ProceduralSurfaceDerivedAssetQuality_Name(quality);
    if (!out_digest) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "out_digest", "cache identity output is required");
        return false;
    }
    out_digest[0] = '\0';
    if (!derived_asset_digest(recipe_digest) ||
        !derived_asset_digest(field_graph_digest) ||
        !derived_asset_digest(binding_digest) ||
        !derived_asset_digest(cage_digest) ||
        !derived_asset_digest(shell_digest) ||
        !derived_asset_digest(material_digest) ||
        strcmp(quality_name, "invalid") == 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "cache_identity", "cache identity inputs are invalid");
        return false;
    }
    length = snprintf(
        canonical, sizeof(canonical),
        "{\"schema_version\":2,\"recipe_digest_sha256\":\"%s\","
        "\"field_graph_digest_sha256\":\"%s\","
        "\"binding_digest_sha256\":\"%s\","
        "\"cage_digest_sha256\":\"%s\",\"quality\":\"%s\","
        "\"shell_digest_sha256\":\"%s\","
        "\"material_digest_sha256\":\"%s\"}",
        recipe_digest, field_graph_digest, binding_digest, cage_digest,
        quality_name, shell_digest, material_digest);
    if (length < 0 || (size_t)length >= sizeof(canonical) ||
        !ray_tracing_sha256_bytes(canonical, (size_t)length, out_digest)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "cache_identity", "unable to create cache identity");
        return false;
    }
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;
}

bool ProceduralSurfaceDerivedAssetManifest_Validate(
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    ProceduralSurfaceDerivedAssetReport *report) {
    char cage_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char cache_identity[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    if (!manifest) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "manifest", "derived asset manifest is required");
        return false;
    }
    if (manifest->schema_version !=
            PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION_V1 &&
        manifest->schema_version !=
            PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "schema_version", "unsupported derived asset schema version");
        return false;
    }
    if (!manifest->asset_id[0] || !manifest->source_asset_id[0]) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
            "asset_id", "asset and source asset identities are required");
        return false;
    }
    if (!manifest->recipe_path[0] || !manifest->mesh_path[0] ||
        !manifest->material_path[0]) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_PATH,
            "paths", "recipe, mesh, and material paths are required");
        return false;
    }
    if (manifest->schema_version >= 2u &&
        (!manifest->field_graph_path[0] || !manifest->binding_path[0] ||
         !derived_asset_digest(manifest->field_graph_digest_sha256) ||
         !derived_asset_digest(manifest->binding_digest_sha256))) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_PATH,
            "procedural_sources",
            "field graph and binding references are required for schema v2");
        return false;
    }
    if (strcmp(manifest->collision_owner, "semantic_cage") != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "collision_owner", "collision owner must remain semantic_cage");
        return false;
    }
    if (!ProceduralSurfaceDerivedAsset_CageDigest(
            manifest->cage_kind, manifest->cage_width_units,
            manifest->cage_height_units, manifest->cage_depth_units,
            cage_digest, report) ||
        strcmp(cage_digest, manifest->cage_digest_sha256) != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE,
            "cage_digest_sha256", "semantic cage digest is stale");
        return false;
    }
    if (!((manifest->schema_version == 1u &&
           ProceduralSurfaceDerivedAsset_CacheIdentity(
               manifest->recipe_digest_sha256, manifest->cage_digest_sha256,
               manifest->quality, manifest->shell_digest_sha256,
               manifest->material_digest_sha256, cache_identity, report)) ||
          (manifest->schema_version == 2u &&
           ProceduralSurfaceDerivedAsset_CacheIdentityV2(
               manifest->recipe_digest_sha256,
               manifest->field_graph_digest_sha256,
               manifest->binding_digest_sha256,
               manifest->cage_digest_sha256, manifest->quality,
               manifest->shell_digest_sha256,
               manifest->material_digest_sha256, cache_identity, report))) ||
        strcmp(cache_identity, manifest->cache_identity_sha256) != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE,
            "cache_identity_sha256", "derived asset cache identity is stale");
        return false;
    }
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;
}

static json_object *derived_asset_new_cage(
    const ProceduralSurfaceDerivedAssetManifest *manifest) {
    json_object *cage = json_object_new_object();
    if (!cage) return NULL;
    json_object_object_add(cage, "kind",
                           json_object_new_string("rectangular_prism"));
    json_object_object_add(cage, "width_units",
                           json_object_new_double(manifest->cage_width_units));
    json_object_object_add(cage, "height_units",
                           json_object_new_double(manifest->cage_height_units));
    json_object_object_add(cage, "depth_units",
                           json_object_new_double(manifest->cage_depth_units));
    json_object_object_add(cage, "digest_sha256",
                           json_object_new_string(
                               manifest->cage_digest_sha256));
    return cage;
}

bool ProceduralSurfaceDerivedAssetManifest_SaveJsonFile(
    const char *path,
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    ProceduralSurfaceDerivedAssetReport *report) {
    json_object *root = NULL;
    json_object *recipe = NULL;
    json_object *field_graph = NULL;
    json_object *binding = NULL;
    json_object *derived = NULL;
    int result;
    if (!path || !ProceduralSurfaceDerivedAssetManifest_Validate(
                     manifest, report)) {
        if (!path) {
            derived_asset_report(
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
                "path", "manifest output path is required");
        }
        return false;
    }
    root = json_object_new_object();
    recipe = json_object_new_object();
    derived = json_object_new_object();
    if (!root || !recipe || !derived) goto allocation_failed;
    json_object_object_add(root, "schema_family",
                           json_object_new_string(
                               PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA));
    json_object_object_add(root, "schema_version",
                           json_object_new_int64(manifest->schema_version));
    json_object_object_add(root, "asset_id",
                           json_object_new_string(manifest->asset_id));
    json_object_object_add(root, "source_asset_id",
                           json_object_new_string(manifest->source_asset_id));
    json_object_object_add(root, "semantic_cage",
                           derived_asset_new_cage(manifest));
    json_object_object_add(recipe, "path",
                           json_object_new_string(manifest->recipe_path));
    json_object_object_add(recipe, "digest_sha256",
                           json_object_new_string(
                               manifest->recipe_digest_sha256));
    json_object_object_add(root, "recipe_reference", recipe);
    recipe = NULL;
    if (manifest->schema_version >= 2u) {
        field_graph = json_object_new_object();
        binding = json_object_new_object();
        if (!field_graph || !binding) goto allocation_failed;
        json_object_object_add(field_graph, "path",
                               json_object_new_string(
                                   manifest->field_graph_path));
        json_object_object_add(field_graph, "digest_sha256",
                               json_object_new_string(
                                   manifest->field_graph_digest_sha256));
        json_object_object_add(root, "field_graph_reference", field_graph);
        field_graph = NULL;
        json_object_object_add(binding, "path",
                               json_object_new_string(manifest->binding_path));
        json_object_object_add(binding, "digest_sha256",
                               json_object_new_string(
                                   manifest->binding_digest_sha256));
        json_object_object_add(root, "surface_binding_reference", binding);
        binding = NULL;
    }
    json_object_object_add(root, "quality",
                           json_object_new_string(
                               ProceduralSurfaceDerivedAssetQuality_Name(
                                   manifest->quality)));
    json_object_object_add(derived, "mesh_path",
                           json_object_new_string(manifest->mesh_path));
    json_object_object_add(derived, "shell_digest_sha256",
                           json_object_new_string(
                               manifest->shell_digest_sha256));
    json_object_object_add(derived, "material_path",
                           json_object_new_string(manifest->material_path));
    json_object_object_add(derived, "material_digest_sha256",
                           json_object_new_string(
                               manifest->material_digest_sha256));
    json_object_object_add(root, "derived_runtime_asset", derived);
    derived = NULL;
    json_object_object_add(root, "cache_identity_sha256",
                           json_object_new_string(
                               manifest->cache_identity_sha256));
    json_object_object_add(root, "collision_owner",
                           json_object_new_string(manifest->collision_owner));
    result = json_object_to_file_ext(
        path, root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    json_object_put(root);
    if (result != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IO,
            "path", "unable to save derived asset manifest");
        return false;
    }
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;

allocation_failed:
    if (binding) json_object_put(binding);
    if (field_graph) json_object_put(field_graph);
    if (derived) json_object_put(derived);
    if (recipe) json_object_put(recipe);
    if (root) json_object_put(root);
    derived_asset_report(
        report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_ALLOCATION,
        "json", "unable to allocate derived asset manifest json");
    return false;
}

static const char *derived_asset_json_string(json_object *object,
                                             const char *key) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static bool derived_asset_json_double(json_object *object,
                                      const char *key,
                                      double *out_value) {
    json_object *value = NULL;
    if (!object || !out_value ||
        !json_object_object_get_ex(object, key, &value) ||
        (!json_object_is_type(value, json_type_double) &&
         !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return isfinite(*out_value);
}

bool ProceduralSurfaceDerivedAssetManifest_LoadJsonFile(
    const char *path,
    ProceduralSurfaceDerivedAssetManifest *out_manifest,
    ProceduralSurfaceDerivedAssetReport *report) {
    json_object *root = NULL;
    json_object *schema_version = NULL;
    json_object *cage = NULL;
    json_object *recipe = NULL;
    json_object *field_graph = NULL;
    json_object *binding = NULL;
    json_object *derived = NULL;
    ProceduralSurfaceDerivedAssetManifest manifest;
    const char *value = NULL;
    if (!path || !out_manifest) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "path", "manifest input and output are required");
        return false;
    }
    memset(&manifest, 0, sizeof(manifest));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_JSON,
            "path", "unable to parse derived asset manifest");
        return false;
    }
#define COPY_STRING(OBJECT, KEY, DEST, FIELD)                                  \
    do {                                                                       \
        value = derived_asset_json_string((OBJECT), (KEY));                    \
        if (!derived_asset_copy((DEST), sizeof(DEST), value)) {                \
            derived_asset_report(                                               \
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,         \
                (FIELD), "required derived asset string is invalid");          \
            goto fail;                                                          \
        }                                                                      \
    } while (0)
    value = derived_asset_json_string(root, "schema_family");
    if (!value || strcmp(value, PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &schema_version)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "schema_family", "derived asset schema is invalid");
        goto fail;
    }
    manifest.schema_version = (unsigned int)json_object_get_int64(schema_version);
    COPY_STRING(root, "asset_id", manifest.asset_id, "asset_id");
    COPY_STRING(root, "source_asset_id", manifest.source_asset_id,
                "source_asset_id");
    COPY_STRING(root, "cache_identity_sha256",
                manifest.cache_identity_sha256, "cache_identity_sha256");
    COPY_STRING(root, "collision_owner", manifest.collision_owner,
                "collision_owner");
    value = derived_asset_json_string(root, "quality");
    manifest.quality = derived_asset_quality_from_name(value);
    if (!json_object_object_get_ex(root, "semantic_cage", &cage) ||
        !json_object_is_type(cage, json_type_object) ||
        strcmp(derived_asset_json_string(cage, "kind"),
               "rectangular_prism") != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "semantic_cage", "semantic cage contract is invalid");
        goto fail;
    }
    manifest.cage_kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM;
    if (!derived_asset_json_double(cage, "width_units",
                                   &manifest.cage_width_units) ||
        !derived_asset_json_double(cage, "height_units",
                                   &manifest.cage_height_units) ||
        !derived_asset_json_double(cage, "depth_units",
                                   &manifest.cage_depth_units)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "semantic_cage", "semantic cage dimensions are invalid");
        goto fail;
    }
    COPY_STRING(cage, "digest_sha256", manifest.cage_digest_sha256,
                "cage_digest_sha256");
    if (!json_object_object_get_ex(root, "recipe_reference", &recipe) ||
        !json_object_is_type(recipe, json_type_object) ||
        !json_object_object_get_ex(root, "derived_runtime_asset", &derived) ||
        !json_object_is_type(derived, json_type_object)) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
            "references", "recipe or derived runtime asset reference missing");
        goto fail;
    }
    COPY_STRING(recipe, "path", manifest.recipe_path, "recipe_path");
    COPY_STRING(recipe, "digest_sha256", manifest.recipe_digest_sha256,
                "recipe_digest_sha256");
    if (manifest.schema_version >= 2u) {
        if (!json_object_object_get_ex(root, "field_graph_reference",
                                       &field_graph) ||
            !json_object_is_type(field_graph, json_type_object) ||
            !json_object_object_get_ex(root, "surface_binding_reference",
                                       &binding) ||
            !json_object_is_type(binding, json_type_object)) {
            derived_asset_report(
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
                "procedural_sources",
                "field graph or surface binding reference missing");
            goto fail;
        }
        COPY_STRING(field_graph, "path", manifest.field_graph_path,
                    "field_graph_path");
        COPY_STRING(field_graph, "digest_sha256",
                    manifest.field_graph_digest_sha256,
                    "field_graph_digest_sha256");
        COPY_STRING(binding, "path", manifest.binding_path, "binding_path");
        COPY_STRING(binding, "digest_sha256",
                    manifest.binding_digest_sha256,
                    "binding_digest_sha256");
    }
    COPY_STRING(derived, "mesh_path", manifest.mesh_path, "mesh_path");
    COPY_STRING(derived, "shell_digest_sha256", manifest.shell_digest_sha256,
                "shell_digest_sha256");
    COPY_STRING(derived, "material_path", manifest.material_path,
                "material_path");
    COPY_STRING(derived, "material_digest_sha256",
                manifest.material_digest_sha256, "material_digest_sha256");
#undef COPY_STRING
    if (!ProceduralSurfaceDerivedAssetManifest_Validate(&manifest, report)) {
        goto fail;
    }
    *out_manifest = manifest;
    json_object_put(root);
    return true;

fail:
    json_object_put(root);
    return false;
}

static bool derived_asset_json_unit(json_object *object,
                                    const char *key,
                                    double *out_value) {
    return derived_asset_json_double(object, key, out_value) &&
           *out_value >= 0.0 && *out_value <= 1.0;
}

bool ProceduralSurfaceDerivedAssetMaterial_LoadJsonFile(
    const char *path,
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    size_t expected_vertex_count,
    size_t expected_triangle_count,
    ProceduralSurfaceDerivedAssetMaterial *out_material,
    ProceduralSurfaceDerivedAssetReport *report) {
    json_object *root = NULL;
    json_object *vertices = NULL;
    json_object *triangles = NULL;
    ProceduralSurfaceDerivedAssetMaterial material;
    const char *schema = NULL;
    const char *digest = NULL;
    const char *recipe_digest = NULL;
    const char *shell_digest = NULL;
    if (!path || !manifest || !out_material) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
            "material", "material path, manifest, and output are required");
        return false;
    }
    ProceduralSurfaceDerivedAssetMaterial_Init(&material);
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_JSON,
            "material_path", "unable to parse material artifact");
        return false;
    }
    schema = derived_asset_json_string(root, "schema_version");
    digest = derived_asset_json_string(root, "material_digest_sha256");
    recipe_digest = derived_asset_json_string(root, "recipe_digest_sha256");
    shell_digest = derived_asset_json_string(root, "shell_digest_sha256");
    if (!schema ||
        strcmp(schema, "procedural_surface_material_artifact_v1") != 0 ||
        !digest || strcmp(digest, manifest->material_digest_sha256) != 0 ||
        !recipe_digest ||
        strcmp(recipe_digest, manifest->recipe_digest_sha256) != 0 ||
        !shell_digest ||
        strcmp(shell_digest, manifest->shell_digest_sha256) != 0) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE,
            "material_identity", "material artifact identity is stale");
        goto fail;
    }
    if (!json_object_object_get_ex(root, "vertices", &vertices) ||
        !json_object_is_type(vertices, json_type_array) ||
        !json_object_object_get_ex(root, "triangles", &triangles) ||
        !json_object_is_type(triangles, json_type_array) ||
        json_object_array_length(vertices) != expected_vertex_count ||
        json_object_array_length(triangles) != expected_triangle_count) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE,
            "material_topology", "material artifact topology is stale");
        goto fail;
    }
    material.vertex_samples =
        calloc(expected_vertex_count, sizeof(*material.vertex_samples));
    material.triangle_indices =
        calloc(expected_triangle_count * 3u,
               sizeof(*material.triangle_indices));
    if (!material.vertex_samples || !material.triangle_indices) {
        derived_asset_report(
            report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_ALLOCATION,
            "material", "unable to allocate material artifact");
        goto fail;
    }
    for (size_t i = 0u; i < expected_vertex_count; ++i) {
        json_object *vertex = json_object_array_get_idx(vertices, i);
        json_object *color = NULL;
        ProceduralSurfaceMaterialSample *sample = &material.vertex_samples[i];
        if (!vertex || !json_object_is_type(vertex, json_type_object) ||
            !json_object_object_get_ex(vertex, "color", &color) ||
            !json_object_is_type(color, json_type_array) ||
            json_object_array_length(color) != 3u) {
            derived_asset_report(
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL,
                "vertices", "material vertex color is invalid");
            goto fail;
        }
        sample->final_color_r =
            json_object_get_double(json_object_array_get_idx(color, 0u));
        sample->final_color_g =
            json_object_get_double(json_object_array_get_idx(color, 1u));
        sample->final_color_b =
            json_object_get_double(json_object_array_get_idx(color, 2u));
        if (!derived_asset_json_unit(vertex, "roughness",
                                     &sample->final_roughness) ||
            !derived_asset_json_unit(vertex, "snow_likelihood",
                                     &sample->snow_likelihood) ||
            !isfinite(sample->final_color_r) ||
            !isfinite(sample->final_color_g) ||
            !isfinite(sample->final_color_b) ||
            sample->final_color_r < 0.0 || sample->final_color_r > 1.0 ||
            sample->final_color_g < 0.0 || sample->final_color_g > 1.0 ||
            sample->final_color_b < 0.0 || sample->final_color_b > 1.0) {
            derived_asset_report(
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL,
                "vertices", "material vertex channels are invalid");
            goto fail;
        }
    }
    for (size_t i = 0u; i < expected_triangle_count; ++i) {
        json_object *triangle = json_object_array_get_idx(triangles, i);
        if (!triangle || !json_object_is_type(triangle, json_type_array) ||
            json_object_array_length(triangle) != 3u) {
            derived_asset_report(
                report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL,
                "triangles", "material triangle is invalid");
            goto fail;
        }
        for (size_t j = 0u; j < 3u; ++j) {
            int64_t index =
                json_object_get_int64(json_object_array_get_idx(triangle, j));
            if (index < 0 || (uint64_t)index >= expected_vertex_count) {
                derived_asset_report(
                    report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL,
                    "triangles", "material triangle index is invalid");
                goto fail;
            }
            material.triangle_indices[(i * 3u) + j] = (unsigned int)index;
        }
    }
    material.vertex_count = expected_vertex_count;
    material.triangle_count = expected_triangle_count;
    material.valid = true;
    *out_material = material;
    json_object_put(root);
    derived_asset_report(report, PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK,
                         "", "ok");
    return true;

fail:
    ProceduralSurfaceDerivedAssetMaterial_Free(&material);
    json_object_put(root);
    return false;
}
