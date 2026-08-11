#include "procedural/procedural_solid_authoring.h"

#include "core_io.h"

#include <json-c/json.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EDITS 64u

typedef enum Mode {
    MODE_NONE = 0,
    MODE_INSPECT,
    MODE_APPLY,
    MODE_RESTORE
} Mode;

typedef struct Edit {
    char id[PROCEDURAL_SOLID_AUTHORING_PARAMETER_ID_CAPACITY];
    double value;
} Edit;

typedef struct Options {
    Mode mode;
    const char *graph_path;
    const char *output_path;
    const char *undo_path;
    const char *restore_path;
    const char *receipt_path;
    const char *expected_digest;
    size_t edit_count;
    Edit edits[MAX_EDITS];
} Options;

static void usage(const char *program) {
    fprintf(
        stderr,
        "usage:\n"
        "  %s inspect --graph PATH [--receipt-out PATH]\n"
        "  %s apply --graph PATH --expected-base-digest HEX "
        "--set PARAM=VALUE [--set ...] --out PATH --undo-out PATH "
        "[--receipt-out PATH]\n"
        "  %s restore --graph CURRENT --restore UNDO "
        "--expected-base-digest CURRENT_HEX --out PATH "
        "[--receipt-out PATH]\n",
        program, program, program);
}

static bool parse_edit(const char *text, Edit *out) {
    const char *equals;
    char *end = NULL;
    size_t length;
    if (!text || !out || !(equals = strchr(text, '='))) return false;
    length = (size_t)(equals - text);
    if (length == 0u || length >= sizeof(out->id)) return false;
    errno = 0;
    out->value = strtod(equals + 1, &end);
    if (errno || !end || *end || !isfinite(out->value)) return false;
    memset(out->id, 0, sizeof(out->id));
    memcpy(out->id, text, length);
    return true;
}

static bool parse_options(int argc, char **argv, Options *options) {
    if (argc < 2) return false;
    memset(options, 0, sizeof(*options));
    if (strcmp(argv[1], "inspect") == 0) options->mode = MODE_INSPECT;
    else if (strcmp(argv[1], "apply") == 0) options->mode = MODE_APPLY;
    else if (strcmp(argv[1], "restore") == 0) options->mode = MODE_RESTORE;
    else return false;
    for (int i = 2; i < argc; ++i) {
        if (i + 1 >= argc) return false;
        if (strcmp(argv[i], "--graph") == 0) {
            options->graph_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--undo-out") == 0) {
            options->undo_path = argv[++i];
        } else if (strcmp(argv[i], "--restore") == 0) {
            options->restore_path = argv[++i];
        } else if (strcmp(argv[i], "--receipt-out") == 0) {
            options->receipt_path = argv[++i];
        } else if (strcmp(argv[i], "--expected-base-digest") == 0) {
            options->expected_digest = argv[++i];
        } else if (strcmp(argv[i], "--set") == 0) {
            if (options->edit_count >= MAX_EDITS ||
                !parse_edit(argv[++i],
                            &options->edits[options->edit_count])) {
                return false;
            }
            ++options->edit_count;
        } else {
            return false;
        }
    }
    if (options->mode == MODE_INSPECT) return options->graph_path != NULL;
    if (options->mode == MODE_APPLY) {
        return options->graph_path && options->output_path &&
               options->undo_path && options->expected_digest &&
               options->edit_count > 0u;
    }
    return options->graph_path && options->output_path &&
           options->restore_path && options->expected_digest;
}

static bool write_receipt(const char *path, json_object *root) {
    const char *text;
    CoreResult result;
    if (!path) return true;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static json_object *parameter_json(
    const ProceduralSolidParameter *parameter) {
    json_object *entry = json_object_new_object();
    json_object_object_add(entry, "id",
                           json_object_new_string(parameter->id));
    json_object_object_add(entry, "node_id",
                           json_object_new_string(parameter->node_id));
    json_object_object_add(
        entry, "target",
        json_object_new_string(
            ProceduralSolidParameterTarget_Name(parameter->target)));
    json_object_object_add(entry, "label",
                           json_object_new_string(parameter->label));
    json_object_object_add(entry, "unit",
                           json_object_new_string(parameter->unit));
    json_object_object_add(entry, "value",
                           json_object_new_double(parameter->value));
    json_object_object_add(entry, "minimum",
                           json_object_new_double(parameter->minimum));
    json_object_object_add(entry, "maximum",
                           json_object_new_double(parameter->maximum));
    return entry;
}

static json_object *base_receipt(const char *mode) {
    json_object *root = json_object_new_object();
    json_object_object_add(
        root, "schema",
        json_object_new_string("ray_tracing.procedural_solid_agent"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string(mode));
    return root;
}

static int inspect_graph(const Options *options) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidAuthoringView view;
    ProceduralSolidAuthoringReport report;
    json_object *root;
    json_object *nodes;
    json_object *parameters;
    if (!ProceduralSolidGraphV1_LoadJsonFile(
            options->graph_path, &graph, &graph_report) ||
        !ProceduralSolidAuthoring_Inspect(&graph, &view, &report)) {
        fprintf(stderr, "inspect failed: %s\n",
                report.message[0] ? report.message : graph_report.message);
        return 1;
    }
    root = base_receipt("inspect");
    nodes = json_object_new_array();
    parameters = json_object_new_array();
    json_object_object_add(root, "graph_id",
                           json_object_new_string(graph.graph_id));
    json_object_object_add(root, "semantic_source_id",
                           json_object_new_string(graph.semantic_source_id));
    json_object_object_add(root, "output_node_id",
                           json_object_new_string(graph.output));
    json_object_object_add(root, "graph_digest_sha256",
                           json_object_new_string(view.graph_digest_sha256));
    json_object_object_add(root, "node_count",
                           json_object_new_int64((int64_t)view.node_count));
    json_object_object_add(
        root, "connection_count",
        json_object_new_int64((int64_t)view.connection_count));
    for (size_t i = 0u; i < graph.node_count; ++i) {
        const ProceduralSolidGraphNode *node = &graph.nodes[i];
        json_object *entry = json_object_new_object();
        json_object *inputs = json_object_new_array();
        json_object_object_add(entry, "id",
                               json_object_new_string(node->id));
        json_object_object_add(
            entry, "op",
            json_object_new_string(ProceduralSolidNodeOp_Name(node->op)));
        for (size_t j = 0u; j < node->input_count; ++j) {
            json_object_array_add(
                inputs, json_object_new_string(node->inputs[j]));
        }
        json_object_object_add(entry, "inputs", inputs);
        json_object_array_add(nodes, entry);
    }
    for (size_t i = 0u; i < view.parameter_count; ++i) {
        json_object_array_add(parameters,
                              parameter_json(&view.parameters[i]));
    }
    json_object_object_add(root, "nodes", nodes);
    json_object_object_add(root, "parameters", parameters);
    if (!write_receipt(options->receipt_path, root)) {
        fprintf(stderr, "inspect receipt write failed\n");
        json_object_put(root);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static int apply_edits(const Options *options) {
    ProceduralSolidGraphV1 base;
    ProceduralSolidGraphV1 edited;
    ProceduralSolidGraphV1 next;
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidAuthoringReport report;
    char base_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char result_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    const char *expected;
    json_object *root;
    json_object *edits;
    if (!ProceduralSolidGraphV1_LoadJsonFile(
            options->graph_path, &base, &graph_report) ||
        !ProceduralSolidGraphV1_Digest(
            &base, base_digest, &graph_report) ||
        strcmp(base_digest, options->expected_digest) != 0) {
        fprintf(stderr, "apply preflight failed: current graph changed\n");
        return 1;
    }
    edited = base;
    expected = base_digest;
    for (size_t i = 0u; i < options->edit_count; ++i) {
        if (!ProceduralSolidAuthoring_ApplyParameter(
                &edited, expected, options->edits[i].id,
                options->edits[i].value, &next, &report)) {
            fprintf(stderr, "edit %zu failed: %s\n", i, report.message);
            return 1;
        }
        edited = next;
        snprintf(result_digest, sizeof(result_digest), "%s",
                 report.result_graph_digest_sha256);
        expected = result_digest;
    }
    if (!ProceduralSolidAuthoring_SaveGraphAtomic(
            options->undo_path, &base, &report) ||
        !ProceduralSolidAuthoring_SaveGraphAtomic(
            options->output_path, &edited, &report)) {
        fprintf(stderr, "apply save failed: %s\n", report.message);
        return 1;
    }
    root = base_receipt("apply");
    edits = json_object_new_array();
    json_object_object_add(root, "status",
                           json_object_new_string("committed"));
    json_object_object_add(root, "base_graph_digest_sha256",
                           json_object_new_string(base_digest));
    json_object_object_add(root, "result_graph_digest_sha256",
                           json_object_new_string(result_digest));
    json_object_object_add(root, "undo_graph_path",
                           json_object_new_string(options->undo_path));
    for (size_t i = 0u; i < options->edit_count; ++i) {
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "parameter_id",
                               json_object_new_string(options->edits[i].id));
        json_object_object_add(entry, "value",
                               json_object_new_double(options->edits[i].value));
        json_object_array_add(edits, entry);
    }
    json_object_object_add(root, "edits", edits);
    if (!write_receipt(options->receipt_path, root)) {
        fprintf(stderr, "apply receipt write failed\n");
        json_object_put(root);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static int restore_graph(const Options *options) {
    ProceduralSolidGraphV1 current;
    ProceduralSolidGraphV1 restore;
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidAuthoringReport report;
    char current_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char restore_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    json_object *root;
    if (!ProceduralSolidGraphV1_LoadJsonFile(
            options->graph_path, &current, &graph_report) ||
        !ProceduralSolidGraphV1_LoadJsonFile(
            options->restore_path, &restore, &graph_report) ||
        !ProceduralSolidGraphV1_Digest(
            &current, current_digest, &graph_report) ||
        !ProceduralSolidGraphV1_Digest(
            &restore, restore_digest, &graph_report) ||
        strcmp(current_digest, options->expected_digest) != 0) {
        fprintf(stderr, "restore preflight failed: current graph changed\n");
        return 1;
    }
    if (!ProceduralSolidAuthoring_SaveGraphAtomic(
            options->output_path, &restore, &report)) {
        fprintf(stderr, "restore failed: %s\n", report.message);
        return 1;
    }
    root = base_receipt("restore");
    json_object_object_add(root, "status",
                           json_object_new_string("committed"));
    json_object_object_add(root, "base_graph_digest_sha256",
                           json_object_new_string(current_digest));
    json_object_object_add(root, "result_graph_digest_sha256",
                           json_object_new_string(restore_digest));
    if (!write_receipt(options->receipt_path, root)) {
        fprintf(stderr, "restore receipt write failed\n");
        json_object_put(root);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (options.mode == MODE_INSPECT) return inspect_graph(&options);
    if (options.mode == MODE_APPLY) return apply_edits(&options);
    if (options.mode == MODE_RESTORE) return restore_graph(&options);
    return 2;
}
