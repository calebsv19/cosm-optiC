#include "render/runtime_spectral_convergence.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* All state belongs to this one evaluated camera frame. No accumulated pixel
 * or pilot mask survives a camera/scene/time change or a canceled render. */
typedef struct {
  Vec3 sum[2];
  uint32_t count[2];
} Pixel;
typedef struct {
  Pixel *pixels;
  float *sample;
  uint8_t *edge, *mask, *visible;
  size_t count;
} Film;
static double display(double x) {
  x = fmin(1, fmax(0, x));
  return 255 * (x <= .0031308 ? 12.92 * x : 1.055 * pow(x, 1 / 2.4) - .055);
}
static double luminance(Vec3 p) {
  return .2126729 * p.x + .7151522 * p.y + .072175 * p.z;
}
static Vec3 mean(const Pixel *p, int group) {
  return p->count[group] ? vec3_scale(p->sum[group], 1.0 / p->count[group])
                         : vec3(0, 0, 0);
}
static bool canceled(const RuntimeNative3DTileSchedulerCancelToken *token) {
  return token && RuntimeNative3DTileSchedulerCancelToken_IsRequested(token);
}
static void release(Film *f) {
  free(f->pixels);
  free(f->sample);
  free(f->edge);
  free(f->mask);
  free(f->visible);
}
static bool allocate(Film *f, int width, int height) {
  if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / (size_t)height)
    return false;
  f->count = (size_t)width * height;
  size_t per_pixel = sizeof(Pixel) + 4 * sizeof(float) + 3;
  if (f->count > 512u * 1024u * 1024u / per_pixel)
    return false;
  f->pixels = calloc(f->count, sizeof(Pixel));
  f->sample = calloc(f->count, 4 * sizeof(float));
  f->edge = calloc(f->count, 1);
  f->mask = calloc(f->count, 1);
  f->visible = calloc(f->count, 1);
  return f->pixels && f->sample && f->edge && f->mask && f->visible;
}
static void pilot_mask(Film *f, int w, int h,
                       RuntimeSpectralSamplingStats *st) {
  /* Pilot is an independent photon map excluded from the final estimator.
   * Expand visible color/luminance edges by three pixels, including borders. */
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      size_t i = (size_t)y * w + x;
      double peak = 0, gradient = 0;
      for (int c = 0; c < 3; ++c) {
        double a = display(f->sample[4 * i + c]);
        peak = fmax(peak, a);
        if (x + 1 < w)
          gradient =
              fmax(gradient, fabs(a - display(f->sample[4 * (i + 1) + c])));
        if (y + 1 < h)
          gradient =
              fmax(gradient, fabs(a - display(f->sample[4 * (i + w) + c])));
      }
      f->visible[i] = peak > .5;
      if (gradient > 2 || peak > 240) {
        int ya = y > 3 ? y - 3 : 0, yb = y + 3 < h ? y + 3 : h - 1,
            xa = x > 3 ? x - 3 : 0, xb = x + 3 < w ? x + 3 : w - 1;
        for (int yy = ya; yy <= yb; ++yy)
          for (int xx = xa; xx <= xb; ++xx)
            f->edge[(size_t)yy * w + xx] = 1;
      }
    }
  for (size_t i = 0; i < f->count; ++i)
    st->edge_pixels += f->edge[i] != 0;
}
static void resolve(Film *f, uint8_t *pixels) {
  for (size_t i = 0; i < f->count; ++i)
    RuntimeSpectral_DisplayPixel(
        pixels + 4 * i,
        vec3_scale(vec3_add(mean(&f->pixels[i], 0), mean(&f->pixels[i], 1)),
                   .5));
}
static void measure(Film *f, const RuntimeSpectralSamplingConfig *config,
                    RuntimeSpectralSamplingStats *st) {
  double lit_error = 0, edge_error = 0, total_y = 0, delta_y = 0;
  size_t lit = 0, edges = 0;
  for (size_t i = 0; i < f->count; ++i) {
    Vec3 a = mean(&f->pixels[i], 0), b = mean(&f->pixels[i], 1);
    double dx = (display(a.x) - display(b.x)) * .5,
           dy = (display(a.y) - display(b.y)) * .5,
           dz = (display(a.z) - display(b.z)) * .5;
    double error = (dx * dx + dy * dy + dz * dz) / 3;
    /* Include newly illuminated pixels as well as the independent pilot. */
    bool visible = f->visible[i] ||
                   fmax(display(a.x), fmax(display(a.y), display(a.z))) > .5 ||
                   fmax(display(b.x), fmax(display(b.y), display(b.z))) > .5;
    if (visible) {
      lit_error += error;
      ++lit;
      if (f->edge[i]) {
        edge_error += error;
        ++edges;
      }
    }
    total_y += (luminance(a) + luminance(b)) * .5;
    delta_y += (luminance(a) - luminance(b)) * .5;
  }
  st->visible_error = lit ? sqrt(lit_error / lit) : 0;
  st->edge_error = edges ? sqrt(edge_error / edges) : 0;
  st->energy_error = fabs(delta_y) / fmax(fabs(total_y), 1e-20);
  ++st->checkpoints;
  bool pass = st->visible_error <= config->visible_error &&
              st->edge_error <= config->edge_error &&
              st->energy_error <= config->energy_error;
  st->consecutive = pass ? st->consecutive + 1 : 0;
  st->converged = st->passes >= config->min_passes && st->consecutive >= 2;
}
static bool sample_map(const RuntimeSpectralFrame *source,
                       const RuntimeNative3DPreparedFrame *frame, Film *film,
                       double dx, double dy, const uint8_t *mask,
                       const RuntimeNative3DTileSchedulerCancelToken *cancel,
                       RuntimeSpectralReadout *readout,
                       RuntimeNative3DTileSchedulerProgressCallback progress,
                       void *user, int completed, int started, char *diag,
                       size_t size) {
  RuntimeSpectralSamplingStats previous = readout->sampling;
  *readout = (RuntimeSpectralReadout){.map = source->stats, .sampling = previous};
  for (int y = 0; y < frame->height; y += 8) {
    if (canceled(cancel))
      return false;
    int end = y + 8 < frame->height ? y + 8 : frame->height;
    if (!RuntimeSpectralFilm_Sample(&source->profile, source->map, frame, y,
                                    end, dx, dy, mask, film->sample, readout,
                                    diag, size))
      return false;
    if (progress) {
      RuntimeNative3DTileSchedulerProgress p = {
          .startedSubpasses = started,
          .completedSubpasses = completed,
          .totalSubpasses = source->profile.sampling.max_passes
                                ? source->profile.sampling.max_passes
                                : completed + 8,
          .completedTilesInSubpass = (size_t)(end + 7) / 8,
          .totalTilesInSubpass = (size_t)(frame->height + 7) / 8};
      if (!progress(&p, user)) {
        snprintf(diag, size, "Spectral progress publication failed.");
        return false;
      }
    }
  }
  return true;
}
bool RuntimeSpectralConvergence_Render(
    const RuntimeSpectralPencil *profile,
    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
    const RuntimeNative3DTileSchedulerCancelToken *cancel,
    RuntimeNative3DTileSchedulerProgressCallback progress, void *user,
    RuntimeSpectralReadout *out, char *diag, size_t size) {
  Film film = {0};
  RuntimeSpectralSamplingStats st = {.enabled = true};
  const RuntimeScene3D *scene =
      frame->traceScene ? frame->traceScene : &frame->scene;
  RuntimeSpectralPencil current = *profile;
  RuntimeSpectralFrame *map = NULL;
  bool ok = false;
  Vec3 center_sum = vec3(0, 0, 0);
  double sensors[4] = {0};
  unsigned center_count = 0;
  if (!profile->sampling.enabled || !profile->map.enabled ||
      !allocate(&film, frame->width, frame->height)) {
    snprintf(diag, size,
             "V3 convergence requires a photon map and at most 512 MiB of film "
             "storage.");
    goto done;
  }
  out->sampling = st;
  /* Reserve a distinct seed for the pilot. Each following map uses a bijective
   * unsigned sequence, preserving identical camera-frame replay. */
  current.map.seed = profile->map.seed ^ UINT32_C(0xb5297a4d);
  if (canceled(cancel))
    goto done;
  if (progress) {
    RuntimeNative3DTileSchedulerProgress preparing = {
        .startedSubpasses = st.passes ? st.passes + 1 : 0,
        .completedSubpasses = st.passes,
        .totalSubpasses = profile->sampling.max_passes};
    out->sampling = st;
    if (!progress(&preparing, user)) {
      snprintf(diag,size,"Spectral progress publication failed."); goto done;
    }
  }
  map = RuntimeSpectralFrame_Create(&current, scene, diag, size);
  if (!map || !sample_map(map, frame, &film, 0, 0, NULL, cancel, out, progress,
                          user, 0, 0, diag, size))
    goto done;
  pilot_mask(&film, frame->width, frame->height, &st);
  RuntimeSpectralFrame_Free(map);
  map = NULL;
  for (int pass = 0;; ++pass) {
    if (canceled(cancel))
      goto done;
    int batch = pass / 8;
    bool full = batch % 4 == 0;
    /* A shared mask gives equal sample counts in both four-quadrant groups.
     * Periodic full batches also refresh low-signal and newly visible areas. */
    for (int y = 0; y < frame->height; ++y)
      for (int x = 0; x < frame->width; ++x) {
        size_t i = (size_t)y * frame->width + x;
        film.mask[i] = full || film.edge[i] ||
                       (x % 6 == batch % 6 && y % 6 == (batch / 6) % 6);
      }
    current.map.seed =
        profile->map.seed + UINT32_C(0x9e3779b9) * (unsigned)(pass + 1);
    if (progress) {
      RuntimeNative3DTileSchedulerProgress preparing = {
          .startedSubpasses = pass + 1,
          .completedSubpasses = st.passes,
          .totalSubpasses = profile->sampling.max_passes};
      out->sampling = st;
      if (!progress(&preparing, user)) {
        snprintf(diag, size, "Spectral progress publication failed.");
        goto done;
      }
    }
    map = RuntimeSpectralFrame_Create(&current, scene, diag, size);
    if (!map)
      goto done;
    double dx, dy;
    RuntimeSpectralSampling_Jitter(profile->map.seed, pass, &dx, &dy);
    if (!sample_map(map, frame, &film, dx, dy, film.mask, cancel, out, progress,
                    user, st.passes, st.passes + 1, diag, size))
      goto done;
    size_t center =
        (size_t)(frame->height / 2) * frame->width + frame->width / 2;
    if (film.mask[center]) {
      center_sum = vec3_add(center_sum, out->center_pixel_xyz);
      ++center_count;
      for (int i = 0; i < 4; ++i)
        sensors[i] += out->sensor_signal[i];
    }
    int group = (pass % 8) / 4;
    for (size_t i = 0; i < film.count; ++i)
      if (film.mask[i]) {
        Vec3 value = vec3(film.sample[4 * i], film.sample[4 * i + 1],
                          film.sample[4 * i + 2]);
        if (!isfinite(value.x) || !isfinite(value.y) || !isfinite(value.z)) {
          snprintf(diag, size, "Non-finite spectral camera contribution.");
          goto done;
        }
        film.pixels[i].sum[group] = vec3_add(film.pixels[i].sum[group], value);
        ++film.pixels[i].count[group];
        ++st.camera_samples;
      }
    ++st.passes;
    if (full)
      ++st.full_passes;
    RuntimeSpectralFrame_Free(map);
    map = NULL;
    if (st.passes % 8 == 0) {
      resolve(&film, pixels);
      measure(&film, &profile->sampling, &st);
      st.pass_limit = !st.converged && profile->sampling.max_passes &&
                      st.passes >= profile->sampling.max_passes;
      out->sampling = st;
      if (progress) {
        IntegratorTile tile = {.originX = 0,
                               .originY = 0,
                               .width = frame->width,
                               .height = frame->height};
        RuntimeNative3DTileSchedulerProgress p = {
            .dirtyTiles = &tile,
            .dirtyTileCount = 1,
            .startedSubpasses = st.passes,
            .completedSubpasses = st.passes,
            .totalSubpasses = profile->sampling.max_passes
                                  ? profile->sampling.max_passes
                                  : st.passes + 8,
            .completedTilesInSubpass = 1,
            .totalTilesInSubpass = 1};
        if (!progress(&p, user)) {
          snprintf(diag, size, "Spectral progress publication failed.");
          goto done;
        }
      }
      if (st.converged || st.pass_limit) {
        ok = true;
        break;
      }
    }
  }
done:
  st.canceled = canceled(cancel);
  out->sampling = st;
  if (center_count) {
    out->center_pixel_xyz = vec3_scale(center_sum, 1.0 / center_count);
    for (int i = 0; i < 4; ++i)
      out->sensor_signal[i] = sensors[i] / center_count;
  }
  if (st.canceled)
    snprintf(diag, size, "Spectral convergence canceled.");
  RuntimeSpectralFrame_Free(map);
  release(&film);
  return ok && !st.canceled;
}
