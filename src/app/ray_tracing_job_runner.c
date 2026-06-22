#include "app/ray_tracing_job_runner.h"
#include "app/ray_tracing_job_runner_internal.h"
#include "app/ray_tracing_request_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/agent_render_request.h"
#include "app/ray_tracing_headless_job_bundle.h"

static void diag_set(char *out, size_t out_size, const char *message) {
    RayTracingRequestSetDiag(out, out_size, message);
}

bool copy_string(char *dst, size_t dst_size, const char *src) {
    return RayTracingCopyString(dst, dst_size, src);
}


bool read_text_file(const char *path, char **out_text) {
    return RayTracingReadTextFile(path, out_text);
}

bool write_text_file(const char *path, const char *text) {
    FILE *file = NULL;
    if (!path || !path[0] || !text) return false;
    if (!ray_tracing_job_runner_ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (fputs(text, file) < 0) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

void json_write_string(FILE *file, const char *value) {
    RayTracingJsonWriteString(file, value);
}

bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
#if defined(__APPLE__) || defined(__unix__)
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
#else
    {
        struct tm *tmp = gmtime(&now);
        if (!tmp) return false;
        tm_utc = *tmp;
    }
#endif
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}


static bool generate_job_id(char *out_job_id, size_t out_job_id_size) {
    struct timespec ts;
    uint32_t salt = 0u;
    if (!out_job_id || out_job_id_size == 0u) return false;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return false;
    salt = (uint32_t)((uint64_t)getpid() ^ (uint64_t)ts.tv_nsec);
    return snprintf(out_job_id,
                    out_job_id_size,
                    "rtjob_%lld_%06u",
                    (long long)ts.tv_sec,
                    (unsigned)(salt % 1000000u)) < (int)out_job_id_size;
}


static bool spawn_detached_render(const char *render_cli_path,
                                  const RayTracingDetachedJobPaths *paths,
                                  const char *job_id,
                                  pid_t *out_pid) {
    pid_t pid = 0;
    FILE *stdout_file = NULL;
    FILE *stderr_file = NULL;
    if (out_pid) *out_pid = 0;
    if (!render_cli_path || !paths || !job_id) return false;
    if (!ray_tracing_job_runner_ensure_directory_exists(paths->job_root)) return false;

    pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        char *const argv[] = {
            (char *)render_cli_path,
            (char *)"--request",
            (char *)paths->job_request_path,
            (char *)"--render",
            (char *)"--summary",
            (char *)paths->result_summary_path,
            (char *)"--job-id",
            (char *)job_id,
            (char *)"--job-status",
            (char *)paths->job_status_path,
            NULL
        };
        int null_fd = -1;
        if (setsid() < 0) _exit(126);
        stdout_file = fopen(paths->stdout_log_path, "ab");
        stderr_file = fopen(paths->stderr_log_path, "ab");
        if (!stdout_file || !stderr_file) _exit(126);
        if (dup2(fileno(stdout_file), STDOUT_FILENO) < 0) _exit(126);
        if (dup2(fileno(stderr_file), STDERR_FILENO) < 0) _exit(126);
        null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }
        execv(render_cli_path, argv);
        _exit(127);
    }
    if (out_pid) *out_pid = pid;
    return true;
}

static bool load_request_for_job(const char *request_path,
                                 RayTracingAgentRenderRequest *out_request,
                                 RayTracingHeadlessJobBundle *out_bundle,
                                 bool *out_is_shared_bundle,
                                 char *out_diagnostics,
                                 size_t out_diagnostics_size) {
    char request_diag[256];
    char bundle_diag[256];

    if (out_is_shared_bundle) *out_is_shared_bundle = false;
    if (out_bundle) memset(out_bundle, 0, sizeof(*out_bundle));

    if (ray_tracing_agent_render_request_load_file(request_path,
                                                   out_request,
                                                   request_diag,
                                                   sizeof(request_diag))) {
        diag_set(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    if (!ray_tracing_headless_job_bundle_load(request_path,
                                              out_bundle,
                                              bundle_diag,
                                              sizeof(bundle_diag))) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "request load failed (%s); outer bundle load failed (%s)",
                     request_diag,
                     bundle_diag);
        }
        return false;
    }

    if (!ray_tracing_agent_render_request_load_file(out_bundle->resolved_run_config_path,
                                                    out_request,
                                                    request_diag,
                                                    sizeof(request_diag))) {
        diag_set(out_diagnostics, out_diagnostics_size, request_diag);
        return false;
    }
    if (!copy_string(out_request->runtime_scene_path,
                     sizeof(out_request->runtime_scene_path),
                     out_bundle->resolved_scene_payload_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to apply outer scene payload path");
        return false;
    }
    if (out_is_shared_bundle) *out_is_shared_bundle = true;
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool build_default_shared_job_envelope(const RayTracingAgentRenderRequest *request,
                                              const RayTracingDetachedJobPaths *paths,
                                              const char *job_id,
                                              const char *created_at_utc,
                                              CoreHeadlessJobEnvelope *out_envelope) {
    if (!request || !paths || !job_id || !created_at_utc || !out_envelope) return false;
    core_headless_job_envelope_init(out_envelope);
    if (!copy_string(out_envelope->job_id, sizeof(out_envelope->job_id), job_id) ||
        !copy_string(out_envelope->program, sizeof(out_envelope->program), "ray_tracing") ||
        !copy_string(out_envelope->tool.name, sizeof(out_envelope->tool.name), "ray_tracing_render_headless") ||
        !copy_string(out_envelope->tool.version, sizeof(out_envelope->tool.version), "0.1.0") ||
        !copy_string(out_envelope->tool.target_os, sizeof(out_envelope->tool.target_os), "linux") ||
        !copy_string(out_envelope->tool.target_arch, sizeof(out_envelope->tool.target_arch), "x86_64") ||
        !copy_string(out_envelope->scene_payload.schema_family,
                     sizeof(out_envelope->scene_payload.schema_family),
                     "codework_scene") ||
        !copy_string(out_envelope->scene_payload.schema_variant,
                     sizeof(out_envelope->scene_payload.schema_variant),
                     "scene_runtime_v1") ||
        !copy_string(out_envelope->scene_payload.path,
                     sizeof(out_envelope->scene_payload.path),
                     request->runtime_scene_path) ||
        !copy_string(out_envelope->run_config.schema_family,
                     sizeof(out_envelope->run_config.schema_family),
                     "ray_tracing_request") ||
        !copy_string(out_envelope->run_config.schema_variant,
                     sizeof(out_envelope->run_config.schema_variant),
                     RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA) ||
        !copy_string(out_envelope->run_config.path,
                     sizeof(out_envelope->run_config.path),
                     paths->job_request_path) ||
        !copy_string(out_envelope->outputs.root,
                     sizeof(out_envelope->outputs.root),
                     paths->job_root) ||
        !copy_string(out_envelope->outputs.report_path,
                     sizeof(out_envelope->outputs.report_path),
                     paths->shared_report_path) ||
        !copy_string(out_envelope->outputs.logs_dir,
                     sizeof(out_envelope->outputs.logs_dir),
                     paths->job_root) ||
        !copy_string(out_envelope->outputs.artifacts_dir,
                     sizeof(out_envelope->outputs.artifacts_dir),
                     request->output_root) ||
        !copy_string(out_envelope->metadata.created_by,
                     sizeof(out_envelope->metadata.created_by),
                     "ray_tracing_job_runner") ||
        !copy_string(out_envelope->metadata.created_at,
                     sizeof(out_envelope->metadata.created_at),
                     created_at_utc)) {
        return false;
    }
    return core_headless_job_envelope_validate(out_envelope);
}

static bool write_canonical_request_file(const char *path,
                                         const RayTracingAgentRenderRequest *request,
                                         const RayTracingDetachedJobPaths *paths) {
    FILE *file = NULL;
    char progress_path[PATH_MAX];
    bool wrote_field = false;
    if (!path || !request || !paths) return false;
    if (snprintf(progress_path,
                 sizeof(progress_path),
                 "%s/render_progress.json",
                 paths->job_root) >= (int)sizeof(progress_path)) {
        return false;
    }
    if (!ray_tracing_job_runner_ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": ");
    json_write_string(file, request->schema_version);
    fprintf(file, ",\n");
    fprintf(file, "  \"run_id\": ");
    json_write_string(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"scene\": {\n");
    fprintf(file, "    \"runtime_scene_path\": ");
    json_write_string(file, request->runtime_scene_path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"volume\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", request->volume_enabled ? "true" : "false");
    if (request->volume_enabled && request->volume_source_path[0]) {
        fprintf(file, "    \"source_kind\": ");
        json_write_string(file,
                          ray_tracing_agent_render_request_volume_kind_label(
                              request->volume_source_kind));
        fprintf(file, ",\n");
        fprintf(file, "    \"source_path\": ");
        json_write_string(file, request->volume_source_path);
        fprintf(file, ",\n");
        fprintf(file, "    \"affects_lighting\": %s,\n",
                request->volume_affects_lighting ? "true" : "false");
        fprintf(file, "    \"debug_overlay\": %s\n",
                request->volume_debug_overlay ? "true" : "false");
    } else {
        fprintf(file, "    \"affects_lighting\": false,\n");
        fprintf(file, "    \"debug_overlay\": false\n");
    }
    fprintf(file, "  },\n");
    fprintf(file, "  \"render\": {\n");
    fprintf(file, "    \"start_frame\": %d,\n", request->start_frame);
    fprintf(file, "    \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "    \"width\": %d,\n", request->width);
    fprintf(file, "    \"height\": %d,\n", request->height);
    fprintf(file, "    \"normalized_t\": %.9f,\n", request->normalized_t);
    fprintf(file, "    \"temporal_frames\": %d,\n", request->temporal_frames);
    fprintf(file, "    \"integrator_3d\": ");
    json_write_string(file,
                      ray_tracing_agent_render_request_integrator_label(
                          request->integrator_3d));
    fprintf(file, "\n  },\n");
    if (request->has_sampling_window) {
        fprintf(file, "  \"sampling\": {\n");
        fprintf(file, "    \"frame_offset\": %d,\n", request->sampling_frame_offset);
        fprintf(file, "    \"frame_count\": %d\n", request->sampling_frame_count);
        fprintf(file, "  },\n");
    }
    fprintf(file, "  \"output\": {\n");
    fprintf(file, "    \"root\": ");
    json_write_string(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "    \"overwrite\": %s\n", request->overwrite ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"progress\": {\n");
    fprintf(file, "    \"summary_path\": ");
    json_write_string(file, paths->result_summary_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"progress_path\": ");
    json_write_string(file, progress_path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"inspection\": {\n");
    if (request->has_camera_zoom_override) {
        fprintf(file, "    \"camera_zoom\": %.9f", request->camera_zoom_override);
        wrote_field = true;
    }
    if (request->has_camera_position_override) {
        fprintf(file,
                "%s    \"camera_position\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f }",
                wrote_field ? ",\n" : "",
                request->camera_position_x,
                request->camera_position_y,
                request->camera_position_z);
        wrote_field = true;
    }
    if (request->has_camera_look_at_override) {
        fprintf(file,
                "%s    \"camera_look_at\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f }",
                wrote_field ? ",\n" : "",
                request->camera_look_at_x,
                request->camera_look_at_y,
                request->camera_look_at_z);
        wrote_field = true;
    }
    if (request->has_environment_brightness_override) {
        fprintf(file,
                "%s    \"environment_brightness\": %.9f",
                wrote_field ? ",\n" : "",
                request->environment_brightness_override);
        wrote_field = true;
    }
    if (request->has_ambient_strength_override) {
        fprintf(file,
                "%s    \"ambient_strength\": %.9f",
                wrote_field ? ",\n" : "",
                request->ambient_strength_override);
        wrote_field = true;
    }
    if (request->has_environment_light_mode_override) {
        const char *mode = "off";
        if (request->environment_light_mode_override == ENVIRONMENT_LIGHT_MODE_TOP_FILL) {
            mode = "top_fill";
        } else if (request->environment_light_mode_override == ENVIRONMENT_LIGHT_MODE_AMBIENT) {
            mode = "ambient";
        }
        fprintf(file,
                "%s    \"environment_light_mode\": \"%s\"",
                wrote_field ? ",\n" : "",
                mode);
        wrote_field = true;
    }
    if (request->has_top_fill_strength_override) {
        fprintf(file,
                "%s    \"top_fill_strength\": %.9f",
                wrote_field ? ",\n" : "",
                request->top_fill_strength_override);
        wrote_field = true;
    }
    if (request->has_light_intensity_override) {
        fprintf(file,
                "%s    \"light_intensity\": %.9f",
                wrote_field ? ",\n" : "",
                request->light_intensity_override);
        wrote_field = true;
    }
    if (request->has_light_radius_override) {
        fprintf(file,
                "%s    \"light_radius\": %.9f",
                wrote_field ? ",\n" : "",
                request->light_radius_override);
        wrote_field = true;
    }
    if (request->has_forward_decay_override) {
        fprintf(file,
                "%s    \"forward_decay\": %.9f",
                wrote_field ? ",\n" : "",
                request->forward_decay_override);
        wrote_field = true;
    }
    if (request->has_volume_scatter_gain_override) {
        fprintf(file,
                "%s    \"volume_scatter_gain\": %.9f",
                wrote_field ? ",\n" : "",
                request->volume_scatter_gain_override);
        wrote_field = true;
    }
    if (request->has_volume_step_scale_override) {
        fprintf(file,
                "%s    \"volume_step_scale\": %.9f",
                wrote_field ? ",\n" : "",
                request->volume_step_scale_override);
        wrote_field = true;
    }
    if (request->has_secondary_diffuse_samples_3d_override) {
        fprintf(file,
                "%s    \"secondary_diffuse_samples_3d\": %d",
                wrote_field ? ",\n" : "",
                request->secondary_diffuse_samples_3d_override);
        wrote_field = true;
    }
    if (request->has_transmission_samples_3d_override) {
        fprintf(file,
                "%s    \"transmission_samples_3d\": %d",
                wrote_field ? ",\n" : "",
                request->transmission_samples_3d_override);
        wrote_field = true;
    }
    if (request->has_volume_tint_override) {
        fprintf(file,
                "%s    \"volume_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f }",
                wrote_field ? ",\n" : "",
                request->volume_tint_r,
                request->volume_tint_g,
                request->volume_tint_b);
        wrote_field = true;
    }
    if (wrote_field) {
        fprintf(file, "\n");
    }
    fprintf(file, "  }\n");
    fprintf(file, "}\n");
    fclose(file);
    return true;
}

static int count_contiguous_existing_frames(const RayTracingAgentRenderRequest *request) {
    int existing = 0;
    char frame_path[PATH_MAX];
    if (!request || request->frame_count <= 0) return 0;
    for (int i = 0; i < request->frame_count; ++i) {
        if (!ray_tracing_job_runner_build_frame_path(request->output_root,
                                                     request->start_frame + i,
                                                     frame_path,
                                                     sizeof(frame_path))) {
            break;
        }
        if (!ray_tracing_job_runner_file_exists(frame_path)) {
            break;
        }
        existing += 1;
    }
    return existing;
}

bool ray_tracing_job_runner_submit(const char *argv0,
                                   const char *request_path,
                                   const char *jobs_root_override,
                                   bool overwrite,
                                   bool resume,
                                   char *out_job_id,
                                   size_t out_job_id_size,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    RayTracingAgentRenderRequest request;
    RayTracingDetachedJobPaths paths;
    RayTracingDetachedJobRecord record;
    RayTracingHeadlessJobBundle source_bundle;
    CoreHeadlessJobEnvelope shared_job;
    char jobs_root[PATH_MAX];
    char render_cli_path[PATH_MAX];
    char diagnostics[256];
    pid_t pid = 0;
    int contiguous_existing = 0;
    bool is_shared_bundle = false;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !request_path || !request_path[0] || !out_job_id || out_job_id_size == 0u) {
        return false;
    }
    if (!load_request_for_job(request_path,
                              &request,
                              &source_bundle,
                              &is_shared_bundle,
                              diagnostics,
                              sizeof(diagnostics))) {
        diag_set(out_diagnostics, out_diagnostics_size, diagnostics);
        return false;
    }
    if (overwrite && resume) {
        diag_set(out_diagnostics, out_diagnostics_size, "overwrite and resume are mutually exclusive");
        return false;
    }
    if (!ray_tracing_job_runner_build_jobs_root(argv0,
                                                jobs_root_override,
                                                jobs_root,
                                                sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    if (!ray_tracing_job_runner_derive_render_cli_path(argv0,
                                                       render_cli_path,
                                                       sizeof(render_cli_path))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve render cli path");
        return false;
    }
    if (is_shared_bundle) {
        if (!copy_string(out_job_id, out_job_id_size, source_bundle.envelope.job_id)) {
            diag_set(out_diagnostics, out_diagnostics_size, "bundle job id exceeds local buffer");
            return false;
        }
    } else if (!generate_job_id(out_job_id, out_job_id_size)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to generate job id");
        return false;
    }
    if (!ray_tracing_job_runner_build_job_paths(jobs_root, out_job_id, &paths)) {
        diag_set(out_diagnostics, out_diagnostics_size, "invalid job id or job path");
        return false;
    }
    if (is_shared_bundle) {
        if (snprintf(request.output_root,
                     sizeof(request.output_root),
                     "%s/output/artifacts",
                     paths.job_root) >= (int)sizeof(request.output_root)) {
            diag_set(out_diagnostics, out_diagnostics_size, "failed to derive bundle artifacts path");
            return false;
        }
    }
    if (!ray_tracing_job_runner_validate_output_root(request.output_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "invalid detached output root");
        return false;
    }
    contiguous_existing = count_contiguous_existing_frames(&request);
    if (!overwrite && !resume && contiguous_existing > 0) {
        diag_set(out_diagnostics,
                 out_diagnostics_size,
                 "existing output frames found; use --overwrite or --resume");
        return false;
    }
    if (overwrite) {
        request.overwrite = true;
    }
    if (resume) {
        if (contiguous_existing <= 0) {
            request.overwrite = false;
        } else if (contiguous_existing >= request.frame_count) {
            diag_set(out_diagnostics,
                     out_diagnostics_size,
                     "all requested frames already exist; nothing to resume");
            return false;
        } else {
            request.has_sampling_window = true;
            request.sampling_frame_offset = contiguous_existing;
            request.sampling_frame_count = request.frame_count;
            request.start_frame += contiguous_existing;
            request.frame_count -= contiguous_existing;
            request.overwrite = false;
        }
    }
    if (ray_tracing_job_runner_file_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job id collision");
        return false;
    }
    if (!ray_tracing_job_runner_ensure_directory_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to create job directory");
        return false;
    }
    if (!write_canonical_request_file(paths.job_request_path, &request, &paths)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to stage job request");
        return false;
    }

    if (!ray_tracing_detached_job_record_init_queued(
            &record,
            &paths,
            out_job_id,
            request.output_root,
            overwrite ? "overwrite" : (resume ? "resume" : "fail_if_exists"),
            request.start_frame - (request.has_sampling_window ? request.sampling_frame_offset : 0),
            request.has_sampling_window ? request.sampling_frame_count : request.frame_count,
            request.start_frame,
            request.frame_count,
            request.temporal_frames > 0 ? request.temporal_frames : 1)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to initialize job status");
        return false;
    }
    if (!build_default_shared_job_envelope(&request,
                                           &paths,
                                           out_job_id,
                                           record.submitted_at_utc,
                                           &shared_job)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to build shared outer job envelope");
        return false;
    }
    if (is_shared_bundle) {
        copy_string(shared_job.scene_payload.schema_family,
                    sizeof(shared_job.scene_payload.schema_family),
                    source_bundle.envelope.scene_payload.schema_family);
        copy_string(shared_job.scene_payload.schema_variant,
                    sizeof(shared_job.scene_payload.schema_variant),
                    source_bundle.envelope.scene_payload.schema_variant);
        copy_string(shared_job.run_config.schema_family,
                    sizeof(shared_job.run_config.schema_family),
                    source_bundle.envelope.run_config.schema_family);
        copy_string(shared_job.run_config.schema_variant,
                    sizeof(shared_job.run_config.schema_variant),
                    source_bundle.envelope.run_config.schema_variant);
        if (!copy_string(shared_job.metadata.title,
                         sizeof(shared_job.metadata.title),
                         source_bundle.envelope.metadata.title) ||
            !copy_string(shared_job.metadata.description,
                         sizeof(shared_job.metadata.description),
                         source_bundle.envelope.metadata.description) ||
            !copy_string(shared_job.metadata.created_by,
                         sizeof(shared_job.metadata.created_by),
                         source_bundle.envelope.metadata.created_by) ||
            !copy_string(shared_job.metadata.created_at,
                         sizeof(shared_job.metadata.created_at),
                         source_bundle.envelope.metadata.created_at)) {
            diag_set(out_diagnostics, out_diagnostics_size, "failed to preserve shared job metadata");
            return false;
        }
    }
    if (!ray_tracing_headless_job_bundle_write(paths.shared_job_path,
                                               &shared_job,
                                               diagnostics,
                                               sizeof(diagnostics))) {
        diag_set(out_diagnostics, out_diagnostics_size, diagnostics);
        return false;
    }
    if (!ray_tracing_job_runner_persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to write queued job status");
        return false;
    }

    if (!spawn_detached_render(render_cli_path, &paths, out_job_id, &pid)) {
        ray_tracing_detached_job_record_mark_spawn_failed(&record, &paths, render_cli_path);
        (void)ray_tracing_job_runner_persist_job_state(&paths, &record);
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "failed to spawn detached render cli=%s job_status=%s stdout=%s stderr=%s",
                     render_cli_path,
                     paths.job_status_path,
                     paths.stdout_log_path,
                     paths.stderr_log_path);
        }
        return false;
    }

    ray_tracing_detached_job_record_mark_started(&record, pid);
    if (!ray_tracing_job_runner_write_pid_file(paths.pid_path, pid) ||
        !ray_tracing_job_runner_persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to persist detached job state");
        return false;
    }

    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_job_runner_print_status(FILE *out,
                                         const char *argv0,
                                         const char *job_id,
                                         const char *jobs_root_override,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    RayTracingDetachedJobPaths paths;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!out || !argv0 || !job_id || !job_id[0]) return false;
    if (!ray_tracing_job_runner_build_jobs_root(argv0,
                                                jobs_root_override,
                                                jobs_root,
                                                sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    if (!ray_tracing_job_runner_build_job_paths(jobs_root, job_id, &paths)) {
        diag_set(out_diagnostics, out_diagnostics_size, "invalid job id or job path");
        return false;
    }
    if (!ray_tracing_job_runner_file_exists(paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job status file not found");
        return false;
    }
    {
        RayTracingDetachedJobRecord record;
        if (ray_tracing_job_runner_load_job_status_record(&paths, &record)) {
            (void)ray_tracing_job_runner_refresh_job_status_record(&paths, &record);
        }
    }
    if (!ray_tracing_job_runner_print_file_to_stream(out, paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read job status file");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_job_runner_cancel(const char *argv0,
                                   const char *job_id,
                                   const char *jobs_root_override,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    RayTracingDetachedJobPaths paths;
    RayTracingDetachedJobRecord record;
    pid_t pid = 0;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !job_id || !job_id[0]) return false;
    if (!ray_tracing_job_runner_build_jobs_root(argv0,
                                                jobs_root_override,
                                                jobs_root,
                                                sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    if (!ray_tracing_job_runner_build_job_paths(jobs_root, job_id, &paths)) {
        diag_set(out_diagnostics, out_diagnostics_size, "invalid job id or job path");
        return false;
    }
    if (!ray_tracing_job_runner_read_job_pid(&paths, &pid)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read job pid");
        return false;
    }
    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to signal job pid");
        return false;
    }
    ray_tracing_detached_job_record_defaults(&record);
    (void)ray_tracing_job_runner_load_job_status_record(&paths, &record);
    ray_tracing_detached_job_record_mark_cancelled(&record, &paths, job_id, pid);
    if (!ray_tracing_job_runner_persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to update cancelled job status");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
