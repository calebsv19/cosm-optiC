#ifndef RAY_TRACING_WORKER_CLIENT_H
#define RAY_TRACING_WORKER_CLIENT_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "app/ray_tracing_sha256.h"

typedef struct RayTracingWorkerClientSpawnRequest {
    const char *job_id;
    const char *worker_runtime_path;
    const char *render_cli_path;
    const char *canonical_request_path;
    const char *capabilities_path;
    const char *worker_request_path;
    const char *event_directory;
    const char *cancellation_path;
    const char *recovery_descriptor_path;
    const char *resume_authority_path;
    const char *resume_receipt_path;
    const char *recovery_worker_id;
    const char *output_root;
    const char *progress_path;
    const char *job_status_path;
    const char *result_summary_path;
    const char *stdout_log_path;
    const char *stderr_log_path;
    int width;
    int height;
    int start_frame;
    int frame_count;
    int temporal_frames;
    bool force_direct_fallback;
} RayTracingWorkerClientSpawnRequest;

typedef struct RayTracingWorkerClientSpawnResult {
    pid_t pid;
    int protocol_version;
    char execution_mode[32];
    char request_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char renderer_build_sha256[RAY_TRACING_SHA256_HEX_SIZE];
} RayTracingWorkerClientSpawnResult;

bool ray_tracing_worker_client_spawn(
    const RayTracingWorkerClientSpawnRequest *request,
    RayTracingWorkerClientSpawnResult *result,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_worker_client_request_cancel(const char *job_id,
                                              const char *cancellation_path,
                                              pid_t pid,
                                              char *diagnostics,
                                              size_t diagnostics_size);

#endif
