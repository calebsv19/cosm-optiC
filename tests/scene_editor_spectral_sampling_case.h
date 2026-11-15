#include "render/runtime_spectral_convergence.h"

static bool
spectral_cancel_checkpoint(const RuntimeNative3DTileSchedulerProgress *progress,
                           void *user) {
  if (progress->completedSubpasses == 8)
    atomic_store((atomic_bool *)user, true);
  return true;
}
static void
spectral_sampling_frame_checks(RuntimeNative3DPreparedFrame *frame) {
  assert(frame->spectralFrame && frame->spectralFrame->profile.sampling.enabled);
  RuntimeSpectralPencil profile = frame->spectralFrame->profile;
  profile.map.samples = 1024;
  profile.sampling = RuntimeSpectralSampling_Defaults();
  profile.sampling.max_passes = 16;
  profile.sampling.visible_error = profile.sampling.edge_error =
      profile.sampling.local_error = 255;
  profile.sampling.energy_error = 1;
  size_t bytes = (size_t)frame->width * frame->height * 4;
  uint8_t *a = malloc(bytes), *b = malloc(bytes);
  assert(a && b);
  RuntimeSpectralReadout result = {0};
  char message[256];
  assert(RuntimeSpectralConvergence_Render(&profile, frame, a, NULL, NULL, NULL,
                                           &result, message, sizeof(message)));
  assert(result.sampling.converged && result.sampling.passes == 16);
  frame->spectralFrame->profile=profile;
  RuntimeNative3DRenderStats native_stats={0};
  assert(RuntimeNative3DRenderPreparedFrameTemporalTiled(b,RAY_TRACING_3D_INTEGRATOR_DISNEY_V3_SPECTRAL,frame,1,NULL,NULL,&native_stats));
  assert(native_stats.temporalCommittedSubpasses==16 && !memcmp(a,b,bytes));
  Vec3 original = frame->projector.origin;
  frame->projector.origin.y += .008;
  frame->projector.origin.z += .02;
  assert(RuntimeSpectralConvergence_Render(&profile, frame, b, NULL, NULL, NULL,
                                           &result, message, sizeof(message)));
  assert(result.sampling.passes == 16 && memcmp(a, b, bytes));
  frame->projector.origin = original;
  assert(RuntimeSpectralConvergence_Render(&profile, frame, b, NULL, NULL, NULL,
                                           &result, message, sizeof(message)));
  assert(result.sampling.passes == 16 && !memcmp(a, b, bytes));
  atomic_bool stop = false;
  RuntimeNative3DTileSchedulerCancelToken token = {.cancelRequested = &stop,
                                                   .generation = 1};
  assert(!RuntimeSpectralConvergence_Render(&profile, frame, b, &token,
                                            spectral_cancel_checkpoint, &stop,
                                            &result, message, sizeof(message)));
  assert(result.sampling.canceled && result.sampling.passes == 8 &&
         !result.sampling.converged);
  assert(!RuntimeSpectralConvergence_Render(&profile, frame, b, &token, NULL,
                                            NULL, &result, message,
                                            sizeof(message)));
  assert(result.sampling.canceled && result.sampling.passes == 0);
  free(a);
  free(b);
  puts("PASS same-process V3 camera movement, reset, replay, checkpoint cancel "
       "and pre-start cancel");
}
