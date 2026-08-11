#include "procedural/procedural_surface_authoring.h"
#include "procedural/procedural_surface_binding.h"

#include "core_io.h"

#include <json-c/json.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EDITS 32u

typedef enum ToolMode {
    TOOL_MODE_NONE = 0,
    TOOL_MODE_INSPECT,
    TOOL_MODE_APPLY,
    TOOL_MODE_RESTORE
} ToolMode;

typedef struct ParameterEdit {
    char parameter_id[PROCEDURAL_SURFACE_PARAMETER_ID_CAPACITY];
    double value;
} ParameterEdit;

typedef struct ToolOptions {
    ToolMode mode;
    const char *graph_path;
    const char *manifest_path;
    const char *binding_path;
    const char *output_path;
    const char *undo_output_path;
    const char *receipt_output_path;
    const char *restore_path;
    const char *expected_base_digest;
    ParameterEdit edits[MAX_EDITS];
    size_t edit_count;
} ToolOptions;

static void usage(const char *program) {
    fprintf(
        stderr,
        "usage:\n"
        "  %s inspect --graph PATH --manifest PATH [--binding PATH]\n"
        "  %s apply --graph PATH --manifest PATH --expected-base-digest HEX "
        "--set PARAM=VALUE [--set ...] --out PATH --undo-out PATH "
        "[--receipt-out PATH] [--binding PATH]\n"
        "  %s restore --graph CURRENT --restore UNDO "
        "--expected-base-digest CURRENT_HEX --out PATH "
        "[--receipt-out PATH]\n",
        program, program, program);
}

static bool parse_edit(const char *text, ParameterEdit *out) {
    const char *equals;
    char *end = NULL;
    size_t id_length;
    double value;
    if (!text || !out || !(equals = strchr(text, '='))) return false;
    id_length = (size_t)(equals - text);
    if (id_length == 0u ||
        id_length >= sizeof(out->parameter_id)) return false;
    errno = 0;
    value = strtod(equals + 1, &end);
    if (errno || !end || *end != '\0' || !isfinite(value)) return false;
    memset(out, 0, sizeof(*out));
    memcpy(out->parameter_id, text, id_length);
    out->value = value;
    return true;
}

static bool parse_options(
    int argc,
    char **argv,
    ToolOptions *options) {
    if (argc < 2 || !options) return false;
    memset(options, 0, sizeof(*options));
    if (strcmp(argv[1], "inspect") == 0) {
        options->mode = TOOL_MODE_INSPECT;
    } else if (strcmp(argv[1], "apply") == 0) {
        options->mode = TOOL_MODE_APPLY;
    } else if (strcmp(argv[1], "restore") == 0) {
        options->mode = TOOL_MODE_RESTORE;
    } else {
        return false;
    }
    for (int i = 2; i < argc; ++i) {
        const char *flag = argv[i];
        if (i + 1 >= argc) return false;
#define PATH_OPTION(name, member) \
        if (strcmp(flag, name) == 0) options->member = argv[++i]
        PATH_OPTION("--graph", graph_path);
        else PATH_OPTION("--manifest", manifest_path);
        else PATH_OPTION("--binding", binding_path);
        else PATH_OPTION("--out", output_path);
        else PATH_OPTION("--undo-out", undo_output_path);
        else PATH_OPTION("--receipt-out", receipt_output_path);
        else PATH_OPTION("--restore", restore_path);
        else PATH_OPTION("--expected-base-digest", expected_base_digest);
#undef PATH_OPTION
        else if (strcmp(flag, "--set") == 0) {
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
    if (options->mode == TOOL_MODE_INSPECT) {
        return options->graph_path && options->manifest_path;
    }
    if (options->mode == TOOL_MODE_APPLY) {
        return options->graph_path && options->manifest_path &&
               options->expected_base_digest && options->output_path &&
               options->undo_output_path && options->edit_count > 0u;
    }
    return options->graph_path && options->restore_path &&
           options->expected_base_digest && options->output_path;
}

static bool write_json_atomic(
    const char *path,
    struct json_object *root) {
    const char *text;
    CoreResult result;
    if (!path || !root) return true;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    if (!text) return false;
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static struct json_object *new_parameter_json(
    const ProceduralSurfaceParameter *parameter) {
    struct json_object *entry = json_object_new_object();
    json_object_object_add(entry, "id",
                           json_object_new_string(parameter->id));
    json_object_object_add(entry, "label",
                           json_object_new_string(parameter->label));
    json_object_object_add(entry, "unit",
                           json_object_new_string(parameter->unit));
    json_object_object_add(entry, "node_id",
                           json_object_new_string(parameter->node_id));
    json_object_object_add(
        entry, "target",
        json_object_new_string(
            ProceduralSurfaceParameterTarget_Name(parameter->target)));
    json_object_object_add(entry, "minimum",
                           json_object_new_double(parameter->minimum));
    json_object_object_add(entry, "maximum",
                           json_object_new_double(parameter->maximum));
    json_object_object_add(entry, "default",
                           json_object_new_double(parameter->default_value));
    return entry;
}

static int run_inspect(const ToolOptions *options) {
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceParameterManifestV1 manifest;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceAuthoringReport authoring_report;
    ProceduralSurfaceBindingReport binding_report;
    char graph_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char manifest_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char binding_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY] = "";
    struct json_object *root = NULL;
    struct json_object *parameters = NULL;
    if (!ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options->graph_path, &graph, &graph_report) ||
        !ProceduralSurfaceParameterManifestV1_LoadJsonFile(
            options->manifest_path, &manifest, &authoring_report) ||
        !ProceduralSurfaceParameterManifestV1_Validate(
            &manifest, &graph, &authoring_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &graph, graph_digest, &graph_report) ||
        !ProceduralSurfaceParameterManifestV1_Digest(
            &manifest, manifest_digest, &authoring_report)) {
        fprintf(stderr, "inspect failed: %s\n",
                authoring_report.message[0]
                    ? authoring_report.message
                    : graph_report.message);
        return 1;
    }
    if (options->binding_path &&
        (!ProceduralSurfaceBindingV1_LoadJsonFile(
             options->binding_path, &binding, &binding_report) ||
         !ProceduralSurfaceBindingV1_Validate(
             &binding, &graph, &binding_report) ||
         !ProceduralSurfaceBindingV1_Digest(
             &binding, binding_digest, &binding_report))) {
        fprintf(stderr, "binding inspect failed: %s\n",
                binding_report.message);
        return 1;
    }
    root = json_object_new_object();
    parameters = json_object_new_array();
    json_object_object_add(
        root, "schema",
        json_object_new_string("ray_tracing.procedural_surface_agent_readback"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string("inspect"));
    json_object_object_add(root, "program_id",
                           json_object_new_string(graph.program_id));
    json_object_object_add(root, "graph_digest_sha256",
                           json_object_new_string(graph_digest));
    json_object_object_add(root, "manifest_digest_sha256",
                           json_object_new_string(manifest_digest));
    json_object_object_add(root, "node_count",
                           json_object_new_int64(graph.node_count));
    json_object_object_add(root, "parameter_count",
                           json_object_new_int64(manifest.parameter_count));
    for (size_t i = 0u; i < manifest.parameter_count; ++i) {
        json_object_array_add(
            parameters, new_parameter_json(&manifest.parameters[i]));
    }
    json_object_object_add(root, "parameters", parameters);
    if (options->binding_path) {
        struct json_object *binding_json = json_object_new_object();
        json_object_object_add(
            binding_json, "binding_id",
            json_object_new_string(binding.binding_id));
        json_object_object_add(
            binding_json, "digest_sha256",
            json_object_new_string(binding_digest));
        json_object_object_add(
            binding_json, "selector",
            json_object_new_string(
                ProceduralSurfaceSelectorKind_Name(binding.selector)));
        json_object_object_add(
            binding_json, "projection",
            json_object_new_string(
                ProceduralSurfaceProjectionKind_Name(binding.projection)));
        json_object_object_add(
            binding_json, "displacement_direction",
            json_object_new_string(
                ProceduralSurfaceDisplacementDirection_Name(
                    binding.displacement_direction)));
        json_object_object_add(root, "binding", binding_json);
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static int run_apply(const ToolOptions *options) {
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphV1 edited;
    ProceduralSurfaceFieldGraphV1 next;
    ProceduralSurfaceParameterManifestV1 manifest;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceAuthoringReport authoring_report;
    ProceduralSurfaceBindingReport binding_report;
    char base_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char result_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char manifest_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char binding_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY] = "";
    const char *expected_digest = options->expected_base_digest;
    struct json_object *root = NULL;
    struct json_object *edits = NULL;
    if (!ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options->graph_path, &graph, &graph_report) ||
        !ProceduralSurfaceParameterManifestV1_LoadJsonFile(
            options->manifest_path, &manifest, &authoring_report) ||
        !ProceduralSurfaceParameterManifestV1_Validate(
            &manifest, &graph, &authoring_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &graph, base_digest, &graph_report) ||
        strcmp(base_digest, options->expected_base_digest) != 0 ||
        !ProceduralSurfaceParameterManifestV1_Digest(
            &manifest, manifest_digest, &authoring_report)) {
        fprintf(stderr, "apply preflight failed: graph or base digest changed\n");
        return 1;
    }
    if (options->binding_path &&
        (!ProceduralSurfaceBindingV1_LoadJsonFile(
             options->binding_path, &binding, &binding_report) ||
         !ProceduralSurfaceBindingV1_Validate(
             &binding, &graph, &binding_report) ||
         !ProceduralSurfaceBindingV1_Digest(
             &binding, binding_digest, &binding_report))) {
        fprintf(stderr, "binding preflight failed: %s\n",
                binding_report.message);
        return 1;
    }
    edited = graph;
    for (size_t i = 0u; i < options->edit_count; ++i) {
        if (!ProceduralSurfaceAuthoring_ApplyParameter(
                &edited, &manifest, expected_digest,
                options->edits[i].parameter_id, options->edits[i].value,
                &next, &authoring_report)) {
            fprintf(stderr, "edit %zu failed: %s\n",
                    i, authoring_report.message);
            return 1;
        }
        edited = next;
        expected_digest = authoring_report.result_graph_digest_sha256;
    }
    if (!ProceduralSurfaceFieldGraphV1_Digest(
            &edited, result_digest, &graph_report) ||
        !ProceduralSurfaceAuthoring_SaveGraphAtomic(
            options->undo_output_path, &graph, &authoring_report) ||
        !ProceduralSurfaceAuthoring_SaveGraphAtomic(
            options->output_path, &edited, &authoring_report)) {
        fprintf(stderr, "apply save failed: %s\n", authoring_report.message);
        return 1;
    }
    root = json_object_new_object();
    edits = json_object_new_array();
    json_object_object_add(
        root, "schema",
        json_object_new_string(
            "ray_tracing.procedural_surface_agent_transaction"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string("apply"));
    json_object_object_add(root, "status", json_object_new_string("committed"));
    json_object_object_add(root, "base_graph_digest_sha256",
                           json_object_new_string(base_digest));
    json_object_object_add(root, "result_graph_digest_sha256",
                           json_object_new_string(result_digest));
    json_object_object_add(root, "manifest_digest_sha256",
                           json_object_new_string(manifest_digest));
    if (binding_digest[0]) {
        json_object_object_add(root, "binding_digest_sha256",
                               json_object_new_string(binding_digest));
    }
    json_object_object_add(root, "undo_graph_path",
                           json_object_new_string(options->undo_output_path));
    for (size_t i = 0u; i < options->edit_count; ++i) {
        struct json_object *entry = json_object_new_object();
        json_object_object_add(
            entry, "parameter_id",
            json_object_new_string(options->edits[i].parameter_id));
        json_object_object_add(
            entry, "value",
            json_object_new_double(options->edits[i].value));
        json_object_array_add(edits, entry);
    }
    json_object_object_add(root, "edits", edits);
    if (!write_json_atomic(options->receipt_output_path, root)) {
        fprintf(stderr, "receipt write failed\n");
        json_object_put(root);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static int run_restore(const ToolOptions *options) {
    ProceduralSurfaceFieldGraphV1 current;
    ProceduralSurfaceFieldGraphV1 restore;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceAuthoringReport authoring_report;
    char current_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char restore_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    struct json_object *root = NULL;
    if (!ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options->graph_path, &current, &graph_report) ||
        !ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options->restore_path, &restore, &graph_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &current, current_digest, &graph_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &restore, restore_digest, &graph_report) ||
        strcmp(current_digest, options->expected_base_digest) != 0) {
        fprintf(stderr, "restore preflight failed: current graph changed\n");
        return 1;
    }
    if (!ProceduralSurfaceAuthoring_SaveGraphAtomic(
            options->output_path, &restore, &authoring_report)) {
        fprintf(stderr, "restore save failed: %s\n", authoring_report.message);
        return 1;
    }
    root = json_object_new_object();
    json_object_object_add(
        root, "schema",
        json_object_new_string(
            "ray_tracing.procedural_surface_agent_transaction"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string("restore"));
    json_object_object_add(root, "status", json_object_new_string("committed"));
    json_object_object_add(root, "base_graph_digest_sha256",
                           json_object_new_string(current_digest));
    json_object_object_add(root, "result_graph_digest_sha256",
                           json_object_new_string(restore_digest));
    if (!write_json_atomic(options->receipt_output_path, root)) {
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
    ToolOptions options;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (options.mode == TOOL_MODE_INSPECT) return run_inspect(&options);
    if (options.mode == TOOL_MODE_APPLY) return run_apply(&options);
    if (options.mode == TOOL_MODE_RESTORE) return run_restore(&options);
    return 2;
}
