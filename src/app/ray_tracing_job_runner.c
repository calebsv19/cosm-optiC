#include "app/ray_tracing_job_runner.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/agent_render_request.h"

typedef struct RayTracingDetachedJobPaths {
    char jobs_root[PATH_MAX];
    char job_root[PATH_MAX];
    char job_request_path[PATH_MAX];
    char job_status_path[PATH_MAX];
    char stdout_log_path[PATH_MAX];
    char stderr_log_path[PATH_MAX];
    char pid_path[PATH_MAX];
    char result_summary_path[PATH_MAX];
} RayTracingDetachedJobPaths;

typedef struct RayTracingDetachedJobRecord {
    char job_id[64];
    char state[32];
    char stage[32];
    char overwrite_policy[32];
    char submitted_at_utc[32];
    char started_at_utc[32];
    char updated_at_utc[32];
    char finished_at_utc[32];
    char request_path[PATH_MAX];
    char output_root[PATH_MAX];
    char progress_path[PATH_MAX];
    char summary_path[PATH_MAX];
    char stdout_path[PATH_MAX];
    char stderr_path[PATH_MAX];
    pid_t pid;
    int exit_code;
    int requested_start_frame;
    int requested_frame_count;
    int effective_start_frame;
    int effective_frame_count;
    int frame_index;
    int frames_completed;
    int temporal_subpasses_started;
    int temporal_subpasses_completed;
    int temporal_subpasses_total;
    char diagnostics[256];
} RayTracingDetachedJobRecord;

#define RAY_TRACING_JOB_STALL_TIMEOUT_SECONDS (15 * 60)

static void diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}

static bool copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

static bool file_exists(const char *path) {
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

static bool ensure_directory_exists(const char *path) {
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

static bool ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!parent_dir_of(path, dir, sizeof(dir))) return false;
    return ensure_directory_exists(dir);
}

static bool read_text_file(const char *path, char **out_text) {
    FILE *file = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;

    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

static bool write_text_file(const char *path, const char *text) {
    FILE *file = NULL;
    if (!path || !path[0] || !text) return false;
    if (!ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (fputs(text, file) < 0) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static void json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
#if defined(__APPLE__) || defined(__unix__)
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
#else
    {
        struct tm *tmp = gmtime(&now);
        if (!tmp) return false;
        tm_utc = *tmp;
    }
#endif
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

static bool resolve_real_path(const char *path, char *out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !path[0] || !out || out_size == 0u) return false;
    if (!realpath(path, resolved)) return false;
    return copy_string(out, out_size, resolved);
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

static bool derive_render_cli_path(const char *argv0, char *out_path, size_t out_path_size) {
    char binary_path[PATH_MAX];
    char dir_path[PATH_MAX];
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    if (!parent_dir_of(binary_path, dir_path, sizeof(dir_path))) return false;
    if (snprintf(out_path, out_path_size, "%s/ray_tracing_render_headless", dir_path) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return file_exists(out_path);
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

static bool build_jobs_root(const char *argv0,
                            const char *jobs_root_override,
                            char *out_jobs_root,
                            size_t out_jobs_root_size) {
    char resolved[PATH_MAX];
    if (jobs_root_override && jobs_root_override[0]) {
        if (jobs_root_override[0] == '/') {
            return copy_string(out_jobs_root, out_jobs_root_size, jobs_root_override);
        }
        if (!resolve_real_path(jobs_root_override, resolved, sizeof(resolved))) {
            return copy_string(out_jobs_root, out_jobs_root_size, jobs_root_override);
        }
        return copy_string(out_jobs_root, out_jobs_root_size, resolved);
    }
    return ray_tracing_job_runner_default_jobs_root(argv0, out_jobs_root, out_jobs_root_size);
}

static void build_job_paths(const char *jobs_root,
                            const char *job_id,
                            RayTracingDetachedJobPaths *out_paths) {
    if (!jobs_root || !job_id || !out_paths) return;
    memset(out_paths, 0, sizeof(*out_paths));
    snprintf(out_paths->jobs_root, sizeof(out_paths->jobs_root), "%s", jobs_root);
    snprintf(out_paths->job_root, sizeof(out_paths->job_root), "%s/%s", jobs_root, job_id);
    snprintf(out_paths->job_request_path,
             sizeof(out_paths->job_request_path),
             "%s/job_request.json",
             out_paths->job_root);
    snprintf(out_paths->job_status_path,
             sizeof(out_paths->job_status_path),
             "%s/job_status.json",
             out_paths->job_root);
    snprintf(out_paths->stdout_log_path,
             sizeof(out_paths->stdout_log_path),
             "%s/stdout.log",
             out_paths->job_root);
    snprintf(out_paths->stderr_log_path,
             sizeof(out_paths->stderr_log_path),
             "%s/stderr.log",
             out_paths->job_root);
    snprintf(out_paths->pid_path, sizeof(out_paths->pid_path), "%s/pid.txt", out_paths->job_root);
    snprintf(out_paths->result_summary_path,
             sizeof(out_paths->result_summary_path),
             "%s/result_summary.json",
             out_paths->job_root);
}

static bool generate_job_id(char *out_job_id, size_t out_job_id_size) {
    struct timespec ts;
    uint32_t salt = 0u;
    if (!out_job_id || out_job_id_size == 0u) return false;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return false;
    salt = (uint32_t)((uint64_t)getpid() ^ (uint64_t)ts.tv_nsec);
    return snprintf(out_job_id,
                    out_job_id_size,
                    "rtjob_%lld_%06u",
                    (long long)ts.tv_sec,
                    (unsigned)(salt % 1000000u)) < (int)out_job_id_size;
}

static void detached_job_record_defaults(RayTracingDetachedJobRecord *record) {
    if (!record) return;
    memset(record, 0, sizeof(*record));
    snprintf(record->state, sizeof(record->state), "queued");
    snprintf(record->stage, sizeof(record->stage), "queued");
    snprintf(record->overwrite_policy, sizeof(record->overwrite_policy), "fail_if_exists");
    snprintf(record->diagnostics, sizeof(record->diagnostics), "queued");
    record->pid = 0;
    record->exit_code = -1;
}

static bool write_job_status_file(const RayTracingDetachedJobPaths *paths,
                                  const RayTracingDetachedJobRecord *record) {
    FILE *file = NULL;
    if (!paths || !record) return false;
    if (!ensure_parent_directory_exists(paths->job_status_path)) return false;
    file = fopen(paths->job_status_path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": ");
    json_write_string(file, RAY_TRACING_DETACHED_JOB_STATUS_SCHEMA);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": \"ray_tracing\",\n");
    fprintf(file, "  \"tool\": \"ray_tracing_render_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    json_write_string(file, record->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    json_write_string(file, record->state);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    json_write_string(file, record->stage);
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    json_write_string(file, record->request_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    json_write_string(file, record->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    json_write_string(file, record->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    json_write_string(file, record->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    json_write_string(file, record->stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    json_write_string(file, record->stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)record->pid);
    fprintf(file, "  \"exit_code\": %d,\n", record->exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    json_write_string(file, record->overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"requested_start_frame\": %d,\n", record->requested_start_frame);
    fprintf(file, "  \"requested_frame_count\": %d,\n", record->requested_frame_count);
    fprintf(file, "  \"effective_start_frame\": %d,\n", record->effective_start_frame);
    fprintf(file, "  \"effective_frame_count\": %d,\n", record->effective_frame_count);
    fprintf(file, "  \"frame_index\": %d,\n", record->frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", record->frames_completed);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", record->temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", record->temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", record->temporal_subpasses_total);
    fprintf(file, "  \"progress_ratio\": %.6f,\n",
            (record->temporal_subpasses_total > 0)
                ? ((double)record->temporal_subpasses_completed /
                   (double)record->temporal_subpasses_total)
                : ((record->effective_frame_count > 0)
                       ? ((double)record->frames_completed /
                          (double)record->effective_frame_count)
                       : 0.0));
    fprintf(file, "  \"submitted_at_utc\": ");
    json_write_string(file, record->submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    json_write_string(file, record->started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    json_write_string(file, record->updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    json_write_string(file, record->finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    json_write_string(file, record->diagnostics);
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

static bool read_job_pid(const RayTracingDetachedJobPaths *paths, pid_t *out_pid) {
    char *text = NULL;
    long value = 0;
    char *end = NULL;
    if (out_pid) *out_pid = 0;
    if (!paths || !out_pid || !read_text_file(paths->pid_path, &text)) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    free(text);
    if (errno != 0 || end == text) return false;
    *out_pid = (pid_t)value;
    return true;
}

static bool spawn_detached_render(const char *render_cli_path,
                                  const RayTracingDetachedJobPaths *paths,
                                  const char *job_id,
                                  pid_t *out_pid) {
    pid_t pid = 0;
    FILE *stdout_file = NULL;
    FILE *stderr_file = NULL;
    if (out_pid) *out_pid = 0;
    if (!render_cli_path || !paths || !job_id) return false;
    if (!ensure_directory_exists(paths->job_root)) return false;

    pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        char *const argv[] = {
            (char *)render_cli_path,
            (char *)"--request",
            (char *)paths->job_request_path,
            (char *)"--render",
            (char *)"--summary",
            (char *)paths->result_summary_path,
            (char *)"--job-id",
            (char *)job_id,
            (char *)"--job-status",
            (char *)paths->job_status_path,
            NULL
        };
        int null_fd = -1;
        if (setsid() < 0) _exit(126);
        stdout_file = fopen(paths->stdout_log_path, "ab");
        stderr_file = fopen(paths->stderr_log_path, "ab");
        if (!stdout_file || !stderr_file) _exit(126);
        if (dup2(fileno(stdout_file), STDOUT_FILENO) < 0) _exit(126);
        if (dup2(fileno(stderr_file), STDERR_FILENO) < 0) _exit(126);
        null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }
        execv(render_cli_path, argv);
        _exit(127);
    }
    if (out_pid) *out_pid = pid;
    return true;
}

static bool load_request_for_job(const char *request_path,
                                 RayTracingAgentRenderRequest *out_request,
                                 char *out_diagnostics,
                                 size_t out_diagnostics_size) {
    return ray_tracing_agent_render_request_load_file(request_path,
                                                      out_request,
                                                      out_diagnostics,
                                                      out_diagnostics_size);
}

static bool write_canonical_request_file(const char *path,
                                         const RayTracingAgentRenderRequest *request,
                                         const RayTracingDetachedJobPaths *paths) {
    FILE *file = NULL;
    char progress_path[PATH_MAX];
    bool wrote_field = false;
    if (!path || !request || !paths) return false;
    if (snprintf(progress_path,
                 sizeof(progress_path),
                 "%s/render_progress.json",
                 paths->job_root) >= (int)sizeof(progress_path)) {
        return false;
    }
    if (!ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": ");
    json_write_string(file, request->schema_version);
    fprintf(file, ",\n");
    fprintf(file, "  \"run_id\": ");
    json_write_string(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"scene\": {\n");
    fprintf(file, "    \"runtime_scene_path\": ");
    json_write_string(file, request->runtime_scene_path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"volume\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", request->volume_enabled ? "true" : "false");
    if (request->volume_enabled && request->volume_source_path[0]) {
        fprintf(file, "    \"source_kind\": ");
        json_write_string(file,
                          ray_tracing_agent_render_request_volume_kind_label(
                              request->volume_source_kind));
        fprintf(file, ",\n");
        fprintf(file, "    \"source_path\": ");
        json_write_string(file, request->volume_source_path);
        fprintf(file, ",\n");
        fprintf(file, "    \"affects_lighting\": %s,\n",
                request->volume_affects_lighting ? "true" : "false");
        fprintf(file, "    \"debug_overlay\": %s\n",
                request->volume_debug_overlay ? "true" : "false");
    } else {
        fprintf(file, "    \"affects_lighting\": false,\n");
        fprintf(file, "    \"debug_overlay\": false\n");
    }
    fprintf(file, "  },\n");
    fprintf(file, "  \"render\": {\n");
    fprintf(file, "    \"start_frame\": %d,\n", request->start_frame);
    fprintf(file, "    \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "    \"width\": %d,\n", request->width);
    fprintf(file, "    \"height\": %d,\n", request->height);
    fprintf(file, "    \"normalized_t\": %.9f,\n", request->normalized_t);
    fprintf(file, "    \"temporal_frames\": %d,\n", request->temporal_frames);
    fprintf(file, "    \"integrator_3d\": ");
    json_write_string(file,
                      ray_tracing_agent_render_request_integrator_label(
                          request->integrator_3d));
    fprintf(file, "\n  },\n");
    if (request->has_sampling_window) {
        fprintf(file, "  \"sampling\": {\n");
        fprintf(file, "    \"frame_offset\": %d,\n", request->sampling_frame_offset);
        fprintf(file, "    \"frame_count\": %d\n", request->sampling_frame_count);
        fprintf(file, "  },\n");
    }
    fprintf(file, "  \"output\": {\n");
    fprintf(file, "    \"root\": ");
    json_write_string(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "    \"overwrite\": %s\n", request->overwrite ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"progress\": {\n");
    fprintf(file, "    \"summary_path\": ");
    json_write_string(file, paths->result_summary_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"progress_path\": ");
    json_write_string(file, progress_path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"inspection\": {\n");
    if (request->has_camera_zoom_override) {
        fprintf(file, "    \"camera_zoom\": %.9f", request->camera_zoom_override);
        wrote_field = true;
    }
    if (request->has_camera_position_override) {
        fprintf(file,
                "%s    \"camera_position\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f }",
                wrote_field ? ",\n" : "",
                request->camera_position_x,
                request->camera_position_y,
                request->camera_position_z);
        wrote_field = true;
    }
    if (request->has_camera_look_at_override) {
        fprintf(file,
                "%s    \"camera_look_at\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f }",
                wrote_field ? ",\n" : "",
                request->camera_look_at_x,
                request->camera_look_at_y,
                request->camera_look_at_z);
        wrote_field = true;
    }
    if (request->has_environment_brightness_override) {
        fprintf(file,
                "%s    \"environment_brightness\": %.9f",
                wrote_field ? ",\n" : "",
                request->environment_brightness_override);
        wrote_field = true;
    }
    if (request->has_light_intensity_override) {
        fprintf(file,
                "%s    \"light_intensity\": %.9f",
                wrote_field ? ",\n" : "",
                request->light_intensity_override);
        wrote_field = true;
    }
    if (request->has_light_radius_override) {
        fprintf(file,
                "%s    \"light_radius\": %.9f",
                wrote_field ? ",\n" : "",
                request->light_radius_override);
        wrote_field = true;
    }
    if (request->has_forward_decay_override) {
        fprintf(file,
                "%s    \"forward_decay\": %.9f",
                wrote_field ? ",\n" : "",
                request->forward_decay_override);
        wrote_field = true;
    }
    if (request->has_volume_scatter_gain_override) {
        fprintf(file,
                "%s    \"volume_scatter_gain\": %.9f",
                wrote_field ? ",\n" : "",
                request->volume_scatter_gain_override);
        wrote_field = true;
    }
    if (request->has_volume_step_scale_override) {
        fprintf(file,
                "%s    \"volume_step_scale\": %.9f",
                wrote_field ? ",\n" : "",
                request->volume_step_scale_override);
        wrote_field = true;
    }
    if (request->has_secondary_diffuse_samples_3d_override) {
        fprintf(file,
                "%s    \"secondary_diffuse_samples_3d\": %d",
                wrote_field ? ",\n" : "",
                request->secondary_diffuse_samples_3d_override);
        wrote_field = true;
    }
    if (request->has_transmission_samples_3d_override) {
        fprintf(file,
                "%s    \"transmission_samples_3d\": %d",
                wrote_field ? ",\n" : "",
                request->transmission_samples_3d_override);
        wrote_field = true;
    }
    if (request->has_volume_tint_override) {
        fprintf(file,
                "%s    \"volume_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f }",
                wrote_field ? ",\n" : "",
                request->volume_tint_r,
                request->volume_tint_g,
                request->volume_tint_b);
        wrote_field = true;
    }
    if (wrote_field) {
        fprintf(file, "\n");
    }
    fprintf(file, "  }\n");
    fprintf(file, "}\n");
    fclose(file);
    return true;
}

static bool write_pid_file(const char *path, pid_t pid) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%ld\n", (long)pid);
    return write_text_file(path, buffer);
}

static bool build_frame_path(const char *output_root,
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

static int count_contiguous_existing_frames(const RayTracingAgentRenderRequest *request) {
    int existing = 0;
    char frame_path[PATH_MAX];
    if (!request || request->frame_count <= 0) return 0;
    for (int i = 0; i < request->frame_count; ++i) {
        if (!build_frame_path(request->output_root,
                              request->start_frame + i,
                              frame_path,
                              sizeof(frame_path))) {
            break;
        }
        if (!file_exists(frame_path)) {
            break;
        }
        existing += 1;
    }
    return existing;
}

static bool print_file_to_stream(FILE *out, const char *path) {
    char *text = NULL;
    if (!out || !path || !read_text_file(path, &text)) return false;
    fputs(text, out);
    free(text);
    return true;
}

static bool json_get_string(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

static bool json_get_int(json_object *owner, const char *key, int *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_int(obj);
    return true;
}

static bool load_job_status_record(const RayTracingDetachedJobPaths *paths,
                                   RayTracingDetachedJobRecord *out_record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!paths || !out_record) return false;
    detached_job_record_defaults(out_record);
    root = json_object_from_file(paths->job_status_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    if (json_get_string(root, "job_id", &text_value)) {
        copy_string(out_record->job_id, sizeof(out_record->job_id), text_value);
    }
    if (json_get_string(root, "state", &text_value)) {
        copy_string(out_record->state, sizeof(out_record->state), text_value);
    }
    if (json_get_string(root, "stage", &text_value)) {
        copy_string(out_record->stage, sizeof(out_record->stage), text_value);
    }
    if (json_get_string(root, "overwrite_policy", &text_value)) {
        copy_string(out_record->overwrite_policy, sizeof(out_record->overwrite_policy), text_value);
    }
    if (json_get_string(root, "request_path", &text_value)) {
        copy_string(out_record->request_path, sizeof(out_record->request_path), text_value);
    }
    if (json_get_string(root, "output_root", &text_value)) {
        copy_string(out_record->output_root, sizeof(out_record->output_root), text_value);
    }
    if (json_get_string(root, "progress_path", &text_value)) {
        copy_string(out_record->progress_path, sizeof(out_record->progress_path), text_value);
    }
    if (json_get_string(root, "summary_path", &text_value)) {
        copy_string(out_record->summary_path, sizeof(out_record->summary_path), text_value);
    }
    if (json_get_string(root, "stdout_path", &text_value)) {
        copy_string(out_record->stdout_path, sizeof(out_record->stdout_path), text_value);
    }
    if (json_get_string(root, "stderr_path", &text_value)) {
        copy_string(out_record->stderr_path, sizeof(out_record->stderr_path), text_value);
    }
    if (json_get_string(root, "submitted_at_utc", &text_value)) {
        copy_string(out_record->submitted_at_utc, sizeof(out_record->submitted_at_utc), text_value);
    }
    if (json_get_string(root, "started_at_utc", &text_value)) {
        copy_string(out_record->started_at_utc, sizeof(out_record->started_at_utc), text_value);
    }
    if (json_get_string(root, "updated_at_utc", &text_value)) {
        copy_string(out_record->updated_at_utc, sizeof(out_record->updated_at_utc), text_value);
    }
    if (json_get_string(root, "finished_at_utc", &text_value)) {
        copy_string(out_record->finished_at_utc, sizeof(out_record->finished_at_utc), text_value);
    }
    if (json_get_string(root, "diagnostics", &text_value)) {
        copy_string(out_record->diagnostics, sizeof(out_record->diagnostics), text_value);
    }
    if (json_get_int(root, "pid", &int_value)) out_record->pid = (pid_t)int_value;
    if (json_get_int(root, "exit_code", &int_value)) out_record->exit_code = int_value;
    if (json_get_int(root, "requested_start_frame", &int_value)) out_record->requested_start_frame = int_value;
    if (json_get_int(root, "requested_frame_count", &int_value)) out_record->requested_frame_count = int_value;
    if (json_get_int(root, "effective_start_frame", &int_value)) out_record->effective_start_frame = int_value;
    if (json_get_int(root, "effective_frame_count", &int_value)) out_record->effective_frame_count = int_value;
    if (json_get_int(root, "frame_index", &int_value)) out_record->frame_index = int_value;
    if (json_get_int(root, "frames_completed", &int_value)) out_record->frames_completed = int_value;
    if (json_get_int(root, "temporal_subpasses_started", &int_value)) {
        out_record->temporal_subpasses_started = int_value;
    }
    if (json_get_int(root, "temporal_subpasses_completed", &int_value)) {
        out_record->temporal_subpasses_completed = int_value;
    }
    if (json_get_int(root, "temporal_subpasses_total", &int_value)) {
        out_record->temporal_subpasses_total = int_value;
    }
    json_object_put(root);
    return true;
}

static bool merge_progress_into_record(const char *progress_path,
                                       RayTracingDetachedJobRecord *record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!progress_path || !progress_path[0] || !record || !file_exists(progress_path)) return false;
    root = json_object_from_file(progress_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    if (json_get_string(root, "stage", &text_value)) {
        copy_string(record->stage, sizeof(record->stage), text_value);
    }
    if (json_get_string(root, "state", &text_value)) {
        copy_string(record->state, sizeof(record->state), text_value);
    }
    if (json_get_string(root, "updated_at_utc", &text_value)) {
        copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), text_value);
        if (record->started_at_utc[0] == '\0' &&
            strcmp(record->state, "running") == 0) {
            copy_string(record->started_at_utc,
                        sizeof(record->started_at_utc),
                        text_value);
        }
    }
    if (json_get_int(root, "frame_index", &int_value)) record->frame_index = int_value;
    if (json_get_int(root, "frames_completed", &int_value)) record->frames_completed = int_value;
    if (json_get_int(root, "frame_count", &int_value)) record->effective_frame_count = int_value;
    if (json_get_int(root, "temporal_subpasses_started", &int_value)) {
        record->temporal_subpasses_started = int_value;
    }
    if (json_get_int(root, "temporal_subpasses_completed", &int_value)) {
        record->temporal_subpasses_completed = int_value;
    }
    if (json_get_int(root, "temporal_subpasses_total", &int_value)) {
        record->temporal_subpasses_total = int_value;
    }
    if (json_get_string(root, "diagnostics", &text_value)) {
        copy_string(record->diagnostics, sizeof(record->diagnostics), text_value);
    }
    json_object_put(root);
    return true;
}

static bool pid_is_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    return errno != ESRCH;
}

static bool parse_utc_timestamp(const char *text, time_t *out_time) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    struct tm tm_value;
    time_t value = (time_t)-1;
    if (out_time) *out_time = (time_t)-1;
    if (!text || !text[0] || !out_time) return false;
    if (sscanf(text,
               "%d-%d-%dT%d:%d:%dZ",
               &year,
               &month,
               &day,
               &hour,
               &minute,
               &second) != 6) {
        return false;
    }
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
#if defined(__APPLE__) || defined(__unix__)
    value = timegm(&tm_value);
#else
    value = mktime(&tm_value);
#endif
    if (value == (time_t)-1) return false;
    *out_time = value;
    return true;
}

static bool refresh_job_status_record(const RayTracingDetachedJobPaths *paths,
                                      RayTracingDetachedJobRecord *record) {
    char now_utc[32] = {0};
    time_t now_time = (time_t)-1;
    time_t updated_time = (time_t)-1;
    bool changed = false;
    bool alive = false;
    if (!paths || !record) return false;
    if (merge_progress_into_record(record->progress_path, record)) {
        changed = true;
    }
    alive = pid_is_alive(record->pid);
    if ((strcmp(record->state, "starting") == 0 || strcmp(record->state, "running") == 0) &&
        !alive) {
        utc_now_string(now_utc, sizeof(now_utc));
        if (file_exists(record->summary_path)) {
            snprintf(record->state, sizeof(record->state), "completed");
            snprintf(record->stage, sizeof(record->stage), "completed");
            record->exit_code = 0;
            if (record->finished_at_utc[0] == '\0') {
                copy_string(record->finished_at_utc, sizeof(record->finished_at_utc), now_utc);
            }
            if (record->updated_at_utc[0] == '\0') {
                copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), now_utc);
            }
        } else {
            snprintf(record->state, sizeof(record->state), "failed");
            if (record->stage[0] == '\0' || strcmp(record->stage, "starting") == 0) {
                snprintf(record->stage, sizeof(record->stage), "failed");
            }
            if (record->exit_code < 0) record->exit_code = 1;
            copy_string(record->finished_at_utc, sizeof(record->finished_at_utc), now_utc);
            copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), now_utc);
            if (record->diagnostics[0] == '\0') {
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "process exited without completion summary");
            }
        }
        changed = true;
    }
    if ((strcmp(record->state, "starting") == 0 ||
         strcmp(record->state, "running") == 0 ||
         strcmp(record->state, "stalled") == 0) &&
        alive &&
        utc_now_string(now_utc, sizeof(now_utc)) &&
        parse_utc_timestamp(now_utc, &now_time) &&
        parse_utc_timestamp(record->updated_at_utc, &updated_time)) {
        const double stale_seconds = difftime(now_time, updated_time);
        if (stale_seconds >= (double)RAY_TRACING_JOB_STALL_TIMEOUT_SECONDS) {
            if (strcmp(record->state, "stalled") != 0) {
                snprintf(record->state, sizeof(record->state), "stalled");
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "no progress update for %.0f seconds while process remained alive",
                         stale_seconds);
                changed = true;
            }
        } else if (strcmp(record->state, "stalled") == 0) {
            snprintf(record->state, sizeof(record->state), "running");
            if (record->diagnostics[0] == '\0' ||
                strstr(record->diagnostics, "no progress update for") != NULL) {
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "render resumed within stall threshold");
            }
            changed = true;
        }
    }
    if (changed) {
        if (!write_job_status_file(paths, record)) return false;
    }
    return true;
}

bool ray_tracing_job_runner_submit(const char *argv0,
                                   const char *request_path,
                                   const char *jobs_root_override,
                                   bool overwrite,
                                   bool resume,
                                   char *out_job_id,
                                   size_t out_job_id_size,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    RayTracingAgentRenderRequest request;
    RayTracingDetachedJobPaths paths;
    RayTracingDetachedJobRecord record;
    char jobs_root[PATH_MAX];
    char render_cli_path[PATH_MAX];
    char diagnostics[256];
    pid_t pid = 0;
    int contiguous_existing = 0;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !request_path || !request_path[0] || !out_job_id || out_job_id_size == 0u) {
        return false;
    }
    if (!load_request_for_job(request_path, &request, diagnostics, sizeof(diagnostics))) {
        diag_set(out_diagnostics, out_diagnostics_size, diagnostics);
        return false;
    }
    if (overwrite && resume) {
        diag_set(out_diagnostics, out_diagnostics_size, "overwrite and resume are mutually exclusive");
        return false;
    }
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    if (!derive_render_cli_path(argv0, render_cli_path, sizeof(render_cli_path))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve render cli path");
        return false;
    }
    contiguous_existing = count_contiguous_existing_frames(&request);
    if (!overwrite && !resume && contiguous_existing > 0) {
        diag_set(out_diagnostics,
                 out_diagnostics_size,
                 "existing output frames found; use --overwrite or --resume");
        return false;
    }
    if (overwrite) {
        request.overwrite = true;
    }
    if (resume) {
        if (contiguous_existing <= 0) {
            request.overwrite = false;
        } else if (contiguous_existing >= request.frame_count) {
            diag_set(out_diagnostics,
                     out_diagnostics_size,
                     "all requested frames already exist; nothing to resume");
            return false;
        } else {
            request.has_sampling_window = true;
            request.sampling_frame_offset = contiguous_existing;
            request.sampling_frame_count = request.frame_count;
            request.start_frame += contiguous_existing;
            request.frame_count -= contiguous_existing;
            request.overwrite = false;
        }
    }
    if (!generate_job_id(out_job_id, out_job_id_size)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to generate job id");
        return false;
    }
    build_job_paths(jobs_root, out_job_id, &paths);
    if (file_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job id collision");
        return false;
    }
    if (!ensure_directory_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to create job directory");
        return false;
    }
    if (!write_canonical_request_file(paths.job_request_path, &request, &paths)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to stage job request");
        return false;
    }

    detached_job_record_defaults(&record);
    snprintf(record.job_id, sizeof(record.job_id), "%s", out_job_id);
    copy_string(record.request_path, sizeof(record.request_path), paths.job_request_path);
    copy_string(record.output_root, sizeof(record.output_root), request.output_root);
    snprintf(record.progress_path,
             sizeof(record.progress_path),
             "%s/render_progress.json",
             paths.job_root);
    copy_string(record.summary_path, sizeof(record.summary_path), paths.result_summary_path);
    copy_string(record.stdout_path, sizeof(record.stdout_path), paths.stdout_log_path);
    copy_string(record.stderr_path, sizeof(record.stderr_path), paths.stderr_log_path);
    utc_now_string(record.submitted_at_utc, sizeof(record.submitted_at_utc));
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    snprintf(record.state, sizeof(record.state), "queued");
    snprintf(record.stage, sizeof(record.stage), "queued");
    snprintf(record.overwrite_policy,
             sizeof(record.overwrite_policy),
             "%s",
             overwrite ? "overwrite" : (resume ? "resume" : "fail_if_exists"));
    record.requested_start_frame =
        request.start_frame - (request.has_sampling_window ? request.sampling_frame_offset : 0);
    record.requested_frame_count =
        request.has_sampling_window ? request.sampling_frame_count : request.frame_count;
    record.effective_start_frame = request.start_frame;
    record.effective_frame_count = request.frame_count;
    record.frame_index = request.start_frame;
    record.frames_completed = 0;
    record.temporal_subpasses_started = 0;
    record.temporal_subpasses_completed = 0;
    record.temporal_subpasses_total = request.temporal_frames > 0 ? request.temporal_frames : 1;
    snprintf(record.diagnostics, sizeof(record.diagnostics), "queued for detached render");
    if (!write_job_status_file(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to write queued job status");
        return false;
    }

    if (!spawn_detached_render(render_cli_path, &paths, out_job_id, &pid)) {
        snprintf(record.state, sizeof(record.state), "failed");
        utc_now_string(record.finished_at_utc, sizeof(record.finished_at_utc));
        utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
        record.exit_code = 127;
        snprintf(record.diagnostics, sizeof(record.diagnostics), "failed to spawn detached render");
        (void)write_job_status_file(&paths, &record);
        diag_set(out_diagnostics, out_diagnostics_size, "failed to spawn detached render");
        return false;
    }

    record.pid = pid;
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    snprintf(record.state, sizeof(record.state), "starting");
    snprintf(record.stage, sizeof(record.stage), "starting");
    snprintf(record.diagnostics, sizeof(record.diagnostics), "detached render launched");
    if (!write_pid_file(paths.pid_path, pid) || !write_job_status_file(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to persist detached job state");
        return false;
    }

    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_job_runner_print_status(FILE *out,
                                         const char *argv0,
                                         const char *job_id,
                                         const char *jobs_root_override,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    RayTracingDetachedJobPaths paths;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!out || !argv0 || !job_id || !job_id[0]) return false;
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    build_job_paths(jobs_root, job_id, &paths);
    if (!file_exists(paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job status file not found");
        return false;
    }
    {
        RayTracingDetachedJobRecord record;
        if (load_job_status_record(&paths, &record)) {
            (void)refresh_job_status_record(&paths, &record);
        }
    }
    if (!print_file_to_stream(out, paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read job status file");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_job_runner_cancel(const char *argv0,
                                   const char *job_id,
                                   const char *jobs_root_override,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    RayTracingDetachedJobPaths paths;
    RayTracingDetachedJobRecord record;
    pid_t pid = 0;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !job_id || !job_id[0]) return false;
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    build_job_paths(jobs_root, job_id, &paths);
    if (!read_job_pid(&paths, &pid)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read job pid");
        return false;
    }
    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to signal job pid");
        return false;
    }
    detached_job_record_defaults(&record);
    snprintf(record.job_id, sizeof(record.job_id), "%s", job_id);
    snprintf(record.state, sizeof(record.state), "cancelled");
    snprintf(record.stage, sizeof(record.stage), "cancelled");
    record.pid = pid;
    record.exit_code = 143;
    copy_string(record.request_path, sizeof(record.request_path), paths.job_request_path);
    copy_string(record.summary_path, sizeof(record.summary_path), paths.result_summary_path);
    copy_string(record.stdout_path, sizeof(record.stdout_path), paths.stdout_log_path);
    copy_string(record.stderr_path, sizeof(record.stderr_path), paths.stderr_log_path);
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    utc_now_string(record.finished_at_utc, sizeof(record.finished_at_utc));
    snprintf(record.diagnostics, sizeof(record.diagnostics), "cancel requested");
    if (!write_job_status_file(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to update cancelled job status");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
