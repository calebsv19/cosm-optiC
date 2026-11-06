#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <time.h>

double ray_tracing_elapsed_seconds_since(const struct timespec *start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec);
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

double ray_tracing_elapsed_ms_since(const struct timespec *start_time) {
    return ray_tracing_elapsed_seconds_since(start_time) * 1000.0;
}

static double ray_tracing_estimate_remaining_seconds(double elapsed_seconds,
                                                     int started_subpasses,
                                                     int completed_subpasses,
                                                     int total_subpasses,
                                                     size_t completed_tiles_in_subpass,
                                                     size_t total_tiles_in_subpass) {
    double progress = 0.0;
    if (elapsed_seconds <= 0.0 || total_subpasses <= 0) return -1.0;
    progress = (double)completed_subpasses / (double)total_subpasses;
    if (total_tiles_in_subpass > 0u && started_subpasses > completed_subpasses) {
        progress += ((double)completed_tiles_in_subpass / (double)total_tiles_in_subpass) /
                    (double)total_subpasses;
    }
    if (progress <= 0.001) return -1.0;
    if (progress >= 1.0) return 0.0;
    return (elapsed_seconds / progress) - elapsed_seconds;
}

void ray_tracing_temporal_progress_callback(int started_subpasses,
                                            int completed_subpasses,
                                            int total_subpasses,
                                            void *user_data) {
    RayTracingTemporalProgressContext *context =
        (RayTracingTemporalProgressContext *)user_data;
    char diagnostics[128];
    if (!context || !context->request) return;
    context->started_subpasses = started_subpasses;
    context->completed_subpasses = completed_subpasses;
    context->total_subpasses = total_subpasses;
    if (completed_subpasses < started_subpasses) {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d active)",
                 started_subpasses,
                 total_subpasses);
    } else {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d committed)",
                 completed_subpasses,
                 total_subpasses);
    }
    (void)ray_tracing_render_headless_write_progress_and_job_status(context->request->progress_path,
                                        context->request,
                                        "rendering_frame",
                                        context->frame_index,
                                        context->frames_completed,
                                        started_subpasses,
                                        completed_subpasses,
                                        total_subpasses,
                                        context->completed_tiles_in_subpass,
                                        context->total_tiles_in_subpass,
                                        ray_tracing_elapsed_seconds_since(&context->frame_started_at),
                                        ray_tracing_estimate_remaining_seconds(
                                            ray_tracing_elapsed_seconds_since(&context->frame_started_at),
                                            started_subpasses,
                                            completed_subpasses,
                                            total_subpasses,
                                            context->completed_tiles_in_subpass,
                                            context->total_tiles_in_subpass),
                                        "running",
                                        diagnostics,
                                        context->job_status_path,
                                        context->job_id,
                                        context->request_path,
                                        -1);
}

void ray_tracing_tile_progress_callback(int started_subpasses,
                                        int completed_subpasses,
                                        int total_subpasses,
                                        size_t completed_tiles_in_subpass,
                                        size_t total_tiles_in_subpass,
                                        void *user_data) {
    RayTracingTemporalProgressContext *context =
        (RayTracingTemporalProgressContext *)user_data;
    char diagnostics[160];
    double elapsed_seconds = 0.0;
    double estimated_remaining_seconds = 0.0;
    if (!context || !context->request) return;
    context->started_subpasses = started_subpasses;
    context->completed_subpasses = completed_subpasses;
    context->total_subpasses = total_subpasses;
    context->completed_tiles_in_subpass = completed_tiles_in_subpass;
    context->total_tiles_in_subpass = total_tiles_in_subpass;
    elapsed_seconds = ray_tracing_elapsed_seconds_since(&context->frame_started_at);
    estimated_remaining_seconds = ray_tracing_estimate_remaining_seconds(
        elapsed_seconds,
        started_subpasses,
        completed_subpasses,
        total_subpasses,
        completed_tiles_in_subpass,
        total_tiles_in_subpass);
    snprintf(diagnostics,
             sizeof(diagnostics),
             "rendering frame (subpass %d/%d, tiles %zu/%zu)",
             started_subpasses > 0 ? started_subpasses : 1,
             total_subpasses > 0 ? total_subpasses : 1,
             completed_tiles_in_subpass,
             total_tiles_in_subpass);
    (void)ray_tracing_render_headless_write_progress_and_job_status(
        context->request->progress_path,
        context->request,
        "rendering_frame",
        context->frame_index,
        context->frames_completed,
        started_subpasses,
        completed_subpasses,
        total_subpasses,
        completed_tiles_in_subpass,
        total_tiles_in_subpass,
        elapsed_seconds,
        estimated_remaining_seconds,
        "running",
        diagnostics,
        context->job_status_path,
        context->job_id,
        context->request_path,
        -1);
}

/* V3 convergence publishes through the existing durable CLI progress owner.
 * The companion snapshot is overwritten atomically; stderr keeps checkpoint
 * history. */
#include "app/ray_tracing_durable_io.h"
#include "render/runtime_spectral_convergence.h"
#include "tools/ray_tracing_headless_spectral_map.h"
#include <string.h>

typedef struct {
  const RayTracingAgentRenderRequest *request;
  const RuntimeNative3DPreparedFrame *frame;
  RuntimeSpectralReadout *readout;
  struct timespec start;
  double last;
  int started;
  bool published;
} SpectralProgress;
static bool spectral_progress(const RuntimeNative3DTileSchedulerProgress *p,
                              void *user) {
  SpectralProgress *c = user;
  double elapsed = ray_tracing_elapsed_seconds_since(&c->start);
  bool checkpoint = p->dirtyTileCount != 0;
  bool preparing = p->totalTilesInSubpass == 0;
  if (c->published && !checkpoint && !preparing &&
      c->started == p->startedSubpasses && elapsed - c->last < 1)
    return true;
  c->last = elapsed;
  c->started = p->startedSubpasses;
  c->published = true;
  const RuntimeSpectralSamplingStats *s = &c->readout->sampling;
  const char *stage = checkpoint  ? "checkpoint"
                      : preparing ? "preparing_map"
                                  : "sampling";
  int frame_index =
      c->frame->evaluatedSceneBound
          ? (int)c->frame->evaluatedScene.frame.sample.absolute_frame
          : c->request->start_frame;
  json_object *obj = json_object_new_object();
  if (!obj)
    return false;
  json_object_object_add(obj, "schema_version",
                         json_object_new_string("optic_spectral_progress_v1"));
  json_object_object_add(obj, "run_id",
                         json_object_new_string(c->request->run_id));
  json_object_object_add(obj, "frame_index", json_object_new_int(frame_index));
  json_object_object_add(obj, "stage", json_object_new_string(stage));
  json_object_object_add(obj, "elapsed_seconds",
                         json_object_new_double(elapsed));
  json_object_object_add(obj, "started_passes",
                         json_object_new_int(p->startedSubpasses));
  json_object_object_add(obj, "completed_tiles",
                         json_object_new_int64(p->completedTilesInSubpass));
  json_object_object_add(obj, "total_tiles",
                         json_object_new_int64(p->totalTilesInSubpass));
  json_object_object_add(
      obj, "max_passes",
      json_object_new_int(c->request->spectral.sampling.max_passes));
  json_object_object_add(obj, "sampling", RuntimeSpectralSampling_JSON(s));
  const char *text =
      json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
  bool ok = text != NULL;
  if (ok && c->request->progress_path[0]) {
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s.spectral.json",
                     c->request->progress_path);
    ok = n > 0 && (size_t)n < sizeof(path);
    if (ok)
      ok = ray_tracing_render_headless_write_progress_and_job_status(
          c->request->progress_path, c->request, "spectral_convergence",
          frame_index, frame_index - c->request->start_frame,
          p->startedSubpasses, s->passes,
          c->request->spectral.sampling.max_passes, p->completedTilesInSubpass,
          p->totalTilesInSubpass, elapsed, -1, "running", text, NULL, NULL,
          NULL, 0);
    if (ok) {
      RayTracingDurableOutput output;
      ok = ray_tracing_durable_output_begin(&output, path);
      if (ok) {
        bool written = fprintf(output.stream, "%s\n", text) >= 0;
        if (written)
          ok = ray_tracing_durable_output_commit(&output);
        else {
          ray_tracing_durable_output_abort(&output);
          ok = false;
        }
      }
    }
  }
  if (ok)
    ok = fprintf(stderr, "SPECTRAL_PROGRESS %s\n", text) >= 0 &&
         fflush(stderr) == 0;
  json_object_put(obj);
  return ok;
}
bool RayTracingHeadlessSpectralMap_Converge(
    const RayTracingAgentRenderRequest *request,
    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
    RuntimeSpectralReadout *out, char *diag, size_t size) {
  SpectralProgress context = {
      .request = request, .frame = frame, .readout = out};
  clock_gettime(CLOCK_MONOTONIC, &context.start);
  return RuntimeSpectralConvergence_Render(&request->spectral, frame, pixels,
                                           NULL, spectral_progress, &context,
                                           out, diag, size);
}
