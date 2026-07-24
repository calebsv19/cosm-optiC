#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_worker_protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <json-c/json.h>

#include "app/ray_tracing_durable_io.h"

static void set_diag(char *diagnostics, size_t size, const char *message) {
    if (!diagnostics || size == 0u) return;
    snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool string_copy(char *destination, size_t size, const char *source) {
    if (!destination || size == 0u || !source || strlen(source) >= size) return false;
    memcpy(destination, source, strlen(source) + 1u);
    return true;
}

static bool json_get_string(json_object *root,
                            const char *key,
                            char *destination,
                            size_t size) {
    json_object *value = NULL;
    const char *text = NULL;
    if (!json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return false;
    }
    text = json_object_get_string(value);
    return text && string_copy(destination, size, text);
}

static bool json_get_int(json_object *root, const char *key, int *out_value) {
    json_object *value = NULL;
    if (!out_value || !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static bool json_get_u64(json_object *root, const char *key, uint64_t *out_value) {
    json_object *value = NULL;
    int64_t parsed = 0;
    if (!out_value || !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0) return false;
    *out_value = (uint64_t)parsed;
    return true;
}

static bool durable_write_json(const char *path, json_object *root) {
    RayTracingDurableOutput output;
    const char *text = NULL;
    if (!path || !root) return false;
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    if (!text || !ray_tracing_durable_output_begin(&output, path)) return false;
    if (fputs(text, output.stream) < 0 || fputc('\n', output.stream) == EOF) {
        ray_tracing_durable_output_abort(&output);
        return false;
    }
    return ray_tracing_durable_output_commit(&output);
}

static bool ensure_directory(const char *path) {
    char buffer[PATH_MAX];
    size_t length = 0u;
    if (!path || !path[0]) return false;
    length = strlen(path);
    if (length >= sizeof(buffer)) return false;
    memcpy(buffer, path, length + 1u);
    for (size_t i = 1u; i < length; ++i) {
        if (buffer[i] != '/') continue;
        buffer[i] = '\0';
        if (mkdir(buffer, 0700) != 0 && errno != EEXIST) return false;
        buffer[i] = '/';
    }
    return mkdir(buffer, 0700) == 0 || errno == EEXIST;
}

const char *ray_tracing_worker_message_type_label(RayTracingWorkerMessageType type) {
    switch (type) {
        case RAY_TRACING_WORKER_MESSAGE_REQUEST: return "request";
        case RAY_TRACING_WORKER_MESSAGE_CAPABILITIES: return "capabilities";
        case RAY_TRACING_WORKER_MESSAGE_PROGRESS: return "progress";
        case RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION: return "dirty_region";
        case RAY_TRACING_WORKER_MESSAGE_CANCELLATION: return "cancellation";
        case RAY_TRACING_WORKER_MESSAGE_COMPLETION: return "completion";
        case RAY_TRACING_WORKER_MESSAGE_INTERRUPTION: return "interruption";
        case RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE:
            return "checkpoint_reference";
        default: return "invalid";
    }
}

RayTracingWorkerMessageType ray_tracing_worker_message_type_from_label(const char *label) {
    if (!label) return RAY_TRACING_WORKER_MESSAGE_INVALID;
    for (int value = (int)RAY_TRACING_WORKER_MESSAGE_REQUEST;
         value <= (int)RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE;
         ++value) {
        if (strcmp(label,
                   ray_tracing_worker_message_type_label(
                       (RayTracingWorkerMessageType)value)) == 0) {
            return (RayTracingWorkerMessageType)value;
        }
    }
    return RAY_TRACING_WORKER_MESSAGE_INVALID;
}

void ray_tracing_worker_capabilities_defaults(RayTracingWorkerCapabilities *capabilities) {
    if (!capabilities) return;
    memset(capabilities, 0, sizeof(*capabilities));
    capabilities->protocol_min = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    capabilities->protocol_max = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    capabilities->checkpoint_schema_min = RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MIN;
    capabilities->checkpoint_schema_max = RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MAX;
    capabilities->capability_bits = RAY_TRACING_WORKER_REQUIRED_CAPABILITIES;
    string_copy(capabilities->worker_runtime_version,
                sizeof(capabilities->worker_runtime_version),
                RAY_TRACING_WORKER_RUNTIME_VERSION);
}

bool ray_tracing_worker_capabilities_validate(
    const RayTracingWorkerCapabilities *capabilities,
    char *diagnostics,
    size_t diagnostics_size) {
    if (!capabilities || capabilities->protocol_min <= 0 ||
        capabilities->protocol_max < capabilities->protocol_min ||
        capabilities->checkpoint_schema_min < 0 ||
        capabilities->checkpoint_schema_max < capabilities->checkpoint_schema_min ||
        !capabilities->worker_runtime_version[0] ||
        !ray_tracing_sha256_is_valid_hex(capabilities->renderer_build_sha256)) {
        set_diag(diagnostics, diagnostics_size, "invalid worker capabilities");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_worker_capabilities_negotiate(
    const RayTracingWorkerCapabilities *capabilities,
    int requested_protocol,
    uint32_t required_capability_bits,
    char *diagnostics,
    size_t diagnostics_size) {
    if (!ray_tracing_worker_capabilities_validate(capabilities,
                                                  diagnostics,
                                                  diagnostics_size)) {
        return false;
    }
    if (requested_protocol < RAY_TRACING_DESKTOP_WORKER_PROTOCOL_MIN ||
        requested_protocol > RAY_TRACING_DESKTOP_WORKER_PROTOCOL_MAX ||
        requested_protocol < capabilities->protocol_min ||
        requested_protocol > capabilities->protocol_max) {
        set_diag(diagnostics, diagnostics_size, "worker protocol version is incompatible");
        return false;
    }
    if (capabilities->checkpoint_schema_max <
            RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MIN ||
        capabilities->checkpoint_schema_min >
            RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MAX) {
        set_diag(diagnostics,
                 diagnostics_size,
                 "worker checkpoint schema range is incompatible");
        return false;
    }
    if ((capabilities->capability_bits & required_capability_bits) !=
        required_capability_bits) {
        set_diag(diagnostics, diagnostics_size, "worker is missing required capabilities");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

static json_object *capabilities_to_json(const RayTracingWorkerCapabilities *capabilities) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    json_object_object_add(root, "schema",
                           json_object_new_string(RAY_TRACING_WORKER_PROTOCOL_SCHEMA));
    json_object_object_add(root, "message_type", json_object_new_string("capabilities"));
    json_object_object_add(root, "protocol_min",
                           json_object_new_int(capabilities->protocol_min));
    json_object_object_add(root, "protocol_max",
                           json_object_new_int(capabilities->protocol_max));
    json_object_object_add(root, "checkpoint_schema_min",
                           json_object_new_int(capabilities->checkpoint_schema_min));
    json_object_object_add(root, "checkpoint_schema_max",
                           json_object_new_int(capabilities->checkpoint_schema_max));
    json_object_object_add(root, "capability_bits",
                           json_object_new_int64((int64_t)capabilities->capability_bits));
    json_object_object_add(root, "worker_runtime_version",
                           json_object_new_string(capabilities->worker_runtime_version));
    json_object_object_add(root, "renderer_build_sha256",
                           json_object_new_string(capabilities->renderer_build_sha256));
    return root;
}

bool ray_tracing_worker_capabilities_write_file(
    const char *path,
    const RayTracingWorkerCapabilities *capabilities) {
    json_object *root = NULL;
    char diagnostics[128];
    bool written = false;
    if (!path || !ray_tracing_worker_capabilities_validate(capabilities,
                                                            diagnostics,
                                                            sizeof(diagnostics))) {
        return false;
    }
    root = capabilities_to_json(capabilities);
    if (!root) return false;
    written = durable_write_json(path, root);
    json_object_put(root);
    return written;
}

bool ray_tracing_worker_capabilities_load_file(
    const char *path,
    RayTracingWorkerCapabilities *capabilities,
    char *diagnostics,
    size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    char type[32];
    int capability_bits = 0;
    if (!path || !capabilities) return false;
    memset(capabilities, 0, sizeof(*capabilities));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_WORKER_PROTOCOL_SCHEMA) != 0 ||
        !json_get_string(root, "message_type", type, sizeof(type)) ||
        strcmp(type, "capabilities") != 0 ||
        !json_get_int(root, "protocol_min", &capabilities->protocol_min) ||
        !json_get_int(root, "protocol_max", &capabilities->protocol_max) ||
        !json_get_int(root,
                      "checkpoint_schema_min",
                      &capabilities->checkpoint_schema_min) ||
        !json_get_int(root,
                      "checkpoint_schema_max",
                      &capabilities->checkpoint_schema_max) ||
        !json_get_int(root, "capability_bits", &capability_bits) ||
        !json_get_string(root,
                         "worker_runtime_version",
                         capabilities->worker_runtime_version,
                         sizeof(capabilities->worker_runtime_version)) ||
        !json_get_string(root,
                         "renderer_build_sha256",
                         capabilities->renderer_build_sha256,
                         sizeof(capabilities->renderer_build_sha256))) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid capabilities message");
        return false;
    }
    capabilities->capability_bits = (uint32_t)capability_bits;
    json_object_put(root);
    return ray_tracing_worker_capabilities_validate(capabilities,
                                                    diagnostics,
                                                    diagnostics_size);
}

bool ray_tracing_worker_request_validate(const RayTracingWorkerRequest *request,
                                         char *diagnostics,
                                         size_t diagnostics_size) {
    if (!request || request->protocol_version != RAY_TRACING_WORKER_PROTOCOL_VERSION ||
        !request->job_id[0] || !request->request_path[0] ||
        !ray_tracing_sha256_is_valid_hex(request->request_sha256) ||
        !request->render_cli_path[0] ||
        !ray_tracing_sha256_is_valid_hex(request->renderer_build_sha256) ||
        !request->output_root[0] || !request->progress_path[0] ||
        !request->job_status_path[0] || !request->result_summary_path[0] ||
        !request->event_directory[0] || !request->cancellation_path[0] ||
        (request->recovery_authorized &&
         (!request->recovery_descriptor_path[0] ||
          !request->resume_authority_path[0] ||
          !request->recovery_worker_id[0])) ||
        request->width <= 0 || request->height <= 0 ||
        request->start_frame < 0 || request->frame_count <= 0 ||
        request->temporal_frames <= 0 ||
        (request->required_capability_bits & RAY_TRACING_WORKER_REQUIRED_CAPABILITIES) !=
            RAY_TRACING_WORKER_REQUIRED_CAPABILITIES) {
        set_diag(diagnostics, diagnostics_size, "invalid worker request");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

static json_object *request_to_json(const RayTracingWorkerRequest *request) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    json_object_object_add(root, "schema",
                           json_object_new_string(RAY_TRACING_WORKER_PROTOCOL_SCHEMA));
    json_object_object_add(root, "message_type", json_object_new_string("request"));
    json_object_object_add(root, "protocol_version",
                           json_object_new_int(request->protocol_version));
    json_object_object_add(root, "required_capability_bits",
                           json_object_new_int64((int64_t)request->required_capability_bits));
    json_object_object_add(root, "sequence",
                           json_object_new_int64((int64_t)request->sequence));
    json_object_object_add(root, "job_id", json_object_new_string(request->job_id));
    json_object_object_add(root, "request_path",
                           json_object_new_string(request->request_path));
    json_object_object_add(root, "request_sha256",
                           json_object_new_string(request->request_sha256));
    json_object_object_add(root, "render_cli_path",
                           json_object_new_string(request->render_cli_path));
    json_object_object_add(root, "renderer_build_sha256",
                           json_object_new_string(request->renderer_build_sha256));
    json_object_object_add(root, "output_root",
                           json_object_new_string(request->output_root));
    json_object_object_add(root, "progress_path",
                           json_object_new_string(request->progress_path));
    json_object_object_add(root, "job_status_path",
                           json_object_new_string(request->job_status_path));
    json_object_object_add(root, "result_summary_path",
                           json_object_new_string(request->result_summary_path));
    json_object_object_add(root, "event_directory",
                           json_object_new_string(request->event_directory));
    json_object_object_add(root, "cancellation_path",
                           json_object_new_string(request->cancellation_path));
    json_object_object_add(root, "recovery_authorized",
                           json_object_new_boolean(request->recovery_authorized));
    json_object_object_add(root, "recovery_descriptor_path",
                           json_object_new_string(request->recovery_descriptor_path));
    json_object_object_add(root, "resume_authority_path",
                           json_object_new_string(request->resume_authority_path));
    json_object_object_add(root, "resume_receipt_path",
                           json_object_new_string(request->resume_receipt_path));
    json_object_object_add(root, "recovery_worker_id",
                           json_object_new_string(request->recovery_worker_id));
    json_object_object_add(root, "width", json_object_new_int(request->width));
    json_object_object_add(root, "height", json_object_new_int(request->height));
    json_object_object_add(root, "start_frame",
                           json_object_new_int(request->start_frame));
    json_object_object_add(root, "frame_count",
                           json_object_new_int(request->frame_count));
    json_object_object_add(root, "temporal_frames",
                           json_object_new_int(request->temporal_frames));
    return root;
}

bool ray_tracing_worker_request_write_file(const char *path,
                                           const RayTracingWorkerRequest *request) {
    json_object *root = NULL;
    char diagnostics[128];
    bool written = false;
    if (!path ||
        !ray_tracing_worker_request_validate(request, diagnostics, sizeof(diagnostics))) {
        return false;
    }
    root = request_to_json(request);
    if (!root) return false;
    written = durable_write_json(path, root);
    json_object_put(root);
    return written;
}

bool ray_tracing_worker_request_load_file(const char *path,
                                          RayTracingWorkerRequest *request,
                                          char *diagnostics,
                                          size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    char type[32];
    int required_bits = 0;
    json_object *recovery_value = NULL;
    if (!path || !request) return false;
    memset(request, 0, sizeof(*request));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_WORKER_PROTOCOL_SCHEMA) != 0 ||
        !json_get_string(root, "message_type", type, sizeof(type)) ||
        strcmp(type, "request") != 0 ||
        !json_get_int(root, "protocol_version", &request->protocol_version) ||
        !json_get_int(root, "required_capability_bits", &required_bits) ||
        !json_get_u64(root, "sequence", &request->sequence) ||
        !json_get_string(root, "job_id", request->job_id, sizeof(request->job_id)) ||
        !json_get_string(root,
                         "request_path",
                         request->request_path,
                         sizeof(request->request_path)) ||
        !json_get_string(root,
                         "request_sha256",
                         request->request_sha256,
                         sizeof(request->request_sha256)) ||
        !json_get_string(root,
                         "render_cli_path",
                         request->render_cli_path,
                         sizeof(request->render_cli_path)) ||
        !json_get_string(root,
                         "renderer_build_sha256",
                         request->renderer_build_sha256,
                         sizeof(request->renderer_build_sha256)) ||
        !json_get_string(root,
                         "output_root",
                         request->output_root,
                         sizeof(request->output_root)) ||
        !json_get_string(root,
                         "progress_path",
                         request->progress_path,
                         sizeof(request->progress_path)) ||
        !json_get_string(root,
                         "job_status_path",
                         request->job_status_path,
                         sizeof(request->job_status_path)) ||
        !json_get_string(root,
                         "result_summary_path",
                         request->result_summary_path,
                         sizeof(request->result_summary_path)) ||
        !json_get_string(root,
                         "event_directory",
                         request->event_directory,
                         sizeof(request->event_directory)) ||
        !json_get_string(root,
                         "cancellation_path",
                         request->cancellation_path,
                         sizeof(request->cancellation_path)) ||
        !json_get_int(root, "width", &request->width) ||
        !json_get_int(root, "height", &request->height) ||
        !json_get_int(root, "start_frame", &request->start_frame) ||
        !json_get_int(root, "frame_count", &request->frame_count) ||
        !json_get_int(root, "temporal_frames", &request->temporal_frames)) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid worker request message");
        return false;
    }
    if (json_object_object_get_ex(root, "recovery_authorized", &recovery_value) &&
        json_object_is_type(recovery_value, json_type_boolean)) {
        request->recovery_authorized = json_object_get_boolean(recovery_value) != 0;
    }
    if (request->recovery_authorized &&
        (!json_get_string(root,
                          "recovery_descriptor_path",
                          request->recovery_descriptor_path,
                          sizeof(request->recovery_descriptor_path)) ||
         !json_get_string(root,
                          "resume_authority_path",
                          request->resume_authority_path,
                          sizeof(request->resume_authority_path)) ||
         !json_get_string(root,
                          "recovery_worker_id",
                          request->recovery_worker_id,
                          sizeof(request->recovery_worker_id)))) {
        json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid recovery authority request");
        return false;
    }
    request->required_capability_bits = (uint32_t)required_bits;
    json_object_put(root);
    return ray_tracing_worker_request_validate(request, diagnostics, diagnostics_size);
}

bool ray_tracing_worker_event_validate(const RayTracingWorkerEvent *event,
                                       char *diagnostics,
                                       size_t diagnostics_size) {
    bool type_valid = false;
    if (!event || event->protocol_version != RAY_TRACING_WORKER_PROTOCOL_VERSION ||
        event->type < RAY_TRACING_WORKER_MESSAGE_PROGRESS ||
        event->type > RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE ||
        !event->job_id[0]) {
        set_diag(diagnostics, diagnostics_size, "invalid worker event envelope");
        return false;
    }
    switch (event->type) {
        case RAY_TRACING_WORKER_MESSAGE_PROGRESS:
            type_valid = event->frames_completed >= 0 &&
                         event->temporal_subpasses_completed >= 0 &&
                         event->temporal_subpasses_total >= 0;
            break;
        case RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION:
            type_valid = event->frame_index >= 0 && event->region_x >= 0 &&
                         event->region_y >= 0 && event->region_width > 0 &&
                         event->region_height > 0;
            break;
        case RAY_TRACING_WORKER_MESSAGE_CANCELLATION:
            type_valid = event->state[0] && event->diagnostics[0];
            break;
        case RAY_TRACING_WORKER_MESSAGE_COMPLETION:
            type_valid = event->exit_code == 0 && event->state[0] &&
                         ray_tracing_sha256_is_valid_hex(event->summary_sha256);
            break;
        case RAY_TRACING_WORKER_MESSAGE_INTERRUPTION:
            type_valid = event->exit_code != 0 && event->state[0] &&
                         event->diagnostics[0];
            break;
        case RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE:
            type_valid = event->frame_index >= 0 && event->reference_path[0] &&
                         ray_tracing_sha256_is_valid_hex(event->reference_sha256);
            break;
        default:
            type_valid = false;
            break;
    }
    set_diag(diagnostics, diagnostics_size, type_valid ? "ok" : "invalid worker event payload");
    return type_valid;
}

static json_object *event_to_json(const RayTracingWorkerEvent *event) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    json_object_object_add(root, "schema",
                           json_object_new_string(RAY_TRACING_WORKER_PROTOCOL_SCHEMA));
    json_object_object_add(root, "message_type",
                           json_object_new_string(
                               ray_tracing_worker_message_type_label(event->type)));
    json_object_object_add(root, "protocol_version",
                           json_object_new_int(event->protocol_version));
    json_object_object_add(root, "sequence",
                           json_object_new_int64((int64_t)event->sequence));
    json_object_object_add(root, "job_id", json_object_new_string(event->job_id));
    json_object_object_add(root, "state", json_object_new_string(event->state));
    json_object_object_add(root, "diagnostics",
                           json_object_new_string(event->diagnostics));
    json_object_object_add(root, "exit_code", json_object_new_int(event->exit_code));
    json_object_object_add(root, "frame_index", json_object_new_int(event->frame_index));
    json_object_object_add(root, "frames_completed",
                           json_object_new_int(event->frames_completed));
    json_object_object_add(root, "temporal_subpasses_completed",
                           json_object_new_int(event->temporal_subpasses_completed));
    json_object_object_add(root, "temporal_subpasses_total",
                           json_object_new_int(event->temporal_subpasses_total));
    json_object_object_add(root, "region_x", json_object_new_int(event->region_x));
    json_object_object_add(root, "region_y", json_object_new_int(event->region_y));
    json_object_object_add(root, "region_width",
                           json_object_new_int(event->region_width));
    json_object_object_add(root, "region_height",
                           json_object_new_int(event->region_height));
    json_object_object_add(root, "reference_path",
                           json_object_new_string(event->reference_path));
    json_object_object_add(root, "reference_sha256",
                           json_object_new_string(event->reference_sha256));
    json_object_object_add(root, "summary_sha256",
                           json_object_new_string(event->summary_sha256));
    return root;
}

bool ray_tracing_worker_event_write(const char *event_directory,
                                    const RayTracingWorkerEvent *event,
                                    char *out_path,
                                    size_t out_path_size) {
    json_object *root = NULL;
    char path[PATH_MAX];
    char diagnostics[128];
    bool written = false;
    if (!event_directory || !ensure_directory(event_directory) ||
        !ray_tracing_worker_event_validate(event, diagnostics, sizeof(diagnostics)) ||
        snprintf(path,
                 sizeof(path),
                 "%s/%08llu_%s.json",
                 event_directory,
                 (unsigned long long)event->sequence,
                 ray_tracing_worker_message_type_label(event->type)) >=
            (int)sizeof(path)) {
        return false;
    }
    {
        struct stat existing;
        if (lstat(path, &existing) == 0) return false;
    }
    root = event_to_json(event);
    if (!root) return false;
    written = durable_write_json(path, root);
    json_object_put(root);
    if (written && out_path && out_path_size > 0u) {
        written = string_copy(out_path, out_path_size, path);
    }
    return written;
}

bool ray_tracing_worker_event_load_file(const char *path,
                                        RayTracingWorkerEvent *event,
                                        char *diagnostics,
                                        size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    char type[32];
    if (!path || !event) return false;
    memset(event, 0, sizeof(*event));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_WORKER_PROTOCOL_SCHEMA) != 0 ||
        !json_get_string(root, "message_type", type, sizeof(type)) ||
        !json_get_int(root, "protocol_version", &event->protocol_version) ||
        !json_get_u64(root, "sequence", &event->sequence) ||
        !json_get_string(root, "job_id", event->job_id, sizeof(event->job_id)) ||
        !json_get_string(root, "state", event->state, sizeof(event->state)) ||
        !json_get_string(root,
                         "diagnostics",
                         event->diagnostics,
                         sizeof(event->diagnostics)) ||
        !json_get_int(root, "exit_code", &event->exit_code) ||
        !json_get_int(root, "frame_index", &event->frame_index) ||
        !json_get_int(root, "frames_completed", &event->frames_completed) ||
        !json_get_int(root,
                      "temporal_subpasses_completed",
                      &event->temporal_subpasses_completed) ||
        !json_get_int(root,
                      "temporal_subpasses_total",
                      &event->temporal_subpasses_total) ||
        !json_get_int(root, "region_x", &event->region_x) ||
        !json_get_int(root, "region_y", &event->region_y) ||
        !json_get_int(root, "region_width", &event->region_width) ||
        !json_get_int(root, "region_height", &event->region_height) ||
        !json_get_string(root,
                         "reference_path",
                         event->reference_path,
                         sizeof(event->reference_path)) ||
        !json_get_string(root,
                         "reference_sha256",
                         event->reference_sha256,
                         sizeof(event->reference_sha256)) ||
        !json_get_string(root,
                         "summary_sha256",
                         event->summary_sha256,
                         sizeof(event->summary_sha256))) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid worker event message");
        return false;
    }
    event->type = ray_tracing_worker_message_type_from_label(type);
    json_object_put(root);
    return ray_tracing_worker_event_validate(event, diagnostics, diagnostics_size);
}

bool ray_tracing_worker_cancellation_write_file(const char *path,
                                                const char *job_id,
                                                const char *reason) {
    RayTracingWorkerEvent event;
    json_object *root = NULL;
    char diagnostics[128];
    bool written = false;
    if (!path || !job_id || !job_id[0]) return false;
    memset(&event, 0, sizeof(event));
    event.protocol_version = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    event.type = RAY_TRACING_WORKER_MESSAGE_CANCELLATION;
    event.sequence = 0u;
    event.exit_code = -1;
    event.frame_index = -1;
    snprintf(event.job_id, sizeof(event.job_id), "%s", job_id);
    snprintf(event.state, sizeof(event.state), "%s", "cancel_requested");
    snprintf(event.diagnostics,
             sizeof(event.diagnostics),
             "%s",
             reason && reason[0] ? reason : "client requested cancellation");
    if (!ray_tracing_worker_event_validate(&event, diagnostics, sizeof(diagnostics))) {
        return false;
    }
    root = event_to_json(&event);
    if (!root) return false;
    written = durable_write_json(path, root);
    json_object_put(root);
    return written;
}

bool ray_tracing_worker_cancellation_load_file(const char *path,
                                               RayTracingWorkerEvent *event,
                                               char *diagnostics,
                                               size_t diagnostics_size) {
    if (!ray_tracing_worker_event_load_file(path,
                                            event,
                                            diagnostics,
                                            diagnostics_size)) {
        return false;
    }
    if (event->type != RAY_TRACING_WORKER_MESSAGE_CANCELLATION) {
        set_diag(diagnostics, diagnostics_size, "message is not cancellation");
        return false;
    }
    return true;
}
