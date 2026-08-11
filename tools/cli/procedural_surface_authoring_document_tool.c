#include "procedural/procedural_surface_authoring_document.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s create --output PATH --document-id ID --source-object-id ID "
            "--source-mesh-digest HEX [--material ID DIGEST DOMAINS] "
            "[--surface-field ID DIGEST DOMAINS] [--face-selector ID DIGEST DOMAINS] "
            "[--attachment ID DIGEST DOMAINS]...\n"
            "       %s inspect --input PATH\n"
            "       %s canvas --input PATH\n"
            "       %s edit --input PATH --output PATH --replace FIELD ID DIGEST DOMAINS "
            "--expected-document-digest HEX --expected-source-mesh-digest HEX "
            "--expected-reference-digest HEX\n", program, program, program, program);
}

static void report_json(const char *operation,
                        const ProceduralSurfaceAuthoringDocumentReport *report) {
    printf("{\"operation\":\"%s\",\"status\":\"error\",\"field\":\"%s\",\"message\":\"%s\"}\n",
           operation, report ? report->field : "arguments",
           report ? report->message : "invalid arguments");
}

static bool copy_arg(char *out, size_t capacity, const char *value) {
    if (!value || strlen(value) >= capacity) return false;
    snprintf(out, capacity, "%s", value);
    return true;
}

static bool parse_domains(const char *text, uint32_t *out) {
    char *end = NULL;
    unsigned long numeric;
    uint32_t domains = 0u;
    char buffer[160];
    char *token;
    if (!text || !out || strlen(text) >= sizeof(buffer)) return false;
    errno = 0;
    numeric = strtoul(text, &end, 0);
    if (errno == 0 && end && *end == '\0' && numeric <= UINT32_MAX) {
        *out = (uint32_t)numeric;
        return domains != 0u || *out != 0u;
    }
    snprintf(buffer, sizeof(buffer), "%s", text);
    for (token = strtok(buffer, ",|"); token; token = strtok(NULL, ",|")) {
        if (strcmp(token, "material") == 0) domains |= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MATERIAL;
        else if (strcmp(token, "microdetail_normal") == 0) domains |= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL;
        else if (strcmp(token, "signed_relief") == 0) domains |= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_SIGNED_RELIEF;
        else if (strcmp(token, "deep_inset") == 0) domains |= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_DEEP_INSET;
        else if (strcmp(token, "attached_asset") == 0) domains |= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET;
        else return false;
    }
    *out = domains;
    return domains != 0u;
}

static bool parse_ref_args(int argc, char **argv, int *index,
                           ProceduralSurfaceAuthoringDocumentRef *ref) {
    uint32_t domains;
    if (*index + 3 >= argc || !copy_arg(ref->id, sizeof(ref->id), argv[*index + 1]) ||
        !copy_arg(ref->digest_sha256, sizeof(ref->digest_sha256), argv[*index + 2]) ||
        !parse_domains(argv[*index + 3], &domains)) return false;
    ref->output_domains = domains;
    *index += 3;
    return true;
}

static bool write_atomic(const char *path, const char *json,
                         ProceduralSurfaceAuthoringDocumentReport *report) {
    char temp[1024];
    FILE *file;
    bool failed = false;
    if (!path || !json || strlen(path) + 5u >= sizeof(temp)) {
        return false;
    }
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    file = fopen(temp, "w");
    if (!file) failed = true;
    else if (fputs(json, file) < 0 || fputc('\n', file) == EOF) failed = true;
    if (file && fclose(file) != 0) failed = true;
    if (failed) {
        remove(temp);
        if (report) {
            report->status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO;
            snprintf(report->field, sizeof(report->field), "output");
            snprintf(report->message, sizeof(report->message), "unable to write output atomically");
        }
        return false;
    }
    if (rename(temp, path) != 0) {
        remove(temp);
        if (report) {
            report->status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO;
            snprintf(report->field, sizeof(report->field), "output");
            snprintf(report->message, sizeof(report->message), "unable to replace output atomically");
        }
        return false;
    }
    return true;
}

static bool canonical(const ProceduralSurfaceAuthoringDocumentV1 *document,
                      char *out, size_t capacity,
                      ProceduralSurfaceAuthoringDocumentReport *report) {
    return ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(document, out, capacity, report);
}

static void plan_json(const ProceduralSurfaceAuthoringDocumentCompilePlan *plan) {
    printf("{\"valid\":%s,\"document_id\":\"%s\",\"document_digest_sha256\":\"%s\","
           "\"source_object_id\":\"%s\",\"source_mesh_digest_sha256\":\"%s\","
           "\"output_domains\":%u,\"attachment_count\":%u,"
           "\"material_graph_bound\":%s,\"surface_field_graph_bound\":%s,"
           "\"face_region_selector_bound\":%s}",
           plan->valid ? "true" : "false", plan->document_id,
           plan->document_digest_sha256, plan->source_object_id,
           plan->source_mesh_digest_sha256, plan->output_domains,
           plan->attachment_count, plan->material_graph_bound ? "true" : "false",
           plan->surface_field_graph_bound ? "true" : "false",
           plan->face_region_selector_bound ? "true" : "false");
}

static int inspect_document(const char *operation,
                            const ProceduralSurfaceAuthoringDocumentV1 *document) {
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    char json[16384];
    if (!canonical(document, json, sizeof(json), &report) ||
        !ProceduralSurfaceAuthoringDocumentV1_Compile(document, &plan, &report)) {
        report_json(operation, &report);
        return 1;
    }
    printf("{\"operation\":\"%s\",\"status\":\"ok\",\"canonical_document\":%s,\"compile_plan\":",
           operation, json);
    plan_json(&plan);
    printf("}\n");
    return 0;
}

static void canvas_node(const char *id, const char *kind, const char *label,
                        int x, int y, const char *digest, uint32_t domains) {
    printf("{\"id\":\"%s\",\"kind\":\"%s\",\"label\":\"%s\",\"x\":%d,\"y\":%d",
           id, kind, label, x, y);
    if (digest) {
        printf(",\"digest_sha256\":\"%s\",\"output_domains\":%u",
               digest, domains);
    }
    printf("}");
}

static void canvas_edge(const char *from, const char *to) {
    printf("{\"from\":\"%s\",\"to\":\"%s\"}", from, to);
}

static int canvas_document(const ProceduralSurfaceAuthoringDocumentV1 *document) {
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    char digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    const char *lanes[] = {"material_graph", "surface_field_graph",
                           "face_region_selector", "attachment_graph"};
    if (!ProceduralSurfaceAuthoringDocumentV1_Compile(document, &plan, &report) ||
        !ProceduralSurfaceAuthoringDocumentV1_Digest(document, digest, &report)) {
        report_json("canvas", &report);
        return 1;
    }
    printf("{\"schema\":\"ray_tracing.surface_authoring_document_canvas\","
           "\"schema_version\":1,\"mode\":\"inspect\","
           "\"interaction\":{\"read_only\":true,\"can_select\":true,"
           "\"can_zoom\":true,\"can_pan\":true,\"can_edit\":false,"
           "\"can_save\":false,\"can_promote\":false},"
           "\"document_id\":\"%s\",\"document_digest_sha256\":\"%s\","
           "\"source_object_id\":\"%s\",\"source_mesh_digest_sha256\":\"%s\","
           "\"nodes\":[", document->document_id, digest,
           document->source_object_id, document->source_mesh_digest_sha256);
    canvas_node("source_mesh", "source", document->source_object_id, 40, 280,
                document->source_mesh_digest_sha256, 0u);
    for (size_t i = 0u; i < 4u; ++i) {
        printf(",");
        canvas_node(lanes[i], "lane", lanes[i], 260, 80 + (int)i * 130, NULL, 0u);
    }
    if (document->material_graph.id[0]) {
        printf(","); canvas_node("ref:material_graph", "reference", document->material_graph.id,
                                  540, 80, document->material_graph.digest_sha256,
                                  document->material_graph.output_domains);
    }
    if (document->surface_field_graph.id[0]) {
        printf(","); canvas_node("ref:surface_field_graph", "reference", document->surface_field_graph.id,
                                  540, 210, document->surface_field_graph.digest_sha256,
                                  document->surface_field_graph.output_domains);
    }
    if (document->face_region_selector.id[0]) {
        printf(","); canvas_node("ref:face_region_selector", "reference", document->face_region_selector.id,
                                  540, 340, document->face_region_selector.digest_sha256,
                                  document->face_region_selector.output_domains);
    }
    for (size_t i = 0u; i < document->attachment_count; ++i) {
        printf(",");
        printf("{\"id\":\"ref:attachment:%s\",\"kind\":\"%s\",\"label\":\"%s\",\"x\":540,\"y\":%d,\"digest_sha256\":\"%s\",\"output_domains\":%u}",
               document->attachments[i].id, "attachment", document->attachments[i].id,
               470 + (int)i * 70, document->attachments[i].digest_sha256,
               document->attachments[i].output_domains);
    }
    printf("],\"edges\":[");
    canvas_edge("source_mesh", "material_graph");
    printf(","); canvas_edge("source_mesh", "surface_field_graph");
    printf(","); canvas_edge("source_mesh", "face_region_selector");
    printf(","); canvas_edge("source_mesh", "attachment_graph");
    if (document->material_graph.id[0]) { printf(","); canvas_edge("material_graph", "ref:material_graph"); }
    if (document->surface_field_graph.id[0]) { printf(","); canvas_edge("surface_field_graph", "ref:surface_field_graph"); }
    if (document->face_region_selector.id[0]) { printf(","); canvas_edge("face_region_selector", "ref:face_region_selector"); }
    for (size_t i = 0u; i < document->attachment_count; ++i) {
        printf(","); printf("{\"from\":\"attachment_graph\",\"to\":\"ref:attachment:%s\"}", document->attachments[i].id);
    }
    printf("],\"compile_plan\":");
    plan_json(&plan);
    printf("}\n");
    return 0;
}

static int create_document(int argc, char **argv) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentReport report;
    char *output = NULL;
    char json[16384];
    memset(&report, 0, sizeof(report));
    ProceduralSurfaceAuthoringDocumentV1_Init(&document);
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--document-id") == 0 && i + 1 < argc) {
            if (!copy_arg(document.document_id, sizeof(document.document_id), argv[++i])) return 2;
        } else if (strcmp(argv[i], "--source-object-id") == 0 && i + 1 < argc) {
            if (!copy_arg(document.source_object_id, sizeof(document.source_object_id), argv[++i])) return 2;
        } else if (strcmp(argv[i], "--source-mesh-digest") == 0 && i + 1 < argc) {
            if (!copy_arg(document.source_mesh_digest_sha256, sizeof(document.source_mesh_digest_sha256), argv[++i])) return 2;
        } else if (strcmp(argv[i], "--material") == 0) {
            if (!parse_ref_args(argc, argv, &i, &document.material_graph)) return 2;
        } else if (strcmp(argv[i], "--surface-field") == 0) {
            if (!parse_ref_args(argc, argv, &i, &document.surface_field_graph)) return 2;
        } else if (strcmp(argv[i], "--face-selector") == 0) {
            if (!parse_ref_args(argc, argv, &i, &document.face_region_selector)) return 2;
        } else if (strcmp(argv[i], "--attachment") == 0) {
            if (document.attachment_count >= PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_MAX_ATTACHMENTS ||
                !parse_ref_args(argc, argv, &i, &document.attachments[document.attachment_count])) return 2;
            document.attachment_count++;
        } else {
            return 2;
        }
    }
    if (!output || !canonical(&document, json, sizeof(json), &report) ||
        !write_atomic(output, json, &report)) {
        if (!report.message[0]) {
            report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT;
            snprintf(report.field, sizeof(report.field), "arguments");
            snprintf(report.message, sizeof(report.message), "create arguments are incomplete");
        }
        report_json("create", &report);
        return 1;
    }
    return inspect_document("create", &document);
}

static ProceduralSurfaceAuthoringDocumentRef *field_ref(
    ProceduralSurfaceAuthoringDocumentV1 *document, const char *field) {
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

static int edit_document(int argc, char **argv) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentV1 prior;
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentRef replacement;
    ProceduralSurfaceAuthoringDocumentRef *target = NULL;
    const char *input = NULL, *output = NULL, *field = NULL;
    const char *expected_document = NULL, *expected_source = NULL, *expected_reference = NULL;
    char prior_json[16384], json[16384], digest[65];
    memset(&report, 0, sizeof(report));
    memset(&replacement, 0, sizeof(replacement));
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) input = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--expected-document-digest") == 0 && i + 1 < argc) expected_document = argv[++i];
        else if (strcmp(argv[i], "--expected-source-mesh-digest") == 0 && i + 1 < argc) expected_source = argv[++i];
        else if (strcmp(argv[i], "--expected-reference-digest") == 0 && i + 1 < argc) expected_reference = argv[++i];
        else if (strcmp(argv[i], "--replace") == 0 && i + 4 < argc) {
            field = argv[++i];
            if (!copy_arg(replacement.id, sizeof(replacement.id), argv[++i]) ||
                !copy_arg(replacement.digest_sha256, sizeof(replacement.digest_sha256), argv[++i]) ||
                !parse_domains(argv[++i], &replacement.output_domains)) return 2;
        } else return 2;
    }
    if (!input || !output || !field || !expected_document || !expected_source || !expected_reference ||
        !ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(input, &document, &report)) {
        if (!report.message[0]) {
            report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT;
            snprintf(report.field, sizeof(report.field), "arguments");
            snprintf(report.message, sizeof(report.message), "edit arguments are incomplete");
        }
        report_json("edit", &report);
        return 1;
    }
    prior = document;
    if (!ProceduralSurfaceAuthoringDocumentV1_Digest(&document, digest, &report) || strcmp(digest, expected_document) != 0) {
        report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST;
        snprintf(report.field, sizeof(report.field), "expected_document_digest");
        snprintf(report.message, sizeof(report.message), "document digest is stale");
        report_json("edit", &report);
        return 1;
    }
    if (strcmp(document.source_mesh_digest_sha256, expected_source) != 0) {
        report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST;
        snprintf(report.field, sizeof(report.field), "expected_source_mesh_digest");
        snprintf(report.message, sizeof(report.message), "source mesh digest is stale");
        report_json("edit", &report);
        return 1;
    }
    target = field_ref(&document, field);
    if (!target) {
        report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA;
        snprintf(report.field, sizeof(report.field), "replace");
        snprintf(report.message, sizeof(report.message), "unsupported replacement field");
        report_json("edit", &report);
        return 1;
    }
    if (strcmp(target->digest_sha256, expected_reference) != 0) {
        report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST;
        snprintf(report.field, sizeof(report.field), "expected_reference_digest");
        snprintf(report.message, sizeof(report.message), "upstream reference digest is stale");
        report_json("edit", &report);
        return 1;
    }
    *target = replacement;
    if (!canonical(&document, json, sizeof(json), &report) ||
        !canonical(&prior, prior_json, sizeof(prior_json), &report) ||
        !write_atomic(output, json, &report)) {
        report_json("edit", &report);
        return 1;
    }
    printf("{\"operation\":\"edit\",\"status\":\"ok\",\"canonical_document\":%s,\"undo_document\":%s,\"compile_plan\":",
           json, prior_json);
    { ProceduralSurfaceAuthoringDocumentCompilePlan plan;
      if (!ProceduralSurfaceAuthoringDocumentV1_Compile(&document, &plan, &report)) {
          report_json("edit", &report); return 1;
      }
      plan_json(&plan); }
    printf("}\n");
    return 0;
}

int main(int argc, char **argv) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentReport report;
    if (argc < 2) { usage(argv[0]); return 2; }
    if (strcmp(argv[1], "create") == 0) return create_document(argc, argv);
    if (strcmp(argv[1], "edit") == 0) return edit_document(argc, argv);
    if (strcmp(argv[1], "inspect") == 0 && argc == 4 && strcmp(argv[2], "--input") == 0 &&
        ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(argv[3], &document, &report)) {
        return inspect_document("inspect", &document);
    }
    if (strcmp(argv[1], "canvas") == 0 && argc == 4 && strcmp(argv[2], "--input") == 0 &&
        ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(argv[3], &document, &report)) {
        return canvas_document(&document);
    }
    if (strcmp(argv[1], "inspect") == 0) {
        if (argc == 4) {
            ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(argv[3], &document, &report);
        } else {
            report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT;
            snprintf(report.field, sizeof(report.field), "arguments");
            snprintf(report.message, sizeof(report.message), "inspect requires --input PATH");
        }
        report_json("inspect", &report);
        return 1;
    }
    if (strcmp(argv[1], "canvas") == 0) {
        if (argc == 4) {
            ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(argv[3], &document, &report);
        } else {
            report.status = PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT;
            snprintf(report.field, sizeof(report.field), "arguments");
            snprintf(report.message, sizeof(report.message), "canvas requires --input PATH");
        }
        report_json("canvas", &report);
        return 1;
    }
    usage(argv[0]);
    return 2;
}
