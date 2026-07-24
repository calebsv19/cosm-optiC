#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_recovery_authority.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/ray_tracing_durable_io.h"

static void set_diag(char *diagnostics, size_t size, const char *message) {
    if (!diagnostics || size == 0u) return;
    snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool valid_identifier(const char *value) {
    if (!value || !value[0]) return false;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; ++cursor) {
        if ((*cursor >= 'a' && *cursor <= 'z') ||
            (*cursor >= 'A' && *cursor <= 'Z') ||
            (*cursor >= '0' && *cursor <= '9') ||
            *cursor == '_' || *cursor == '-' || *cursor == '.') {
            continue;
        }
        return false;
    }
    return true;
}

static bool get_string(json_object *root,
                       const char *key,
                       char *out,
                       size_t out_size) {
    json_object *value = NULL;
    const char *text = NULL;
    if (!root || !key || !out || out_size == 0u ||
        !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return false;
    }
    text = json_object_get_string(value);
    return text && snprintf(out, out_size, "%s", text) < (int)out_size;
}

static bool get_string_alias(json_object *root,
                             const char *primary,
                             const char *alias,
                             char *out,
                             size_t out_size) {
    return get_string(root, primary, out, out_size) ||
           (alias && get_string(root, alias, out, out_size));
}

static bool parent_directory(const char *path, char *out, size_t out_size) {
    const char *slash = NULL;
    size_t length = 0u;
    if (!path || !path[0] || !out || out_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return snprintf(out, out_size, "%s", ".") < (int)out_size;
    length = (size_t)(slash - path);
    if (length == 0u) return snprintf(out, out_size, "%s", "/") < (int)out_size;
    if (length >= out_size) return false;
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static bool resolve_artifact_path(json_object *root,
                                  const char *authority_path,
                                  const char *absolute_key,
                                  const char *relative_key,
                                  char *out,
                                  size_t out_size) {
    char relative[PATH_MAX];
    char directory[PATH_MAX];
    if (get_string(root, absolute_key, out, out_size)) return true;
    if (!get_string(root, relative_key, relative, sizeof(relative)) ||
        !relative[0] || relative[0] == '/' || strstr(relative, "..") ||
        !parent_directory(authority_path, directory, sizeof(directory))) {
        return false;
    }
    return snprintf(out, out_size, "%s/%s", directory, relative) < (int)out_size;
}

static bool get_u64(json_object *root, const char *key, uint64_t *out) {
    json_object *value = NULL;
    int64_t number = 0;
    if (!root || !key || !out ||
        !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    number = json_object_get_int64(value);
    if (number <= 0) return false;
    *out = (uint64_t)number;
    return true;
}

static bool get_int(json_object *root, const char *key, int *out) {
    json_object *value = NULL;
    if (!root || !key || !out ||
        !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out = json_object_get_int(value);
    return true;
}

static bool get_bool(json_object *root, const char *key, bool *out) {
    json_object *value = NULL;
    if (!root || !key || !out ||
        !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out = json_object_get_boolean(value) != 0;
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

static bool descriptor_valid(const RayTracingRecoveryDescriptor *descriptor) {
    if (!descriptor || !valid_identifier(descriptor->job_id) ||
        (strcmp(descriptor->recovery_state, "interrupted") != 0 &&
         strcmp(descriptor->recovery_state, "resumable") != 0 &&
         strcmp(descriptor->recovery_state, "recovery_required") != 0) ||
        !ray_tracing_sha256_is_valid_hex(descriptor->request_sha256) ||
        !descriptor->output_root[0] || descriptor->resume_from_frame < 0 ||
        descriptor->durable_frames_completed < 0) {
        return false;
    }
    if (descriptor->resume_available) {
        return descriptor->checkpoint_kind[0] &&
               descriptor->checkpoint_path[0] &&
               ray_tracing_sha256_is_valid_hex(descriptor->checkpoint_sha256) &&
               strcmp(descriptor->recovery_state, "resumable") == 0;
    }
    return strcmp(descriptor->recovery_state, "resumable") != 0;
}

bool ray_tracing_recovery_descriptor_write(
    const char *path,
    const RayTracingRecoveryDescriptor *descriptor) {
    json_object *root = NULL;
    bool ok = false;
    if (!path || !descriptor_valid(descriptor)) return false;
    root = json_object_new_object();
    if (!root) return false;
    json_object_object_add(root, "schema",
                           json_object_new_string(RAY_TRACING_RECOVERY_DESCRIPTOR_SCHEMA));
    json_object_object_add(root, "job_id", json_object_new_string(descriptor->job_id));
    json_object_object_add(root, "recovery_state",
                           json_object_new_string(descriptor->recovery_state));
    json_object_object_add(root, "request_sha256",
                           json_object_new_string(descriptor->request_sha256));
    json_object_object_add(root, "request_digest",
                           json_object_new_string(descriptor->request_sha256));
    json_object_object_add(root, "checkpoint_kind",
                           json_object_new_string(descriptor->checkpoint_kind));
    json_object_object_add(root, "checkpoint_path",
                           json_object_new_string(descriptor->checkpoint_path));
    json_object_object_add(root, "checkpoint_sha256",
                           json_object_new_string(descriptor->checkpoint_sha256));
    json_object_object_add(root, "checkpoint_digest",
                           json_object_new_string(descriptor->checkpoint_sha256));
    {
        const char *relative = descriptor->checkpoint_path;
        const size_t output_length = strlen(descriptor->output_root);
        if (strncmp(descriptor->checkpoint_path,
                    descriptor->output_root,
                    output_length) == 0 &&
            descriptor->checkpoint_path[output_length] == '/') {
            relative = descriptor->checkpoint_path + output_length + 1u;
        } else if (strcmp(descriptor->checkpoint_kind, "empty_prefix") == 0) {
            relative = "empty_prefix";
        }
        json_object_object_add(root, "checkpoint_relpath",
                               json_object_new_string(relative));
    }
    json_object_object_add(root, "output_root",
                           json_object_new_string(descriptor->output_root));
    json_object_object_add(root, "resume_from_frame",
                           json_object_new_int(descriptor->resume_from_frame));
    json_object_object_add(root, "durable_frames_completed",
                           json_object_new_int(descriptor->durable_frames_completed));
    json_object_object_add(root, "resume_available",
                           json_object_new_boolean(descriptor->resume_available));
    ok = durable_write_json(path, root);
    json_object_put(root);
    return ok;
}

bool ray_tracing_recovery_descriptor_load(
    const char *path,
    RayTracingRecoveryDescriptor *descriptor,
    char *diagnostics,
    size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    if (!path || !descriptor) return false;
    memset(descriptor, 0, sizeof(*descriptor));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_RECOVERY_DESCRIPTOR_SCHEMA) != 0 ||
        !get_string(root, "job_id", descriptor->job_id, sizeof(descriptor->job_id)) ||
        !get_string(root, "recovery_state",
                    descriptor->recovery_state, sizeof(descriptor->recovery_state)) ||
        !get_string_alias(root, "request_sha256", "request_digest",
                    descriptor->request_sha256, sizeof(descriptor->request_sha256)) ||
        !get_string(root, "checkpoint_kind",
                    descriptor->checkpoint_kind, sizeof(descriptor->checkpoint_kind)) ||
        !get_string(root, "checkpoint_path",
                    descriptor->checkpoint_path, sizeof(descriptor->checkpoint_path)) ||
        !get_string_alias(root, "checkpoint_sha256", "checkpoint_digest",
                    descriptor->checkpoint_sha256, sizeof(descriptor->checkpoint_sha256)) ||
        !get_string(root, "output_root",
                    descriptor->output_root, sizeof(descriptor->output_root)) ||
        !get_int(root, "resume_from_frame", &descriptor->resume_from_frame) ||
        !get_int(root, "durable_frames_completed",
                 &descriptor->durable_frames_completed) ||
        !get_bool(root, "resume_available", &descriptor->resume_available) ||
        !descriptor_valid(descriptor)) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid recovery descriptor");
        return false;
    }
    json_object_put(root);
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_resume_authority_load(
    const char *path,
    RayTracingResumeAuthority *authority,
    char *diagnostics,
    size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    if (!path || !authority) return false;
    memset(authority, 0, sizeof(*authority));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_RECOVERY_AUTHORITY_SCHEMA) != 0 ||
        !get_string(root, "token_id", authority->token_id, sizeof(authority->token_id)) ||
        !get_string_alias(root, "source_job_id", "job_id",
                    authority->source_job_id, sizeof(authority->source_job_id)) ||
        !get_string(root, "worker_id", authority->worker_id, sizeof(authority->worker_id)) ||
        !get_string(root, "lease_id", authority->lease_id, sizeof(authority->lease_id)) ||
        (!get_u64(root, "lease_generation", &authority->lease_generation) &&
         !get_u64(root, "output_generation", &authority->lease_generation)) ||
        !get_u64(root, "output_generation", &authority->output_generation) ||
        !get_string_alias(root, "request_sha256", "request_digest",
                    authority->request_sha256, sizeof(authority->request_sha256)) ||
        !get_string_alias(root, "checkpoint_sha256", "checkpoint_digest",
                    authority->checkpoint_sha256, sizeof(authority->checkpoint_sha256)) ||
        !resolve_artifact_path(root,
                               path,
                               "fence_path",
                               "fence_relpath",
                               authority->fence_path,
                               sizeof(authority->fence_path)) ||
        !resolve_artifact_path(root,
                               path,
                               "receipt_path",
                               "receipt_relpath",
                               authority->receipt_path,
                               sizeof(authority->receipt_path)) ||
        !get_string_alias(root, "expires_at_utc", "expires_at",
                    authority->expires_at_utc, sizeof(authority->expires_at_utc))) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid resume authority");
        return false;
    }
    json_object_put(root);
    if (!valid_identifier(authority->token_id) ||
        !valid_identifier(authority->source_job_id) ||
        !valid_identifier(authority->worker_id) ||
        !valid_identifier(authority->lease_id) ||
        !ray_tracing_sha256_is_valid_hex(authority->request_sha256) ||
        !ray_tracing_sha256_is_valid_hex(authority->checkpoint_sha256) ||
        !authority->fence_path[0] || !authority->receipt_path[0] ||
        !authority->expires_at_utc[0]) {
        set_diag(diagnostics, diagnostics_size, "invalid resume authority fields");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

static bool authority_not_expired(const char *expires_at_utc) {
    struct tm parsed;
    time_t expiry = 0;
    time_t now = time(NULL);
    char *end = NULL;
    if (!expires_at_utc || !expires_at_utc[0] || now == (time_t)-1) return false;
    memset(&parsed, 0, sizeof(parsed));
    end = strptime(expires_at_utc, "%Y-%m-%dT%H:%M:%SZ", &parsed);
    if (!end || *end != '\0') return false;
#if defined(__APPLE__) || defined(__unix__)
    expiry = timegm(&parsed);
#else
    expiry = mktime(&parsed);
#endif
    return expiry != (time_t)-1 && now < expiry;
}

bool ray_tracing_resume_authority_validate(
    const RayTracingResumeAuthority *authority,
    const RayTracingRecoveryDescriptor *descriptor,
    const char *worker_id,
    char *diagnostics,
    size_t diagnostics_size) {
    if (!authority || !descriptor || !worker_id ||
        !descriptor_valid(descriptor) || !descriptor->resume_available ||
        strcmp(authority->source_job_id, descriptor->job_id) != 0 ||
        strcmp(authority->worker_id, worker_id) != 0 ||
        strcmp(authority->request_sha256, descriptor->request_sha256) != 0 ||
        strcmp(authority->checkpoint_sha256, descriptor->checkpoint_sha256) != 0 ||
        !authority_not_expired(authority->expires_at_utc)) {
        set_diag(diagnostics, diagnostics_size, "resume authority binding rejected");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_resume_authority_consume(
    const RayTracingResumeAuthority *authority,
    const char *receipt_path,
    char *diagnostics,
    size_t diagnostics_size) {
    int fd = -1;
    char receipt[512];
    ssize_t length = 0;
    bool ok = true;
    if (!authority) return false;
    if (!receipt_path || !receipt_path[0]) receipt_path = authority->receipt_path;
    if (!receipt_path || !receipt_path[0]) return false;
    fd = open(receipt_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        set_diag(diagnostics, diagnostics_size,
                 errno == EEXIST ? "resume authority already consumed"
                                  : "failed to create authority receipt");
        return false;
    }
    length = snprintf(receipt,
                      sizeof(receipt),
                      "{\"schema\":\"ray_tracing_resume_receipt_v1\","
                      "\"token_id\":\"%s\",\"output_generation\":%llu}\n",
                      authority->token_id,
                      (unsigned long long)authority->output_generation);
    if (length <= 0 || length >= (ssize_t)sizeof(receipt) ||
        write(fd, receipt, (size_t)length) != length || fsync(fd) != 0) {
        ok = false;
    }
    if (close(fd) != 0) ok = false;
    if (!ok) (void)unlink(receipt_path);
    set_diag(diagnostics, diagnostics_size, ok ? "ok" : "failed to persist authority receipt");
    return ok;
}

bool ray_tracing_output_fence_write(
    const char *path,
    const RayTracingOutputFence *fence) {
    json_object *root = NULL;
    bool ok = false;
    if (!path || !fence || !valid_identifier(fence->token_id) ||
        !valid_identifier(fence->worker_id) || !valid_identifier(fence->lease_id) ||
        fence->lease_generation == 0u || fence->output_generation == 0u) {
        return false;
    }
    root = json_object_new_object();
    if (!root) return false;
    json_object_object_add(root, "schema",
                           json_object_new_string(RAY_TRACING_OUTPUT_FENCE_SCHEMA));
    json_object_object_add(root, "token_id", json_object_new_string(fence->token_id));
    json_object_object_add(root, "worker_id", json_object_new_string(fence->worker_id));
    json_object_object_add(root, "lease_id", json_object_new_string(fence->lease_id));
    json_object_object_add(root, "lease_generation",
                           json_object_new_int64((int64_t)fence->lease_generation));
    json_object_object_add(root, "output_generation",
                           json_object_new_int64((int64_t)fence->output_generation));
    json_object_object_add(root, "active", json_object_new_boolean(fence->active));
    ok = durable_write_json(path, root);
    json_object_put(root);
    return ok;
}

bool ray_tracing_output_fence_load(
    const char *path,
    RayTracingOutputFence *fence,
    char *diagnostics,
    size_t diagnostics_size) {
    json_object *root = NULL;
    char schema[64];
    if (!path || !fence) return false;
    memset(fence, 0, sizeof(*fence));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !get_string(root, "schema", schema, sizeof(schema)) ||
        strcmp(schema, RAY_TRACING_OUTPUT_FENCE_SCHEMA) != 0 ||
        !get_string(root, "token_id", fence->token_id, sizeof(fence->token_id)) ||
        !get_string(root, "worker_id", fence->worker_id, sizeof(fence->worker_id)) ||
        !get_string(root, "lease_id", fence->lease_id, sizeof(fence->lease_id)) ||
        !get_u64(root, "lease_generation", &fence->lease_generation) ||
        !get_u64(root, "output_generation", &fence->output_generation) ||
        !get_bool(root, "active", &fence->active)) {
        if (root) json_object_put(root);
        set_diag(diagnostics, diagnostics_size, "invalid output fence");
        return false;
    }
    json_object_put(root);
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_output_fence_validate(
    const char *path,
    const RayTracingResumeAuthority *authority,
    char *diagnostics,
    size_t diagnostics_size) {
    RayTracingOutputFence fence;
    if (!authority ||
        !ray_tracing_output_fence_load(path, &fence, diagnostics, diagnostics_size) ||
        !fence.active ||
        strcmp(fence.token_id, authority->token_id) != 0 ||
        strcmp(fence.worker_id, authority->worker_id) != 0 ||
        strcmp(fence.lease_id, authority->lease_id) != 0 ||
        fence.lease_generation != authority->lease_generation ||
        fence.output_generation != authority->output_generation) {
        set_diag(diagnostics, diagnostics_size, "output ownership fence rejected");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_output_fence_validate_environment(void) {
    const char *path = getenv("RAY_TRACING_OUTPUT_FENCE_PATH");
    const char *token = getenv("RAY_TRACING_RESUME_TOKEN_ID");
    const char *worker = getenv("RAY_TRACING_WORKER_ID");
    const char *lease = getenv("RAY_TRACING_LEASE_ID");
    const char *lease_generation = getenv("RAY_TRACING_LEASE_GENERATION");
    const char *output_generation = getenv("RAY_TRACING_OUTPUT_GENERATION");
    RayTracingResumeAuthority authority;
    char *end = NULL;
    unsigned long long parsed = 0u;
    if (!path || !path[0]) return true;
    if (!token || !worker || !lease || !lease_generation || !output_generation) return false;
    memset(&authority, 0, sizeof(authority));
    snprintf(authority.token_id, sizeof(authority.token_id), "%s", token);
    snprintf(authority.worker_id, sizeof(authority.worker_id), "%s", worker);
    snprintf(authority.lease_id, sizeof(authority.lease_id), "%s", lease);
    errno = 0;
    parsed = strtoull(lease_generation, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed == 0u) return false;
    authority.lease_generation = (uint64_t)parsed;
    errno = 0;
    parsed = strtoull(output_generation, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed == 0u) return false;
    authority.output_generation = (uint64_t)parsed;
    return ray_tracing_output_fence_validate(path, &authority, NULL, 0u);
}
