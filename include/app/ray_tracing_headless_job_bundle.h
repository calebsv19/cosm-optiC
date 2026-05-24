#ifndef RAY_TRACING_HEADLESS_JOB_BUNDLE_H
#define RAY_TRACING_HEADLESS_JOB_BUNDLE_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "core_headless_job.h"

typedef struct RayTracingHeadlessJobBundle {
    CoreHeadlessJobEnvelope envelope;
    char bundle_path[PATH_MAX];
    char bundle_dir[PATH_MAX];
    char resolved_scene_payload_path[PATH_MAX];
    char resolved_run_config_path[PATH_MAX];
    char resolved_report_path[PATH_MAX];
    char resolved_logs_dir[PATH_MAX];
    char resolved_artifacts_dir[PATH_MAX];
} RayTracingHeadlessJobBundle;

bool ray_tracing_headless_job_bundle_load(const char *job_json_path,
                                          RayTracingHeadlessJobBundle *out_bundle,
                                          char *out_diagnostics,
                                          size_t out_diagnostics_size);
bool ray_tracing_headless_job_bundle_write(const char *job_json_path,
                                           const CoreHeadlessJobEnvelope *envelope,
                                           char *out_diagnostics,
                                           size_t out_diagnostics_size);
bool ray_tracing_headless_job_report_write(const char *report_path,
                                           const CoreHeadlessJobReport *report,
                                           const CoreHeadlessJobArtifact *artifacts,
                                           size_t artifact_count,
                                           char *out_diagnostics,
                                           size_t out_diagnostics_size);

#endif
