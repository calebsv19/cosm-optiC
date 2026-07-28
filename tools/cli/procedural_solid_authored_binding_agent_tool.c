#include "procedural/procedural_solid_authored_material_binding.h"

#include "core_io.h"

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EDITS 64u

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
    EDIT_KIND
} EditKind;

typedef struct Edit {
    EditKind kind;
    char selector[64];
    char material_path[PROCEDURAL_SOLID_AUTHORED_BINDING_PATH_CAPACITY];
} Edit;

typedef struct Options {
    Mode mode;
    const char *mesh_path;
    const char *region_binding_path;
    const char *authored_binding_path;
    const char *binding_id;
    const char *expected_digest;
    const char *output_path;
    const char *undo_path;
    const char *restore_path;
    const char *receipt_path;
    size_t edit_count;
    Edit edits[MAX_EDITS];
} Options;

static void usage(const char *program) {
    fprintf(stderr,
        "usage:\n"
        "  %s init --mesh PATH --region-binding PATH --binding-id ID "
        "--out PATH [--receipt-out PATH]\n"
        "  %s inspect --mesh PATH --region-binding PATH "
        "--authored-binding PATH [--receipt-out PATH]\n"
        "  %s apply --mesh PATH --region-binding PATH "
        "--authored-binding PATH --expected-base-digest HEX "
        "(--set-region REGION=MATERIAL_PATH | "
        "--set-kind KIND=MATERIAL_PATH) [...] "
        "--out PATH --undo-out PATH [--receipt-out PATH]\n"
        "  %s restore --mesh PATH --region-binding PATH "
        "--authored-binding CURRENT --restore UNDO "
        "--expected-base-digest HEX --out PATH [--receipt-out PATH]\n"
        "  %s compile --mesh PATH --region-binding PATH "
        "--authored-binding PATH --out PATH [--receipt-out PATH]\n",
        program, program, program, program, program);
}

static bool parse_edit(const char *text, EditKind kind, Edit *edit) {
    const char *equals;
    size_t selector_length;
    if (!text || !edit || !(equals = strchr(text, '='))) return false;
    selector_length = (size_t)(equals - text);
    if (selector_length == 0u ||
        selector_length >= sizeof(edit->selector) ||
        !equals[1] ||
        strlen(equals + 1) >= sizeof(edit->material_path)) return false;
    memset(edit, 0, sizeof(*edit));
    edit->kind = kind;
    memcpy(edit->selector, text, selector_length);
    snprintf(edit->material_path, sizeof(edit->material_path), "%s",
             equals + 1);
    return true;
}

static bool parse_options(int argc, char **argv, Options *options) {
    if (argc < 2 || !options) return false;
    memset(options, 0, sizeof(*options));
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
        } else if (strcmp(argv[i], "--region-binding") == 0) {
            options->region_binding_path = argv[++i];
        } else if (strcmp(argv[i], "--authored-binding") == 0) {
            options->authored_binding_path = argv[++i];
        } else if (strcmp(argv[i], "--binding-id") == 0) {
            options->binding_id = argv[++i];
        } else if (strcmp(argv[i], "--expected-base-digest") == 0) {
            options->expected_digest = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            options->output_path = argv[++i];
        } else if (strcmp(argv[i], "--undo-out") == 0) {
            options->undo_path = argv[++i];
        } else if (strcmp(argv[i], "--restore") == 0) {
            options->restore_path = argv[++i];
        } else if (strcmp(argv[i], "--receipt-out") == 0) {
            options->receipt_path = argv[++i];
        } else if (strcmp(argv[i], "--set-region") == 0 ||
                   strcmp(argv[i], "--set-kind") == 0) {
            EditKind kind = strcmp(argv[i], "--set-kind") == 0
                                ? EDIT_KIND : EDIT_REGION;
            if (options->edit_count >= MAX_EDITS ||
                !parse_edit(argv[++i], kind,
                            &options->edits[options->edit_count])) {
                return false;
            }
            ++options->edit_count;
        } else {
            return false;
        }
    }
    if (!options->mesh_path || !options->region_binding_path) return false;
    if (options->mode == MODE_INIT) {
        return options->binding_id && options->output_path;
    }
    if (!options->authored_binding_path) return false;
    if (options->mode == MODE_APPLY) {
        return options->expected_digest && options->output_path &&
               options->undo_path && options->edit_count > 0u;
    }
    if (options->mode == MODE_RESTORE) {
        return options->restore_path && options->expected_digest &&
               options->output_path;
    }
    if (options->mode == MODE_COMPILE) return options->output_path != NULL;
    return true;
}

static bool load_mesh(
    const char *path,
    CoreMeshAssetRuntimeDocument *mesh) {
    core_mesh_asset_runtime_document_init(mesh);
    {
        CoreResult result =
            core_mesh_asset_runtime_document_load_file(path, mesh);
        if (result.code != CORE_OK) {
            fprintf(stderr, "mesh load failed: %s\n", result.message);
            return false;
        }
    }
    return true;
}

static bool load_region_binding(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    ProceduralSolidMaterialBindingV1 *binding) {
    ProceduralSolidMaterialBindingReport report;
    if (!ProceduralSolidMaterialBindingV1_LoadJsonFile(
            path, binding, &report) ||
        !ProceduralSolidMaterialBindingV1_Validate(
            binding, mesh, &report)) {
        fprintf(stderr, "region binding load failed: %s\n", report.message);
        return false;
    }
    return true;
}

static bool load_authored_binding(
    const char *path,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    ProceduralSolidAuthoredMaterialBindingV1 *binding,
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidAuthoredBindingReport *report) {
    return ProceduralSolidAuthoredMaterialBindingV1_LoadJsonFile(
               path, binding, report) &&
           ProceduralSolidAuthoredMaterialBindingV1_Validate(
               binding, region_binding, report) &&
           ProceduralSolidAuthoredMaterialBindingV1_Digest(
               binding, digest, report);
}

static bool write_json(const char *path, json_object *root) {
    const char *text;
    CoreResult result;
    if (!path) return true;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static bool validate_material_references(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    json_object *materials) {
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidAuthoredMaterialReferenceV1 *ref =
            &binding->assignments[i];
        ProceduralSolidAuthoredMaterialV1 material;
        ProceduralSolidAuthoredMaterialReport report;
        char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
        json_object *entry = json_object_new_object();
        if (!ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                ref->material_path, &material, &report) ||
            !ProceduralSolidAuthoredMaterialV1_Digest(
                &material, digest, &report) ||
            strcmp(material.material_id, ref->material_id) != 0 ||
            strcmp(digest, ref->material_digest_sha256) != 0) {
            fprintf(stderr, "material reference validation failed: %s\n",
                    ref->material_path);
            if (entry) json_object_put(entry);
            return false;
        }
        json_object_object_add(entry, "region_id",
            json_object_new_string(ref->region_id));
        json_object_object_add(entry, "material_id",
            json_object_new_string(ref->material_id));
        json_object_object_add(entry, "material_path",
            json_object_new_string(ref->material_path));
        json_object_object_add(entry, "material_digest_sha256",
            json_object_new_string(ref->material_digest_sha256));
        json_object_array_add(materials, entry);
    }
    return true;
}

static int emit(
    const char *mode,
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const char *digest,
    const char *required_output,
    const char *optional_receipt) {
    json_object *root = json_object_new_object();
    json_object *materials = json_object_new_array();
    json_object_object_add(root, "schema",
        json_object_new_string(
            "ray_tracing.procedural_solid_authored_binding_agent"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string(mode));
    json_object_object_add(root, "binding_id",
        json_object_new_string(binding->binding_id));
    json_object_object_add(root, "region_binding_id",
        json_object_new_string(binding->region_binding_id));
    json_object_object_add(root, "region_binding_digest_sha256",
        json_object_new_string(binding->region_binding_digest_sha256));
    json_object_object_add(root, "binding_digest_sha256",
        json_object_new_string(digest));
    json_object_object_add(root, "assignment_count",
        json_object_new_int64((int64_t)binding->assignment_count));
    if (!validate_material_references(binding, materials)) {
        json_object_put(materials);
        json_object_put(root);
        return 1;
    }
    json_object_object_add(root, "assignments", materials);
    if ((required_output && !write_json(required_output, root)) ||
        !write_json(optional_receipt, root)) {
        fprintf(stderr, "%s receipt write failed\n", mode);
        json_object_put(root);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(root);
    return 0;
}

static ProceduralSolidRegionKind parse_kind(const char *text) {
    if (strcmp(text, "retained") == 0)
        return PROCEDURAL_SOLID_REGION_RETAINED;
    if (strcmp(text, "cut") == 0) return PROCEDURAL_SOLID_REGION_CUT;
    if (strcmp(text, "blend") == 0) return PROCEDURAL_SOLID_REGION_BLEND;
    return (ProceduralSolidRegionKind)-1;
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument mesh;
    ProceduralSolidMaterialBindingV1 region_binding;
    ProceduralSolidAuthoredMaterialBindingV1 binding, base, edited, next, restore;
    ProceduralSolidAuthoredBindingReport report;
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    char next_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    int result = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!load_mesh(options.mesh_path, &mesh)) return 1;
    if (!load_region_binding(
            options.region_binding_path, &mesh, &region_binding)) goto done;
    if (options.mode == MODE_INIT) {
        if (!ProceduralSolidAuthoredMaterialBindingV1_FromRegionBinding(
                options.binding_id, &region_binding, &binding, &report) ||
            !ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
                options.output_path, &binding, &report) ||
            !ProceduralSolidAuthoredMaterialBindingV1_Digest(
                &binding, digest, &report)) {
            fprintf(stderr, "init failed: %s\n", report.message);
            goto done;
        }
        result = emit("init", &binding, digest, NULL, options.receipt_path);
        goto done;
    }
    if (!load_authored_binding(
            options.authored_binding_path, &region_binding, &base,
            digest, &report)) {
        fprintf(stderr, "authored binding load failed: %s\n", report.message);
        goto done;
    }
    if (options.mode == MODE_INSPECT) {
        result = emit("inspect", &base, digest, NULL, options.receipt_path);
        goto done;
    }
    if (options.mode == MODE_APPLY) {
        if (strcmp(digest, options.expected_digest) != 0) {
            fprintf(stderr, "apply preflight failed: binding changed\n");
            goto done;
        }
        edited = base;
        for (size_t i = 0u; i < options.edit_count; ++i) {
            ProceduralSolidAuthoredMaterialV1 material;
            ProceduralSolidAuthoredMaterialReport material_report;
            bool ok;
            if (!ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                    options.edits[i].material_path, &material,
                    &material_report)) {
                fprintf(stderr, "material load failed: %s\n",
                        material_report.message);
                goto done;
            }
            if (options.edits[i].kind == EDIT_REGION) {
                ok = ProceduralSolidAuthoredMaterialBindingV1_AssignRegion(
                    &edited, digest, options.edits[i].selector,
                    options.edits[i].material_path, &material,
                    &next, &report);
            } else {
                ProceduralSolidRegionKind kind =
                    parse_kind(options.edits[i].selector);
                ok = (int)kind >= 0 &&
                     ProceduralSolidAuthoredMaterialBindingV1_AssignKind(
                         &edited, digest, kind, &region_binding,
                         options.edits[i].material_path, &material,
                         &next, &report);
            }
            if (!ok ||
                !ProceduralSolidAuthoredMaterialBindingV1_Validate(
                    &next, &region_binding, &report) ||
                !ProceduralSolidAuthoredMaterialBindingV1_Digest(
                    &next, next_digest, &report)) {
                fprintf(stderr, "edit %zu failed: %s\n", i, report.message);
                goto done;
            }
            edited = next;
            snprintf(digest, sizeof(digest), "%s", next_digest);
        }
        if (!ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
                options.undo_path, &base, &report) ||
            !ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
                options.output_path, &edited, &report)) {
            fprintf(stderr, "apply commit failed: %s\n", report.message);
            goto done;
        }
        result = emit("apply", &edited, digest, NULL, options.receipt_path);
        goto done;
    }
    if (options.mode == MODE_RESTORE) {
        char restore_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
        if (strcmp(digest, options.expected_digest) != 0 ||
            !load_authored_binding(
                options.restore_path, &region_binding, &restore,
                restore_digest, &report) ||
            !ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
                options.output_path, &restore, &report)) {
            fprintf(stderr, "restore failed: binding changed\n");
            goto done;
        }
        result = emit("restore", &restore, restore_digest,
                      NULL, options.receipt_path);
        goto done;
    }
    result = emit("compile", &base, digest,
                  options.output_path, options.receipt_path);
done:
    core_mesh_asset_runtime_document_free(&mesh);
    return result;
}
