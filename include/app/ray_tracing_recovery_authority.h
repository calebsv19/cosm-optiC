#ifndef RAY_TRACING_RECOVERY_AUTHORITY_H
#define RAY_TRACING_RECOVERY_AUTHORITY_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/ray_tracing_sha256.h"

#define RAY_TRACING_RECOVERY_DESCRIPTOR_SCHEMA "ray_tracing_recovery_descriptor_v1"
#define RAY_TRACING_RECOVERY_AUTHORITY_SCHEMA "ray_tracing_resume_authority_v1"
#define RAY_TRACING_OUTPUT_FENCE_SCHEMA "ray_tracing_output_fence_v1"

typedef struct RayTracingRecoveryDescriptor {
    char job_id[96];
    char recovery_state[32];
    char request_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char checkpoint_kind[32];
    char checkpoint_path[PATH_MAX];
    char checkpoint_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char output_root[PATH_MAX];
    int resume_from_frame;
    int durable_frames_completed;
    bool resume_available;
} RayTracingRecoveryDescriptor;

typedef struct RayTracingResumeAuthority {
    char token_id[96];
    char source_job_id[96];
    char worker_id[96];
    char lease_id[96];
    uint64_t lease_generation;
    uint64_t output_generation;
    char request_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char checkpoint_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char fence_path[PATH_MAX];
    char receipt_path[PATH_MAX];
    char expires_at_utc[32];
} RayTracingResumeAuthority;

typedef struct RayTracingOutputFence {
    char token_id[96];
    char worker_id[96];
    char lease_id[96];
    uint64_t lease_generation;
    uint64_t output_generation;
    bool active;
} RayTracingOutputFence;

bool ray_tracing_recovery_descriptor_write(
    const char *path,
    const RayTracingRecoveryDescriptor *descriptor);
bool ray_tracing_recovery_descriptor_load(
    const char *path,
    RayTracingRecoveryDescriptor *descriptor,
    char *diagnostics,
    size_t diagnostics_size);

bool ray_tracing_resume_authority_load(
    const char *path,
    RayTracingResumeAuthority *authority,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_resume_authority_validate(
    const RayTracingResumeAuthority *authority,
    const RayTracingRecoveryDescriptor *descriptor,
    const char *worker_id,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_resume_authority_consume(
    const RayTracingResumeAuthority *authority,
    const char *receipt_path,
    char *diagnostics,
    size_t diagnostics_size);

bool ray_tracing_output_fence_write(
    const char *path,
    const RayTracingOutputFence *fence);
bool ray_tracing_output_fence_load(
    const char *path,
    RayTracingOutputFence *fence,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_output_fence_validate(
    const char *path,
    const RayTracingResumeAuthority *authority,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_output_fence_validate_environment(void);

#endif
