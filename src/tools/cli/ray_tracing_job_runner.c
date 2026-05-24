#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/ray_tracing_job_runner.h"

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s <submit|status|cancel> [--request <request.json|job.json>|--job-id <job_id>] [--jobs-root <path>]\n",
            argv0 ? argv0 : "ray_tracing_job_runner");
}

int main(int argc, char **argv) {
    const char *mode = NULL;
    const char *request_path = NULL;
    const char *job_id = NULL;
    const char *jobs_root = NULL;
    bool overwrite = false;
    bool resume = false;
    char diagnostics[256] = {0};
    char generated_job_id[96] = {0};

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    mode = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0 && i + 1 < argc) {
            request_path = argv[++i];
        } else if (strcmp(argv[i], "--job-id") == 0 && i + 1 < argc) {
            job_id = argv[++i];
        } else if (strcmp(argv[i], "--jobs-root") == 0 && i + 1 < argc) {
            jobs_root = argv[++i];
        } else if (strcmp(argv[i], "--overwrite") == 0) {
            overwrite = true;
        } else if (strcmp(argv[i], "--resume") == 0) {
            resume = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (strcmp(mode, "submit") == 0) {
        if (!request_path) {
            usage(argv[0]);
            return 2;
        }
        if (!ray_tracing_job_runner_submit(argv[0],
                                           request_path,
                                           jobs_root,
                                           overwrite,
                                           resume,
                                           generated_job_id,
                                           sizeof(generated_job_id),
                                           diagnostics,
                                           sizeof(diagnostics))) {
            fprintf(stderr, "ray_tracing_job_runner submit: %s\n", diagnostics);
            return 1;
        }
        printf("{\"job_id\":\"%s\",\"status\":\"submitted\"}\n", generated_job_id);
        return 0;
    }
    if (strcmp(mode, "status") == 0) {
        if (!job_id) {
            usage(argv[0]);
            return 2;
        }
        if (!ray_tracing_job_runner_print_status(stdout,
                                                 argv[0],
                                                 job_id,
                                                 jobs_root,
                                                 diagnostics,
                                                 sizeof(diagnostics))) {
            fprintf(stderr, "ray_tracing_job_runner status: %s\n", diagnostics);
            return 1;
        }
        return 0;
    }
    if (strcmp(mode, "cancel") == 0) {
        if (!job_id) {
            usage(argv[0]);
            return 2;
        }
        if (!ray_tracing_job_runner_cancel(argv[0],
                                           job_id,
                                           jobs_root,
                                           diagnostics,
                                           sizeof(diagnostics))) {
            fprintf(stderr, "ray_tracing_job_runner cancel: %s\n", diagnostics);
            return 1;
        }
        printf("{\"job_id\":\"%s\",\"status\":\"cancelled\"}\n", job_id);
        return 0;
    }

    usage(argv[0]);
    return 2;
}
