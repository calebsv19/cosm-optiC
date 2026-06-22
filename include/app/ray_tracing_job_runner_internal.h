#ifndef RAY_TRACING_JOB_RUNNER_INTERNAL_H
#define RAY_TRACING_JOB_RUNNER_INTERNAL_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include <json-c/json.h>

#include "app/ray_tracing_job_runner.h"

#if defined(__APPLE__) || defined(__unix__)
extern time_t timegm(struct tm *tm);
#endif

typedef struct RayTracingDetachedJobPaths {
    char jobs_root[PATH_MAX];
    char job_root[PATH_MAX];
    char shared_job_path[PATH_MAX];
    char shared_report_path[PATH_MAX];
    char job_request_path[PATH_MAX];
    char job_status_path[PATH_MAX];
    char stdout_log_path[PATH_MAX];
    char stderr_log_path[PATH_MAX];
    char pid_path[PATH_MAX];
    char result_summary_path[PATH_MAX];
} RayTracingDetachedJobPaths;

typedef struct RayTracingDetachedJobRecord {
    char job_id[96];
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

bool copy_string(char *dst, size_t dst_size, const char *src);
bool read_text_file(const char *path, char **out_text);
bool write_text_file(const char *path, const char *text);
void json_write_string(FILE *file, const char *value);
bool utc_now_string(char *out, size_t out_size);
bool ray_tracing_job_runner_file_exists(const char *path);
bool ray_tracing_job_runner_ensure_directory_exists(const char *path);
bool ray_tracing_job_runner_ensure_parent_directory_exists(const char *path);
bool ray_tracing_job_runner_derive_render_cli_path(const char *argv0,
                                                   char *out_path,
                                                   size_t out_path_size);
bool ray_tracing_job_runner_build_jobs_root(const char *argv0,
                                            const char *jobs_root_override,
                                            char *out_jobs_root,
                                            size_t out_jobs_root_size);
bool ray_tracing_job_runner_validate_job_id(const char *job_id);
bool ray_tracing_job_runner_build_job_paths(const char *jobs_root,
                                            const char *job_id,
                                            RayTracingDetachedJobPaths *out_paths);
bool ray_tracing_job_runner_validate_output_root(const char *output_root);
bool ray_tracing_job_runner_build_frames_dir_path(const char *output_root,
                                                  char *out_path,
                                                  size_t out_path_size);
bool ray_tracing_job_runner_build_frame_path(const char *output_root,
                                             int frame_index,
                                             char *out_path,
                                             size_t out_path_size);
const char *ray_tracing_job_runner_shared_report_state_label(const char *state);
void ray_tracing_detached_job_record_defaults(RayTracingDetachedJobRecord *record);
bool ray_tracing_detached_job_record_init_queued(RayTracingDetachedJobRecord *record,
                                                 const RayTracingDetachedJobPaths *paths,
                                                 const char *job_id,
                                                 const char *output_root,
                                                 const char *overwrite_policy,
                                                 int requested_start_frame,
                                                 int requested_frame_count,
                                                 int effective_start_frame,
                                                 int effective_frame_count,
                                                 int temporal_subpasses_total);
void ray_tracing_detached_job_record_mark_spawn_failed(RayTracingDetachedJobRecord *record,
                                                       const RayTracingDetachedJobPaths *paths,
                                                       const char *render_cli_path);
void ray_tracing_detached_job_record_mark_started(RayTracingDetachedJobRecord *record, pid_t pid);
void ray_tracing_detached_job_record_mark_cancelled(RayTracingDetachedJobRecord *record,
                                                    const RayTracingDetachedJobPaths *paths,
                                                    const char *job_id,
                                                    pid_t pid);
bool ray_tracing_job_runner_write_job_status_file(const RayTracingDetachedJobPaths *paths,
                                                  const RayTracingDetachedJobRecord *record);
bool ray_tracing_job_runner_write_shared_report_file(const RayTracingDetachedJobPaths *paths,
                                                     const RayTracingDetachedJobRecord *record);
bool ray_tracing_job_runner_persist_job_state(const RayTracingDetachedJobPaths *paths,
                                              const RayTracingDetachedJobRecord *record);
bool ray_tracing_job_runner_write_pid_file(const char *path, pid_t pid);
bool ray_tracing_job_runner_read_job_pid(const RayTracingDetachedJobPaths *paths, pid_t *out_pid);
bool ray_tracing_job_runner_print_file_to_stream(FILE *out, const char *path);
bool ray_tracing_job_runner_json_get_string(json_object *owner,
                                            const char *key,
                                            const char **out_value);
bool ray_tracing_job_runner_json_get_int(json_object *owner,
                                         const char *key,
                                         int *out_value);
bool ray_tracing_job_runner_load_job_status_record(const RayTracingDetachedJobPaths *paths,
                                                   RayTracingDetachedJobRecord *out_record);
bool ray_tracing_job_runner_merge_progress_into_record(const char *progress_path,
                                                       RayTracingDetachedJobRecord *record);
bool ray_tracing_job_runner_pid_is_alive(pid_t pid);
bool ray_tracing_job_runner_parse_utc_timestamp(const char *text, time_t *out_time);
bool ray_tracing_job_runner_refresh_job_status_record(const RayTracingDetachedJobPaths *paths,
                                                      RayTracingDetachedJobRecord *record);

#endif
