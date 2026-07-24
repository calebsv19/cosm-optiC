#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/ray_tracing_durable_io.h"
#include "app/ray_tracing_recovery_authority.h"
#include "app/ray_tracing_sha256.h"

static int failures = 0;

static void expect_true(const char *label, bool value) {
    if (!value) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures += 1;
    }
}

static bool write_text(const char *path, const char *text) {
    RayTracingDurableOutput output;
    if (!ray_tracing_durable_output_begin(&output, path)) return false;
    if (fputs(text, output.stream) < 0) {
        ray_tracing_durable_output_abort(&output);
        return false;
    }
    return ray_tracing_durable_output_commit(&output);
}

int main(void) {
    char root_template[] = "/tmp/ray_tracing_recovery_authority_XXXXXX";
    char *root = mkdtemp(root_template);
    char descriptor_path[PATH_MAX];
    char authority_path[PATH_MAX];
    char receipt_path[PATH_MAX];
    char fence_path[PATH_MAX];
    char protected_path[PATH_MAX];
    char digest[RAY_TRACING_SHA256_HEX_SIZE];
    char diagnostics[256];
    char authority_json[2048];
    RayTracingRecoveryDescriptor descriptor;
    RayTracingRecoveryDescriptor loaded_descriptor;
    RayTracingResumeAuthority authority;
    RayTracingOutputFence fence;
    if (!root) return 1;
    expect_true("fixture digest", ray_tracing_sha256_bytes("phase-e", 7u, digest));
    snprintf(descriptor_path, sizeof(descriptor_path), "%s/recovery_descriptor.json", root);
    snprintf(authority_path, sizeof(authority_path), "%s/resume_authority.json", root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/resume_receipt.json", root);
    snprintf(fence_path, sizeof(fence_path), "%s/output_fence.json", root);
    snprintf(protected_path, sizeof(protected_path), "%s/protected.json", root);

    memset(&descriptor, 0, sizeof(descriptor));
    snprintf(descriptor.job_id, sizeof(descriptor.job_id), "%s", "source_job");
    snprintf(descriptor.recovery_state, sizeof(descriptor.recovery_state), "%s", "resumable");
    snprintf(descriptor.request_sha256, sizeof(descriptor.request_sha256), "%s", digest);
    snprintf(descriptor.checkpoint_kind, sizeof(descriptor.checkpoint_kind), "%s", "completed_frame");
    snprintf(descriptor.checkpoint_path,
             sizeof(descriptor.checkpoint_path),
             "%s/frame_0002.bmp",
             root);
    snprintf(descriptor.checkpoint_sha256, sizeof(descriptor.checkpoint_sha256), "%s", digest);
    snprintf(descriptor.output_root, sizeof(descriptor.output_root), "%s/output", root);
    descriptor.resume_from_frame = 3;
    descriptor.durable_frames_completed = 3;
    descriptor.resume_available = true;
    expect_true("descriptor write",
                ray_tracing_recovery_descriptor_write(descriptor_path, &descriptor));
    expect_true("descriptor exact roundtrip",
                ray_tracing_recovery_descriptor_load(descriptor_path,
                                                     &loaded_descriptor,
                                                     diagnostics,
                                                     sizeof(diagnostics)) &&
                    strcmp(loaded_descriptor.request_sha256, digest) == 0 &&
                    strcmp(loaded_descriptor.checkpoint_sha256, digest) == 0);

    snprintf(authority_json,
             sizeof(authority_json),
             "{\n"
             " \"schema\":\"%s\",\n"
             " \"token_id\":\"token_a\",\n"
             " \"source_job_id\":\"source_job\",\n"
             " \"worker_id\":\"worker_a\",\n"
             " \"lease_id\":\"lease_a\",\n"
             " \"lease_generation\":7,\n"
             " \"output_generation\":11,\n"
             " \"request_sha256\":\"%s\",\n"
             " \"checkpoint_sha256\":\"%s\",\n"
             " \"fence_path\":\"%s\",\n"
             " \"receipt_path\":\"%s\",\n"
             " \"expires_at_utc\":\"2999-01-01T00:00:00Z\"\n"
             "}\n",
             RAY_TRACING_RECOVERY_AUTHORITY_SCHEMA,
             digest,
             digest,
             fence_path,
             receipt_path);
    expect_true("authority fixture write", write_text(authority_path, authority_json));
    expect_true("authority load",
                ray_tracing_resume_authority_load(authority_path,
                                                  &authority,
                                                  diagnostics,
                                                  sizeof(diagnostics)));
    expect_true("exact binding accepted",
                ray_tracing_resume_authority_validate(&authority,
                                                      &loaded_descriptor,
                                                      "worker_a",
                                                      diagnostics,
                                                      sizeof(diagnostics)));
    loaded_descriptor.checkpoint_sha256[0] =
        loaded_descriptor.checkpoint_sha256[0] == '0' ? '1' : '0';
    expect_true("checkpoint digest drift rejected",
                !ray_tracing_resume_authority_validate(&authority,
                                                       &loaded_descriptor,
                                                       "worker_a",
                                                       diagnostics,
                                                       sizeof(diagnostics)));
    loaded_descriptor = descriptor;
    expect_true("duplicate host rejected",
                !ray_tracing_resume_authority_validate(&authority,
                                                       &loaded_descriptor,
                                                       "worker_b",
                                                       diagnostics,
                                                       sizeof(diagnostics)));
    expect_true("token consumes once",
                ray_tracing_resume_authority_consume(&authority,
                                                     NULL,
                                                     diagnostics,
                                                     sizeof(diagnostics)));
    expect_true("token replay rejected",
                !ray_tracing_resume_authority_consume(&authority,
                                                      NULL,
                                                      diagnostics,
                                                      sizeof(diagnostics)));

    memset(&fence, 0, sizeof(fence));
    snprintf(fence.token_id, sizeof(fence.token_id), "%s", authority.token_id);
    snprintf(fence.worker_id, sizeof(fence.worker_id), "%s", authority.worker_id);
    snprintf(fence.lease_id, sizeof(fence.lease_id), "%s", authority.lease_id);
    fence.lease_generation = authority.lease_generation;
    fence.output_generation = authority.output_generation;
    fence.active = true;
    expect_true("fence write", ray_tracing_output_fence_write(fence_path, &fence));
    expect_true("matching fence accepted",
                ray_tracing_output_fence_validate(fence_path,
                                                  &authority,
                                                  diagnostics,
                                                  sizeof(diagnostics)));

    expect_true("set fence environment",
                setenv("RAY_TRACING_OUTPUT_FENCE_PATH", fence_path, 1) == 0 &&
                    setenv("RAY_TRACING_RESUME_TOKEN_ID", authority.token_id, 1) == 0 &&
                    setenv("RAY_TRACING_WORKER_ID", authority.worker_id, 1) == 0 &&
                    setenv("RAY_TRACING_LEASE_ID", authority.lease_id, 1) == 0 &&
                    setenv("RAY_TRACING_LEASE_GENERATION", "7", 1) == 0 &&
                    setenv("RAY_TRACING_OUTPUT_GENERATION", "11", 1) == 0);
    expect_true("current holder durable write accepted",
                write_text(protected_path, "generation 11\n"));

    fence.active = false;
    unsetenv("RAY_TRACING_OUTPUT_FENCE_PATH");
    expect_true("lease revocation fence write",
                ray_tracing_output_fence_write(fence_path, &fence));
    setenv("RAY_TRACING_OUTPUT_FENCE_PATH", fence_path, 1);
    expect_true("stale holder durable write rejected",
                !write_text(protected_path, "stale generation 11\n"));

    unsetenv("RAY_TRACING_OUTPUT_FENCE_PATH");
    fence.active = true;
    snprintf(fence.token_id, sizeof(fence.token_id), "%s", "token_b");
    snprintf(fence.worker_id, sizeof(fence.worker_id), "%s", "worker_b");
    snprintf(fence.lease_id, sizeof(fence.lease_id), "%s", "lease_b");
    fence.lease_generation = 8u;
    fence.output_generation = 12u;
    expect_true("reassignment advances generation",
                ray_tracing_output_fence_write(fence_path, &fence));
    setenv("RAY_TRACING_OUTPUT_FENCE_PATH", fence_path, 1);
    expect_true("old generation rejected after reassignment",
                !ray_tracing_output_fence_validate_environment());
    setenv("RAY_TRACING_RESUME_TOKEN_ID", "token_b", 1);
    setenv("RAY_TRACING_WORKER_ID", "worker_b", 1);
    setenv("RAY_TRACING_LEASE_ID", "lease_b", 1);
    setenv("RAY_TRACING_LEASE_GENERATION", "8", 1);
    setenv("RAY_TRACING_OUTPUT_GENERATION", "12", 1);
    expect_true("new generation accepted after reassignment",
                ray_tracing_output_fence_validate_environment() &&
                    write_text(protected_path, "generation 12\n"));

    unsetenv("RAY_TRACING_OUTPUT_FENCE_PATH");
    unlink(protected_path);
    unlink(fence_path);
    unlink(receipt_path);
    unlink(authority_path);
    unlink(descriptor_path);
    rmdir(root);
    if (failures != 0) {
        fprintf(stderr, "%d recovery authority assertion(s) failed\n", failures);
        return 1;
    }
    puts("ray tracing recovery authority contract passed");
    return 0;
}
