#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
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
    const char *slash = NULL;
    char dir[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    slash = strrchr(path, '/');
    if (!slash) return true;
    len = (size_t)(slash - path);
    if (len == 0u) {
        return true;
    }
    if (len >= sizeof(dir)) return false;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return ensure_directory_exists(dir);
}

static bool write_progress_file(const char *path,
                                const RayTracingAgentRenderRequest *request,
                                const char *stage,
                                int frame_index,
                                int frames_completed,
                                int temporal_subpasses_started,
                                int temporal_subpasses_completed,
                                int temporal_subpasses_total,
                                size_t completed_tiles_in_subpass,
                                size_t total_tiles_in_subpass,
                                double elapsed_seconds,
                                double estimated_remaining_seconds,
                                const char *state,
                                const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    if (!path || !path[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_render_progress_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    RayTracingJsonWriteString(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

bool ray_tracing_render_headless_write_progress_and_job_status(
    const char *progress_path,
    const RayTracingAgentRenderRequest *request,
    const char *stage,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *state,
    const char *diagnostics,
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    int exit_code) {
    if (!write_progress_file(progress_path,
                             request,
                             stage,
                             frame_index,
                             frames_completed,
                             temporal_subpasses_started,
                             temporal_subpasses_completed,
                             temporal_subpasses_total,
                             completed_tiles_in_subpass,
                             total_tiles_in_subpass,
                             elapsed_seconds,
                             estimated_remaining_seconds,
                             state,
                             diagnostics)) {
        return false;
    }
    if (job_status_path && job_status_path[0] && job_id && job_id[0]) {
        if (!ray_tracing_render_headless_write_job_status_file(job_status_path,
                                                               job_id,
                                                               request_path,
                                                               request,
                                                               state,
                                                               stage,
                                                               exit_code,
                                                               frame_index,
                                                               frames_completed,
                                                               temporal_subpasses_started,
                                                               temporal_subpasses_completed,
                                                               temporal_subpasses_total,
                                                               completed_tiles_in_subpass,
                                                               total_tiles_in_subpass,
                                                               elapsed_seconds,
                                                               estimated_remaining_seconds,
                                                               diagnostics)) {
            return false;
        }
    }
    return true;
}

void ray_tracing_render_headless_write_process_started_status(
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    bool render_mode) {
    if (!job_status_path || !job_status_path[0] || !request) return;
    ray_tracing_render_headless_write_job_status_file(
        job_status_path,
        job_id ? job_id : request->run_id,
        request_path,
        request,
        render_mode ? "running" : "preflight",
        render_mode ? "loading_request" : "preflight",
        -1,
        request->start_frame,
        0,
        0,
        0,
        request->temporal_frames > 0 ? request->temporal_frames : 1,
        0u,
        0u,
        0.0,
        -1.0,
        "render process started");
}

void ray_tracing_render_headless_write_process_finished_status(
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    int run_code) {
    if (!job_status_path || !job_status_path[0] || !request || !preflight) return;
    ray_tracing_render_headless_write_job_status_file(
        job_status_path,
        job_id ? job_id : request->run_id,
        request_path,
        request,
        run_code == 0 ? "completed" : "failed",
        run_code == 0 ? "completed" : "failed",
        run_code,
        run_code == 0 ? (request->start_frame + request->frame_count - 1) : request->start_frame,
        run_code == 0 ? request->frame_count : 0,
        run_code == 0 ? (request->temporal_frames > 0 ? request->temporal_frames : 1) : 0,
        run_code == 0 ? (request->temporal_frames > 0 ? request->temporal_frames : 1) : 0,
        request->temporal_frames > 0 ? request->temporal_frames : 1,
        0u,
        0u,
        0.0,
        run_code == 0 ? 0.0 : -1.0,
        preflight->diagnostics);
}

static void load_existing_job_status_times(const char *path,
                                           char *out_submitted_at_utc,
                                           size_t submitted_size,
                                           char *out_started_at_utc,
                                           size_t started_size) {
    json_object *root = NULL;
    json_object *value = NULL;
    const char *text_value = NULL;
    if (out_submitted_at_utc && submitted_size > 0u) out_submitted_at_utc[0] = '\0';
    if (out_started_at_utc && started_size > 0u) out_started_at_utc[0] = '\0';
    if (!path || !path[0]) return;
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }
    if (out_submitted_at_utc && submitted_size > 0u &&
        json_object_object_get_ex(root, "submitted_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_submitted_at_utc, submitted_size, "%s", text_value);
    }
    if (out_started_at_utc && started_size > 0u &&
        json_object_object_get_ex(root, "started_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_started_at_utc, started_size, "%s", text_value);
    }
    json_object_put(root);
}

bool ray_tracing_render_headless_write_job_status_file(
    const char *path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    const char *state,
    const char *stage,
    int exit_code,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    char started_at_utc[32] = {0};
    char finished_at_utc[32] = {0};
    char submitted_at_utc[32] = {0};
    char stdout_path[PATH_MAX] = {0};
    char stderr_path[PATH_MAX] = {0};
    char pid_path[PATH_MAX] = {0};
    char job_root[PATH_MAX] = {0};
    char overwrite_policy[32] = {0};
    const char *slash = NULL;
    size_t job_root_len = 0u;
    if (!path || !path[0] || !job_id || !job_id[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    load_existing_job_status_times(path,
                                   submitted_at_utc,
                                   sizeof(submitted_at_utc),
                                   started_at_utc,
                                   sizeof(started_at_utc));
    slash = strrchr(path, '/');
    if (slash) {
        job_root_len = (size_t)(slash - path);
        if (job_root_len > 0u && job_root_len < sizeof(job_root)) {
            memcpy(job_root, path, job_root_len);
            job_root[job_root_len] = '\0';
            snprintf(stdout_path, sizeof(stdout_path), "%s/stdout.log", job_root);
            snprintf(stderr_path, sizeof(stderr_path), "%s/stderr.log", job_root);
            snprintf(pid_path, sizeof(pid_path), "%s/pid.txt", job_root);
        }
    }
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    if ((strcmp(state ? state : "", "running") == 0 ||
         strcmp(state ? state : "", "completed") == 0 ||
         strcmp(state ? state : "", "failed") == 0) &&
        started_at_utc[0] == '\0') {
        snprintf(started_at_utc, sizeof(started_at_utc), "%s", updated_at_utc);
    }
    if (strcmp(state ? state : "", "completed") == 0 ||
        strcmp(state ? state : "", "failed") == 0 ||
        strcmp(state ? state : "", "cancelled") == 0) {
        snprintf(finished_at_utc, sizeof(finished_at_utc), "%s", updated_at_utc);
    }
    if (submitted_at_utc[0] == '\0') {
        snprintf(submitted_at_utc, sizeof(submitted_at_utc), "%s", updated_at_utc);
    }
    if (request->overwrite) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "overwrite");
    } else if (request->has_sampling_window && request->sampling_frame_offset > 0) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "resume");
    } else {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "fail_if_exists");
    }
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_detached_job_status_v1\",\n");
    fprintf(file, "  \"program\": \"ray_tracing\",\n");
    fprintf(file, "  \"tool\": \"ray_tracing_render_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    RayTracingJsonWriteString(file, job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    RayTracingJsonWriteString(file, request_path ? request_path : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    RayTracingJsonWriteString(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    RayTracingJsonWriteString(file, request->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    RayTracingJsonWriteString(file, request->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    RayTracingJsonWriteString(file, stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    RayTracingJsonWriteString(file, stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid_path\": ");
    RayTracingJsonWriteString(file, pid_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)getpid());
    fprintf(file, "  \"exit_code\": %d,\n", exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    RayTracingJsonWriteString(file, overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"requested_start_frame\": %d,\n",
            request->start_frame - (request->has_sampling_window ? request->sampling_frame_offset : 0));
    fprintf(file, "  \"requested_frame_count\": %d,\n",
            request->has_sampling_window ? request->sampling_frame_count : request->frame_count);
    fprintf(file, "  \"effective_start_frame\": %d,\n", request->start_frame);
    fprintf(file, "  \"effective_frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"submitted_at_utc\": ");
    RayTracingJsonWriteString(file, submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    RayTracingJsonWriteString(file, started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    RayTracingJsonWriteString(file, finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}
