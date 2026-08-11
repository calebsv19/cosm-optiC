#include "procedural/procedural_surface_authoring.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"

#include <json-c/json.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralSurfaceAuthoringReport *report,
    ProceduralSurfaceAuthoringStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSurfaceParameterTarget_Name(
    ProceduralSurfaceParameterTarget target) {
    switch (target) {
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_VALUE:
            return "node.value";
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_SEED:
            return "node.seed";
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_OCTAVES:
            return "node.octaves";
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_LACUNARITY:
            return "node.lacunarity";
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_PERSISTENCE:
            return "node.persistence";
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID:
            break;
    }
    return "invalid";
}

const char *ProceduralSurfaceAuthoringStatus_Name(
    ProceduralSurfaceAuthoringStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_IO: return "io";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_JSON: return "json";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_PARAMETER: return "parameter";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE: return "range";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_TARGET: return "target";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_BASE_DIGEST:
            return "base_digest";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH: return "graph";
        case PROCEDURAL_SURFACE_AUTHORING_STATUS_SAVE: return "save";
    }
    return "unknown";
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

static ProceduralSurfaceParameterTarget parse_target(const char *text) {
    if (!text) return PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID;
    if (strcmp(text, "node.value") == 0) {
        return PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_VALUE;
    }
    if (strcmp(text, "node.seed") == 0) {
        return PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_SEED;
    }
    if (strcmp(text, "node.octaves") == 0) {
        return PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_OCTAVES;
    }
    if (strcmp(text, "node.lacunarity") == 0) {
        return PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_LACUNARITY;
    }
    if (strcmp(text, "node.persistence") == 0) {
        return PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_PERSISTENCE;
    }
    return PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID;
}

static bool target_compatible(
    const ProceduralSurfaceParameter *parameter,
    const ProceduralSurfaceFieldGraphNode *node) {
    switch (parameter->target) {
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_VALUE:
            return node->op == PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_SEED:
            return node->op == PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D ||
                   node->op == PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D ||
                   node->op == PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D ||
                   node->op == PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_OCTAVES:
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_LACUNARITY:
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_PERSISTENCE:
            return node->op == PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D ||
                   node->op == PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID:
            return false;
    }
    return false;
}

void ProceduralSurfaceParameterManifestV1_Init(
    ProceduralSurfaceParameterManifestV1 *manifest) {
    if (!manifest) return;
    memset(manifest, 0, sizeof(*manifest));
    manifest->schema_version =
        PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA_VERSION;
}

bool ProceduralSurfaceParameterManifestV1_Validate(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceAuthoringReport *report) {
    ProceduralSurfaceFieldGraphReport graph_report;
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_OK, "", "ok");
    if (!manifest || !graph) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
                   "arguments", "parameter manifest and graph are required");
        return false;
    }
    if (manifest->schema_version !=
            PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA_VERSION ||
        !id_valid(manifest->manifest_id, sizeof(manifest->manifest_id)) ||
        !id_valid(manifest->graph_program_id,
                  sizeof(manifest->graph_program_id)) ||
        strcmp(manifest->graph_program_id, graph->program_id) != 0 ||
        manifest->parameter_count == 0u ||
        manifest->parameter_count > PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA,
                   "manifest", "parameter manifest root is invalid");
        return false;
    }
    if (!ProceduralSurfaceFieldGraphV1_Validate(graph, &graph_report)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
                   graph_report.field, graph_report.message);
        return false;
    }
    for (size_t i = 0u; i < manifest->parameter_count; ++i) {
        const ProceduralSurfaceParameter *parameter =
            &manifest->parameters[i];
        int node_index = -1;
        if (!id_valid(parameter->id, sizeof(parameter->id)) ||
            !parameter->label[0] || !parameter->unit[0] ||
            !id_valid(parameter->node_id, sizeof(parameter->node_id)) ||
            !isfinite(parameter->minimum) ||
            !isfinite(parameter->maximum) ||
            !isfinite(parameter->default_value) ||
            parameter->maximum < parameter->minimum ||
            parameter->default_value < parameter->minimum ||
            parameter->default_value > parameter->maximum) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_PARAMETER,
                       parameter->id, "parameter definition is invalid");
            return false;
        }
        for (size_t j = 0u; j < i; ++j) {
            if (strcmp(parameter->id, manifest->parameters[j].id) == 0) {
                report_set(report,
                           PROCEDURAL_SURFACE_AUTHORING_STATUS_PARAMETER,
                           parameter->id, "parameter id is duplicated");
                return false;
            }
        }
        for (size_t j = 0u; j < graph->node_count; ++j) {
            if (strcmp(parameter->node_id, graph->nodes[j].id) == 0) {
                node_index = (int)j;
                break;
            }
        }
        if (node_index < 0 ||
            !target_compatible(parameter, &graph->nodes[node_index])) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_TARGET,
                       parameter->id,
                       "parameter target does not match its graph node");
            return false;
        }
    }
    return true;
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
    if (!text || !text[0] || strlen(text) >= capacity) return false;
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

bool ProceduralSurfaceParameterManifestV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceParameterManifestV1 *out_manifest,
    ProceduralSurfaceAuthoringReport *report) {
    struct json_object *root = NULL;
    struct json_object *schema = NULL;
    struct json_object *version = NULL;
    struct json_object *parameters = NULL;
    ProceduralSurfaceParameterManifestV1 manifest;
    bool result = false;
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_OK, "", "ok");
    if (!path || !out_manifest) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
                   "path", "manifest path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_IO,
                   "path", "unable to parse parameter manifest");
        return false;
    }
    ProceduralSurfaceParameterManifestV1_Init(&manifest);
    if (!json_object_object_get_ex(root, "schema", &schema) ||
        json_object_get_type(schema) != json_type_string ||
        strcmp(json_object_get_string(schema),
               PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &version) ||
        json_object_get_type(version) != json_type_int ||
        json_object_get_int64(version) !=
            PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA_VERSION ||
        !json_text(root, "manifest_id", manifest.manifest_id,
                   sizeof(manifest.manifest_id)) ||
        !json_text(root, "graph_program_id", manifest.graph_program_id,
                   sizeof(manifest.graph_program_id)) ||
        !json_object_object_get_ex(root, "parameters", &parameters) ||
        json_object_get_type(parameters) != json_type_array) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA,
                   "root", "parameter manifest JSON root is invalid");
        goto cleanup;
    }
    manifest.parameter_count = json_object_array_length(parameters);
    if (manifest.parameter_count == 0u ||
        manifest.parameter_count > PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA,
                   "parameters", "parameter count is outside capacity");
        goto cleanup;
    }
    for (size_t i = 0u; i < manifest.parameter_count; ++i) {
        struct json_object *entry = json_object_array_get_idx(parameters, i);
        struct json_object *target = NULL;
        ProceduralSurfaceParameter *parameter = &manifest.parameters[i];
        if (!entry || json_object_get_type(entry) != json_type_object ||
            !json_text(entry, "id", parameter->id, sizeof(parameter->id)) ||
            !json_text(entry, "label", parameter->label,
                       sizeof(parameter->label)) ||
            !json_text(entry, "unit", parameter->unit,
                       sizeof(parameter->unit)) ||
            !json_text(entry, "node_id", parameter->node_id,
                       sizeof(parameter->node_id)) ||
            !json_object_object_get_ex(entry, "target", &target) ||
            json_object_get_type(target) != json_type_string ||
            (parameter->target =
                 parse_target(json_object_get_string(target))) ==
                PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID ||
            !json_number(entry, "minimum", &parameter->minimum) ||
            !json_number(entry, "maximum", &parameter->maximum) ||
            !json_number(entry, "default", &parameter->default_value)) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_JSON,
                       "parameters", "parameter entry is invalid");
            goto cleanup;
        }
    }
    *out_manifest = manifest;
    result = true;
cleanup:
    json_object_put(root);
    return result;
}

static int parameter_compare(const void *a, const void *b) {
    const ProceduralSurfaceParameter *left = a;
    const ProceduralSurfaceParameter *right = b;
    return strcmp(left->id, right->id);
}

bool ProceduralSurfaceParameterManifestV1_CanonicalJson(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceAuthoringReport *report) {
    ProceduralSurfaceParameter sorted[PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT];
    size_t used = 0u;
    int written;
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_OK, "", "ok");
    if (!manifest || !out_json || out_capacity == 0u ||
        manifest->parameter_count == 0u ||
        manifest->parameter_count > PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
                   "canonical", "manifest and output buffer are required");
        return false;
    }
    memcpy(sorted, manifest->parameters,
           manifest->parameter_count * sizeof(sorted[0]));
    qsort(sorted, manifest->parameter_count, sizeof(sorted[0]),
          parameter_compare);
#define APPEND(...) \
    do { \
        written = snprintf(out_json + used, out_capacity - used, __VA_ARGS__); \
        if (written < 0 || (size_t)written >= out_capacity - used) goto capacity; \
        used += (size_t)written; \
    } while (0)
    APPEND("{\"schema\":\"%s\",\"schema_version\":%u,"
           "\"manifest_id\":\"%s\",\"graph_program_id\":\"%s\","
           "\"parameters\":[",
           PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA,
           manifest->schema_version, manifest->manifest_id,
           manifest->graph_program_id);
    for (size_t i = 0u; i < manifest->parameter_count; ++i) {
        const ProceduralSurfaceParameter *parameter = &sorted[i];
        APPEND("%s{\"id\":\"%s\",\"label\":\"%s\",\"unit\":\"%s\","
               "\"node_id\":\"%s\",\"target\":\"%s\","
               "\"minimum\":%.17g,\"maximum\":%.17g,\"default\":%.17g}",
               i ? "," : "", parameter->id, parameter->label,
               parameter->unit, parameter->node_id,
               ProceduralSurfaceParameterTarget_Name(parameter->target),
               parameter->minimum, parameter->maximum,
               parameter->default_value);
    }
    APPEND("]}");
#undef APPEND
    return true;
capacity:
#undef APPEND
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA,
               "canonical", "canonical manifest exceeds output capacity");
    return false;
}

bool ProceduralSurfaceParameterManifestV1_Digest(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    char out_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceAuthoringReport *report) {
    char canonical[65536];
    if (!out_digest ||
        !ProceduralSurfaceParameterManifestV1_CanonicalJson(
            manifest, canonical, sizeof(canonical), report)) {
        return false;
    }
    if (!ray_tracing_sha256_bytes(
            canonical, strlen(canonical), out_digest)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
                   "digest", "unable to digest canonical manifest");
        return false;
    }
    return true;
}

bool ProceduralSurfaceAuthoring_ApplyParameter(
    const ProceduralSurfaceFieldGraphV1 *base_graph,
    const ProceduralSurfaceParameterManifestV1 *manifest,
    const char *expected_base_digest,
    const char *parameter_id,
    double value,
    ProceduralSurfaceFieldGraphV1 *out_graph,
    ProceduralSurfaceAuthoringReport *report) {
    ProceduralSurfaceFieldGraphV1 edited;
    ProceduralSurfaceFieldGraphReport graph_report;
    const ProceduralSurfaceParameter *parameter = NULL;
    ProceduralSurfaceFieldGraphNode *node = NULL;
    char base_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char result_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_OK, "", "ok");
    if (!base_graph || !manifest || !parameter_id || !out_graph ||
        !isfinite(value)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
                   "arguments", "authoring edit arguments are invalid");
        return false;
    }
    if (!ProceduralSurfaceParameterManifestV1_Validate(
            manifest, base_graph, report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            base_graph, base_digest, &graph_report)) {
        if (report && report->status == PROCEDURAL_SURFACE_AUTHORING_STATUS_OK) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
                       graph_report.field, graph_report.message);
        }
        return false;
    }
    if (expected_base_digest && expected_base_digest[0] &&
        strcmp(expected_base_digest, base_digest) != 0) {
        report_set(report,
                   PROCEDURAL_SURFACE_AUTHORING_STATUS_BASE_DIGEST,
                   "base_graph_digest_sha256",
                   "base graph changed since the edit was planned");
        snprintf(report->base_graph_digest_sha256,
                 sizeof(report->base_graph_digest_sha256), "%s", base_digest);
        return false;
    }
    for (size_t i = 0u; i < manifest->parameter_count; ++i) {
        if (strcmp(manifest->parameters[i].id, parameter_id) == 0) {
            parameter = &manifest->parameters[i];
            break;
        }
    }
    if (!parameter) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_PARAMETER,
                   parameter_id, "parameter id is not exposed");
        return false;
    }
    if (value < parameter->minimum || value > parameter->maximum) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE,
                   parameter_id, "parameter value is outside its range");
        return false;
    }
    edited = *base_graph;
    for (size_t i = 0u; i < edited.node_count; ++i) {
        if (strcmp(edited.nodes[i].id, parameter->node_id) == 0) {
            node = &edited.nodes[i];
            break;
        }
    }
    if (!node || !target_compatible(parameter, node)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_TARGET,
                   parameter_id, "parameter target is unavailable");
        return false;
    }
    switch (parameter->target) {
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_VALUE:
            node->value = value;
            break;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_SEED:
            if (floor(value) != value || value < 0.0 ||
                value > 9007199254740991.0) {
                report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE,
                           parameter_id, "seed must be an exact nonnegative integer");
                return false;
            }
            node->seed = (uint64_t)value;
            break;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_OCTAVES:
            if (floor(value) != value || value < 1.0 ||
                value > (double)UINT32_MAX) {
                report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE,
                           parameter_id, "octaves must be a positive integer");
                return false;
            }
            node->octaves = (uint32_t)value;
            break;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_LACUNARITY:
            node->lacunarity = value;
            break;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_PERSISTENCE:
            node->persistence = value;
            break;
        case PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID:
            return false;
    }
    if (!ProceduralSurfaceFieldGraphV1_Validate(&edited, &graph_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &edited, result_digest, &graph_report)) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
                   graph_report.field, graph_report.message);
        return false;
    }
    *out_graph = edited;
    if (report) {
        snprintf(report->base_graph_digest_sha256,
                 sizeof(report->base_graph_digest_sha256), "%s", base_digest);
        snprintf(report->result_graph_digest_sha256,
                 sizeof(report->result_graph_digest_sha256), "%s",
                 result_digest);
    }
    return true;
}

bool ProceduralSurfaceAuthoring_SaveGraphAtomic(
    const char *path,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceAuthoringReport *report) {
    char *canonical = NULL;
    CoreResult result;
    bool ok = false;
    report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_OK, "", "ok");
    if (!path || !graph) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
                   "path", "graph path and graph are required");
        return false;
    }
    canonical = calloc(PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY, 1u);
    if (!canonical) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SAVE,
                   "allocation", "unable to allocate canonical graph buffer");
        return false;
    }
    {
        ProceduralSurfaceFieldGraphReport graph_report;
        if (!ProceduralSurfaceFieldGraphV1_CanonicalJson(
                graph, canonical,
                PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY,
                &graph_report)) {
            report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
                       graph_report.field, graph_report.message);
            goto cleanup;
        }
    }
    result = core_io_write_all_atomic(path, canonical, strlen(canonical));
    if (result.code != CORE_OK) {
        report_set(report, PROCEDURAL_SURFACE_AUTHORING_STATUS_SAVE,
                   "path", result.message);
        goto cleanup;
    }
    ok = true;
cleanup:
    free(canonical);
    return ok;
}
