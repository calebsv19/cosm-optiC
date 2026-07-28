#include "procedural/procedural_solid_authored_material.h"

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

typedef struct Edit {
    char parameter[64];
    char value[96];
} Edit;

typedef struct Options {
    Mode mode;
    const char *material_path;
    const char *template_id;
    const char *material_id;
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
        "  %s init --template ID --material-id ID --out PATH "
        "[--receipt-out PATH]\n"
        "  %s inspect --material PATH [--receipt-out PATH]\n"
        "  %s apply --material PATH --expected-base-digest HEX "
        "--set PARAM=VALUE [...] --out PATH --undo-out PATH "
        "[--receipt-out PATH]\n"
        "  %s restore --material CURRENT --restore UNDO "
        "--expected-base-digest HEX --out PATH [--receipt-out PATH]\n"
        "  %s compile --material PATH --out PATH [--receipt-out PATH]\n",
        program, program, program, program, program);
}

static bool parse_edit(const char *text, Edit *edit) {
    const char *equals;
    size_t key_length;
    if (!text || !edit || !(equals = strchr(text, '='))) return false;
    key_length = (size_t)(equals - text);
    if (key_length == 0u || key_length >= sizeof(edit->parameter) ||
        !equals[1] || strlen(equals + 1) >= sizeof(edit->value)) return false;
    memset(edit, 0, sizeof(*edit));
    memcpy(edit->parameter, text, key_length);
    snprintf(edit->value, sizeof(edit->value), "%s", equals + 1);
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
        if (strcmp(argv[i], "--material") == 0) {
            options->material_path = argv[++i];
        } else if (strcmp(argv[i], "--template") == 0) {
            options->template_id = argv[++i];
        } else if (strcmp(argv[i], "--material-id") == 0) {
            options->material_id = argv[++i];
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
        } else if (strcmp(argv[i], "--set") == 0) {
            if (options->edit_count >= MAX_EDITS ||
                !parse_edit(argv[++i], &options->edits[options->edit_count])) {
                return false;
            }
            ++options->edit_count;
        } else {
            return false;
        }
    }
    if (options->mode == MODE_INIT) {
        return options->template_id && options->material_id &&
               options->output_path;
    }
    if (!options->material_path) return false;
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

static bool write_json(const char *path, json_object *root) {
    const char *text;
    CoreResult result;
    if (!path) return true;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static json_object *parameters_manifest(void) {
    static const struct {
        const char *id;
        const char *type;
        double minimum;
        double maximum;
        const char *unit;
    } parameters[] = {
        {"base_color.r", "number", 0.0, 1.0, "normalized"},
        {"base_color.g", "number", 0.0, 1.0, "normalized"},
        {"base_color.b", "number", 0.0, 1.0, "normalized"},
        {"roughness", "number", 0.02, 1.0, "normalized"},
        {"metallic", "number", 0.0, 1.0, "normalized"},
        {"reflectivity", "number", 0.0, 1.0, "normalized"},
        {"specular", "number", 0.0, 1.0, "normalized"},
        {"emission.color.r", "number", 0.0, 1.0, "normalized"},
        {"emission.color.g", "number", 0.0, 1.0, "normalized"},
        {"emission.color.b", "number", 0.0, 1.0, "normalized"},
        {"emission.strength", "number", 0.0, 100.0, "radiance_scale"},
        {"transparency", "number", 0.0, 1.0, "normalized"},
        {"ior", "number", 1.0, 3.0, "ratio"},
        {"texture.enabled", "boolean", 0.0, 1.0, "boolean"},
        {"texture.kind", "enum", 0.0, 0.0, "stable_id"},
        {"texture.scale_units", "number", 0.001, 1000.0, "object_units"},
        {"texture.strength", "number", 0.0, 1.0, "normalized"},
        {"texture.microdetail_normal_strength", "number", 0.0, 1.0, "normalized"},
        {"texture.coverage", "number", 0.0, 1.0, "normalized"},
        {"texture.grain", "number", 0.0, 1.0, "normalized"},
        {"texture.edge_softness", "number", 0.0, 1.0, "normalized"},
        {"texture.contrast", "number", 0.0, 1.0, "normalized"},
        {"texture.flow", "number", 0.0, 1.0, "normalized"},
        {"texture.color_depth", "number", 0.0, 1.0, "normalized"},
        {"texture.surface_damage", "number", 0.0, 1.0, "normalized"},
        {"texture.seed", "integer", -2147483648.0, 2147483647.0, "seed"}
    };
    json_object *array = json_object_new_array();
    for (size_t i = 0u; i < sizeof(parameters) / sizeof(parameters[0]); ++i) {
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "parameter_id",
            json_object_new_string(parameters[i].id));
        json_object_object_add(entry, "type",
            json_object_new_string(parameters[i].type));
        json_object_object_add(entry, "minimum",
            json_object_new_double(parameters[i].minimum));
        json_object_object_add(entry, "maximum",
            json_object_new_double(parameters[i].maximum));
        json_object_object_add(entry, "unit",
            json_object_new_string(parameters[i].unit));
        json_object_array_add(array, entry);
    }
    return array;
}

static json_object *material_receipt(
    const char *mode,
    const ProceduralSolidAuthoredMaterialV1 *material,
    const char *digest) {
    const ProceduralSolidAuthoredMaterialSurfaceV1 *s = &material->surface;
    json_object *root = json_object_new_object();
    json_object *color = json_object_new_object();
    json_object *texture = json_object_new_object();
    json_object_object_add(root, "schema",
        json_object_new_string(
            "ray_tracing.procedural_solid_authored_material_agent"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "mode", json_object_new_string(mode));
    json_object_object_add(root, "material_id",
        json_object_new_string(material->material_id));
    json_object_object_add(root, "template_id",
        json_object_new_string(material->template_id));
    json_object_object_add(root, "material_digest_sha256",
        json_object_new_string(digest));
    json_object_object_add(color, "r", json_object_new_double(s->base_color_r));
    json_object_object_add(color, "g", json_object_new_double(s->base_color_g));
    json_object_object_add(color, "b", json_object_new_double(s->base_color_b));
    json_object_object_add(root, "base_color", color);
    json_object_object_add(root, "roughness", json_object_new_double(s->roughness));
    json_object_object_add(root, "metallic", json_object_new_double(s->metallic));
    json_object_object_add(root, "reflectivity", json_object_new_double(s->reflectivity));
    json_object_object_add(root, "specular", json_object_new_double(s->specular));
    json_object_object_add(root, "emission_strength",
        json_object_new_double(s->emission_strength));
    json_object_object_add(texture, "enabled",
        json_object_new_boolean(s->texture.enabled));
    json_object_object_add(texture, "kind",
        json_object_new_string(s->texture.kind));
    json_object_object_add(texture, "scale_units",
        json_object_new_double(s->texture.scale_units));
    json_object_object_add(texture, "strength",
        json_object_new_double(s->texture.strength));
    json_object_object_add(texture, "microdetail_normal_strength",
        json_object_new_double(s->texture.microdetail_normal_strength));
    json_object_object_add(texture, "coverage",
        json_object_new_double(s->texture.coverage));
    json_object_object_add(texture, "grain",
        json_object_new_double(s->texture.grain));
    json_object_object_add(texture, "seed",
        json_object_new_int(s->texture.seed));
    json_object_object_add(root, "texture_node", texture);
    json_object_object_add(root, "parameters", parameters_manifest());
    return root;
}

static int emit(
    const char *mode,
    const ProceduralSolidAuthoredMaterialV1 *material,
    const char *digest,
    const char *required_output,
    const char *optional_receipt) {
    json_object *root = material_receipt(mode, material, digest);
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

static bool load_material(
    const char *path,
    ProceduralSolidAuthoredMaterialV1 *material,
    char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY],
    ProceduralSolidAuthoredMaterialReport *report) {
    return ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
               path, material, report) &&
           ProceduralSolidAuthoredMaterialV1_Digest(
               material, digest, report);
}

int main(int argc, char **argv) {
    Options options;
    ProceduralSolidAuthoredMaterialV1 material, base, edited, next, restore;
    ProceduralSolidAuthoredMaterialReport report;
    char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    char next_digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (options.mode == MODE_INIT) {
        if (!ProceduralSolidAuthoredMaterialV1_FromTemplate(
                options.template_id, options.material_id, &material, &report) ||
            !ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
                options.output_path, &material, &report) ||
            !ProceduralSolidAuthoredMaterialV1_Digest(
                &material, digest, &report)) {
            fprintf(stderr, "init failed: %s\n", report.message);
            return 1;
        }
        return emit("init", &material, digest, NULL, options.receipt_path);
    }
    if (!load_material(options.material_path, &base, digest, &report)) {
        fprintf(stderr, "material load failed: %s\n", report.message);
        return 1;
    }
    if (options.mode == MODE_INSPECT) {
        return emit("inspect", &base, digest, NULL, options.receipt_path);
    }
    if (options.mode == MODE_APPLY) {
        if (strcmp(digest, options.expected_digest) != 0) {
            fprintf(stderr, "apply preflight failed: current material changed\n");
            return 1;
        }
        edited = base;
        for (size_t i = 0u; i < options.edit_count; ++i) {
            if (!ProceduralSolidAuthoredMaterialV1_SetParameter(
                    &edited, digest, options.edits[i].parameter,
                    options.edits[i].value, &next, &report) ||
                !ProceduralSolidAuthoredMaterialV1_Digest(
                    &next, next_digest, &report)) {
                fprintf(stderr, "edit %zu failed: %s\n", i, report.message);
                return 1;
            }
            edited = next;
            snprintf(digest, sizeof(digest), "%s", next_digest);
        }
        if (!ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
                options.undo_path, &base, &report) ||
            !ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
                options.output_path, &edited, &report)) {
            fprintf(stderr, "apply commit failed: %s\n", report.message);
            return 1;
        }
        return emit("apply", &edited, digest, NULL, options.receipt_path);
    }
    if (options.mode == MODE_RESTORE) {
        char restore_digest[
            PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
        if (strcmp(digest, options.expected_digest) != 0 ||
            !load_material(options.restore_path, &restore,
                           restore_digest, &report) ||
            !ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
                options.output_path, &restore, &report)) {
            fprintf(stderr, "restore failed: current material changed\n");
            return 1;
        }
        return emit("restore", &restore, restore_digest,
                    NULL, options.receipt_path);
    }
    return emit("compile", &base, digest,
                options.output_path, options.receipt_path);
}
