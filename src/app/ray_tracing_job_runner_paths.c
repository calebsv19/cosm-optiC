#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_job_runner_internal.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool ray_tracing_job_runner_file_exists(const char *path) {
    return path && path[0] && access(path, F_OK) == 0;
}

static bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return copy_string(out_dir, out_dir_size, ".");
    len = (size_t)(slash - path);
    if (len == 0u) return copy_string(out_dir, out_dir_size, "/");
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return true;
}

bool ray_tracing_job_runner_ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool ray_tracing_job_runner_ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!parent_dir_of(path, dir, sizeof(dir))) return false;
    return ray_tracing_job_runner_ensure_directory_exists(dir);
}

static bool resolve_real_path(const char *path, char *out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !path[0] || !out || out_size == 0u) return false;
    if (!realpath(path, resolved)) return false;
    return copy_string(out, out_size, resolved);
}

static bool path_contains_parent_segment(const char *path) {
    const char *cursor = path;
    if (!path || !path[0]) return true;
    while (*cursor) {
        while (*cursor == '/') cursor += 1;
        if (cursor[0] == '.' && cursor[1] == '.' &&
            (cursor[2] == '/' || cursor[2] == '\0')) {
            return true;
        }
        while (*cursor && *cursor != '/') cursor += 1;
    }
    return false;
}

static bool derive_repo_root_from_argv0(const char *argv0, char *out_root, size_t out_root_size) {
    char binary_path[PATH_MAX];
    char *build_marker = NULL;
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    build_marker = strstr(binary_path, "/build/");
    if (!build_marker) return false;
    *build_marker = '\0';
    return copy_string(out_root, out_root_size, binary_path);
}

bool ray_tracing_job_runner_derive_render_cli_path(const char *argv0,
                                                   char *out_path,
                                                   size_t out_path_size) {
    char binary_path[PATH_MAX];
    char dir_path[PATH_MAX];
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    if (!parent_dir_of(binary_path, dir_path, sizeof(dir_path))) return false;
    if (snprintf(out_path, out_path_size, "%s/ray_tracing_render_headless", dir_path) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return ray_tracing_job_runner_file_exists(out_path);
}

bool ray_tracing_job_runner_default_jobs_root(const char *argv0,
                                              char *out_path,
                                              size_t out_path_size) {
    char repo_root[PATH_MAX];
    if (!derive_repo_root_from_argv0(argv0, repo_root, sizeof(repo_root))) return false;
    if (snprintf(out_path, out_path_size, "%s/build/agent_runs/jobs", repo_root) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool ray_tracing_job_runner_build_jobs_root(const char *argv0,
                                            const char *jobs_root_override,
                                            char *out_jobs_root,
                                            size_t out_jobs_root_size) {
    char resolved[PATH_MAX];
    if (jobs_root_override && jobs_root_override[0]) {
        if (jobs_root_override[0] != '/' || path_contains_parent_segment(jobs_root_override)) {
            return false;
        }
        if (!resolve_real_path(jobs_root_override, resolved, sizeof(resolved))) return false;
        return copy_string(out_jobs_root, out_jobs_root_size, resolved);
    }
    return ray_tracing_job_runner_default_jobs_root(argv0, out_jobs_root, out_jobs_root_size);
}

bool ray_tracing_job_runner_validate_job_id(const char *job_id) {
    if (!job_id || !job_id[0] || strcmp(job_id, ".") == 0 || strcmp(job_id, "..") == 0 ||
        job_id[0] == '-') {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)job_id; *cursor; ++cursor) {
        if (isalnum(*cursor) || *cursor == '_' || *cursor == '-' || *cursor == '.') continue;
        return false;
    }
    return true;
}

bool ray_tracing_job_runner_build_job_paths(const char *jobs_root,
                                            const char *job_id,
                                            RayTracingDetachedJobPaths *out_paths) {
    if (!jobs_root || !job_id || !out_paths || !ray_tracing_job_runner_validate_job_id(job_id)) {
        return false;
    }
    memset(out_paths, 0, sizeof(*out_paths));
    if (snprintf(out_paths->jobs_root, sizeof(out_paths->jobs_root), "%s", jobs_root) >=
            (int)sizeof(out_paths->jobs_root) ||
        snprintf(out_paths->job_root, sizeof(out_paths->job_root), "%s/%s", jobs_root, job_id) >=
            (int)sizeof(out_paths->job_root) ||
        snprintf(out_paths->shared_job_path,
                 sizeof(out_paths->shared_job_path),
                 "%s/job.json",
                 out_paths->job_root) >= (int)sizeof(out_paths->shared_job_path) ||
        snprintf(out_paths->shared_report_path,
                 sizeof(out_paths->shared_report_path),
                 "%s/output/report.json",
                 out_paths->job_root) >= (int)sizeof(out_paths->shared_report_path) ||
        snprintf(out_paths->job_request_path,
                 sizeof(out_paths->job_request_path),
                 "%s/job_request.json",
                 out_paths->job_root) >= (int)sizeof(out_paths->job_request_path) ||
        snprintf(out_paths->job_status_path,
                 sizeof(out_paths->job_status_path),
                 "%s/job_status.json",
                 out_paths->job_root) >= (int)sizeof(out_paths->job_status_path) ||
        snprintf(out_paths->stdout_log_path,
                 sizeof(out_paths->stdout_log_path),
                 "%s/stdout.log",
                 out_paths->job_root) >= (int)sizeof(out_paths->stdout_log_path) ||
        snprintf(out_paths->stderr_log_path,
                 sizeof(out_paths->stderr_log_path),
                 "%s/stderr.log",
                 out_paths->job_root) >= (int)sizeof(out_paths->stderr_log_path) ||
        snprintf(out_paths->pid_path, sizeof(out_paths->pid_path), "%s/pid.txt", out_paths->job_root) >=
            (int)sizeof(out_paths->pid_path) ||
        snprintf(out_paths->result_summary_path,
                 sizeof(out_paths->result_summary_path),
                 "%s/result_summary.json",
                 out_paths->job_root) >= (int)sizeof(out_paths->result_summary_path)) {
        memset(out_paths, 0, sizeof(*out_paths));
        return false;
    }
    return true;
}

bool ray_tracing_job_runner_validate_output_root(const char *output_root) {
    return output_root && output_root[0] == '/' && !path_contains_parent_segment(output_root) &&
           strcmp(output_root, "/") != 0;
}

bool ray_tracing_job_runner_build_frames_dir_path(const char *output_root,
                                                  char *out_path,
                                                  size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path, out_path_size, "%s/frames", output_root) < (int)out_path_size;
}

bool ray_tracing_job_runner_build_frame_path(const char *output_root,
                                             int frame_index,
                                             char *out_path,
                                             size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path,
                    out_path_size,
                    "%s/frames/frame_%04d.bmp",
                    output_root,
                    frame_index) < (int)out_path_size;
}
