#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/ray_tracing_sha256.h"
#include "app/ray_tracing_worker_protocol.h"

static int failures = 0;

static void expect_true(const char *label, bool value) {
    if (!value) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures += 1;
    }
}

static void fill_request(RayTracingWorkerRequest *request,
                         const char *root,
                         const char *digest) {
    memset(request, 0, sizeof(*request));
    request->protocol_version = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    request->required_capability_bits = RAY_TRACING_WORKER_REQUIRED_CAPABILITIES;
    request->sequence = 7u;
    request->width = 32;
    request->height = 24;
    request->start_frame = 3;
    request->frame_count = 2;
    request->temporal_frames = 4;
    snprintf(request->job_id, sizeof(request->job_id), "%s", "protocol_fixture");
    snprintf(request->request_path, sizeof(request->request_path), "%s/request.json", root);
    snprintf(request->request_sha256, sizeof(request->request_sha256), "%s", digest);
    snprintf(request->render_cli_path, sizeof(request->render_cli_path), "%s/render", root);
    snprintf(request->renderer_build_sha256,
             sizeof(request->renderer_build_sha256),
             "%s",
             digest);
    snprintf(request->output_root, sizeof(request->output_root), "%s/output", root);
    snprintf(request->progress_path, sizeof(request->progress_path), "%s/progress.json", root);
    snprintf(request->job_status_path, sizeof(request->job_status_path), "%s/status.json", root);
    snprintf(request->result_summary_path,
             sizeof(request->result_summary_path),
             "%s/summary.json",
             root);
    snprintf(request->event_directory, sizeof(request->event_directory), "%s/events", root);
    snprintf(request->cancellation_path,
             sizeof(request->cancellation_path),
             "%s/cancel.json",
             root);
}

static void fill_event(RayTracingWorkerEvent *event,
                       RayTracingWorkerMessageType type,
                       uint64_t sequence,
                       const char *digest) {
    memset(event, 0, sizeof(*event));
    event->protocol_version = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    event->type = type;
    event->sequence = sequence;
    event->exit_code = -1;
    event->frame_index = 3;
    event->frames_completed = 1;
    event->temporal_subpasses_completed = 2;
    event->temporal_subpasses_total = 4;
    snprintf(event->job_id, sizeof(event->job_id), "%s", "protocol_fixture");
    snprintf(event->state, sizeof(event->state), "%s", "running");
    snprintf(event->diagnostics, sizeof(event->diagnostics), "%s", "fixture");
    if (type == RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION) {
        event->region_width = 32;
        event->region_height = 24;
    } else if (type == RAY_TRACING_WORKER_MESSAGE_COMPLETION) {
        event->exit_code = 0;
        snprintf(event->state, sizeof(event->state), "%s", "completed");
        snprintf(event->summary_sha256, sizeof(event->summary_sha256), "%s", digest);
    } else if (type == RAY_TRACING_WORKER_MESSAGE_INTERRUPTION) {
        event->exit_code = 19;
        snprintf(event->state, sizeof(event->state), "%s", "interrupted");
    } else if (type == RAY_TRACING_WORKER_MESSAGE_CANCELLATION) {
        snprintf(event->state, sizeof(event->state), "%s", "cancel_requested");
    } else if (type == RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE) {
        snprintf(event->state, sizeof(event->state), "%s", "completed_frame");
        snprintf(event->reference_path,
                 sizeof(event->reference_path),
                 "%s",
                 "/tmp/frame_0003.bmp");
        snprintf(event->reference_sha256, sizeof(event->reference_sha256), "%s", digest);
    }
}

int main(void) {
    char root_template[] = "/tmp/ray_tracing_worker_protocol_XXXXXX";
    char *root = mkdtemp(root_template);
    char digest[RAY_TRACING_SHA256_HEX_SIZE];
    char path[PATH_MAX];
    char event_path[PATH_MAX];
    char diagnostics[256];
    RayTracingWorkerCapabilities capabilities;
    RayTracingWorkerCapabilities loaded_capabilities;
    RayTracingWorkerRequest request;
    RayTracingWorkerRequest loaded_request;
    const RayTracingWorkerMessageType event_types[] = {
        RAY_TRACING_WORKER_MESSAGE_PROGRESS,
        RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION,
        RAY_TRACING_WORKER_MESSAGE_CANCELLATION,
        RAY_TRACING_WORKER_MESSAGE_COMPLETION,
        RAY_TRACING_WORKER_MESSAGE_INTERRUPTION,
        RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE
    };
    if (!root) return 1;

    expect_true("sha256 known vector",
                ray_tracing_sha256_bytes("abc", 3u, digest) &&
                    strcmp(digest,
                           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") ==
                        0);
    ray_tracing_worker_capabilities_defaults(&capabilities);
    snprintf(capabilities.renderer_build_sha256,
             sizeof(capabilities.renderer_build_sha256),
             "%s",
             digest);
    expect_true("capabilities validate",
                ray_tracing_worker_capabilities_validate(&capabilities,
                                                         diagnostics,
                                                         sizeof(diagnostics)));
    expect_true("capabilities negotiate",
                ray_tracing_worker_capabilities_negotiate(
                    &capabilities,
                    RAY_TRACING_WORKER_PROTOCOL_VERSION,
                    RAY_TRACING_WORKER_REQUIRED_CAPABILITIES,
                    diagnostics,
                    sizeof(diagnostics)));
    expect_true("runtime version comes from canonical worker version",
                strcmp(capabilities.worker_runtime_version,
                       RAY_TRACING_WORKER_RUNTIME_VERSION) == 0);
    capabilities.checkpoint_schema_min =
        RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MAX + 1;
    capabilities.checkpoint_schema_max =
        RAY_TRACING_DESKTOP_CHECKPOINT_SCHEMA_MAX + 1;
    expect_true("incompatible checkpoint schema fails closed",
                !ray_tracing_worker_capabilities_negotiate(
                    &capabilities,
                    RAY_TRACING_WORKER_PROTOCOL_VERSION,
                    RAY_TRACING_WORKER_REQUIRED_CAPABILITIES,
                    diagnostics,
                    sizeof(diagnostics)));
    capabilities.checkpoint_schema_min = RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MIN;
    capabilities.checkpoint_schema_max = RAY_TRACING_WORKER_CHECKPOINT_SCHEMA_MAX;
    expect_true("future protocol fails closed",
                !ray_tracing_worker_capabilities_negotiate(
                    &capabilities,
                    RAY_TRACING_WORKER_PROTOCOL_VERSION + 1,
                    RAY_TRACING_WORKER_REQUIRED_CAPABILITIES,
                    diagnostics,
                    sizeof(diagnostics)));
    capabilities.capability_bits &= ~RAY_TRACING_WORKER_CAP_CANCELLATION;
    expect_true("missing capability fails closed",
                !ray_tracing_worker_capabilities_negotiate(
                    &capabilities,
                    RAY_TRACING_WORKER_PROTOCOL_VERSION,
                    RAY_TRACING_WORKER_REQUIRED_CAPABILITIES,
                    diagnostics,
                    sizeof(diagnostics)));
    capabilities.capability_bits = RAY_TRACING_WORKER_REQUIRED_CAPABILITIES;

    snprintf(path, sizeof(path), "%s/capabilities.json", root);
    expect_true("capabilities write",
                ray_tracing_worker_capabilities_write_file(path, &capabilities));
    expect_true("capabilities roundtrip",
                ray_tracing_worker_capabilities_load_file(path,
                                                          &loaded_capabilities,
                                                          diagnostics,
                                                          sizeof(diagnostics)) &&
                    loaded_capabilities.capability_bits ==
                        RAY_TRACING_WORKER_REQUIRED_CAPABILITIES);

    fill_request(&request, root, digest);
    snprintf(path, sizeof(path), "%s/request_message.json", root);
    expect_true("request validate",
                ray_tracing_worker_request_validate(&request,
                                                    diagnostics,
                                                    sizeof(diagnostics)));
    expect_true("request write",
                ray_tracing_worker_request_write_file(path, &request));
    expect_true("request roundtrip",
                ray_tracing_worker_request_load_file(path,
                                                     &loaded_request,
                                                     diagnostics,
                                                     sizeof(diagnostics)) &&
                    loaded_request.protocol_version ==
                        RAY_TRACING_WORKER_PROTOCOL_VERSION &&
                    loaded_request.temporal_frames == 4 &&
                    strcmp(loaded_request.request_sha256, digest) == 0);
    loaded_request.protocol_version += 1;
    expect_true("unsupported request protocol rejected",
                !ray_tracing_worker_request_validate(&loaded_request,
                                                     diagnostics,
                                                     sizeof(diagnostics)));

    for (size_t i = 0; i < sizeof(event_types) / sizeof(event_types[0]); ++i) {
        RayTracingWorkerEvent event;
        RayTracingWorkerEvent loaded_event;
        fill_event(&event, event_types[i], i + 1u, digest);
        expect_true("event validates",
                    ray_tracing_worker_event_validate(&event,
                                                      diagnostics,
                                                      sizeof(diagnostics)));
        expect_true("event writes",
                    ray_tracing_worker_event_write(request.event_directory,
                                                   &event,
                                                   event_path,
                                                   sizeof(event_path)));
        expect_true("event roundtrip",
                    ray_tracing_worker_event_load_file(event_path,
                                                       &loaded_event,
                                                       diagnostics,
                                                       sizeof(diagnostics)) &&
                        loaded_event.type == event.type &&
                        loaded_event.sequence == event.sequence);
        expect_true("event evidence is create-only",
                    !ray_tracing_worker_event_write(request.event_directory,
                                                    &event,
                                                    NULL,
                                                    0u));
        (void)unlink(event_path);
    }

    {
        RayTracingWorkerEvent cancellation;
        expect_true("cancellation message write",
                    ray_tracing_worker_cancellation_write_file(
                        request.cancellation_path,
                        request.job_id,
                        "operator requested cancellation"));
        expect_true("cancellation message roundtrip",
                    ray_tracing_worker_cancellation_load_file(
                        request.cancellation_path,
                        &cancellation,
                        diagnostics,
                        sizeof(diagnostics)) &&
                        cancellation.type ==
                            RAY_TRACING_WORKER_MESSAGE_CANCELLATION &&
                        strcmp(cancellation.job_id, request.job_id) == 0 &&
                        strcmp(cancellation.state, "cancel_requested") == 0);
        (void)unlink(request.cancellation_path);
    }

    {
        RayTracingWorkerEvent invalid;
        fill_event(&invalid,
                   RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE,
                   99u,
                   digest);
        invalid.reference_sha256[0] = '\0';
        expect_true("checkpoint without digest rejected",
                    !ray_tracing_worker_event_validate(&invalid,
                                                      diagnostics,
                                                      sizeof(diagnostics)));
    }

    snprintf(path, sizeof(path), "%s/capabilities.json", root);
    (void)unlink(path);
    snprintf(path, sizeof(path), "%s/request_message.json", root);
    (void)unlink(path);
    snprintf(path, sizeof(path), "%s/events", root);
    (void)rmdir(path);
    (void)rmdir(root);
    if (failures != 0) {
        fprintf(stderr, "ray tracing worker protocol failures=%d\n", failures);
        return 1;
    }
    puts("ray tracing worker protocol contract passed");
    return 0;
}
