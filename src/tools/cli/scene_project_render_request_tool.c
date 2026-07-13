#include "app/scene_project_render_request.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ToolOptions {
    const char *runtime_scene;
    const char *explicit_request;
    int start;
    int count;
    int stride;
} ToolOptions;

static void print_usage(const char *program) {
    fprintf(stderr,
            "usage: %s --runtime-scene <path> [--request <path>] "
            "--start <frame> --count <frames> --stride <frames>\n",
            program ? program : "scene_project_render_request_tool");
}

static int parse_nonnegative_int(const char *text, int minimum, int *out_value) {
    char *end = NULL;
    long value;
    if (!text || !text[0] || !out_value) return 0;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value < minimum || value > INT_MAX) return 0;
    *out_value = (int)value;
    return 1;
}

static int parse_args(int argc, char **argv, ToolOptions *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->count = 1;
    out->stride = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--runtime-scene") == 0 && i + 1 < argc) {
            out->runtime_scene = argv[++i];
        } else if (strcmp(argv[i], "--request") == 0 && i + 1 < argc) {
            out->explicit_request = argv[++i];
        } else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc) {
            if (!parse_nonnegative_int(argv[++i], 0, &out->start)) return 0;
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            if (!parse_nonnegative_int(argv[++i], 1, &out->count)) return 0;
        } else if (strcmp(argv[i], "--stride") == 0 && i + 1 < argc) {
            if (!parse_nonnegative_int(argv[++i], 1, &out->stride)) return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            return 0;
        }
    }
    return out->runtime_scene != NULL;
}

int main(int argc, char **argv) {
    ToolOptions options;
    RayTracingSceneProjectRenderRequest request;
    char error[512];

    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }
    if (!ray_tracing_scene_project_render_request_resolve(options.runtime_scene,
                                                          options.explicit_request,
                                                          &request,
                                                          error,
                                                          sizeof(error))) {
        fprintf(stderr, "scene-project render request resolve failed: %s\n", error);
        return 1;
    }
    if (!request.project_backed || !request.project_owned) {
        fprintf(stderr, "scene-project render request is not project-owned: %s\n", request.request_path);
        return 1;
    }
    if (!ray_tracing_scene_project_render_request_write(&request,
                                                        options.start,
                                                        options.count,
                                                        options.stride,
                                                        error,
                                                        sizeof(error))) {
        fprintf(stderr, "scene-project render request write failed: %s\n", error);
        return 1;
    }
    printf("{\n");
    printf("  \"status\": \"ok\",\n");
    printf("  \"project_root\": \"%s\",\n", request.project_root);
    printf("  \"request_path\": \"%s\",\n", request.request_path);
    printf("  \"simulation_frames\": {\"start\": %d, \"count\": %d, \"stride\": %d}\n",
           request.simulation_start_frame,
           request.simulation_frame_count,
           request.simulation_frame_stride);
    printf("}\n");
    return 0;
}
