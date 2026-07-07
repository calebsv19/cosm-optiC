#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>

void ray_tracing_render_headless_usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s --request <request.json> [--preflight|--render] [--summary <summary.json>] [--job-id <id>] [--job-status <job_status.json>]\n",
            argv0 ? argv0 : "ray_tracing_render_headless");
}
