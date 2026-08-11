#include "procedural/procedural_solid_authored_material.h"
#include "procedural/procedural_solid_material_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr,
        "usage:\n"
        "  %s init --template ID --graph-id ID --binding-id ID "
        "--binding-digest SHA --output PATH\n"
        "  %s inspect --graph PATH\n"
        "  %s set --graph PATH --expected-digest SHA --node ID "
        "--parameter ID --value N [--snapshot PATH]\n"
        "  %s connect --graph PATH --expected-digest SHA --node ID "
        "--input a|b --source ID [--snapshot PATH]\n"
        "  %s add-node --graph PATH --expected-digest SHA --node ID "
        "--kind KIND [--snapshot PATH]\n"
        "  %s bind-layer --graph PATH --expected-digest SHA "
        "--material-id ID --material PATH [--snapshot PATH]\n"
        "  %s compile --graph PATH\n"
        "  %s restore --graph PATH --snapshot PATH "
        "--expected-digest SHA\n", program, program, program,
        program, program, program, program, program);
}

static const char *arg_value(int argc, char **argv, const char *key) {
    for (int i = 2; i + 1 < argc; ++i)
        if (strcmp(argv[i], key) == 0) return argv[i + 1];
    return NULL;
}

static bool save_snapshot(
    const char *path, const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidMaterialGraphReport *report) {
    return !path ||
        ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(path, graph, report);
}

static int fail(const ProceduralSolidMaterialGraphReport *report) {
    fprintf(stderr, "{\"status\":\"error\",\"field\":\"%s\","
                    "\"message\":\"%s\"}\n",
            report ? report->field : "", report ? report->message : "error");
    return 1;
}

static void print_receipt(
    const char *operation, const ProceduralSolidMaterialGraphV1 *graph,
    const char *digest) {
    printf("{\"status\":\"ok\",\"operation\":\"%s\",\"graph_id\":\"%s\","
           "\"graph_digest_sha256\":\"%s\",\"node_count\":%zu,"
           "\"layer_count\":%zu}\n",
           operation, graph->graph_id, digest,
           graph->node_count, graph->layer_count);
}

int main(int argc, char **argv) {
    ProceduralSolidMaterialGraphV1 graph, edited;
    ProceduralSolidMaterialGraphReport report = {0};
    char digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    const char *command;
    const char *path;
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    command = argv[1];
    path = arg_value(argc, argv, "--graph");
    if (strcmp(command, "init") == 0) {
        const char *template_id = arg_value(argc, argv, "--template");
        const char *graph_id = arg_value(argc, argv, "--graph-id");
        const char *binding_id = arg_value(argc, argv, "--binding-id");
        const char *binding_digest = arg_value(argc, argv, "--binding-digest");
        const char *output = arg_value(argc, argv, "--output");
        if (!template_id || !graph_id || !binding_id || !binding_digest ||
            !output ||
            !ProceduralSolidMaterialGraphV1_FromTemplate(
                template_id, graph_id, binding_id, binding_digest,
                &graph, &report) ||
            !ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(
                output, &graph, &report) ||
            !ProceduralSolidMaterialGraphV1_Digest(
                &graph, digest, &report)) return fail(&report);
        print_receipt("init", &graph, digest);
        return 0;
    }
    if (!path ||
        !ProceduralSolidMaterialGraphV1_LoadJsonFile(
            path, &graph, &report) ||
        !ProceduralSolidMaterialGraphV1_Digest(
            &graph, digest, &report)) return fail(&report);
    if (strcmp(command, "inspect") == 0 ||
        strcmp(command, "compile") == 0) {
        print_receipt(command, &graph, digest);
        if (strcmp(command, "compile") == 0) return 0;
        for (size_t i = 0u; i < graph.node_count; ++i) {
            const ProceduralSolidMaterialNodeV1 *node = &graph.nodes[i];
            printf("{\"node_id\":\"%s\",\"kind\":\"%s\","
                   "\"input_a\":\"%s\",\"input_b\":\"%s\","
                   "\"value\":%.9g,\"minimum\":%.9g,\"maximum\":%.9g,"
                   "\"scale\":%.9g,\"offset\":%.9g}\n",
                   node->node_id,
                   ProceduralSolidMaterialNodeKind_Name(node->kind),
                   node->input_a, node->input_b, node->value,
                   node->minimum, node->maximum, node->scale, node->offset);
        }
        for (size_t i = 0u; i < graph.layer_count; ++i)
            printf("{\"layer\":%zu,\"material_id\":\"%s\","
                   "\"weight_node_id\":\"%s\",\"material_path\":\"%s\"}\n",
                   i, graph.layers[i].material_id,
                   graph.layers[i].weight_node_id,
                   graph.layers[i].material_path);
        return 0;
    }
    if (strcmp(command, "restore") == 0) {
        const char *snapshot = arg_value(argc, argv, "--snapshot");
        const char *expected = arg_value(argc, argv, "--expected-digest");
        if (!snapshot || !expected || strcmp(expected, digest) != 0 ||
            !ProceduralSolidMaterialGraphV1_LoadJsonFile(
                snapshot, &edited, &report) ||
            !ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(
                path, &edited, &report) ||
            !ProceduralSolidMaterialGraphV1_Digest(
                &edited, digest, &report)) return fail(&report);
        print_receipt("restore", &edited, digest);
        return 0;
    }
    {
        const char *expected = arg_value(argc, argv, "--expected-digest");
        const char *snapshot = arg_value(argc, argv, "--snapshot");
        if (!expected || !save_snapshot(snapshot, &graph, &report))
            return fail(&report);
        if (strcmp(command, "set") == 0) {
            if (!ProceduralSolidMaterialGraphV1_SetParameter(
                    &graph, expected, arg_value(argc, argv, "--node"),
                    arg_value(argc, argv, "--parameter"),
                    arg_value(argc, argv, "--value"), &edited, &report))
                return fail(&report);
        } else if (strcmp(command, "connect") == 0) {
            if (!ProceduralSolidMaterialGraphV1_Connect(
                    &graph, expected, arg_value(argc, argv, "--node"),
                    arg_value(argc, argv, "--input"),
                    arg_value(argc, argv, "--source"), &edited, &report))
                return fail(&report);
        } else if (strcmp(command, "add-node") == 0) {
            const char *node_id = arg_value(argc, argv, "--node");
            const char *kind_name = arg_value(argc, argv, "--kind");
            ProceduralSolidMaterialNodeKind kind;
            if (strcmp(expected, digest) != 0 || !node_id || !kind_name ||
                !ProceduralSolidMaterialNodeKind_FromName(kind_name, &kind) ||
                graph.node_count >= PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES)
                return fail(&report);
            edited = graph;
            for (size_t i = 0u; i < edited.node_count; ++i)
                if (strcmp(edited.nodes[i].node_id, node_id) == 0)
                    return fail(&report);
            snprintf(edited.nodes[edited.node_count].node_id,
                     sizeof(edited.nodes[edited.node_count].node_id),
                     "%s", node_id);
            edited.nodes[edited.node_count].kind = kind;
            edited.nodes[edited.node_count].maximum = 1.0;
            edited.nodes[edited.node_count].scale = 1.0;
            edited.nodes[edited.node_count].seed = 1;
            edited.node_count += 1u;
            if (!ProceduralSolidMaterialGraphV1_Validate(&edited, &report))
                return fail(&report);
        } else if (strcmp(command, "bind-layer") == 0) {
            const char *material_id =
                arg_value(argc, argv, "--material-id");
            const char *material_path =
                arg_value(argc, argv, "--material");
            ProceduralSolidAuthoredMaterialV1 material;
            ProceduralSolidAuthoredMaterialReport material_report;
            char material_digest[
                PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY] = {0};
            bool found = false;
            if (strcmp(expected, digest) != 0 || !material_id ||
                !material_path ||
                !ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                    material_path, &material, &material_report) ||
                strcmp(material.material_id, material_id) != 0 ||
                !ProceduralSolidAuthoredMaterialV1_Digest(
                    &material, material_digest, &material_report))
                return fail(&report);
            edited = graph;
            for (size_t i = 0u; i < edited.layer_count; ++i) {
                if (strcmp(edited.layers[i].material_id, material_id) == 0) {
                    snprintf(edited.layers[i].material_path,
                             sizeof(edited.layers[i].material_path), "%s",
                             material_path);
                    snprintf(edited.layers[i].material_digest_sha256,
                             sizeof(edited.layers[i].material_digest_sha256),
                             "%s", material_digest);
                    found = true;
                }
            }
            if (!found ||
                !ProceduralSolidMaterialGraphV1_Validate(&edited, &report))
                return fail(&report);
        } else {
            usage(argv[0]);
            return 2;
        }
        if (!ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(
                path, &edited, &report) ||
            !ProceduralSolidMaterialGraphV1_Digest(
                &edited, digest, &report)) return fail(&report);
        print_receipt(command, &edited, digest);
    }
    return 0;
}
