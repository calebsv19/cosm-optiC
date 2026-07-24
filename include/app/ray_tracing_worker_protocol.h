#ifndef RAY_TRACING_WORKER_PROTOCOL_H
#define RAY_TRACING_WORKER_PROTOCOL_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/ray_tracing_sha256.h"
#include "app/ray_tracing_worker_version.h"

#define RAY_TRACING_WORKER_PROTOCOL_SCHEMA "ray_tracing_worker_protocol"
#define RAY_TRACING_WORKER_PROTOCOL_VERSION 1
#define RAY_TRACING_DESKTOP_WORKER_PROTOCOL_MIN 1
#define RAY_TRACING_DESKTOP_WORKER_PROTOCOL_MAX 1
#define RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MIN 2
#define RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MAX 2
#define RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MIN 2
#define RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MAX 2

typedef enum RayTracingWorkerMessageType {
    RAY_TRACING_WORKER_MESSAGE_INVALID = 0,
    RAY_TRACING_WORKER_MESSAGE_REQUEST,
    RAY_TRACING_WORKER_MESSAGE_CAPABILITIES,
    RAY_TRACING_WORKER_MESSAGE_PROGRESS,
    RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION,
    RAY_TRACING_WORKER_MESSAGE_CANCELLATION,
    RAY_TRACING_WORKER_MESSAGE_COMPLETION,
    RAY_TRACING_WORKER_MESSAGE_INTERRUPTION,
    RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE
} RayTracingWorkerMessageType;

enum {
    RAY_TRACING_WORKER_CAP_PROGRESS = 1u << 0,
    RAY_TRACING_WORKER_CAP_DIRTY_REGION = 1u << 1,
    RAY_TRACING_WORKER_CAP_CANCELLATION = 1u << 2,
    RAY_TRACING_WORKER_CAP_COMPLETION = 1u << 3,
    RAY_TRACING_WORKER_CAP_INTERRUPTION = 1u << 4,
    RAY_TRACING_WORKER_CAP_CHECKPOINT_REFERENCE = 1u << 5,
    RAY_TRACING_WORKER_CAP_RECOVERY_FENCE = 1u << 6
};

#define RAY_TRACING_WORKER_REQUIRED_CAPABILITIES \
    (RAY_TRACING_WORKER_CAP_PROGRESS | RAY_TRACING_WORKER_CAP_DIRTY_REGION | \
     RAY_TRACING_WORKER_CAP_CANCELLATION | RAY_TRACING_WORKER_CAP_COMPLETION | \
     RAY_TRACING_WORKER_CAP_INTERRUPTION | \
     RAY_TRACING_WORKER_CAP_CHECKPOINT_REFERENCE | \
     RAY_TRACING_WORKER_CAP_RECOVERY_FENCE)

typedef struct RayTracingWorkerCapabilities {
    int protocol_min;
    int protocol_max;
    int checkpoint_schema_min;
    int checkpoint_schema_max;
    uint32_t capability_bits;
    char worker_runtime_version[32];
    char renderer_build_sha256[RAY_TRACING_SHA256_HEX_SIZE];
} RayTracingWorkerCapabilities;

typedef struct RayTracingWorkerRequest {
    int protocol_version;
    uint32_t required_capability_bits;
    uint64_t sequence;
    char job_id[96];
    char request_path[PATH_MAX];
    char request_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char render_cli_path[PATH_MAX];
    char renderer_build_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char output_root[PATH_MAX];
    char progress_path[PATH_MAX];
    char job_status_path[PATH_MAX];
    char result_summary_path[PATH_MAX];
    char event_directory[PATH_MAX];
    char cancellation_path[PATH_MAX];
    bool recovery_authorized;
    char recovery_descriptor_path[PATH_MAX];
    char resume_authority_path[PATH_MAX];
    char resume_receipt_path[PATH_MAX];
    char recovery_worker_id[96];
    int width;
    int height;
    int start_frame;
    int frame_count;
    int temporal_frames;
} RayTracingWorkerRequest;

typedef struct RayTracingWorkerEvent {
    int protocol_version;
    RayTracingWorkerMessageType type;
    uint64_t sequence;
    char job_id[96];
    char state[32];
    char diagnostics[256];
    int exit_code;
    int frame_index;
    int frames_completed;
    int temporal_subpasses_completed;
    int temporal_subpasses_total;
    int region_x;
    int region_y;
    int region_width;
    int region_height;
    char reference_path[PATH_MAX];
    char reference_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char summary_sha256[RAY_TRACING_SHA256_HEX_SIZE];
} RayTracingWorkerEvent;

const char *ray_tracing_worker_message_type_label(RayTracingWorkerMessageType type);
RayTracingWorkerMessageType ray_tracing_worker_message_type_from_label(const char *label);

void ray_tracing_worker_capabilities_defaults(RayTracingWorkerCapabilities *capabilities);
bool ray_tracing_worker_capabilities_validate(
    const RayTracingWorkerCapabilities *capabilities,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_worker_capabilities_negotiate(
    const RayTracingWorkerCapabilities *capabilities,
    int requested_protocol,
    uint32_t required_capability_bits,
    char *diagnostics,
    size_t diagnostics_size);
bool ray_tracing_worker_capabilities_write_file(
    const char *path,
    const RayTracingWorkerCapabilities *capabilities);
bool ray_tracing_worker_capabilities_load_file(
    const char *path,
    RayTracingWorkerCapabilities *capabilities,
    char *diagnostics,
    size_t diagnostics_size);

bool ray_tracing_worker_request_validate(const RayTracingWorkerRequest *request,
                                         char *diagnostics,
                                         size_t diagnostics_size);
bool ray_tracing_worker_request_write_file(const char *path,
                                           const RayTracingWorkerRequest *request);
bool ray_tracing_worker_request_load_file(const char *path,
                                          RayTracingWorkerRequest *request,
                                          char *diagnostics,
                                          size_t diagnostics_size);

bool ray_tracing_worker_event_validate(const RayTracingWorkerEvent *event,
                                       char *diagnostics,
                                       size_t diagnostics_size);
bool ray_tracing_worker_event_write(const char *event_directory,
                                    const RayTracingWorkerEvent *event,
                                    char *out_path,
                                    size_t out_path_size);
bool ray_tracing_worker_event_load_file(const char *path,
                                        RayTracingWorkerEvent *event,
                                        char *diagnostics,
                                        size_t diagnostics_size);
bool ray_tracing_worker_cancellation_write_file(const char *path,
                                                const char *job_id,
                                                const char *reason);
bool ray_tracing_worker_cancellation_load_file(const char *path,
                                               RayTracingWorkerEvent *event,
                                               char *diagnostics,
                                               size_t diagnostics_size);

#endif
