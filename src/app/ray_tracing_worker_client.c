#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_worker_client.h"

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "app/ray_tracing_worker_protocol.h"

static void set_diag(char *diagnostics, size_t size, const char *message) {
    if (!diagnostics || size == 0u) return;
    snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool redirect_child_stdio(const RayTracingWorkerClientSpawnRequest *request) {
    FILE *stdout_file = fopen(request->stdout_log_path, "ab");
    FILE *stderr_file = fopen(request->stderr_log_path, "ab");
    int null_fd = -1;
    if (!stdout_file || !stderr_file) return false;
    if (dup2(fileno(stdout_file), STDOUT_FILENO) < 0 ||
        dup2(fileno(stderr_file), STDERR_FILENO) < 0) {
        return false;
    }
    null_fd = open("/dev/null", O_RDONLY);
    if (null_fd >= 0) {
        (void)dup2(null_fd, STDIN_FILENO);
        close(null_fd);
    }
    return true;
}

static bool query_capabilities(const RayTracingWorkerClientSpawnRequest *request,
                               RayTracingWorkerCapabilities *capabilities,
                               char *diagnostics,
                               size_t diagnostics_size) {
    pid_t pid = fork();
    int status = 0;
    if (pid < 0) {
        set_diag(diagnostics, diagnostics_size, "failed to fork capability query");
        return false;
    }
    if (pid == 0) {
        char *const arguments[] = {
            (char *)request->worker_runtime_path,
            (char *)"capabilities",
            (char *)"--render-cli",
            (char *)request->render_cli_path,
            (char *)"--output",
            (char *)request->capabilities_path,
            NULL
        };
        if (!redirect_child_stdio(request)) _exit(126);
        execv(request->worker_runtime_path, arguments);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        set_diag(diagnostics, diagnostics_size, "worker capability query failed");
        return false;
    }
    return ray_tracing_worker_capabilities_load_file(request->capabilities_path,
                                                     capabilities,
                                                     diagnostics,
                                                     diagnostics_size);
}

static bool spawn_direct(const RayTracingWorkerClientSpawnRequest *request,
                         RayTracingWorkerClientSpawnResult *result) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        char *const arguments[] = {
            (char *)request->render_cli_path,
            (char *)"--request",
            (char *)request->canonical_request_path,
            (char *)"--render",
            (char *)"--summary",
            (char *)request->result_summary_path,
            (char *)"--job-id",
            (char *)request->job_id,
            (char *)"--job-status",
            (char *)request->job_status_path,
            NULL
        };
        if (setsid() < 0 || !redirect_child_stdio(request)) _exit(126);
        execv(request->render_cli_path, arguments);
        _exit(127);
    }
    result->pid = pid;
    result->protocol_version = 0;
    snprintf(result->execution_mode, sizeof(result->execution_mode), "%s", "direct_fallback");
    return true;
}

static bool spawn_protocol_worker(const RayTracingWorkerClientSpawnRequest *request,
                                  RayTracingWorkerClientSpawnResult *result) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        char *const arguments[] = {
            (char *)request->worker_runtime_path,
            (char *)"run",
            (char *)"--message",
            (char *)request->worker_request_path,
            NULL
        };
        if (setsid() < 0 || !redirect_child_stdio(request)) _exit(126);
        execv(request->worker_runtime_path, arguments);
        _exit(127);
    }
    result->pid = pid;
    result->protocol_version = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    snprintf(result->execution_mode, sizeof(result->execution_mode), "%s", "worker_protocol");
    return true;
}

bool ray_tracing_worker_client_spawn(
    const RayTracingWorkerClientSpawnRequest *request,
    RayTracingWorkerClientSpawnResult *result,
    char *diagnostics,
    size_t diagnostics_size) {
    RayTracingWorkerCapabilities capabilities;
    RayTracingWorkerRequest protocol_request;
    if (!request || !result || !request->job_id || !request->worker_runtime_path ||
        !request->render_cli_path || !request->canonical_request_path ||
        !request->capabilities_path || !request->worker_request_path ||
        !request->event_directory || !request->cancellation_path ||
        !request->output_root ||
        !request->progress_path || !request->job_status_path ||
        !request->result_summary_path || !request->stdout_log_path ||
        !request->stderr_log_path) {
        set_diag(diagnostics, diagnostics_size, "invalid worker client request");
        return false;
    }
    memset(result, 0, sizeof(*result));
    if (!ray_tracing_sha256_file(request->canonical_request_path,
                                 result->request_sha256) ||
        !ray_tracing_sha256_file(request->render_cli_path,
                                 result->renderer_build_sha256)) {
        set_diag(diagnostics, diagnostics_size, "failed to bind immutable worker inputs");
        return false;
    }
    if (request->force_direct_fallback || access(request->worker_runtime_path, X_OK) != 0) {
        if (!spawn_direct(request, result)) {
            set_diag(diagnostics, diagnostics_size, "failed to spawn direct fallback");
            return false;
        }
        set_diag(diagnostics, diagnostics_size, "direct fallback");
        return true;
    }
    if (!query_capabilities(request,
                            &capabilities,
                            diagnostics,
                            diagnostics_size) ||
        !ray_tracing_worker_capabilities_negotiate(
            &capabilities,
            RAY_TRACING_WORKER_PROTOCOL_VERSION,
            RAY_TRACING_WORKER_REQUIRED_CAPABILITIES,
            diagnostics,
            diagnostics_size)) {
        return false;
    }
    if (strcmp(capabilities.renderer_build_sha256,
               result->renderer_build_sha256) != 0) {
        set_diag(diagnostics, diagnostics_size, "capability renderer digest drift");
        return false;
    }

    memset(&protocol_request, 0, sizeof(protocol_request));
    protocol_request.protocol_version = RAY_TRACING_WORKER_PROTOCOL_VERSION;
    protocol_request.required_capability_bits = RAY_TRACING_WORKER_REQUIRED_CAPABILITIES;
    protocol_request.sequence = 0u;
    protocol_request.width = request->width;
    protocol_request.height = request->height;
    protocol_request.start_frame = request->start_frame;
    protocol_request.frame_count = request->frame_count;
    protocol_request.temporal_frames = request->temporal_frames;
    protocol_request.recovery_authorized =
        request->recovery_descriptor_path && request->recovery_descriptor_path[0];
#define COPY_REQUEST_FIELD(field, source) \
    do { \
        if (snprintf(protocol_request.field, \
                     sizeof(protocol_request.field), \
                     "%s", \
                     (source)) >= (int)sizeof(protocol_request.field)) { \
            set_diag(diagnostics, diagnostics_size, "worker request path exceeds protocol limit"); \
            return false; \
        } \
    } while (0)
    COPY_REQUEST_FIELD(job_id, request->job_id);
    COPY_REQUEST_FIELD(request_path, request->canonical_request_path);
    COPY_REQUEST_FIELD(request_sha256, result->request_sha256);
    COPY_REQUEST_FIELD(render_cli_path, request->render_cli_path);
    COPY_REQUEST_FIELD(renderer_build_sha256, result->renderer_build_sha256);
    COPY_REQUEST_FIELD(output_root, request->output_root);
    COPY_REQUEST_FIELD(progress_path, request->progress_path);
    COPY_REQUEST_FIELD(job_status_path, request->job_status_path);
    COPY_REQUEST_FIELD(result_summary_path, request->result_summary_path);
    COPY_REQUEST_FIELD(event_directory, request->event_directory);
    COPY_REQUEST_FIELD(cancellation_path, request->cancellation_path);
    if (protocol_request.recovery_authorized) {
        if (!request->resume_authority_path || !request->resume_authority_path[0] ||
            !request->recovery_worker_id || !request->recovery_worker_id[0]) {
            set_diag(diagnostics,
                     diagnostics_size,
                     "incomplete recovery authority binding");
            return false;
        }
        COPY_REQUEST_FIELD(recovery_descriptor_path,
                           request->recovery_descriptor_path);
        COPY_REQUEST_FIELD(resume_authority_path,
                           request->resume_authority_path);
        if (request->resume_receipt_path && request->resume_receipt_path[0]) {
            COPY_REQUEST_FIELD(resume_receipt_path,
                               request->resume_receipt_path);
        }
        COPY_REQUEST_FIELD(recovery_worker_id,
                           request->recovery_worker_id);
    }
#undef COPY_REQUEST_FIELD
    if (!ray_tracing_worker_request_write_file(request->worker_request_path,
                                               &protocol_request)) {
        set_diag(diagnostics, diagnostics_size, "failed to persist worker request message");
        return false;
    }
    if (!spawn_protocol_worker(request, result)) {
        set_diag(diagnostics, diagnostics_size, "failed to spawn protocol worker");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool ray_tracing_worker_client_request_cancel(const char *job_id,
                                              const char *cancellation_path,
                                              pid_t pid,
                                              char *diagnostics,
                                              size_t diagnostics_size) {
    if (!job_id || !job_id[0] || !cancellation_path || !cancellation_path[0] ||
        pid <= 0) {
        set_diag(diagnostics, diagnostics_size, "invalid cancellation request");
        return false;
    }
    if (!ray_tracing_worker_cancellation_write_file(cancellation_path,
                                                    job_id,
                                                    "job runner requested cancellation")) {
        set_diag(diagnostics,
                 diagnostics_size,
                 "failed to persist protocol cancellation request");
        return false;
    }
    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
        set_diag(diagnostics, diagnostics_size, "failed to signal worker process");
        return false;
    }
    set_diag(diagnostics, diagnostics_size, "ok");
    return true;
}
