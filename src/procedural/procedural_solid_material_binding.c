#include "procedural/procedural_solid_material_binding.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"

#include <json-c/json.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_report(
    ProceduralSolidMaterialBindingReport *report,
    ProceduralSolidMaterialBindingStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(
        report->message, sizeof(report->message), "%s",
        message ? message : "");
}

static bool id_valid(const char *text, size_t capacity) {
    size_t length;
    if (!text || !text[0]) return false;
    length = strlen(text);
    if (length >= capacity) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

const char *ProceduralSolidMaterialPreset_Name(
    ProceduralSolidMaterialPreset material) {
    switch (material) {
        case PROCEDURAL_SOLID_MATERIAL_DEFAULT: return "default";
        case PROCEDURAL_SOLID_MATERIAL_MIRROR: return "mirror";
        case PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL: return "rough_metal";
        case PROCEDURAL_SOLID_MATERIAL_GLOSSY: return "glossy";
        case PROCEDURAL_SOLID_MATERIAL_EMISSIVE: return "emissive";
        case PROCEDURAL_SOLID_MATERIAL_TRANSPARENT: return "transparent";
        case PROCEDURAL_SOLID_MATERIAL_INVALID: break;
    }
    return "invalid";
}

ProceduralSolidMaterialPreset ProceduralSolidMaterialPreset_Parse(
    const char *name) {
    if (!name) return PROCEDURAL_SOLID_MATERIAL_INVALID;
    for (int value = PROCEDURAL_SOLID_MATERIAL_DEFAULT;
         value < PROCEDURAL_SOLID_MATERIAL_INVALID; ++value) {
        if (strcmp(
                name, ProceduralSolidMaterialPreset_Name(
                          (ProceduralSolidMaterialPreset)value)) == 0) {
            return (ProceduralSolidMaterialPreset)value;
        }
    }
    return PROCEDURAL_SOLID_MATERIAL_INVALID;
}

const char *ProceduralSolidMaterialBindingStatus_Name(
    ProceduralSolidMaterialBindingStatus status) {
    switch (status) {
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK: return "ok";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT:
            return "argument";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IO: return "io";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_JSON: return "json";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IDENTITY:
            return "identity";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION: return "region";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_MATERIAL:
            return "material";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_STALE_BASE:
            return "stale_base";
        case PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_CAPACITY:
            return "capacity";
    }
    return "unknown";
}

void ProceduralSolidMaterialBindingV1_Init(
    ProceduralSolidMaterialBindingV1 *binding) {
    if (!binding) return;
    memset(binding, 0, sizeof(*binding));
    binding->schema_version =
        PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA_VERSION;
    binding->fallback_material = PROCEDURAL_SOLID_MATERIAL_DEFAULT;
}

static ProceduralSolidRegionKind parse_region_kind(const char *text) {
    if (!text) return PROCEDURAL_SOLID_REGION_RETAINED;
    if (strcmp(text, "retained") == 0) {
        return PROCEDURAL_SOLID_REGION_RETAINED;
    }
    if (strcmp(text, "cut") == 0) return PROCEDURAL_SOLID_REGION_CUT;
    if (strcmp(text, "blend") == 0) return PROCEDURAL_SOLID_REGION_BLEND;
    return (ProceduralSolidRegionKind)-1;
}

static bool json_text(
    json_object *object,
    const char *key,
    char *out,
    size_t capacity) {
    json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_string) {
        return false;
    }
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool json_size(
    json_object *object,
    const char *key,
    size_t *out) {
    json_object *value = NULL;
    int64_t parsed;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_int) {
        return false;
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0) return false;
    *out = (size_t)parsed;
    return (int64_t)*out == parsed;
}

static bool region_digest(
    const ProceduralSolidMaterialBindingV1 *binding,
    char out_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY]) {
    char canonical[
        PROCEDURAL_SOLID_REGION_MAX *
        (64u + PROCEDURAL_SOLID_GRAPH_ID_CAPACITY * 2u + 64u)];
    size_t length = 0u;
    for (size_t i = 0u; i < binding->region_count; ++i) {
        const ProceduralSolidRegionRecord *region = &binding->regions[i];
        const int count = snprintf(
            canonical + length, sizeof(canonical) - length,
            "%s|%s|%s|%s|%zu;", region->region_id,
            ProceduralSolidRegionKind_Name(region->kind),
            region->primary_node_id, region->secondary_node_id,
            region->triangle_count);
        if (count < 0 || (size_t)count >= sizeof(canonical) - length) {
            return false;
        }
        length += (size_t)count;
    }
    return ray_tracing_sha256_bytes(canonical, length, out_digest);
}

static bool region_records_load(
    json_object *root,
    ProceduralSolidMaterialBindingV1 *binding) {
    json_object *regions = NULL;
    if (!json_object_object_get_ex(root, "regions", &regions) ||
        json_object_get_type(regions) != json_type_array ||
        json_object_array_length(regions) > PROCEDURAL_SOLID_REGION_MAX) {
        return false;
    }
    binding->region_count = json_object_array_length(regions);
    if (binding->region_count == 0u) return false;
    for (size_t i = 0u; i < binding->region_count; ++i) {
        json_object *entry = json_object_array_get_idx(regions, i);
        json_object *kind_value = NULL;
        ProceduralSolidRegionRecord *region = &binding->regions[i];
        if (!entry || json_object_get_type(entry) != json_type_object ||
            !json_text(
                entry, "region_id", region->region_id,
                sizeof(region->region_id)) ||
            !json_object_object_get_ex(entry, "kind", &kind_value) ||
            json_object_get_type(kind_value) != json_type_string ||
            !json_text(
                entry, "primary_node_id", region->primary_node_id,
                sizeof(region->primary_node_id)) ||
            !json_text(
                entry, "secondary_node_id", region->secondary_node_id,
                sizeof(region->secondary_node_id)) ||
            !json_size(entry, "triangle_count", &region->triangle_count)) {
            return false;
        }
        region->kind =
            parse_region_kind(json_object_get_string(kind_value));
        if ((int)region->kind < 0) return false;
    }
    return true;
}

static bool assignment_records_load(
    json_object *root,
    ProceduralSolidMaterialBindingV1 *binding) {
    json_object *assignments = NULL;
    if (!json_object_object_get_ex(root, "assignments", &assignments) ||
        json_object_get_type(assignments) != json_type_array ||
        json_object_array_length(assignments) >
            PROCEDURAL_SOLID_REGION_MAX) {
        return false;
    }
    binding->assignment_count = json_object_array_length(assignments);
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        json_object *entry = json_object_array_get_idx(assignments, i);
        json_object *material = NULL;
        ProceduralSolidRegionMaterialAssignment *assignment =
            &binding->assignments[i];
        if (!entry || json_object_get_type(entry) != json_type_object ||
            !json_text(
                entry, "region_id", assignment->region_id,
                sizeof(assignment->region_id)) ||
            !json_object_object_get_ex(entry, "material", &material) ||
            json_object_get_type(material) != json_type_string) {
            return false;
        }
        assignment->material = ProceduralSolidMaterialPreset_Parse(
            json_object_get_string(material));
        if (assignment->material == PROCEDURAL_SOLID_MATERIAL_INVALID) {
            return false;
        }
    }
    return true;
}

bool ProceduralSolidMaterialBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report) {
    json_object *root = NULL;
    json_object *value = NULL;
    ProceduralSolidMaterialBindingV1 binding;
    set_report(
        report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK, "", "ok");
    if (!path || !out_binding) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
            "path", "binding path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root || json_object_get_type(root) != json_type_object) {
        if (root) json_object_put(root);
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IO,
            "path", "unable to parse binding JSON");
        return false;
    }
    ProceduralSolidMaterialBindingV1_Init(&binding);
    if (!json_object_object_get_ex(root, "schema", &value) ||
        json_object_get_type(value) != json_type_string ||
        strcmp(
            json_object_get_string(value),
            PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_type(value) != json_type_int ||
        json_object_get_int64(value) !=
            PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA_VERSION ||
        !json_text(
            root, "binding_id", binding.binding_id,
            sizeof(binding.binding_id)) ||
        !json_text(
            root, "asset_id", binding.asset_id,
            sizeof(binding.asset_id)) ||
        !json_text(
            root, "semantic_source_id", binding.semantic_source_id,
            sizeof(binding.semantic_source_id)) ||
        !json_text(
            root, "mesh_digest_sha256", binding.mesh_digest_sha256,
            sizeof(binding.mesh_digest_sha256)) ||
        !json_text(
            root, "region_digest_sha256", binding.region_digest_sha256,
            sizeof(binding.region_digest_sha256)) ||
        !json_object_object_get_ex(root, "fallback_material", &value) ||
        json_object_get_type(value) != json_type_string ||
        !region_records_load(root, &binding) ||
        !assignment_records_load(root, &binding)) {
        json_object_put(root);
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_JSON,
            "binding", "binding JSON contract is incomplete or invalid");
        return false;
    }
    binding.fallback_material = ProceduralSolidMaterialPreset_Parse(
        json_object_get_string(value));
    json_object_put(root);
    if (binding.fallback_material == PROCEDURAL_SOLID_MATERIAL_INVALID) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_MATERIAL,
            "fallback_material", "fallback material is unsupported");
        return false;
    }
    *out_binding = binding;
    return true;
}

static json_object *binding_json(
    const ProceduralSolidMaterialBindingV1 *binding) {
    json_object *root = json_object_new_object();
    json_object *regions = json_object_new_array();
    json_object *assignments = json_object_new_array();
    if (!root || !regions || !assignments) {
        if (root) json_object_put(root);
        if (regions) json_object_put(regions);
        if (assignments) json_object_put(assignments);
        return NULL;
    }
    json_object_object_add(
        root, "schema",
        json_object_new_string(PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA));
    json_object_object_add(
        root, "schema_version",
        json_object_new_int(
            PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA_VERSION));
    json_object_object_add(
        root, "binding_id", json_object_new_string(binding->binding_id));
    json_object_object_add(
        root, "asset_id", json_object_new_string(binding->asset_id));
    json_object_object_add(
        root, "semantic_source_id",
        json_object_new_string(binding->semantic_source_id));
    json_object_object_add(
        root, "mesh_digest_sha256",
        json_object_new_string(binding->mesh_digest_sha256));
    json_object_object_add(
        root, "region_digest_sha256",
        json_object_new_string(binding->region_digest_sha256));
    json_object_object_add(
        root, "fallback_material",
        json_object_new_string(ProceduralSolidMaterialPreset_Name(
            binding->fallback_material)));
    for (size_t i = 0u; i < binding->region_count; ++i) {
        const ProceduralSolidRegionRecord *region = &binding->regions[i];
        json_object *entry = json_object_new_object();
        json_object_object_add(
            entry, "region_id", json_object_new_string(region->region_id));
        json_object_object_add(
            entry, "kind",
            json_object_new_string(
                ProceduralSolidRegionKind_Name(region->kind)));
        json_object_object_add(
            entry, "primary_node_id",
            json_object_new_string(region->primary_node_id));
        json_object_object_add(
            entry, "secondary_node_id",
            json_object_new_string(region->secondary_node_id));
        json_object_object_add(
            entry, "triangle_count",
            json_object_new_int64((int64_t)region->triangle_count));
        json_object_array_add(regions, entry);
    }
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidRegionMaterialAssignment *assignment =
            &binding->assignments[i];
        json_object *entry = json_object_new_object();
        json_object_object_add(
            entry, "region_id",
            json_object_new_string(assignment->region_id));
        json_object_object_add(
            entry, "material",
            json_object_new_string(ProceduralSolidMaterialPreset_Name(
                assignment->material)));
        json_object_array_add(assignments, entry);
    }
    json_object_object_add(root, "regions", regions);
    json_object_object_add(root, "assignments", assignments);
    return root;
}

bool ProceduralSolidMaterialBindingV1_Digest(
    const ProceduralSolidMaterialBindingV1 *binding,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidMaterialBindingReport *report) {
    json_object *root;
    const char *text;
    set_report(
        report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK, "", "ok");
    if (!binding || !out_digest) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
            "binding", "binding and digest output are required");
        return false;
    }
    root = binding_json(binding);
    if (!root) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_CAPACITY,
            "binding", "binding canonicalization allocation failed");
        return false;
    }
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!ray_tracing_sha256_bytes(text, strlen(text), out_digest)) {
        json_object_put(root);
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IDENTITY,
            "binding_digest_sha256", "binding digest failed");
        return false;
    }
    json_object_put(root);
    if (report) {
        snprintf(
            report->binding_digest_sha256,
            sizeof(report->binding_digest_sha256), "%s", out_digest);
    }
    return true;
}

static bool region_exists(
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *region_id,
    size_t *out_index) {
    for (size_t i = 0u; i < binding->region_count; ++i) {
        if (strcmp(binding->regions[i].region_id, region_id) == 0) {
            if (out_index) *out_index = i;
            return true;
        }
    }
    return false;
}

bool ProceduralSolidMaterialBindingV1_Validate(
    const ProceduralSolidMaterialBindingV1 *binding,
    const CoreMeshAssetRuntimeDocument *mesh,
    ProceduralSolidMaterialBindingReport *report) {
    char actual_mesh_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    char actual_region_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    char binding_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    CoreResult result;
    set_report(
        report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK, "", "ok");
    if (!binding || !mesh) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
            "arguments", "binding and mesh are required");
        return false;
    }
    result = core_mesh_asset_runtime_document_validate(mesh);
    if (result.code != CORE_OK ||
        binding->schema_version !=
            PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA_VERSION ||
        !id_valid(binding->binding_id, sizeof(binding->binding_id)) ||
        !id_valid(binding->asset_id, sizeof(binding->asset_id)) ||
        !id_valid(
            binding->semantic_source_id,
            sizeof(binding->semantic_source_id)) ||
        strcmp(binding->asset_id, mesh->contract.asset_id) != 0 ||
        strcmp(
            binding->semantic_source_id,
            mesh->contract.source_asset_id) != 0 ||
        binding->fallback_material >= PROCEDURAL_SOLID_MATERIAL_INVALID) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_SCHEMA,
            "binding", "binding schema, asset, or fallback is invalid");
        return false;
    }
    if (!ProceduralSolidMesh_Digest(mesh, actual_mesh_digest) ||
        strcmp(actual_mesh_digest, binding->mesh_digest_sha256) != 0) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IDENTITY,
            "mesh_digest_sha256",
            "binding does not match the immutable mesh identity");
        return false;
    }
    if (binding->region_count == 0u ||
        binding->region_count != mesh->surface_group_count ||
        binding->assignment_count > binding->region_count ||
        !region_digest(binding, actual_region_digest) ||
        strcmp(actual_region_digest, binding->region_digest_sha256) != 0) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION,
            "regions", "region partition or digest is invalid");
        return false;
    }
    for (size_t i = 0u; i < binding->region_count; ++i) {
        const ProceduralSolidRegionRecord *region = &binding->regions[i];
        const CoreMeshAssetSurfaceGroup *group = &mesh->surface_groups[i];
        if (!id_valid(region->region_id, sizeof(region->region_id)) ||
            !id_valid(
                region->primary_node_id,
                sizeof(region->primary_node_id)) ||
            (region->secondary_node_id[0] &&
             !id_valid(
                 region->secondary_node_id,
                 sizeof(region->secondary_node_id))) ||
            region->triangle_count == 0u ||
            strcmp(region->region_id, group->group_id) != 0 ||
            region->triangle_count != group->triangle_count) {
            set_report(
                report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION,
                "regions", "region records do not match mesh groups");
            return false;
        }
    }
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidRegionMaterialAssignment *assignment =
            &binding->assignments[i];
        if (!region_exists(binding, assignment->region_id, NULL) ||
            assignment->material >= PROCEDURAL_SOLID_MATERIAL_INVALID ||
            (i > 0u &&
             strcmp(
                 binding->assignments[i - 1u].region_id,
                 assignment->region_id) >= 0)) {
            set_report(
                report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_MATERIAL,
                "assignments", "material assignments are invalid");
            return false;
        }
    }
    return ProceduralSolidMaterialBindingV1_Digest(
        binding, binding_digest, report);
}

bool ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidMaterialBindingV1 *binding,
    ProceduralSolidMaterialBindingReport *report) {
    json_object *root;
    const char *text;
    CoreResult result;
    set_report(
        report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK, "", "ok");
    if (!path || !binding) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
            "path", "save path and binding are required");
        return false;
    }
    root = binding_json(binding);
    if (!root) return false;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    if (result.code != CORE_OK) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IO,
            "path", result.message);
        return false;
    }
    return true;
}

bool ProceduralSolidMaterialBindingV1_FromReceiptFile(
    const char *receipt_path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *binding_id,
    ProceduralSolidMaterialPreset fallback,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report) {
    json_object *root = NULL;
    json_object *value = NULL;
    ProceduralSolidMaterialBindingV1 binding;
    char receipt_mesh_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    set_report(
        report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK, "", "ok");
    if (!receipt_path || !mesh || !binding_id || !out_binding ||
        fallback >= PROCEDURAL_SOLID_MATERIAL_INVALID) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
            "arguments", "receipt initialization arguments are invalid");
        return false;
    }
    root = json_object_from_file(receipt_path);
    ProceduralSolidMaterialBindingV1_Init(&binding);
    if (!root || json_object_get_type(root) != json_type_object ||
        !json_object_object_get_ex(root, "schema", &value) ||
        json_object_get_type(value) != json_type_string ||
        strcmp(
            json_object_get_string(value),
            "ray_tracing.procedural_solid_receipt") != 0 ||
        !json_text(
            root, "asset_id", binding.asset_id,
            sizeof(binding.asset_id)) ||
        !json_text(
            root, "semantic_source_id", binding.semantic_source_id,
            sizeof(binding.semantic_source_id)) ||
        !json_text(
            root, "mesh_digest_sha256", receipt_mesh_digest,
            sizeof(receipt_mesh_digest)) ||
        !json_text(
            root, "region_digest_sha256",
            binding.region_digest_sha256,
            sizeof(binding.region_digest_sha256)) ||
        !region_records_load(root, &binding)) {
        if (root) json_object_put(root);
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_JSON,
            "receipt", "PSG-12 region receipt is required");
        return false;
    }
    json_object_put(root);
    snprintf(
        binding.binding_id, sizeof(binding.binding_id), "%s", binding_id);
    snprintf(
        binding.mesh_digest_sha256,
        sizeof(binding.mesh_digest_sha256), "%s", receipt_mesh_digest);
    binding.fallback_material = fallback;
    if (!ProceduralSolidMaterialBindingV1_Validate(
            &binding, mesh, report)) {
        return false;
    }
    *out_binding = binding;
    return true;
}

static void sort_assignments(ProceduralSolidMaterialBindingV1 *binding) {
    for (size_t i = 1u; i < binding->assignment_count; ++i) {
        const ProceduralSolidRegionMaterialAssignment value =
            binding->assignments[i];
        size_t j = i;
        while (j > 0u &&
               strcmp(
                   binding->assignments[j - 1u].region_id,
                   value.region_id) > 0) {
            binding->assignments[j] = binding->assignments[j - 1u];
            --j;
        }
        binding->assignments[j] = value;
    }
}

static bool preflight_edit(
    const ProceduralSolidMaterialBindingV1 *base,
    const char *expected_base_digest,
    char actual[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidMaterialBindingReport *report) {
    if (!base || !expected_base_digest ||
        !ProceduralSolidMaterialBindingV1_Digest(base, actual, report)) {
        return false;
    }
    if (strcmp(actual, expected_base_digest) != 0) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_STALE_BASE,
            "expected_base_digest",
            "material binding changed since the edit was planned");
        return false;
    }
    return true;
}

static bool assign_one(
    ProceduralSolidMaterialBindingV1 *binding,
    const char *region_id,
    ProceduralSolidMaterialPreset material) {
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        if (strcmp(binding->assignments[i].region_id, region_id) == 0) {
            binding->assignments[i].material = material;
            return true;
        }
    }
    if (binding->assignment_count >= PROCEDURAL_SOLID_REGION_MAX) {
        return false;
    }
    snprintf(
        binding->assignments[binding->assignment_count].region_id,
        sizeof(binding->assignments[binding->assignment_count].region_id),
        "%s", region_id);
    binding->assignments[binding->assignment_count].material = material;
    ++binding->assignment_count;
    return true;
}

bool ProceduralSolidMaterialBindingV1_AssignRegion(
    const ProceduralSolidMaterialBindingV1 *base,
    const char *expected_base_digest,
    const char *region_id,
    ProceduralSolidMaterialPreset material,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report) {
    ProceduralSolidMaterialBindingV1 edited;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!region_id || !out_binding ||
        material >= PROCEDURAL_SOLID_MATERIAL_INVALID ||
        !preflight_edit(base, expected_base_digest, digest, report) ||
        !region_exists(base, region_id, NULL)) {
        if (report && report->status ==
                          PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK) {
            set_report(
                report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION,
                "region_id", "material assignment region is unknown");
        }
        return false;
    }
    edited = *base;
    if (!assign_one(&edited, region_id, material)) return false;
    sort_assignments(&edited);
    *out_binding = edited;
    return true;
}

bool ProceduralSolidMaterialBindingV1_AssignKind(
    const ProceduralSolidMaterialBindingV1 *base,
    const char *expected_base_digest,
    ProceduralSolidRegionKind kind,
    ProceduralSolidMaterialPreset material,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report) {
    ProceduralSolidMaterialBindingV1 edited;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    size_t assigned = 0u;
    if (!out_binding || material >= PROCEDURAL_SOLID_MATERIAL_INVALID ||
        !preflight_edit(base, expected_base_digest, digest, report)) {
        return false;
    }
    edited = *base;
    for (size_t i = 0u; i < edited.region_count; ++i) {
        if (edited.regions[i].kind != kind) continue;
        if (!assign_one(
                &edited, edited.regions[i].region_id, material)) {
            return false;
        }
        ++assigned;
    }
    if (assigned == 0u) {
        set_report(
            report, PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION,
            "kind", "material assignment kind has no regions");
        return false;
    }
    sort_assignments(&edited);
    *out_binding = edited;
    return true;
}

bool ProceduralSolidMaterialBindingV1_Resolve(
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *region_id,
    ProceduralSolidMaterialPreset *out_material,
    bool *out_used_fallback) {
    if (!binding || !region_id || !out_material) return false;
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        if (strcmp(binding->assignments[i].region_id, region_id) == 0) {
            *out_material = binding->assignments[i].material;
            if (out_used_fallback) *out_used_fallback = false;
            return true;
        }
    }
    if (!region_exists(binding, region_id, NULL)) return false;
    *out_material = binding->fallback_material;
    if (out_used_fallback) *out_used_fallback = true;
    return true;
}
