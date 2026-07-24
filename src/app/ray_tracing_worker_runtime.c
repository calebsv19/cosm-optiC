#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_worker_runtime.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/ray_tracing_sha256.h"
#include "app/ray_tracing_recovery_authority.h"
#include "app/ray_tracing_temporal_checkpoint.h"
#include "app/ray_tracing_worker_protocol.h"

static volatile sig_atomic_t worker_cancel_requested = 0;
static volatile sig_atomic_t worker_child_pid = 0;

static void set_diag(char *diagnostics, size_t size, const char *message) {
    if (!diagnostics || size == 0u) return;
    snprintf(diagnostics, size, "%s", message ? message : "");
}

static void worker_signal_handler(int signal_number) {
    worker_cancel_requested = signal_number;
    if (worker_child_pid > 0) {
        (void)kill((pid_t)worker_child_pid, SIGTERM);
    }
}

static bool install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = worker_signal_handler;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGTERM, &action, NULL) == 0 &&
           sigaction(SIGINT, &action, NULL) == 0 &&
           sigaction(SIGHUP, &action, NULL) == 0;
}

static bool activate_recovery_authority(
    const RayTracingWorkerRequest *request,
    char *diagnostics,
    size_t diagnostics_size) {
    RayTracingRecoveryDescriptor descriptor;
    RayTracingResumeAuthority authority;
    char lease_generation[32];
    char output_generation[32];
    if (!request || !request->recovery_authorized) return true;
    if (!ray_tracing_recovery_descriptor_load(request->recovery_descriptor_path,
                                              &descriptor,
                                              diagnostics,
                                              diagnostics_size) ||
        !ray_tracing_resume_authority_load(request->resume_authority_path,
                                           &authority,
                                           diagnostics,
                                           diagnostics_size) ||
        !ray_tracing_resume_authority_validate(&authority,
                                               &descriptor,
                                               request->recovery_worker_id,
                                               diagnostics,
                                               diagnostics_size) ||
        !ray_tracing_output_fence_validate(authority.fence_path,
                                           &authority,
                                           diagnostics,
                                           diagnostics_size) ||
        !ray_tracing_resume_authority_consume(&authority,
                                              NULL,
                                              diagnostics,
                                              diagnostics_size)) {
        return false;
    }
    snprintf(lease_generation,
             sizeof(lease_generation),
             "%llu",
             (unsigned long long)authority.lease_generation);
    snprintf(output_generation,
             sizeof(output_generation),
             "%llu",
             (unsigned long long)authority.output_generation);
    if (setenv("RAY_TRACING_OUTPUT_FENCE_PATH", authority.fence_path, 1) != 0 ||
        setenv("RAY_TRACING_RESUME_TOKEN_ID", authority.token_id, 1) != 0 ||
        setenv("RAY_TRACING_WORKER_ID", authority.worker_id, 1) != 0 ||
        setenv("RAY_TRACING_LEASE_ID", authority.lease_id, 1) != 0 ||
        setenv("RAY_TRACING_LEASE_GENERATION", lease_generation, 1) != 0 ||
        setenv("RAY_TRACING_OUTPUT_GENERATION", output_generation, 1) != 0) {
        set_diag(diagnostics, diagnostics_size,
                 "failed to install recovery output fence");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

static bool emit_event(const RayTracingWorkerRequest *request,
                       RayTracingWorkerEvent *event,
                       uint64_t *sequence) {
    if (!request || !event || !sequence) return false;
    event->protocol_version = request->protocol_version;
    event->sequence = (*sequence)++;
    snprintf(event->job_id, sizeof(event->job_id), "%s", request->job_id);
    return ray_tracing_worker_event_write(request->event_directory,
                                          event,
                                          NULL,
                                          0u);
}

static bool emit_interruption(const RayTracingWorkerRequest *request,
                              uint64_t *sequence,
                              int exit_code,
                              const char *message) {
    RayTracingWorkerEvent event;
    memset(&event, 0, sizeof(event));
    event.type = RAY_TRACING_WORKER_MESSAGE_INTERRUPTION;
    event.exit_code = exit_code == 0 ? 1 : exit_code;
    event.frame_index = -1;
    snprintf(event.state, sizeof(event.state), "%s", "interrupted");
    snprintf(event.diagnostics, sizeof(event.diagnostics), "%s", message);
    return emit_event(request, &event, sequence);
}

static bool progress_value(json_object *root, const char *key, int *out_value) {
    json_object *value = NULL;
    if (!root || !out_value || !json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static bool emit_progress_snapshot(const RayTracingWorkerRequest *request,
                                   uint64_t *sequence,
                                   int *last_frame_index,
                                   int *last_frames_completed,
                                   int *last_temporal_completed,
                                   off_t *last_size,
                                   time_t *last_modified) {
    struct stat status;
    json_object *root = NULL;
    RayTracingWorkerEvent event;
    int frame_index = -1;
    int frames_completed = 0;
    int temporal_completed = 0;
    int temporal_total = 0;
    bool emitted = false;
    if (stat(request->progress_path, &status) != 0 ||
        (status.st_size == *last_size && status.st_mtime == *last_modified)) {
        return true;
    }
    *last_size = status.st_size;
    *last_modified = status.st_mtime;
    root = json_object_from_file(request->progress_path);
    if (!root) return true;
    if (!progress_value(root, "frame_index", &frame_index) ||
        !progress_value(root, "frames_completed", &frames_completed)) {
        json_object_put(root);
        return true;
    }
    (void)progress_value(root, "temporal_subpasses_completed", &temporal_completed);
    (void)progress_value(root, "temporal_subpasses_total", &temporal_total);
    memset(&event, 0, sizeof(event));
    event.type = RAY_TRACING_WORKER_MESSAGE_PROGRESS;
    event.exit_code = -1;
    event.frame_index = frame_index;
    event.frames_completed = frames_completed;
    event.temporal_subpasses_completed = temporal_completed;
    event.temporal_subpasses_total = temporal_total;
    snprintf(event.state, sizeof(event.state), "%s", "running");
    snprintf(event.diagnostics, sizeof(event.diagnostics), "%s", "progress observed");
    emitted = emit_event(request, &event, sequence);

    if (emitted && frame_index >= 0) {
        char checkpoint_path[PATH_MAX];
        char checkpoint_sha256[RAY_TRACING_SHA256_HEX_SIZE];
        int checkpoint_completed = 0;
        int checkpoint_active_subpass = 0;
        size_t checkpoint_completed_tiles = 0u;
        RayTracingWorkerEvent checkpoint;
        char checkpoint_root[PATH_MAX];
        if (snprintf(checkpoint_root,
                     sizeof(checkpoint_root),
                     "%s/checkpoints",
                     request->output_root) < (int)sizeof(checkpoint_root) &&
            ray_tracing_temporal_checkpoint_latest_reference(
                checkpoint_root,
                frame_index,
                checkpoint_path,
                sizeof(checkpoint_path),
                checkpoint_sha256,
                &checkpoint_completed,
                &checkpoint_active_subpass,
                &checkpoint_completed_tiles)) {
            memset(&checkpoint, 0, sizeof(checkpoint));
            checkpoint.type = RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE;
            checkpoint.exit_code = -1;
            checkpoint.frame_index = frame_index;
            checkpoint.frames_completed = frames_completed;
            checkpoint.temporal_subpasses_completed = checkpoint_completed;
            checkpoint.temporal_subpasses_total = temporal_total;
            snprintf(checkpoint.state,
                     sizeof(checkpoint.state),
                     "%s",
                     "tile_batch");
            snprintf(checkpoint.diagnostics,
                     sizeof(checkpoint.diagnostics),
                     "durable tile-batch checkpoint subpass=%d tiles=%zu",
                     checkpoint_active_subpass,
                     checkpoint_completed_tiles);
            snprintf(checkpoint.reference_path,
                     sizeof(checkpoint.reference_path),
                     "%s",
                     checkpoint_path);
            snprintf(checkpoint.reference_sha256,
                     sizeof(checkpoint.reference_sha256),
                     "%s",
                     checkpoint_sha256);
            emitted = emit_event(request, &checkpoint, sequence);
        }
    }

    if (emitted && frames_completed > 0 &&
        frames_completed > *last_frames_completed && frame_index >= 0) {
        char frame_path[PATH_MAX];
        RayTracingWorkerEvent dirty;
        RayTracingWorkerEvent checkpoint;
        memset(&dirty, 0, sizeof(dirty));
        dirty.type = RAY_TRACING_WORKER_MESSAGE_DIRTY_REGION;
        dirty.exit_code = -1;
        dirty.frame_index = frame_index;
        dirty.frames_completed = frames_completed;
        dirty.region_width = request->width;
        dirty.region_height = request->height;
        snprintf(dirty.state, sizeof(dirty.state), "%s", "frame_committed");
        snprintf(dirty.diagnostics,
                 sizeof(dirty.diagnostics),
                 "%s",
                 "completed frame dirty region");
        emitted = emit_event(request, &dirty, sequence);

        if (emitted &&
            snprintf(frame_path,
                     sizeof(frame_path),
                     "%s/frames/frame_%04d.bmp",
                     request->output_root,
                     frame_index) < (int)sizeof(frame_path)) {
            memset(&checkpoint, 0, sizeof(checkpoint));
            checkpoint.type = RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE;
            checkpoint.exit_code = -1;
            checkpoint.frame_index = frame_index;
            checkpoint.frames_completed = frames_completed;
            snprintf(checkpoint.state,
                     sizeof(checkpoint.state),
                     "%s",
                     "completed_frame");
            snprintf(checkpoint.diagnostics,
                     sizeof(checkpoint.diagnostics),
                     "%s",
                     "durable completed-frame checkpoint");
            snprintf(checkpoint.reference_path,
                     sizeof(checkpoint.reference_path),
                     "%s",
                     frame_path);
            if (ray_tracing_sha256_file(frame_path, checkpoint.reference_sha256)) {
                emitted = emit_event(request, &checkpoint, sequence);
            }
        }
    }
    *last_frames_completed = frames_completed;
    *last_temporal_completed = temporal_completed;
    *last_frame_index = frame_index;
    json_object_put(root);
    return emitted;
}

bool ray_tracing_worker_runtime_write_capabilities(const char *render_cli_path,
                                                   const char *output_path,
                                                   char *diagnostics,
                                                   size_t diagnostics_size) {
    RayTracingWorkerCapabilities capabilities;
    if (!render_cli_path || !output_path) {
        set_diag(diagnostics, diagnostics_size, "invalid capabilities arguments");
        return false;
    }
    ray_tracing_worker_capabilities_defaults(&capabilities);
    if (!ray_tracing_sha256_file(render_cli_path,
                                 capabilities.renderer_build_sha256)) {
        set_diag(diagnostics, diagnostics_size, "failed to digest renderer build");
        return false;
    }
    if (!ray_tracing_worker_capabilities_write_file(output_path, &capabilities)) {
        set_diag(diagnostics, diagnostics_size, "failed to write capabilities message");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

int ray_tracing_worker_runtime_run(const char *request_message_path,
                                   char *diagnostics,
                                   size_t diagnostics_size) {
    RayTracingWorkerRequest request;
    RayTracingWorkerCapabilities capabilities;
    RayTracingWorkerEvent event;
    char actual_request_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char actual_renderer_sha256[RAY_TRACING_SHA256_HEX_SIZE];
    char protocol_diagnostics[256];
    uint64_t sequence = 1u;
    pid_t child_pid = 0;
    int child_status = 0;
    int last_frames_completed = -1;
    int last_temporal_completed = -1;
    int last_frame_index = -1;
    off_t last_progress_size = -1;
    time_t last_progress_modified = (time_t)-1;
    bool cancellation_emitted = false;
    bool child_finished = false;
    bool lease_lost = false;

    memset(&request, 0, sizeof(request));
    if (!ray_tracing_worker_request_load_file(request_message_path,
                                              &request,
                                              protocol_diagnostics,
                                              sizeof(protocol_diagnostics))) {
        set_diag(diagnostics, diagnostics_size, protocol_diagnostics);
        return 20;
    }
    if (!ray_tracing_sha256_file(request.request_path, actual_request_sha256) ||
        strcmp(actual_request_sha256, request.request_sha256) != 0) {
        (void)emit_interruption(&request, &sequence, 21, "request digest mismatch");
        set_diag(diagnostics, diagnostics_size, "request digest mismatch");
        return 21;
    }
    if (!ray_tracing_sha256_file(request.render_cli_path, actual_renderer_sha256) ||
        strcmp(actual_renderer_sha256, request.renderer_build_sha256) != 0) {
        (void)emit_interruption(&request, &sequence, 22, "renderer build digest mismatch");
        set_diag(diagnostics, diagnostics_size, "renderer build digest mismatch");
        return 22;
    }
    if (!activate_recovery_authority(&request,
                                     protocol_diagnostics,
                                     sizeof(protocol_diagnostics))) {
        set_diag(diagnostics, diagnostics_size, protocol_diagnostics);
        return 30;
    }
    ray_tracing_worker_capabilities_defaults(&capabilities);
    snprintf(capabilities.renderer_build_sha256,
             sizeof(capabilities.renderer_build_sha256),
             "%s",
             actual_renderer_sha256);
    if (!ray_tracing_worker_capabilities_negotiate(&capabilities,
                                                   request.protocol_version,
                                                   request.required_capability_bits,
                                                   protocol_diagnostics,
                                                   sizeof(protocol_diagnostics))) {
        (void)emit_interruption(&request, &sequence, 23, protocol_diagnostics);
        set_diag(diagnostics, diagnostics_size, protocol_diagnostics);
        return 23;
    }
    if (!install_signal_handlers()) {
        (void)emit_interruption(&request, &sequence, 24, "failed to install signal handlers");
        set_diag(diagnostics, diagnostics_size, "failed to install signal handlers");
        return 24;
    }

    memset(&event, 0, sizeof(event));
    event.type = RAY_TRACING_WORKER_MESSAGE_PROGRESS;
    event.exit_code = -1;
    event.frame_index = request.start_frame;
    snprintf(event.state, sizeof(event.state), "%s", "accepted");
    snprintf(event.diagnostics, sizeof(event.diagnostics), "%s", "request accepted");
    if (!emit_event(&request, &event, &sequence)) {
        set_diag(diagnostics, diagnostics_size, "failed to persist request acceptance");
        return 25;
    }

    child_pid = fork();
    if (child_pid < 0) {
        (void)emit_interruption(&request, &sequence, 26, "failed to fork renderer");
        set_diag(diagnostics, diagnostics_size, "failed to fork renderer");
        return 26;
    }
    if (child_pid == 0) {
        char *const arguments[] = {
            request.render_cli_path,
            (char *)"--request",
            request.request_path,
            (char *)"--render",
            (char *)"--summary",
            request.result_summary_path,
            (char *)"--job-id",
            request.job_id,
            (char *)"--job-status",
            request.job_status_path,
            NULL
        };
        (void)setenv("RAY_TRACING_RENDERER_BUILD_SHA256",
                     request.renderer_build_sha256,
                     1);
        (void)setenv("RAY_TRACING_REQUEST_SHA256",
                     request.request_sha256,
                     1);
        execv(request.render_cli_path, arguments);
        _exit(127);
    }
    worker_child_pid = child_pid;

    while (!child_finished) {
        struct timespec pause = {0, 50000000L};
        const pid_t waited = waitpid(child_pid, &child_status, WNOHANG);
        if (request.recovery_authorized &&
            !ray_tracing_output_fence_validate_environment()) {
            lease_lost = true;
            (void)kill(child_pid, SIGTERM);
        }
        if (!worker_cancel_requested && access(request.cancellation_path, F_OK) == 0) {
            RayTracingWorkerEvent cancellation;
            if (ray_tracing_worker_cancellation_load_file(request.cancellation_path,
                                                          &cancellation,
                                                          protocol_diagnostics,
                                                          sizeof(protocol_diagnostics))) {
                worker_cancel_requested = SIGTERM;
                (void)kill(child_pid, SIGTERM);
            }
        }
        if (waited == child_pid) {
            child_finished = true;
        } else if (waited < 0 && errno != EINTR) {
            child_status = 1 << 8;
            child_finished = true;
        }
        if (!emit_progress_snapshot(&request,
                                    &sequence,
                                    &last_frame_index,
                                    &last_frames_completed,
                                    &last_temporal_completed,
                                    &last_progress_size,
                                    &last_progress_modified)) {
            (void)kill(child_pid, SIGTERM);
            (void)waitpid(child_pid, &child_status, 0);
            worker_child_pid = 0;
            (void)emit_interruption(&request,
                                    &sequence,
                                    27,
                                    "failed to persist progress event");
            set_diag(diagnostics, diagnostics_size, "failed to persist progress event");
            return 27;
        }
        if (worker_cancel_requested && !cancellation_emitted) {
            memset(&event, 0, sizeof(event));
            event.type = RAY_TRACING_WORKER_MESSAGE_CANCELLATION;
            event.exit_code = -1;
            event.frame_index = last_frames_completed > 0
                                    ? request.start_frame + last_frames_completed - 1
                                    : request.start_frame;
            event.frames_completed = last_frames_completed > 0
                                         ? last_frames_completed
                                         : 0;
            snprintf(event.state, sizeof(event.state), "%s", "cancel_requested");
            snprintf(event.diagnostics,
                     sizeof(event.diagnostics),
                     "worker received signal %d",
                     (int)worker_cancel_requested);
            cancellation_emitted = emit_event(&request, &event, &sequence);
        }
        if (!child_finished) (void)nanosleep(&pause, NULL);
    }
    worker_child_pid = 0;
    if (lease_lost) {
        set_diag(diagnostics,
                 diagnostics_size,
                 "output ownership lease lost; stale holder stopped");
        return 31;
    }
    (void)emit_progress_snapshot(&request,
                                 &sequence,
                                 &last_frame_index,
                                 &last_frames_completed,
                                 &last_temporal_completed,
                                 &last_progress_size,
                                 &last_progress_modified);
    if (!(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0)) {
        const int checkpoint_frame =
            last_frame_index >= request.start_frame ? last_frame_index
                                                    : request.start_frame;
        {
            char checkpoint_path[PATH_MAX];
            char checkpoint_sha256[RAY_TRACING_SHA256_HEX_SIZE];
            char checkpoint_root[PATH_MAX];
            int checkpoint_completed = 0;
            int checkpoint_active_subpass = 0;
            size_t checkpoint_completed_tiles = 0u;
            RayTracingWorkerEvent checkpoint;
            if (snprintf(checkpoint_root,
                         sizeof(checkpoint_root),
                         "%s/checkpoints",
                         request.output_root) < (int)sizeof(checkpoint_root) &&
                ray_tracing_temporal_checkpoint_latest_reference(
                    checkpoint_root,
                    checkpoint_frame,
                    checkpoint_path,
                    sizeof(checkpoint_path),
                    checkpoint_sha256,
                    &checkpoint_completed,
                    &checkpoint_active_subpass,
                    &checkpoint_completed_tiles)) {
                memset(&checkpoint, 0, sizeof(checkpoint));
                checkpoint.type =
                    RAY_TRACING_WORKER_MESSAGE_CHECKPOINT_REFERENCE;
                checkpoint.exit_code = -1;
                checkpoint.frame_index = checkpoint_frame;
                checkpoint.frames_completed =
                    last_frames_completed > 0 ? last_frames_completed : 0;
                checkpoint.temporal_subpasses_completed = checkpoint_completed;
                snprintf(checkpoint.state,
                         sizeof(checkpoint.state),
                         "%s",
                         "tile_batch");
                snprintf(checkpoint.diagnostics,
                         sizeof(checkpoint.diagnostics),
                         "durable tile-batch checkpoint recovered after interruption subpass=%d tiles=%zu",
                         checkpoint_active_subpass,
                         checkpoint_completed_tiles);
                snprintf(checkpoint.reference_path,
                         sizeof(checkpoint.reference_path),
                         "%s",
                         checkpoint_path);
                snprintf(checkpoint.reference_sha256,
                         sizeof(checkpoint.reference_sha256),
                         "%s",
                         checkpoint_sha256);
                (void)emit_event(&request, &checkpoint, &sequence);
            }
        }
    }

    if (worker_cancel_requested) {
        (void)emit_interruption(&request, &sequence, 128, "renderer cancelled");
        set_diag(diagnostics, diagnostics_size, "renderer cancelled");
        return 128;
    }
    if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0 &&
        ray_tracing_sha256_file(request.result_summary_path, event.summary_sha256)) {
        memset(&event, 0, sizeof(event));
        event.type = RAY_TRACING_WORKER_MESSAGE_COMPLETION;
        event.exit_code = 0;
        event.frame_index = request.start_frame + request.frame_count - 1;
        event.frames_completed =
            last_frames_completed >= 0 ? last_frames_completed : request.frame_count;
        snprintf(event.state, sizeof(event.state), "%s", "completed");
        snprintf(event.diagnostics, sizeof(event.diagnostics), "%s", "render completed");
        if (!ray_tracing_sha256_file(request.result_summary_path, event.summary_sha256) ||
            !emit_event(&request, &event, &sequence)) {
            set_diag(diagnostics, diagnostics_size, "failed to persist completion event");
            return 28;
        }
        set_diag(diagnostics, diagnostics_size, "ok");
        return 0;
    }

    {
        const int exit_code = WIFEXITED(child_status)
                                  ? WEXITSTATUS(child_status)
                                  : (WIFSIGNALED(child_status)
                                         ? 128 + WTERMSIG(child_status)
                                         : 29);
        char message[128];
        snprintf(message,
                 sizeof(message),
                 "renderer exited without valid completion summary (exit=%d)",
                 exit_code);
        (void)emit_interruption(&request, &sequence, exit_code, message);
        set_diag(diagnostics, diagnostics_size, message);
        return exit_code == 0 ? 29 : exit_code;
    }
}
