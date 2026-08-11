#include "procedural/procedural_surface_authoring_document.h"

#include "app/ray_tracing_sha256.h"

#include <json-c/json.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void report_set(
    ProceduralSurfaceAuthoringDocumentReport *report,
    ProceduralSurfaceAuthoringDocumentStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSurfaceAuthoringDocumentStatus_Name(
    ProceduralSurfaceAuthoringDocumentStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO: return "io";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_JSON: return "json";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA:
            return "schema";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IDENTITY:
            return "identity";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST:
            return "digest";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DOMAIN:
            return "domain";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CAPACITY:
            return "capacity";
        case PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION:
            return "canonicalization";
    }
    return "unknown";
}

static bool id_valid(const char *value, size_t capacity) {
    if (!value || !value[0] || strlen(value) >= capacity) return false;
    for (size_t i = 0u; value[i]; ++i) {
        const unsigned char c = (unsigned char)value[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static bool digest_valid(const char *value, bool allow_empty) {
    if (!value || (allow_empty && !value[0])) return allow_empty;
    if (strlen(value) != 64u) return false;
    for (size_t i = 0u; i < 64u; ++i) {
        if (!isxdigit((unsigned char)value[i])) return false;
    }
    return true;
}

static bool ref_present(const ProceduralSurfaceAuthoringDocumentRef *ref) {
    return ref && ref->id[0];
}

static bool ref_validate(
    const ProceduralSurfaceAuthoringDocumentRef *ref,
    uint32_t allowed_domains,
    ProceduralSurfaceAuthoringDocumentReport *report,
    const char *field) {
    if (!ref_present(ref)) return true;
    if (!id_valid(ref->id, sizeof(ref->id))) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IDENTITY,
                   field, "reference id is invalid");
        return false;
    }
    if (!digest_valid(ref->digest_sha256, false)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST,
                   field, "reference digest must be a SHA-256 hex digest");
        return false;
    }
    if ((ref->output_domains & ~allowed_domains) != 0u ||
        ref->output_domains == 0u) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DOMAIN,
                   field, "reference output domain is not allowed");
        return false;
    }
    return true;
}

void ProceduralSurfaceAuthoringDocumentV1_Init(
    ProceduralSurfaceAuthoringDocumentV1 *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    document->schema_version =
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA_VERSION;
}

bool ProceduralSurfaceAuthoringDocumentV1_Validate(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_OK, "",
               "ok");
    if (!document) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
                   "document", "document is required");
        return false;
    }
    if (document->schema_version !=
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA_VERSION ||
        !id_valid(document->document_id, sizeof(document->document_id)) ||
        !id_valid(document->source_object_id,
                  sizeof(document->source_object_id)) ||
        !digest_valid(document->source_mesh_digest_sha256, false)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA,
                   "document", "document identity or source is invalid");
        return false;
    }
    if (!ref_validate(&document->material_graph,
                      PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MATERIAL |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL,
                      report, "material_graph") ||
        !ref_validate(&document->surface_field_graph,
                      PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_SIGNED_RELIEF |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_DEEP_INSET,
                      report, "surface_field_graph") ||
        !ref_validate(&document->face_region_selector,
                      PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MATERIAL |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_SIGNED_RELIEF |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_DEEP_INSET |
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET,
                      report, "face_region_selector")) {
        return false;
    }
    if (document->attachment_count >
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_MAX_ATTACHMENTS) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CAPACITY,
                   "attachments", "attachment count exceeds capacity");
        return false;
    }
    for (size_t i = 0u; i < document->attachment_count; ++i) {
        if (!ref_validate(&document->attachments[i],
                          PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET,
                          report, "attachments")) return false;
        for (size_t j = 0u; j < i; ++j) {
            if (strcmp(document->attachments[i].id,
                       document->attachments[j].id) == 0) {
                report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IDENTITY,
                           "attachments", "attachment ids must be unique");
                return false;
            }
        }
    }
    if (!ref_present(&document->material_graph) &&
        !ref_present(&document->surface_field_graph) &&
        !ref_present(&document->face_region_selector) &&
        document->attachment_count == 0u) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA,
                   "outputs", "document needs at least one graph or asset");
        return false;
    }
    return true;
}

static bool append_ref(
    char *out, size_t capacity, size_t *used,
    const ProceduralSurfaceAuthoringDocumentRef *ref) {
    int written;
    if (!ref_present(ref)) {
        written = snprintf(out + *used, capacity - *used, "null");
    } else {
        written = snprintf(
            out + *used, capacity - *used,
            "{\"digest_sha256\":\"%s\",\"id\":\"%s\",\"output_domains\":%u}",
            ref->digest_sha256, ref->id, ref->output_domains);
    }
    if (written < 0 || (size_t)written >= capacity - *used) return false;
    *used += (size_t)written;
    return true;
}

bool ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    size_t used = 0u;
    int written;
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_OK, "",
               "ok");
    if (!out_json || out_capacity == 0u ||
        !ProceduralSurfaceAuthoringDocumentV1_Validate(document, report)) {
        if (report && report->status == PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_OK)
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
                       "canonical", "document and output buffer are required");
        return false;
    }
#define APPEND(...) \
    do { \
        written = snprintf(out_json + used, out_capacity - used, __VA_ARGS__); \
        if (written < 0 || (size_t)written >= out_capacity - used) { \
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION, \
                       "canonical", "canonical output buffer is too small"); \
            return false; \
        } \
        used += (size_t)written; \
    } while (0)
    APPEND("{\"attachments\":[");
    for (size_t i = 0u; i < document->attachment_count; ++i) {
        if (i > 0u) APPEND(",");
        if (!append_ref(out_json, out_capacity, &used,
                        &document->attachments[i])) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION,
                       "canonical", "canonical output buffer is too small");
            return false;
        }
    }
    APPEND("],\"document_id\":\"%s\",\"face_region_selector\":",
           document->document_id);
    if (!append_ref(out_json, out_capacity, &used,
                    &document->face_region_selector)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION,
                   "canonical", "canonical output buffer is too small");
        return false;
    }
    APPEND(",\"material_graph\":");
    if (!append_ref(out_json, out_capacity, &used, &document->material_graph)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION,
                   "canonical", "canonical output buffer is too small");
        return false;
    }
    APPEND(",\"schema\":\"%s\",\"schema_version\":%u,\"source_mesh_digest_sha256\":\"%s\",\"source_object_id\":\"%s\",\"surface_field_graph\":",
           PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA,
           PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA_VERSION,
           document->source_mesh_digest_sha256, document->source_object_id);
    if (!append_ref(out_json, out_capacity, &used,
                    &document->surface_field_graph)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION,
                   "canonical", "canonical output buffer is too small");
        return false;
    }
    APPEND("}");
#undef APPEND
    return true;
}

bool ProceduralSurfaceAuthoringDocumentV1_Digest(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    char out_digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY],
    ProceduralSurfaceAuthoringDocumentReport *report) {
    char canonical[16384];
    if (!out_digest ||
        !ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(
            document, canonical, sizeof(canonical), report)) return false;
    if (!ray_tracing_sha256_bytes(canonical, strlen(canonical), out_digest)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION,
                   "digest", "unable to hash canonical document");
        return false;
    }
    return true;
}

bool ProceduralSurfaceAuthoringDocumentV1_Compile(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentCompilePlan *out_plan,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    if (!out_plan ||
        !ProceduralSurfaceAuthoringDocumentV1_Validate(document, report)) {
        if (out_plan) memset(out_plan, 0, sizeof(*out_plan));
        return false;
    }
    memset(&plan, 0, sizeof(plan));
    plan.valid = true;
    snprintf(plan.document_id, sizeof(plan.document_id), "%s",
             document->document_id);
    snprintf(plan.source_object_id, sizeof(plan.source_object_id), "%s",
             document->source_object_id);
    snprintf(plan.source_mesh_digest_sha256,
             sizeof(plan.source_mesh_digest_sha256), "%s",
             document->source_mesh_digest_sha256);
    plan.attachment_count = (uint32_t)document->attachment_count;
    plan.material_graph_bound = ref_present(&document->material_graph);
    plan.surface_field_graph_bound = ref_present(&document->surface_field_graph);
    plan.face_region_selector_bound = ref_present(&document->face_region_selector);
    if (plan.material_graph_bound)
        plan.output_domains |= document->material_graph.output_domains;
    if (plan.surface_field_graph_bound)
        plan.output_domains |= document->surface_field_graph.output_domains;
    if (plan.face_region_selector_bound)
        plan.output_domains |= document->face_region_selector.output_domains;
    if (document->attachment_count > 0u)
        plan.output_domains |=
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET;
    if (!ProceduralSurfaceAuthoringDocumentV1_Digest(
            document, plan.document_digest_sha256, report)) {
        memset(out_plan, 0, sizeof(*out_plan));
        return false;
    }
    *out_plan = plan;
    return true;
}

static ProceduralSurfaceAuthoringDocumentRef *document_field_ref(
    ProceduralSurfaceAuthoringDocumentV1 *document, const char *field) {
    if (!document || !field) return NULL;
    if (strcmp(field, "material_graph") == 0) return &document->material_graph;
    if (strcmp(field, "surface_field_graph") == 0) return &document->surface_field_graph;
    if (strcmp(field, "face_region_selector") == 0) return &document->face_region_selector;
    if (strncmp(field, "attachment:", 11u) == 0) {
        for (size_t i = 0u; i < document->attachment_count; ++i) {
            if (strcmp(document->attachments[i].id, field + 11u) == 0)
                return &document->attachments[i];
        }
    }
    return NULL;
}

bool ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional(
    const ProceduralSurfaceAuthoringDocumentV1 *base_document,
    const char *field,
    const ProceduralSurfaceAuthoringDocumentRef *replacement,
    const char *expected_document_digest,
    const char *expected_source_mesh_digest,
    const char *expected_reference_digest,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentV1 *out_undo,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    ProceduralSurfaceAuthoringDocumentV1 candidate;
    ProceduralSurfaceAuthoringDocumentRef *target;
    char digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    if (!base_document || !field || !replacement || !expected_document_digest ||
        !expected_source_mesh_digest || !expected_reference_digest ||
        !out_document || !out_undo) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
                   "arguments", "transactional replacement arguments are required");
        return false;
    }
    if (!ProceduralSurfaceAuthoringDocumentV1_Validate(base_document, report)) return false;
    if (!ProceduralSurfaceAuthoringDocumentV1_Digest(base_document, digest, report) ||
        strcmp(digest, expected_document_digest) != 0) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST,
                   "expected_document_digest", "document digest is stale");
        return false;
    }
    if (strcmp(base_document->source_mesh_digest_sha256,
               expected_source_mesh_digest) != 0) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST,
                   "expected_source_mesh_digest", "source mesh digest is stale");
        return false;
    }
    candidate = *base_document;
    target = document_field_ref(&candidate, field);
    if (!target) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA,
                   "replace", "unsupported replacement field");
        return false;
    }
    if (strcmp(target->digest_sha256, expected_reference_digest) != 0) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST,
                   "expected_reference_digest", "upstream reference digest is stale");
        return false;
    }
    *out_undo = *base_document;
    *target = *replacement;
    if (!ProceduralSurfaceAuthoringDocumentV1_Validate(&candidate, report)) {
        memset(out_document, 0, sizeof(*out_document));
        memset(out_undo, 0, sizeof(*out_undo));
        return false;
    }
    *out_document = candidate;
    return true;
}

bool ProceduralSurfaceAuthoringDocumentV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    char canonical[16384];
    char temp[1024];
    FILE *file = NULL;
    bool failed = false;
    if (!path || !document || strlen(path) + 5u >= sizeof(temp)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
                   "output", "save path and document are required");
        return false;
    }
    if (!ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(
            document, canonical, sizeof(canonical), report)) return false;
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    file = fopen(temp, "w");
    if (!file || fputs(canonical, file) < 0 || fputc('\n', file) == EOF) failed = true;
    if (file && fclose(file) != 0) failed = true;
    if (failed) {
        remove(temp);
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO,
                   "output", "unable to save document atomically");
        return false;
    }
    if (rename(temp, path) != 0) {
        remove(temp);
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO,
                   "output", "unable to save document atomically");
        return false;
    }
    return true;
}

bool ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile(
    const char *path,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentCompilePlan *out_plan,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    if (!out_document || !out_plan ||
        !ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(path, &document, report) ||
        !ProceduralSurfaceAuthoringDocumentV1_Compile(&document, out_plan, report)) {
        if (out_plan) memset(out_plan, 0, sizeof(*out_plan));
        return false;
    }
    *out_document = document;
    return true;
}

static bool json_text(struct json_object *object, const char *key,
                      char *out, size_t capacity, bool required) {
    struct json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value)) return !required;
    if (json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool json_schema(struct json_object *object) {
    struct json_object *value = NULL;
    return json_object_object_get_ex(object, "schema", &value) &&
           json_object_get_type(value) == json_type_string &&
           strcmp(json_object_get_string(value),
                  PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA) == 0;
}

static bool json_uint(struct json_object *object, const char *key,
                      uint32_t *out, bool required) {
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value)) return !required;
    if (json_object_get_type(value) != json_type_int ||
        json_object_get_int64(value) < 0 ||
        (uint64_t)json_object_get_int64(value) > UINT32_MAX) return false;
    *out = (uint32_t)json_object_get_int64(value);
    return true;
}

static bool parse_ref(struct json_object *object,
                      ProceduralSurfaceAuthoringDocumentRef *out) {
    memset(out, 0, sizeof(*out));
    if (!object || json_object_get_type(object) == json_type_null) return true;
    return json_object_get_type(object) == json_type_object &&
           json_text(object, "id", out->id, sizeof(out->id), true) &&
           json_text(object, "digest_sha256", out->digest_sha256,
                     sizeof(out->digest_sha256), true) &&
           json_uint(object, "output_domains", &out->output_domains, true);
}

bool ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentReport *report) {
    struct json_object *root = NULL;
    struct json_object *value = NULL;
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentV1_Init(&document);
    if (!path || !out_document) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
                   "arguments", "path and output document are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root || json_object_get_type(root) != json_type_object ||
        !json_text(root, "document_id", document.document_id,
                   sizeof(document.document_id), true) ||
        !json_text(root, "source_object_id", document.source_object_id,
                   sizeof(document.source_object_id), true) ||
        !json_text(root, "source_mesh_digest_sha256",
                   document.source_mesh_digest_sha256,
                   sizeof(document.source_mesh_digest_sha256), true) ||
        !json_schema(root)) {
        if (root) json_object_put(root);
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_JSON,
                   "root", "document JSON root is invalid");
        return false;
    }
    /* Schema and version are checked explicitly so the loader cannot accept a
       future document with silently different semantics. */
    if (!json_object_object_get_ex(root, "schema", &value) ||
        !json_schema(root) ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_int64(value) !=
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA_VERSION) {
        json_object_put(root);
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA,
                   "schema", "unsupported document schema");
        return false;
    }
    if (json_object_object_get_ex(root, "material_graph", &value) &&
        !parse_ref(value, &document.material_graph)) goto invalid;
    if (json_object_object_get_ex(root, "surface_field_graph", &value) &&
        !parse_ref(value, &document.surface_field_graph)) goto invalid;
    if (json_object_object_get_ex(root, "face_region_selector", &value) &&
        !parse_ref(value, &document.face_region_selector)) goto invalid;
    if (json_object_object_get_ex(root, "attachments", &value)) {
        if (json_object_get_type(value) != json_type_array ||
            json_object_array_length(value) >
                PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_MAX_ATTACHMENTS) goto invalid;
        document.attachment_count = json_object_array_length(value);
        for (size_t i = 0u; i < document.attachment_count; ++i) {
            if (!parse_ref(json_object_array_get_idx(value, i),
                           &document.attachments[i])) goto invalid;
        }
    }
    if (!ProceduralSurfaceAuthoringDocumentV1_Validate(&document, report)) goto done;
    *out_document = document;
    json_object_put(root);
    return true;
invalid:
    json_object_put(root);
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_JSON,
               "document", "document reference JSON is invalid");
done:
    return false;
}
