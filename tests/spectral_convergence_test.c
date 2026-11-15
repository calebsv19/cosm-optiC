/* Deterministic film producer isolates scheduling/acceptance from photon noise.
 * The real convergence renderer and parser are linked unchanged. */
#include "render/runtime_spectral_convergence.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int serial, scenario, freed, calls;
static unsigned expected_bundle;
static bool seen[2][128];
static bool stop_requested;
RuntimeSpectralFrame *
RuntimeSpectralFrame_Create(const RuntimeSpectralPencil *p,
                            const RuntimeScene3D *scene, char *diag,
                            size_t size) {
  (void)scene;
  (void)diag;
  (void)size;
  RuntimeSpectralFrame *f = calloc(1, sizeof(*f));
  assert(f);
  f->profile = *p;
  ++serial;
  return f;
}
void RuntimeSpectralFrame_Free(RuntimeSpectralFrame *f) { if (f) ++freed; free(f); }
bool RuntimeNative3DTileSchedulerCancelToken_IsRequested(
    const RuntimeNative3DTileSchedulerCancelToken *token) {
  (void)token;
  return stop_requested;
}
static unsigned char code(double v) {
  v = fmin(1, fmax(0, v));
  return (unsigned char)lround(
      255 * (v <= .0031308 ? 12.92 * v : 1.055 * pow(v, 1 / 2.4) - .055));
}
void RuntimeSpectral_DisplayPixel(uint8_t *p, Vec3 v) {
  p[0] = code(v.x);
  p[1] = code(v.y);
  p[2] = code(v.z);
  p[3] = 255;
}
bool RuntimeSpectralFilm_Sample(
    const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
    const RuntimeNative3DPreparedFrame *f, int y0, int y1, double dx, double dy,
    const RuntimeNative3DSamplingContext *camera_sampling, const uint8_t *mask,
    float *sample, RuntimeSpectralReadout *out, char *diag, size_t size) {
  if (y0 == 0 && serial > 1) {
    ++calls;
    if (expected_bundle > 1) {
      assert(camera_sampling);
      unsigned group = (unsigned)(serial-2)%8/4;
      unsigned index = camera_sampling->temporalSubpassIndex;
      assert(index < 128 && !seen[group][index]);
      seen[group][index] = true;
    }
  }
  if (scenario == 4 && serial == 2 && calls == 2) stop_requested = true;
  if (scenario == 5 && serial == 2 && calls == 2) return false;
  (void)p;
  (void)map;
  (void)dx;
  (void)dy;
  (void)out;
  (void)diag;
  (void)size;
  for (int y = y0; y < y1; ++y)
    for (int x = 0; x < f->width; ++x) {
      size_t i = (size_t)y * f->width + x;
      if (mask && !mask[i])
        continue;
      double v = scenario == 1 && serial > 9 ? .02 : .01;
      if ((scenario == 2 || scenario == 6) && serial > 1 && x >= 32 && x < 48 && y >= 32 && y < 48)
        v = ((serial - 2) % 8) / 4 ? .02 : .005;
      if (scenario == 3 && serial > 1)
        v = .01 * (1 + camera_sampling->temporalSubpassIndex % 4);
      sample[4 * i] = sample[4 * i + 1] = sample[4 * i + 2] = (float)v;
    }
  return true;
}
static RuntimeSpectralReadout run(int which, bool full, unsigned bundle) {
  serial = freed = calls = 0;
  memset(seen, 0, sizeof(seen));
  expected_bundle = bundle;
  scenario = which;
  stop_requested = false;
  RuntimeSpectralPencil p = {0};
  p.map.enabled = true;
  p.sampling = RuntimeSpectralSampling_Defaults();
  p.sampling.max_passes = 16;
  p.sampling.camera_samples_per_map = bundle;
  p.sampling.progressive_camera = bundle > 1;
  p.sampling.full_camera = full;
  p.sampling.visible_error = 255;
  p.sampling.edge_error = 255;
  p.sampling.energy_error = 1;
  p.sampling.local_error = 1;
  RuntimeNative3DPreparedFrame *f = calloc(1, sizeof(*f));
  assert(f);
  f->width = f->height = 64;
  uint8_t pixels[64 * 64 * 4];
  memset(pixels, 23, sizeof(pixels));
  RuntimeSpectralReadout out = {0};
  char diag[256];
  RuntimeNative3DTileSchedulerCancelToken token = {0};
  bool ok = RuntimeSpectralConvergence_Render(&p, f, pixels, &token, NULL, NULL,
                                              &out, diag, sizeof(diag));
  assert(ok == (which != 4 && which != 5));
  assert(freed == serial);
  if (which == 4 || which == 5) {
    assert(out.sampling.passes == 0 && out.sampling.camera_samples == 0);
    assert(out.sampling.checkpoints == 0 && !out.sampling.converged);
    assert(out.sampling.canceled == (which == 4));
    for (size_t i=0;i<sizeof(pixels);++i) assert(pixels[i]==23);
  } else {
    assert(serial == 17 && calls == 16*(int)bundle);
    assert(out.sampling.min_pixel_maps * bundle == out.sampling.min_pixel_samples);
    assert(out.sampling.max_pixel_maps * bundle == out.sampling.max_pixel_samples);
  }
  if (which == 3) {
    assert(out.sampling.converged && out.sampling.local_error == 0);
    assert(pixels[0] == code(.025));
    assert(out.sampling.passes == 16 && out.sampling.checkpoints == 2);
    assert(out.sampling.camera_samples == 16*64*64*bundle);
  }
  if (which == 1)
    assert(pixels[(20 * 64 + 20) * 4] == code(.015));
  free(f);
  return out;
}
int main(void) {
  RuntimeSpectralReadout r = run(1, false, 1);
  assert(r.sampling.converged && r.sampling.full_passes == 16);
  assert(r.sampling.full_checkpoints == 2 &&
         r.sampling.min_pixel_samples == 16);
  assert(r.sampling.camera_samples == 16 * 64 * 64);
  r = run(2, false, 1);
  assert(!r.sampling.converged && r.sampling.pass_limit &&
         r.sampling.local_error > 1);
  assert(r.sampling.full_passes == 8 && r.sampling.min_pixel_samples == 8);
  assert(r.sampling.visible_error < r.sampling.local_error);
  r = run(1, true, 1);
  assert(r.sampling.converged && r.sampling.min_pixel_samples == 16);
  double map_error = run(2, true, 1).sampling.local_error;
  for (unsigned bundle=4;bundle<=8;bundle*=2) {
    run(3, true, bundle);
    RuntimeSpectralReadout noisy = run(6, true, bundle);
    assert(!noisy.sampling.converged && noisy.sampling.pass_limit);
    assert(fabs(noisy.sampling.local_error-map_error)<1e-10);
    run(4, true, bundle);
    run(5, true, bundle);
  }
  puts("PASS bundle means, map-noise refusal, independent counts, cancellation and failure atomicity");
  puts("PASS fresh full-camera acceptance, delayed interior signal, local haze "
       "refusal and full reference coverage");
}
