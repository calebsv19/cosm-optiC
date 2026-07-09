#ifndef RAY_TRACING_JOB_RUNNER_H
#define RAY_TRACING_JOB_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define RAY_TRACING_DETACHED_JOB_STATUS_SCHEMA "ray_tracing_detached_job_status_v1"

bool ray_tracing_job_runner_default_jobs_root(const char *argv0,
                                              char *out_path,
                                              size_t out_path_size);
bool ray_tracing_job_runner_submit(const char *argv0,
                                   const char *request_path,
                                   const char *jobs_root_override,
                                   bool overwrite,
                                   bool resume,
                                   char *out_job_id,
                                   size_t out_job_id_size,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size);
bool ray_tracing_job_runner_print_status(FILE *out,
                                         const char *argv0,
                                         const char *job_id,
                                         const char *jobs_root_override,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size);
bool ray_tracing_job_runner_cancel(const char *argv0,
                                   const char *job_id,
                                   const char *jobs_root_override,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size);

#endif
