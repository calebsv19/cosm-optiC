#include "procedural/procedural_solid_material_binding.h"

#include "core_io.h"

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EDITS 128u

typedef enum Mode {
    MODE_NONE = 0,
    MODE_INIT,
    MODE_INSPECT,
    MODE_APPLY,
    MODE_RESTORE,
    MODE_COMPILE
} Mode;

typedef enum EditKind {
    EDIT_REGION = 0,
    EDIT_KIND = 1
} EditKind;

typedef struct Edit {
    EditKind kind;
    char selector[64];
    ProceduralSolidMaterialPreset material;
} Edit;

typedef struct Options {
    Mode mode;
    const char *mesh_path;
    const char *receipt_path;
    const char *binding_path;
    const char *binding_id;
    const char *output_path;
    const char *undo_path;
    const char *restore_path;
    const char *result_receipt_path;
    const char *expected_digest;
    ProceduralSolidMaterialPreset fallback;
    size_t edit_count;
    Edit edits[MAX_EDITS];
} Options;

static void usage(const char *program) {
    fprintf(
        stderr,
        "usage:\n"
        "  %s init --mesh PATH --solid-receipt PATH --binding-id ID "
        "--fallback MATERIAL --out PATH [--receipt-out PATH]\n"
        "  %s inspect --mesh PATH --binding PATH [--receipt-out PATH]\n"
        "  %s apply --mesh PATH --binding PATH "
        "--expected-base-digest HEX "
        "(--set-region REGION=MATERIAL | --set-kind KIND=MATERIAL) "
        "[...] --out PATH --undo-out PATH [--receipt-out PATH]\n"
        "  %s restore --mesh PATH --binding CURRENT --restore UNDO "
        "--expected-base-digest CURRENT_HEX --out PATH "
        "[--receipt-out PATH]\n"
        "  %s compile --mesh PATH --binding PATH --out PATH "
        "[--receipt-out PATH]\n",
        program, program, program, program, program);
}

static bool parse_edit(
    const char *text,
    EditKind kind,
    Edit *out) {
    const char *equals;
    size_t length;
    if (!text || !out || !(equals = strchr(text, '='))) return false;
    length = (size_t)(equals - text);
    if (length == 0u || length >= sizeof(out->selector)) return false;
    memset(out, 0, sizeof(*out));
    out->kind = kind;
    memcpy(out->selector, text, length);
    out->material = ProceduralSolidMaterialPreset_Parse(equals + 1);
    return out->material != PROCEDURAL_SOLID_MATERIAL_INVALID;
}

static bool parse_options(int argc, char **argv, Options *options) {
    if (argc < 2 || !options) return false;
    memset(options, 0, sizeof(*options));
    options->fallback = PROCEDURAL_SOLID_MATERIAL_INVALID;
    if (strcmp(argv[1], "init") == 0) options->mode = MODE_INIT;
    else if (strcmp(argv[1], "inspect") == 0) options->mode = MODE_INSPECT;
    else if (strcmp(argv[1], "apply") == 0) options->mode = MODE_APPLY;
    else if (strcmp(argv[1], "restore") == 0) options->mode = MODE_RESTORE;
    else if (strcmp(argv[1], "compile") == 0) options->mode = MODE_COMPILE;
    else return false;
    for (int i = 2; i < argc; ++i) {
        if (i + 1 >= argc) return false;
        if (strcmp(argv[i], "--mesh") == 0) {
            options->mesh_path = argv[++i];
        } else if (strcmp(argv[i], "--solid-receipt") == 0) {
            options->receipt_path = argv[++i];
        } else if (strcmp(argv[i], "--binding") == 0) {
            options->binding_path = argv[++i];
        } else if (strcmp(argv[i], "--binding-id") == 0) {
            options->binding_id = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--undo-out") == 0) {
            options->undo_path = argv[++i];
        } else if (strcmp(argv[i], "--restore") == 0) {
            options->restore_path = argv[++i];
        } else if (strcmp(argv[i], "--receipt-out") == 0) {
            options->result_receipt_path = argv[++i];
        } else if (strcmp(argv[i], "--expected-base-digest") == 0) {
            options->expected_digest = argv[++i];
        } else if (strcmp(argv[i], "--fallback") == 0) {
            options->fallback =
                ProceduralSolidMaterialPreset_Parse(argv[++i]);
        } else if (
            strcmp(argv[i], "--set-region") == 0 ||
            strcmp(argv[i], "--set-kind") == 0) {
            const EditKind kind =
                strcmp(argv[i], "--set-kind") == 0
                    ? EDIT_KIND : EDIT_REGION;
            if (options->edit_count >= MAX_EDITS ||
                !parse_edit(
                    argv[++i], kind,
                    &options->edits[options->edit_count])) {
                return false;
            }
            ++options->edit_count;
        } else {
            return false;
        }
    }
    if (!options->mesh_path) return false;
    if (options->mode == MODE_INIT) {
        return options->receipt_path && options->binding_id &&
               options->output_path &&
               options->fallback != PROCEDURAL_SOLID_MATERIAL_INVALID;
    }
    if (options->mode == MODE_INSPECT) return options->binding_path != NULL;
    if (options->mode == MODE_APPLY) {
        return options->binding_path && options->expected_digest &&
               options->output_path && options->undo_path &&
               options->edit_count > 0u;
    }
    if (options->mode == MODE_RESTORE) {
        return options->binding_path && options->restore_path &&
               options->expected_digest && options->output_path;
    }
    return options->binding_path && options->output_path;
}

static bool load_mesh(
    const char *path,
    CoreMeshAssetRuntimeDocument *mesh) {
    core_mesh_asset_runtime_document_init(mesh);
    {
        const CoreResult result =
            core_mesh_asset_runtime_document_load_file(path, mesh);
        if (result.code != CORE_OK) {
            fprintf(stderr, "mesh load failed: %s\n", result.message);
            return false;
        }
    }
    return true;
}

static bool write_json(const char *path, json_object *root) {
    const char *text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    CoreResult result;
    if (!path) return true;
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static json_object *base_receipt(
    const char *mode,
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *digest) {
    json_object *root = json_object_new_object();
    json_object_object_add(
        root, "schema",
        json_object_new_string(
            "ray_tracing.procedural_solid_material_agent"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string(mode));
    json_object_object_add(
        root, "binding_id",
        json_object_new_string(binding->binding_id));
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
        root, "binding_digest_sha256", json_object_new_string(digest));
    json_object_object_add(
        root, "fallback_material",
        json_object_new_string(ProceduralSolidMaterialPreset_Name(
            binding->fallback_material)));
    return root;
}

static json_object *resolved_regions(
    const ProceduralSolidMaterialBindingV1 *binding) {
    json_object *array = json_object_new_array();
    for (size_t i = 0u; i < binding->region_count; ++i) {
        const ProceduralSolidRegionRecord *region = &binding->regions[i];
        ProceduralSolidMaterialPreset material;
        bool fallback;
        json_object *entry = json_object_new_object();
        ProceduralSolidMaterialBindingV1_Resolve(
            binding, region->region_id, &material, &fallback);
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
        json_object_object_add(
            entry, "material",
            json_object_new_string(
                ProceduralSolidMaterialPreset_Name(material)));
        json_object_object_add(
            entry, "used_fallback", json_object_new_boolean(fallback));
        json_object_array_add(array, entry);
    }
    return array;
}

static int emit_view(
    const char *mode,
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *digest,
    const char *required_output,
    const char *optional_receipt) {
    json_object *root = base_receipt(mode, binding, digest);
    json_object_object_add(
        root, "region_count",
        json_object_new_int64((int64_t)binding->region_count));
    json_object_object_add(
        root, "assignment_count",
        json_object_new_int64((int64_t)binding->assignment_count));
    json_object_object_add(root, "regions", resolved_regions(binding));
    if ((required_output && !write_json(required_output, root)) ||
        !write_json(optional_receipt, root)) {
        fprintf(stderr, "%s receipt write failed\n", mode);
        json_object_put(root);
        return 1;
    }
    printf(
        "%s\n", json_object_to_json_string_ext(
                    root,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static int init_binding(
    const Options *options,
    const CoreMeshAssetRuntimeDocument *mesh) {
    ProceduralSolidMaterialBindingV1 binding;
    ProceduralSolidMaterialBindingReport report;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!ProceduralSolidMaterialBindingV1_FromReceiptFile(
            options->receipt_path, mesh, options->binding_id,
            options->fallback, &binding, &report) ||
        !ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
            options->output_path, &binding, &report) ||
        !ProceduralSolidMaterialBindingV1_Digest(
            &binding, digest, &report)) {
        fprintf(stderr, "init failed: %s\n", report.message);
        return 1;
    }
    return emit_view(
        "init", &binding, digest, NULL, options->result_receipt_path);
}

static bool load_valid_binding(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    ProceduralSolidMaterialBindingV1 *binding,
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidMaterialBindingReport *report) {
    return ProceduralSolidMaterialBindingV1_LoadJsonFile(
               path, binding, report) &&
           ProceduralSolidMaterialBindingV1_Validate(
               binding, mesh, report) &&
           ProceduralSolidMaterialBindingV1_Digest(
               binding, digest, report);
}

static int inspect_binding(
    const Options *options,
    const CoreMeshAssetRuntimeDocument *mesh) {
    ProceduralSolidMaterialBindingV1 binding;
    ProceduralSolidMaterialBindingReport report;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!load_valid_binding(
            options->binding_path, mesh, &binding, digest, &report)) {
        fprintf(stderr, "inspect failed: %s\n", report.message);
        return 1;
    }
    return emit_view(
        "inspect", &binding, digest, NULL,
        options->result_receipt_path);
}

static ProceduralSolidRegionKind parse_kind(const char *text) {
    if (strcmp(text, "retained") == 0) {
        return PROCEDURAL_SOLID_REGION_RETAINED;
    }
    if (strcmp(text, "cut") == 0) return PROCEDURAL_SOLID_REGION_CUT;
    if (strcmp(text, "blend") == 0) return PROCEDURAL_SOLID_REGION_BLEND;
    return (ProceduralSolidRegionKind)-1;
}

static int apply_binding(
    const Options *options,
    const CoreMeshAssetRuntimeDocument *mesh) {
    ProceduralSolidMaterialBindingV1 base;
    ProceduralSolidMaterialBindingV1 edited;
    ProceduralSolidMaterialBindingV1 next;
    ProceduralSolidMaterialBindingReport report;
    char base_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    char result_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    const char *expected;
    if (!load_valid_binding(
            options->binding_path, mesh, &base, base_digest, &report) ||
        strcmp(base_digest, options->expected_digest) != 0) {
        fprintf(stderr, "apply preflight failed: current binding changed\n");
        return 1;
    }
    edited = base;
    expected = base_digest;
    for (size_t i = 0u; i < options->edit_count; ++i) {
        bool ok;
        if (options->edits[i].kind == EDIT_REGION) {
            ok = ProceduralSolidMaterialBindingV1_AssignRegion(
                &edited, expected, options->edits[i].selector,
                options->edits[i].material, &next, &report);
        } else {
            const ProceduralSolidRegionKind kind =
                parse_kind(options->edits[i].selector);
            ok = (int)kind >= 0 &&
                 ProceduralSolidMaterialBindingV1_AssignKind(
                     &edited, expected, kind,
                     options->edits[i].material, &next, &report);
        }
        if (!ok ||
            !ProceduralSolidMaterialBindingV1_Digest(
                &next, result_digest, &report)) {
            fprintf(
                stderr, "edit %zu failed: %s\n", i,
                report.message[0] ? report.message : "invalid selector");
            return 1;
        }
        edited = next;
        expected = result_digest;
    }
    if (!ProceduralSolidMaterialBindingV1_Validate(
            &edited, mesh, &report) ||
        !ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
            options->undo_path, &base, &report) ||
        !ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
            options->output_path, &edited, &report)) {
        fprintf(stderr, "apply commit failed: %s\n", report.message);
        return 1;
    }
    return emit_view(
        "apply", &edited, result_digest, NULL,
        options->result_receipt_path);
}

static int restore_binding(
    const Options *options,
    const CoreMeshAssetRuntimeDocument *mesh) {
    ProceduralSolidMaterialBindingV1 current;
    ProceduralSolidMaterialBindingV1 restore;
    ProceduralSolidMaterialBindingReport report;
    char current_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    char restore_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    if (!load_valid_binding(
            options->binding_path, mesh, &current,
            current_digest, &report) ||
        strcmp(current_digest, options->expected_digest) != 0 ||
        !load_valid_binding(
            options->restore_path, mesh, &restore,
            restore_digest, &report) ||
        !ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
            options->output_path, &restore, &report)) {
        fprintf(
            stderr, "restore failed: %s\n",
            report.message[0] ? report.message : "current binding changed");
        return 1;
    }
    return emit_view(
        "restore", &restore, restore_digest, NULL,
        options->result_receipt_path);
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument mesh;
    int result = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!load_mesh(options.mesh_path, &mesh)) return 1;
    if (options.mode == MODE_INIT) {
        result = init_binding(&options, &mesh);
    } else if (options.mode == MODE_INSPECT) {
        result = inspect_binding(&options, &mesh);
    } else if (options.mode == MODE_APPLY) {
        result = apply_binding(&options, &mesh);
    } else if (options.mode == MODE_RESTORE) {
        result = restore_binding(&options, &mesh);
    } else {
        ProceduralSolidMaterialBindingV1 binding;
        ProceduralSolidMaterialBindingReport report;
        char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
        if (!load_valid_binding(
                options.binding_path, &mesh, &binding,
                digest, &report)) {
            fprintf(stderr, "compile failed: %s\n", report.message);
        } else {
            result = emit_view(
                "compile", &binding, digest, options.output_path,
                options.result_receipt_path);
        }
    }
    core_mesh_asset_runtime_document_free(&mesh);
    return result;
}
