#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/material_preview_headless.h"
#include "app/material_preview_request.h"

static void usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s --request /path/material_preview_request.json\n",
            argv0 ? argv0 : "ray_tracing_material_preview_headless");
}

int main(int argc, char** argv) {
    const char* request_path = NULL;
    MaterialPreviewRequest request;
    char diagnostics[256];
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0 && i + 1 < argc) {
            request_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (!request_path) {
        usage(argv[0]);
        return 1;
    }
    if (!MaterialPreviewRequestLoadFromFile(request_path,
                                            &request,
                                            diagnostics,
                                            sizeof(diagnostics))) {
        fprintf(stderr, "material preview request load failed: %s\n", diagnostics);
        return 1;
    }
    if (!MaterialPreviewHeadlessRun(&request, diagnostics, sizeof(diagnostics))) {
        fprintf(stderr, "material preview render failed: %s\n", diagnostics);
        return 1;
    }
    printf("material preview ready: %s\n", request.output_path);
    if (request.summary_path[0]) {
        printf("summary: %s\n", request.summary_path);
    }
    /* This standalone artifact tool hits unstable global teardown inside the
     * shared runtime after a successful preview/write path. Flush user-visible
     * output and terminate cleanly instead of running the broken shutdown lane.
     */
    fflush(NULL);
    _Exit(0);
}
