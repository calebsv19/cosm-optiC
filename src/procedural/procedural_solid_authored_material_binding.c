#include "procedural/procedural_solid_authored_material_binding.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"

#include <json-c/json.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_report(
    ProceduralSolidAuthoredBindingReport *report,
    ProceduralSolidAuthoredBindingStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool stable_id(const char *text, size_t capacity) {
    size_t length;
    if (!text || !text[0]) return false;
    length = strlen(text);
    if (length >= capacity) return false;
    for (size_t i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static bool digest_text(const char *text) {
    if (!text || strlen(text) != 64u) return false;
    for (size_t i = 0u; i < 64u; ++i) {
        if (!isxdigit((unsigned char)text[i])) return false;
    }
    return true;
}

void ProceduralSolidAuthoredMaterialBindingV1_Init(
    ProceduralSolidAuthoredMaterialBindingV1 *binding) {
    if (!binding) return;
    memset(binding, 0, sizeof(*binding));
    binding->schema_version =
        PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA_VERSION;
}

bool ProceduralSolidAuthoredMaterialBindingV1_FromRegionBinding(
    const char *binding_id,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report) {
    ProceduralSolidMaterialBindingReport region_report;
    ProceduralSolidAuthoredMaterialBindingV1 binding;
    if (!binding_id || !region_binding || !out_binding) return false;
    ProceduralSolidAuthoredMaterialBindingV1_Init(&binding);
    snprintf(binding.binding_id, sizeof(binding.binding_id), "%s", binding_id);
    snprintf(binding.region_binding_id, sizeof(binding.region_binding_id), "%s",
             region_binding->binding_id);
    if (!ProceduralSolidMaterialBindingV1_Digest(
            region_binding, binding.region_binding_digest_sha256,
            &region_report)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IDENTITY,
                   "region_binding", region_report.message);
        return false;
    }
    if (!ProceduralSolidAuthoredMaterialBindingV1_Validate(
            &binding, region_binding, report)) return false;
    *out_binding = binding;
    return true;
}

static int compare_reference(const void *left, const void *right) {
    const ProceduralSolidAuthoredMaterialReferenceV1 *a = left;
    const ProceduralSolidAuthoredMaterialReferenceV1 *b = right;
    return strcmp(a->region_id, b->region_id);
}

static bool region_exists(
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *region_id) {
    for (size_t i = 0u; i < binding->region_count; ++i) {
        if (strcmp(binding->regions[i].region_id, region_id) == 0) return true;
    }
    return false;
}

bool ProceduralSolidAuthoredMaterialBindingV1_Validate(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    ProceduralSolidAuthoredBindingReport *report) {
    ProceduralSolidMaterialBindingReport region_report;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_OK, "", "ok");
    if (!binding || !region_binding) return false;
    if (binding->schema_version !=
            PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA_VERSION) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_SCHEMA,
                   "schema_version", "unsupported authored binding schema");
        return false;
    }
    if (!stable_id(binding->binding_id, sizeof(binding->binding_id)) ||
        strcmp(binding->region_binding_id, region_binding->binding_id) != 0 ||
        !ProceduralSolidMaterialBindingV1_Digest(
            region_binding, digest, &region_report) ||
        strcmp(digest, binding->region_binding_digest_sha256) != 0) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IDENTITY,
                   "region_binding_digest_sha256",
                   "authored binding does not match the exact region binding");
        return false;
    }
    if (binding->assignment_count > PROCEDURAL_SOLID_REGION_MAX) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_CAPACITY,
                   "assignments", "authored assignment capacity exceeded");
        return false;
    }
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidAuthoredMaterialReferenceV1 *ref =
            &binding->assignments[i];
        if (!region_exists(region_binding, ref->region_id) ||
            !stable_id(ref->material_id, sizeof(ref->material_id)) ||
            !ref->material_path[0] ||
            strlen(ref->material_path) >= sizeof(ref->material_path) ||
            !digest_text(ref->material_digest_sha256) ||
            (i > 0u &&
             strcmp(binding->assignments[i - 1u].region_id,
                    ref->region_id) >= 0)) {
            set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_REGION,
                       "assignments",
                       "assignments must be sorted, unique, and reference known regions/materials");
            return false;
        }
    }
    return true;
}

static json_object *binding_json(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding) {
    json_object *root = json_object_new_object();
    json_object *assignments = json_object_new_array();
    if (!root || !assignments) {
        if (root) json_object_put(root);
        if (assignments) json_object_put(assignments);
        return NULL;
    }
    json_object_object_add(root, "schema",
        json_object_new_string(PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA));
    json_object_object_add(root, "schema_version",
        json_object_new_int(PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA_VERSION));
    json_object_object_add(root, "binding_id",
        json_object_new_string(binding->binding_id));
    json_object_object_add(root, "region_binding_id",
        json_object_new_string(binding->region_binding_id));
    json_object_object_add(root, "region_binding_digest_sha256",
        json_object_new_string(binding->region_binding_digest_sha256));
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidAuthoredMaterialReferenceV1 *ref =
            &binding->assignments[i];
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "region_id",
            json_object_new_string(ref->region_id));
        json_object_object_add(entry, "material_id",
            json_object_new_string(ref->material_id));
        json_object_object_add(entry, "material_path",
            json_object_new_string(ref->material_path));
        json_object_object_add(entry, "material_digest_sha256",
            json_object_new_string(ref->material_digest_sha256));
        json_object_array_add(assignments, entry);
    }
    json_object_object_add(root, "assignments", assignments);
    return root;
}

bool ProceduralSolidAuthoredMaterialBindingV1_Digest(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidAuthoredBindingReport *report) {
    json_object *root;
    const char *text;
    bool ok;
    if (!binding || !out_digest) return false;
    root = binding_json(binding);
    if (!root) return false;
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ok = ray_tracing_sha256_bytes(text, strlen(text), out_digest);
    json_object_put(root);
    if (!ok) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IO,
                   "digest", "unable to hash authored binding");
        return false;
    }
    if (report) snprintf(report->binding_digest_sha256,
                         sizeof(report->binding_digest_sha256), "%s",
                         out_digest);
    return true;
}

bool ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    ProceduralSolidAuthoredBindingReport *report) {
    json_object *root;
    const char *text;
    CoreResult result;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!path || !ProceduralSolidAuthoredMaterialBindingV1_Digest(
                     binding, digest, report)) return false;
    root = binding_json(binding);
    if (!root) return false;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    if (result.code != CORE_OK) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IO,
                   "path", result.message);
        return false;
    }
    return true;
}

static bool json_text(
    json_object *root, const char *key, char *out, size_t capacity) {
    json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(root, key, &value) ||
        json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

bool ProceduralSolidAuthoredMaterialBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report) {
    json_object *root = NULL, *value = NULL, *assignments = NULL;
    ProceduralSolidAuthoredMaterialBindingV1 binding;
    if (!path || !out_binding) return false;
    root = json_object_from_file(path);
    if (!root || json_object_get_type(root) != json_type_object) {
        if (root) json_object_put(root);
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IO,
                   "path", "unable to parse authored binding JSON");
        return false;
    }
    ProceduralSolidAuthoredMaterialBindingV1_Init(&binding);
    if (!json_object_object_get_ex(root, "schema", &value) ||
        json_object_get_type(value) != json_type_string ||
        strcmp(json_object_get_string(value),
               PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_int64(value) !=
            PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA_VERSION ||
        !json_text(root, "binding_id", binding.binding_id,
                   sizeof(binding.binding_id)) ||
        !json_text(root, "region_binding_id", binding.region_binding_id,
                   sizeof(binding.region_binding_id)) ||
        !json_text(root, "region_binding_digest_sha256",
                   binding.region_binding_digest_sha256,
                   sizeof(binding.region_binding_digest_sha256)) ||
        !json_object_object_get_ex(root, "assignments", &assignments) ||
        json_object_get_type(assignments) != json_type_array ||
        json_object_array_length(assignments) >
            PROCEDURAL_SOLID_REGION_MAX) {
        json_object_put(root);
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_JSON,
                   "binding", "authored binding JSON is incomplete");
        return false;
    }
    binding.assignment_count = json_object_array_length(assignments);
    for (size_t i = 0u; i < binding.assignment_count; ++i) {
        json_object *entry = json_object_array_get_idx(assignments, i);
        ProceduralSolidAuthoredMaterialReferenceV1 *ref =
            &binding.assignments[i];
        if (!entry || json_object_get_type(entry) != json_type_object ||
            !json_text(entry, "region_id", ref->region_id,
                       sizeof(ref->region_id)) ||
            !json_text(entry, "material_id", ref->material_id,
                       sizeof(ref->material_id)) ||
            !json_text(entry, "material_path", ref->material_path,
                       sizeof(ref->material_path)) ||
            !json_text(entry, "material_digest_sha256",
                       ref->material_digest_sha256,
                       sizeof(ref->material_digest_sha256))) {
            json_object_put(root);
            set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_JSON,
                       "assignments", "authored assignment is invalid");
            return false;
        }
    }
    json_object_put(root);
    *out_binding = binding;
    return true;
}

static bool insert_reference(
    ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const char *region_id,
    const char *material_path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredBindingReport *report) {
    ProceduralSolidAuthoredMaterialReport material_report;
    ProceduralSolidAuthoredMaterialReferenceV1 *ref = NULL;
    char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    if (!ProceduralSolidAuthoredMaterialV1_Digest(
            material, digest, &material_report)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_MATERIAL,
                   "material", material_report.message);
        return false;
    }
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        if (strcmp(binding->assignments[i].region_id, region_id) == 0) {
            ref = &binding->assignments[i];
            break;
        }
    }
    if (!ref) {
        if (binding->assignment_count >= PROCEDURAL_SOLID_REGION_MAX) {
            set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_CAPACITY,
                       "assignments", "authored assignment capacity exceeded");
            return false;
        }
        ref = &binding->assignments[binding->assignment_count++];
    }
    memset(ref, 0, sizeof(*ref));
    snprintf(ref->region_id, sizeof(ref->region_id), "%s", region_id);
    snprintf(ref->material_id, sizeof(ref->material_id), "%s",
             material->material_id);
    snprintf(ref->material_path, sizeof(ref->material_path), "%s",
             material_path);
    snprintf(ref->material_digest_sha256,
             sizeof(ref->material_digest_sha256), "%s", digest);
    qsort(binding->assignments, binding->assignment_count,
          sizeof(binding->assignments[0]), compare_reference);
    return true;
}

bool ProceduralSolidAuthoredMaterialBindingV1_AssignRegion(
    const ProceduralSolidAuthoredMaterialBindingV1 *base,
    const char *expected_base_digest,
    const char *region_id,
    const char *material_path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report) {
    ProceduralSolidAuthoredMaterialBindingV1 edited;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!base || !expected_base_digest || !region_id || !material_path ||
        !material || !out_binding ||
        !ProceduralSolidAuthoredMaterialBindingV1_Digest(
            base, digest, report)) return false;
    if (strcmp(digest, expected_base_digest) != 0) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_STALE_BASE,
                   "expected_base_digest", "authored binding changed");
        return false;
    }
    edited = *base;
    if (!insert_reference(
            &edited, region_id, material_path, material, report)) return false;
    *out_binding = edited;
    return true;
}

bool ProceduralSolidAuthoredMaterialBindingV1_AssignKind(
    const ProceduralSolidAuthoredMaterialBindingV1 *base,
    const char *expected_base_digest,
    ProceduralSolidRegionKind kind,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    const char *material_path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report) {
    ProceduralSolidAuthoredMaterialBindingV1 edited;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    bool found = false;
    if (!base || !expected_base_digest || !region_binding || !out_binding ||
        !ProceduralSolidAuthoredMaterialBindingV1_Digest(
            base, digest, report)) return false;
    if (strcmp(digest, expected_base_digest) != 0) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_STALE_BASE,
                   "expected_base_digest", "authored binding changed");
        return false;
    }
    edited = *base;
    for (size_t i = 0u; i < region_binding->region_count; ++i) {
        if (region_binding->regions[i].kind == kind) {
            found = true;
            if (!insert_reference(
                    &edited, region_binding->regions[i].region_id,
                    material_path, material, report)) return false;
        }
    }
    if (!found) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_REGION,
                   "kind", "region kind is absent from the solid");
        return false;
    }
    *out_binding = edited;
    return true;
}

const ProceduralSolidAuthoredMaterialReferenceV1 *
ProceduralSolidAuthoredMaterialBindingV1_Resolve(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const char *region_id) {
    if (!binding || !region_id) return NULL;
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        if (strcmp(binding->assignments[i].region_id, region_id) == 0) {
            return &binding->assignments[i];
        }
    }
    return NULL;
}
