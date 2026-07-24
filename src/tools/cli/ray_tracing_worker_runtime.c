#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ray_tracing_sha256.h"
#include "app/ray_tracing_worker_runtime.h"

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s capabilities --render-cli <path> --output <path>\n"
            "       %s run --message <worker_request.json>\n",
            program,
            program);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : NULL;
    const char *render_cli = NULL;
    const char *output = NULL;
    const char *message = NULL;
    char diagnostics[256] = {0};
    char runtime_sha256[RAY_TRACING_SHA256_HEX_SIZE] = {0};
    if (ray_tracing_sha256_file(argv[0], runtime_sha256)) {
        (void)setenv("RAY_TRACING_WORKER_RUNTIME_SHA256", runtime_sha256, 1);
    }
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--render-cli") == 0 && i + 1 < argc) {
            render_cli = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "--message") == 0 && i + 1 < argc) {
            message = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (mode && strcmp(mode, "capabilities") == 0 && render_cli && output) {
        if (!ray_tracing_worker_runtime_write_capabilities(render_cli,
                                                           output,
                                                           diagnostics,
                                                           sizeof(diagnostics))) {
            fprintf(stderr, "ray_tracing_worker_runtime: %s\n", diagnostics);
            return 1;
        }
        return 0;
    }
    if (mode && strcmp(mode, "run") == 0 && message) {
        const int result =
            ray_tracing_worker_runtime_run(message, diagnostics, sizeof(diagnostics));
        if (result != 0) {
            fprintf(stderr, "ray_tracing_worker_runtime: %s\n", diagnostics);
        }
        return result;
    }
    usage(argv[0]);
    return 2;
}
