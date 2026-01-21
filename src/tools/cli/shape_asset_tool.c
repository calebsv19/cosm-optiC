#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geo/shape_asset.h"
#include "import/shape_import.h"

static char *dup_string_local(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

typedef struct {
    const char *input_path;
    const char *output_path;
    const char *name_override;
    float max_error;
} Args;

static void usage(const char *exe) {
    fprintf(stderr,
            "Usage: %s [--max-error e] [--name custom] [--out file.asset.json] <shapelib.json>\n",
            exe);
}

static int parse_float(const char *s, float *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    float v = strtof(s, &end);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static const char *default_out_path(const char *input) {
    if (!input) return NULL;
    const char *slash = strrchr(input, '/');
    const char *base = slash ? slash + 1 : input;
    const char *dot = strrchr(base, '.');
    size_t base_len = dot ? (size_t)(dot - base) : strlen(base);
    const char *prefix = getenv("SHAPE_ASSET_DIR");
    if (!prefix || prefix[0] == '\0') {
        prefix = "Configs/objects";
    }
    size_t total = strlen(prefix) + 1 + base_len + strlen(".asset.json") + 1;
    char *buf = (char *)malloc(total);
    if (!buf) return NULL;
    size_t prefix_len = strlen(prefix);
    memcpy(buf, prefix, prefix_len);
    buf[prefix_len] = '/';
    memcpy(buf + prefix_len + 1, base, base_len);
    memcpy(buf + prefix_len + 1 + base_len, ".asset.json", strlen(".asset.json") + 1);
    return buf;
}

static bool parse_args(int argc, char **argv, Args *out) {
    Args a = {
        .max_error = 0.5f,
    };
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--max-error") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.max_error)) {
                fprintf(stderr, "Invalid --max-error value\n");
                return false;
            }
            ++i;
        } else if (strcmp(arg, "--name") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing --name value\n");
                return false;
            }
            a.name_override = argv[++i];
        } else if (strcmp(arg, "--out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing --out value\n");
                return false;
            }
            a.output_path = argv[++i];
        } else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return false;
        } else {
            a.input_path = arg;
        }
    }
    if (!a.input_path) return false;
    *out = a;
    return true;
}

int main(int argc, char **argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        usage(argv[0]);
        return 1;
    }

    ShapeDocument doc;
    if (!shape_import_load(args.input_path, &doc) || doc.shapeCount == 0) {
        fprintf(stderr, "Failed to load shape JSON: %s\n", args.input_path);
        return 1;
    }

    const Shape *src_shape = &doc.shapes[0];
    ShapeAsset asset;
    if (!shape_asset_from_shapelib_shape(src_shape, args.max_error, &asset)) {
        fprintf(stderr, "Failed to flatten shape\n");
        ShapeDocument_Free(&doc);
        return 1;
    }
    if (args.name_override && args.name_override[0] != '\0') {
        free(asset.name);
        asset.name = dup_string_local(args.name_override);
        if (!asset.name) {
            fprintf(stderr, "Out of memory setting name\n");
            shape_asset_free(&asset);
            ShapeDocument_Free(&doc);
            return 1;
        }
    }

    const char *out_path = args.output_path;
    const char *owned_out = NULL;
    if (!out_path) {
        owned_out = default_out_path(args.input_path);
        out_path = owned_out;
    }

    bool ok = out_path && shape_asset_save_file(&asset, out_path);
    if (ok) {
        printf("Exported ShapeAsset to %s (paths=%zu)\n", out_path, asset.path_count);
    } else {
        fprintf(stderr, "Failed to write asset JSON: %s\n", out_path ? out_path : "(null)");
    }

    free((void *)owned_out);
    shape_asset_free(&asset);
    ShapeDocument_Free(&doc);
    return ok ? 0 : 1;
}
