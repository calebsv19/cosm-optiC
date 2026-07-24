#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_job_runner_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app/ray_tracing_recovery_authority.h"
#include "app/ray_tracing_sha256.h"
#include "app/ray_tracing_temporal_checkpoint.h"

static bool job_status_requires_reconciliation(const char *status_path) {
    json_object *root = NULL;
    const char *state = NULL;
    bool requires_reconciliation = false;
    if (!status_path) return false;
    root = json_object_from_file(status_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return true;
    }
    if (!ray_tracing_job_runner_json_get_string(root, "state", &state)) {
        json_object_put(root);
        return true;
    }
    requires_reconciliation =
        strcmp(state, "running") == 0 ||
        strcmp(state, "cancelling") == 0 ||
        strcmp(state, "interrupted") == 0;
    json_object_put(root);
    return requires_reconciliation;
}

static bool recovery_reference_for_record(
    const RayTracingDetachedJobRecord *record,
    RayTracingRecoveryDescriptor *descriptor) {
    char frame_path[PATH_MAX];
    char checkpoint_root[PATH_MAX];
    int completed_subpasses = 0;
    int active_subpass = 0;
    size_t completed_tiles = 0u;
    if (!record || !descriptor) return false;
    if (record->durable_frames_completed > 0) {
        const int frame_index =
            record->requested_start_frame + record->durable_frames_completed - 1;
        if (!ray_tracing_job_runner_build_frame_path(record->output_root,
                                                     frame_index,
                                                     frame_path,
                                                     sizeof(frame_path)) ||
            !ray_tracing_sha256_file(frame_path, descriptor->checkpoint_sha256)) {
            return false;
        }
        snprintf(descriptor->checkpoint_kind,
                 sizeof(descriptor->checkpoint_kind),
                 "%s",
                 "completed_frame");
        snprintf(descriptor->checkpoint_path,
                 sizeof(descriptor->checkpoint_path),
                 "%s",
                 frame_path);
        return true;
    }
    if (snprintf(checkpoint_root,
                 sizeof(checkpoint_root),
                 "%s/checkpoints",
                 record->output_root) < (int)sizeof(checkpoint_root) &&
        ray_tracing_temporal_checkpoint_latest_reference(
            checkpoint_root,
            record->resume_from_frame,
            descriptor->checkpoint_path,
            sizeof(descriptor->checkpoint_path),
            descriptor->checkpoint_sha256,
            &completed_subpasses,
            &active_subpass,
            &completed_tiles)) {
        snprintf(descriptor->checkpoint_kind,
                 sizeof(descriptor->checkpoint_kind),
                 "%s",
                 "tile_batch");
        return true;
    }
    snprintf(descriptor->checkpoint_kind,
             sizeof(descriptor->checkpoint_kind),
             "%s",
             "empty_prefix");
    snprintf(descriptor->checkpoint_path,
             sizeof(descriptor->checkpoint_path),
             "%s",
             record->request_path);
    return ray_tracing_sha256_file(record->request_path,
                                   descriptor->checkpoint_sha256);
}

bool ray_tracing_job_runner_write_recovery_descriptor(
    const RayTracingDetachedJobPaths *paths,
    const RayTracingDetachedJobRecord *record) {
    RayTracingRecoveryDescriptor descriptor;
    if (!paths || !record || strcmp(record->state, "interrupted") != 0) return false;
    memset(&descriptor, 0, sizeof(descriptor));
    snprintf(descriptor.job_id, sizeof(descriptor.job_id), "%s", record->job_id);
    snprintf(descriptor.recovery_state,
             sizeof(descriptor.recovery_state),
             "%s",
             record->resume_available ? "resumable" : "recovery_required");
    snprintf(descriptor.output_root,
             sizeof(descriptor.output_root),
             "%s",
             record->output_root);
    descriptor.resume_from_frame = record->resume_from_frame;
    descriptor.durable_frames_completed = record->durable_frames_completed;
    descriptor.resume_available = record->resume_available;
    if (ray_tracing_sha256_is_valid_hex(record->immutable_request_sha256)) {
        snprintf(descriptor.request_sha256,
                 sizeof(descriptor.request_sha256),
                 "%s",
                 record->immutable_request_sha256);
    } else if (!ray_tracing_sha256_file(record->request_path,
                                        descriptor.request_sha256)) {
        return false;
    }
    if (descriptor.resume_available &&
        !recovery_reference_for_record(record, &descriptor)) {
        descriptor.resume_available = false;
        snprintf(descriptor.recovery_state,
                 sizeof(descriptor.recovery_state),
                 "%s",
                 "recovery_required");
    }
    return ray_tracing_recovery_descriptor_write(paths->recovery_descriptor_path,
                                                  &descriptor);
}

bool ray_tracing_job_runner_reconcile(const char *argv0,
                                      const char *jobs_root_override,
                                      size_t *out_jobs_scanned,
                                      size_t *out_recovery_descriptors,
                                      char *out_diagnostics,
                                      size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    DIR *directory = NULL;
    struct dirent *entry = NULL;
    size_t jobs_scanned = 0u;
    size_t descriptors = 0u;
    bool ok = true;
    if (out_jobs_scanned) *out_jobs_scanned = 0u;
    if (out_recovery_descriptors) *out_recovery_descriptors = 0u;
    if (!argv0 ||
        !ray_tracing_job_runner_build_jobs_root(argv0,
                                                jobs_root_override,
                                                jobs_root,
                                                sizeof(jobs_root))) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s",
                     "failed to resolve jobs root");
        }
        return false;
    }
    directory = opendir(jobs_root);
    if (!directory) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s",
                     "jobs root not present; nothing to reconcile");
        }
        return true;
    }
    while ((entry = readdir(directory)) != NULL) {
        RayTracingDetachedJobPaths paths;
        RayTracingDetachedJobRecord record;
        struct stat status;
        if (!ray_tracing_job_runner_validate_job_id(entry->d_name) ||
            !ray_tracing_job_runner_build_job_paths(jobs_root, entry->d_name, &paths) ||
            lstat(paths.job_root, &status) != 0 || !S_ISDIR(status.st_mode) ||
            !ray_tracing_job_runner_file_exists(paths.job_status_path) ||
            !job_status_requires_reconciliation(paths.job_status_path)) {
            continue;
        }
        jobs_scanned += 1u;
        if (!ray_tracing_job_runner_load_job_status_record(&paths, &record) ||
            !ray_tracing_job_runner_refresh_job_status_record(&paths, &record)) {
            ok = false;
            continue;
        }
        if (strcmp(record.state, "interrupted") == 0) {
            if (ray_tracing_job_runner_write_recovery_descriptor(&paths, &record)) {
                descriptors += 1u;
            } else {
                ok = false;
            }
        }
    }
    closedir(directory);
    if (out_jobs_scanned) *out_jobs_scanned = jobs_scanned;
    if (out_recovery_descriptors) *out_recovery_descriptors = descriptors;
    if (out_diagnostics && out_diagnostics_size > 0u) {
        snprintf(out_diagnostics,
                 out_diagnostics_size,
                 ok ? "ok" : "one or more jobs could not be reconciled");
    }
    return ok;
}
