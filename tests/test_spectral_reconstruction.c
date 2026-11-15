/* Native estimator tests: independent integration and HG oracle, no image-color
 * clipping. */
#include "render/runtime_spectral_reconstruction_internal.h"
#include "render/runtime_spectral_volume.h"
#include "render/runtime_volume_3d_sampling.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RuntimeSpectralVolume *volume(int mode) {
  RuntimeSpectralVolumeConfig cfg = {.kind = SPECTRAL_VOLUME_BOX,
                                     .reconstruction = mode,
                                     .phase_tolerance = .01,
                                     .cell_m = .1,
                                     .lo = {0, 0, 0},
                                     .hi = {1, 1, 1}};
  char diag[192];
  RuntimeSpectralVolume *v =
      RuntimeSpectralVolume_Load(&cfg, diag, sizeof(diag));
  assert(v);
  return v;
}
static RuntimeScene3D plane_scene(RuntimePrimitive3D *p, double x,
                                  double width) {
  *p = (RuntimePrimitive3D){.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE,
                            .shape.plane = {.origin = {x, .5, .5},
                                            .normal = {1, 0, 0},
                                            .axisU = {0, 1, 0},
                                            .axisV = {0, 0, 1},
                                            .width = width,
                                            .height = 2}};
  return (RuntimeScene3D){.primitives = p, .primitiveCount = 1};
}
static void event(RuntimeSpectralVolume *v, Vec3 p) {
  v->count = v->capacity = 1;
  v->points = calloc(1, sizeof(*v->points));
  v->positions = malloc(sizeof(*v->positions));
  v->heads = calloc(65536, sizeof(int));
  assert(v->points && v->positions && v->heads);
  uint64_t cell = ((uint64_t)floor(p.x / v->config.cell_m) << 24) |
                  ((uint64_t)floor(p.y / v->config.cell_m) << 12) |
                  (uint64_t)floor(p.z / v->config.cell_m);
  v->positions[0] = p;
  v->points[0] = (SpectralCollision){
      .incoming = {0, 0, 1}, .watts = 1, .first = true, .cell = cell};
  v->heads[spectral_volume_bucket(cell, 0)] = 1;
}
static double sample(RuntimeSpectralVolume *v, Vec3 p) {
  double first = 0;
  return spectral_reconstruction_source(v, p, 0, &first);
}
static double integral(RuntimeSpectralVolume *v, double split) {
  double knots[23];
  int n = 0;
  for (int i = 0; i <= 20; i++)
    knots[n++] = i * .05;
  if (split > 0 && split < 1) {
    int i = 0;
    while (knots[i] < split)
      i++;
    if (knots[i] != split) {
      memmove(knots + i + 1, knots + i, (n - i) * sizeof(double));
      knots[i] = split;
      n++;
    }
  }
  const double q[] = {.21132486540518713, .7886751345948129};
  double sum = 0;
  for (int z = 0; z < 20; z++)
    for (int y = 0; y < 20; y++)
      for (int x = 0; x < n - 1; x++)
        for (int a = 0; a < 2; a++)
          for (int b = 0; b < 2; b++)
            for (int c = 0; c < 2; c++) {
              Vec3 p = vec3(knots[x] + (knots[x + 1] - knots[x]) * q[a],
                            (y + q[b]) * .05, (z + q[c]) * .05);
              sum += sample(v, p) * (knots[x + 1] - knots[x]) * .05 * .05 / 8;
            }
  return sum * 4 * M_PI;
}
static void bind(RuntimeSpectralVolume *v, const RuntimeScene3D *s) {
  char diag[192];
  bool ok = RuntimeSpectralVolume_BindScene(v, s, diag, sizeof(diag));
  if (!ok)
    fprintf(stderr, "bind: %s\n", diag);
  assert(ok);
}
static void begin(RuntimeSpectralVolume *v) {
  assert(spectral_reconstruction_begin_ray(
      v, RuntimeRay3D_Make(vec3(.5, .5, 3), vec3(0, 0, -1))));
}
static void rectangular(void) {
  for (int mode = 1; mode <= 2; mode++) {
    RuntimeSpectralVolume *v = volume(mode);
    RuntimePrimitive3D p;
    RuntimeScene3D s = plane_scene(&p, .473, 2);
    bind(v, &s);
    double edges[] = {-1e-10,
                      0,
                      nextafter(.473, -INFINITY),
                      .473,
                      nextafter(.473, INFINITY),
                      nextafter(1, 0),
                      1};
    assert(v->reconstruction->domain.rectangular_lookup);
    for (int k = 0; k < 7; k++) {
      Vec3 point = vec3(edges[k], .5, .5);
      uint16_t fast = spectral_domain_region(&v->reconstruction->domain, point);
      v->reconstruction->domain.rectangular_lookup = false;
      assert(fast == spectral_domain_region(&v->reconstruction->domain, point));
      v->reconstruction->domain.rectangular_lookup = true;
    }
    event(v, vec3(.49, .5, .5));
    begin(v);
    double value = integral(v, .473);
    assert(fabs(value - 1) < 2e-12);
    assert(sample(v, vec3(.46, .5, .5)) == 0);
    Ray3D ray = RuntimeRay3D_Make(vec3(.46, .5, .5), vec3(1, 0, 0));
    assert(fabs(spectral_reconstruction_boundary(v, ray, 0, .1) - .013) <
           1e-14);
    printf("planar mode=%d integral=%.17g leak=0\n", mode, value);
    RuntimeSpectralVolume_Free(v);
  }
}
static void field(RuntimeSpectralVolume *v, bool gap, bool mask) {
  v->config.kind = SPECTRAL_VOLUME_VF3D;
  RuntimeVolumeAttachment3D *a = &v->vf;
  a->enabled = a->affectsLighting = a->hasData = true;
  assert(RuntimeVolumeGrid3D_Configure(&a->grid, 1, 20, 20, 20, 0, 0, .1,
                                       vec3(0, 0, 0), .05, vec3(0, 0, 1), 0));
  assert(RuntimeVolumeAttachment3D_AllocateOwnedChannels(
      a, RUNTIME_VOLUME_3D_CHANNEL_DENSITY |
             RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK));
  for (int z = 0; z < 20; z++)
    for (int y = 0; y < 20; y++)
      for (int x = 0; x < 20; x++) {
        size_t i = x + 20 * (y + 20 * z);
        bool absent =
            x >= 7 && x <= 9 && (gap || (y >= 7 && y <= 9 && z >= 7 && z <= 9));
        a->channels.density[i] = absent && !mask ? 0 : 1;
        a->channels.solidMask[i] = absent && mask;
      }
}
static void occupied(void) {
  RuntimeScene3D scene = {0};
  for (int mode = 1; mode <= 2; mode++)
    for (int mask = 0; mask < 2; mask++)
      for (int gap = 0; gap < 2; gap++) {
        RuntimeSpectralVolume *v = volume(mode);
        v->config.cell_m = .2;
        v->dims[0] = v->dims[1] = v->dims[2] = 5;
        field(v, gap, mask);
        bind(v, &scene);
        event(v, vec3(.49, .49, .49));
        begin(v);
        double value = integral(v, 0);
        assert(fabs(value - 1) < 3e-12);
        assert(sample(v, vec3(.4, .4, .4)) == 0);
        if (gap)
          assert(sample(v, vec3(.34, .49, .49)) == 0);
        printf("occupied mode=%d mask=%d gap=%d integral=%.17g\n", mode, mask,
               gap, value);
        RuntimeSpectralVolume_Free(v);
      }
}
/* Independent rectangular tent-kernel convolution and exact HG phase. */
static double kernel(Vec3 source, Vec3 query) {
  double p[] = {source.x, source.y, source.z},
         q[] = {query.x, query.y, query.z}, product = 1;
  for (int a = 0; a < 3; a++) {
    double sum = 0;
    for (int i = 0; i <= 10; i++) {
      double x = i * .1, ws = fmax(0, 1 - fabs(p[a] - x) / .1),
             wq = fmax(0, 1 - fabs(q[a] - x) / .1);
      sum += ws * wq / ((i == 0 || i == 10) ? .05 : .1);
    }
    product *= sum;
  }
  return product;
}
static void angular(void) {
  RuntimeScene3D scene = {0};
  double gs[] = {.35, .9, -.9, .99, .0001, 1e-12};
  for (int exact = 0; exact < 2; exact++)
    for (int near = 0; near < 2; near++)
      for (int g = 0; g < 6; g++) {
        RuntimeSpectralVolume *v = volume(1);
        v->config.g = gs[g];
        v->config.phase_tolerance = exact ? 0 : .01;
        bind(v, &scene);
        Vec3 source = vec3(.45, .45, .45);
        event(v, source);
        Vec3 origin = vec3(.5, .5, near ? .52 : 3);
        double worst = 0;
        for (int z = 0; z < 6; z++)
          for (int y = 0; y < 6; y++)
            for (int x = 0; x < 6; x++) {
              Vec3 p = vec3(.351 + .032 * x, .351 + .032 * y, .351 + .032 * z);
              Ray3D ray = RuntimeRay3D_Make(
                  origin, vec3_normalize(vec3_sub(p, origin)));
              assert(spectral_reconstruction_begin_ray(v, ray));
              double actual = sample(v, p), mu = -ray.direction.z;
              double d = 1 + gs[g] * gs[g] - 2 * gs[g] * mu;
              double expected = kernel(source, p) * (1 - gs[g] * gs[g]) /
                                (4 * M_PI * pow(d, 1.5));
              double relative = fabs(actual - expected) / expected;
              if (relative > worst)
                worst = relative;
              assert(relative <= (exact ? 2e-12 : .010000001));
            }
        assert(v->reconstruction->exact_queries > 0 || !exact);
        printf("angular exact=%d near=%d g=%.2f max_relative=%.17g cached=%llu "
               "fallback=%llu\n",
               exact, near, gs[g], worst,
               (unsigned long long)v->reconstruction->cached_queries,
               (unsigned long long)v->reconstruction->exact_queries);
        RuntimeSpectralVolume_Free(v);
      }
}
static void mixed_directions(void) {
  RuntimeScene3D scene = {0};
  RuntimeSpectralVolume *v = volume(1);
  v->config.g = .9;
  v->config.phase_tolerance = .01;
  bind(v, &scene);
  const Vec3 positions[] = {{.45, .45, .45}, {.47, .48, .43}, {.42, .44, .46}};
  const Vec3 incoming[] = {{0, 0, 1}, {1, 0, 0}, {0, -1, 0}};
  const double watts[] = {.2, .7, 1.3};
  v->count = v->capacity = 3;
  v->points = calloc(3, sizeof(*v->points));
  v->positions = calloc(3, sizeof(*v->positions));
  v->heads = calloc(65536, sizeof(int));
  assert(v->points && v->positions && v->heads);
  for (int i = 0; i < 3; i++) {
    v->positions[i] = positions[i];
    v->points[i] = (SpectralCollision){
        .incoming = incoming[i], .watts = watts[i], .first = i == 0};
  }
  const Vec3 origins[] = {
      {.5, .5, 3}, {.5, .5, .52}, {-.4, .5, .5}, {.5, .5, -1}, {.5, .5, 3}};
  for (int k = 0; k < 5; k++) {
    Vec3 query = vec3(.46, .46, .46);
    Ray3D ray = RuntimeRay3D_Make(origins[k],
                                  vec3_normalize(vec3_sub(query, origins[k])));
    assert(spectral_reconstruction_begin_ray(v, ray));
    double first = 0,
           actual = spectral_reconstruction_source(v, query, 0, &first),
           expected = 0, single = 0;
    for (int i = 0; i < 3; i++) {
      double mu = -vec3_dot(incoming[i], ray.direction), g = .9;
      double value = watts[i] * kernel(positions[i], query) * (1 - g * g) /
                     (4 * M_PI * pow(1 + g * g - 2 * g * mu, 1.5));
      expected += value;
      if (!i)
        single = value;
    }
    assert(fabs(actual - expected) / expected < .010000001);
    assert(fabs(first - single) / single < .010000001);
  }
  assert(v->reconstruction->exact_queries > 0);
  puts("mixed weights/directions, first-scatter separation and changed-camera "
       "cache pass");
  RuntimeSpectralVolume_Free(v);
}
/* Independent composite midpoint integration of the analytic tent source.
 * Thin and optically thick rays exercise both Gauss orders and subdivision. */
static void camera_quadrature(void) {
  RuntimeScene3D scene = {0};
  const double extinction[] = {.01, 1, 100};
  for (int k = 0; k < 3; k++) {
    RuntimeSpectralVolume *v = volume(1);
    v->config.absorption =
        (RuntimeSpectralCurve){.count = 2, .points = {{380, 0}, {780, 0}}};
    v->config.scattering = (RuntimeSpectralCurve){
        .count = 2, .points = {{380, extinction[k]}, {780, extinction[k]}}};
    bind(v, &scene);
    Vec3 source = vec3(.45, .45, .45);
    event(v, source);
    Ray3D ray = RuntimeRay3D_Make(vec3(-.1, .45, .45), vec3(1, 0, 0));
    RuntimeSpectralPencilLine line = {.vacuum_nm = 550, .band = 0};
    RuntimeSpectralPathReadout out = {.weight = 1};
    bool restart = false;
    assert(RuntimeSpectralVolume_Segment(v, &line, &ray, 2, true, 0, &out,
                                         &restart));
    double expected = 0;
    const int steps = 200000;
    for (int i = 0; i < steps; i++) {
      double x = (i + .5) / steps;
      expected += kernel(source, vec3(x, .45, .45)) * exp(-extinction[k] * x) /
                  (4 * M_PI * steps);
    }
    double error = fabs(out.inscattered - expected) / expected;
    assert(error < 2e-6);
    assert(fabs(out.weight - exp(-extinction[k])) < 1e-12);
    assert(fabs(out.single_scatter - out.inscattered) < 1e-12);
    printf("camera tau=%.2g relative_error=%.17g\n", extinction[k], error);
    RuntimeSpectralVolume_Free(v);
  }
}
/* Coherent rays must reuse exact cache anchors without making the result
 * depend on the order of queries. Tiny tolerances fall back safely. */
static void cache_order(void) {
  RuntimeScene3D scene = {0};
  for (int mode = 1; mode <= 2; mode++)
    for (int tiny = 0; tiny < 2; tiny++) {
      RuntimeSpectralVolume *v = volume(mode);
      v->config.g = .35;
      v->config.phase_tolerance = tiny ? 1e-14 : .01;
      bind(v, &scene);
      event(v, vec3(.45, .45, .45));
      double values[100];
      for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < 100; j++) {
          int i = pass ? 99 - j : j;
          Vec3 point = vec3(.44 + i * .00002, .45, .45);
          Ray3D ray = RuntimeRay3D_Make(
              vec3(.5, .5, 3),
              vec3_normalize(vec3_sub(point, vec3(.5, .5, 3))));
          assert(spectral_reconstruction_begin_ray(v, ray));
          double value = sample(v, point);
          if (!pass)
            values[i] = value;
          else
            assert(value == values[i]);
        }
      assert(v->reconstruction->spatial_hits >
             v->reconstruction->spatial_misses);
      if (!tiny)
        assert(v->reconstruction->cached_queries >
               v->reconstruction->exact_queries);
      RuntimeSpectralVolume_Free(v);
    }
  puts("coherent angular/spatial reuse, tiny tolerance and reverse query-order "
       "equality pass");
}
static void refusals(void) {
  char diag[192];
  RuntimePrimitive3D p;
  RuntimeScene3D scene = plane_scene(&p, .5, .2);
  RuntimeSpectralVolume *v = volume(1);
  assert(!RuntimeSpectralVolume_BindScene(v, &scene, diag, sizeof(diag)));
  assert(strstr(diag, "span"));
  RuntimeSpectralVolume_Free(v);
  scene = plane_scene(&p, .5, 2);
  p.shape.plane.normal = vec3_normalize(vec3(1, 0, 1));
  p.shape.plane.axisV = vec3_normalize(vec3(-1, 0, 1));
  v = volume(1);
  assert(!RuntimeSpectralVolume_BindScene(v, &scene, diag, sizeof(diag)));
  assert(strstr(diag, "axis-aligned"));
  RuntimeSpectralVolume_Free(v);
  scene = plane_scene(&p, .5, 1.1);
  p.shape.plane.height = 1.1;
  p.shape.plane.axisU = vec3(0, sqrt(.5), sqrt(.5));
  p.shape.plane.axisV = vec3(0, -sqrt(.5), sqrt(.5));
  v = volume(1);
  assert(!RuntimeSpectralVolume_BindScene(v, &scene, diag, sizeof(diag)));
  assert(strstr(diag, "span"));
  RuntimeSpectralVolume_Free(v);
  scene = (RuntimeScene3D){0};
  v = volume(1);
  bind(v, &scene);
  event(v, vec3(.49, .5, .5));
  v->reconstruction->budget_bytes = 1;
  assert(!RuntimeSpectralVolume_PrepareReconstruction(v));
  assert(strstr(RuntimeSpectralVolume_Error(v), "budget"));
  RuntimeSpectralVolume_Free(v);
  RuntimeSpectralVolumeConfig cfg = {.kind = SPECTRAL_VOLUME_BOX,
                                     .reconstruction = 1,
                                     .cell_m = 1. / 4096,
                                     .lo = {0, 0, 0},
                                     .hi = {1, 1, 1}};
  assert(!RuntimeSpectralVolume_Load(&cfg, diag, sizeof(diag)));
  cfg.reconstruction = 0;
  v = RuntimeSpectralVolume_Load(&cfg, diag, sizeof(diag));
  assert(v);
  RuntimeSpectralVolume_Free(v);
  puts(
      "unsupported boundaries, allocation budget and node-index bounds refuse");
}
static void camera_segment_cache(void) {
  RuntimeScene3D scene = {0};
  for (int mode = 0; mode <= 2; mode++)
    for (int vf = 0; vf <= 1; vf++) {
      RuntimeSpectralVolume *v = volume(mode);
      if (vf)
        field(v, 0, 0);
      bind(v, &scene);
      event(v, vec3(.49, .49, .49));
      v->config.absorption =
          (RuntimeSpectralCurve){.count = 2, .points = {{380, .01}, {780, 2}}};
      v->config.scattering =
          (RuntimeSpectralCurve){.count = 2, .points = {{380, .4}, {780, 5}}};
      assert(RuntimeSpectralVolume_PrepareReconstruction(v));
      RuntimeSpectralCameraCache *cache = RuntimeSpectralCameraCache_Create();
      assert(cache);
      Vec3 origins[] = {{-.1, .49, .49},
                        {1.1, .49, .49},
                        {-.1, -.1, .49},
                        {-.1, .999999, .49},
                        {2, 2, 2}};
      Vec3 dirs[] = {
          {1, 0, 0}, {-1, 0, 0}, {1, 1, 0}, {1, 1e-12, 0}, {1, 0, 0}};
      for (int r = 0; r < 5; r++)
        for (int limit = 0; limit < 2; limit++)
          for (int band = 0; band < 3; band++) {
            Ray3D ray = RuntimeRay3D_Make(origins[r], dirs[r]);
            RuntimeSpectralPencilLine line = {.vacuum_nm = 380 + 200 * band,
                                              .band = 0};
            RuntimeSpectralPathReadout reference = {.weight = 1},
                                       candidate = {.weight = 1};
            bool restart;
            double distance = limit ? .55 : 3;
            assert(RuntimeSpectralVolume_Segment(v, &line, &ray, distance, true,
                                                 0, &reference, &restart));
            line.camera_cache = cache;
            assert(RuntimeSpectralVolume_Segment(v, &line, &ray, distance, true,
                                                 0, &candidate, &restart));
            assert(reference.weight == candidate.weight &&
                   reference.inscattered == candidate.inscattered &&
                   reference.single_scatter == candidate.single_scatter);
          }
      RuntimeSpectralMapStats stats = {0};
      RuntimeSpectralCameraCache_Free(cache, &stats);
      assert(stats.camera_cache_hits > 0 && stats.camera_cache_misses > 0 &&
             stats.camera_cache_segments > 0);
      assert(stats.camera_cache_bytes < 128 * 1024);
      RuntimeSpectralVolume_Free(v);
    }
  /* More than 4096 ray partitions: retain exact traversal and refuse reuse. */
  RuntimeSpectralVolume *v = volume(0);
  v->config.cell_m = 1. / 4096;
  RuntimeSpectralCameraCache *cache = RuntimeSpectralCameraCache_Create();
  assert(cache);
  Ray3D ray = RuntimeRay3D_Make(vec3(-.1, -.09, -.08), vec3(1, .999, .997));
  for (int run = 0; run < 2; run++) {
    SpectralFieldRay a, b;
    assert(spectral_field_begin(&a, v, ray, 3, true));
    assert(spectral_camera_begin(cache, &b, v, ray, 3, true));
    double x, y, z, w, m;
    int count = 0;
    while (spectral_field_next(&a, &x, &y)) {
      assert(spectral_camera_next(cache, &b, &z, &w, &m));
      assert(x == z && y == w && m == spectral_field_mass(&a, x, y));
      ++count;
    }
    assert(count > 4096 && !spectral_camera_next(cache, &b, &z, &w, &m));
  }
  RuntimeSpectralMapStats stats = {0};
  RuntimeSpectralCameraCache_Free(cache, &stats);
  assert(stats.camera_cache_overflows == 2 && stats.camera_cache_hits == 0);
  RuntimeSpectralVolume_Free(v);
  puts("PASS exact camera cache: box/VF3D, continuous/adaptive, wavelength, "
       "ray/limit invalidation and bounded overflow");
}

#include "spectral_mesh_reconstruction_case.h"


static void source_stencil_cache(void) {
  for (int mode = 1; mode <= 2; mode++) {
    RuntimeSpectralVolume *v = volume(mode);
    RuntimePrimitive3D primitive;
    RuntimeTriangle3D triangles[12];
    RuntimeScene3D scene = mesh_cube(&primitive, triangles);
    bind(v, &scene);
    event(v, vec3(.449, .449, .449));
    v->points = realloc(v->points, 4 * sizeof(*v->points));
    v->positions = realloc(v->positions, 4 * sizeof(*v->positions));
    assert(v->points && v->positions);
    v->points[1] = v->points[0];
    v->positions[1] = v->positions[0];
    v->points[1].band = 1;
    v->points[1].watts = 2;
    v->points[1].first = false;
    v->points[1].incoming = vec3(1, 0, 0);
    for (int band = 0; band < 2; band++) {
      v->points[2 + band] = v->points[band];
      v->positions[2 + band] = vec3(.12, .12, .12);
      v->points[2 + band].cell = ((uint64_t)1 << 24) | ((uint64_t)1 << 12) | 1;
      v->points[2 + band].watts = 3 + band;
      v->heads[spectral_volume_bucket(v->points[2 + band].cell, band)] = 3 + band;
    }
    v->count = v->capacity = 4;
    v->heads[spectral_volume_bucket(v->points[1].cell, 1)] = 2;
    v->config.g = .7;
    v->config.phase_tolerance = 0; /* Exercise exact angular fallback. */
    begin(v);
    double totals[2] = {0};
    for (int pass = 0; pass < 3; pass++) {
      Ray3D ray = RuntimeRay3D_Make(vec3(.5, .5, 3),
                                   pass == 1 ? vec3(1, 0, 0) : vec3(0, 0, -1));
      assert(spectral_reconstruction_begin_ray(v, ray));
      for (int i = 0; i < 4096; i++) {
        /* More positions than cache slots, plus inside/occluded/edge cases. */
        Vec3 p = vec3(.1 + (i % 32) * .025, .1 + ((i / 32) % 16) * .05,
                      .1 + (i / 512) * .1);
        for (int band = 0; band < 2; band++) {
          double a = .125, b = .125, c = .125;
          v->config.disable_camera_cache = true;
          double ref = spectral_reconstruction_source(v, p, band, &a);
          v->config.disable_camera_cache = false;
          assert(spectral_reconstruction_begin_ray(v, ray));
          double value = spectral_reconstruction_source(v, p, band, &b);
          assert(spectral_reconstruction_begin_ray(v, ray));
          double again = spectral_reconstruction_source(v, p, band, &c);
          assert(ref == value && value == again && a == b && b == c);
          totals[band] += value;
        }
      }
    }
    assert(totals[0] > 0 && totals[1] > 0 && totals[0] != totals[1]);
    assert(v->reconstruction->source_hits > 0 && v->reconstruction->source_misses > 1024);
    /* Sequential overflow, a changed sample sequence and a shorter replay
     * cannot return a stencil for a different position. */
    for (int pass = 0; pass < 3; pass++) {
      begin(v);
      for (int i = 0; i < 1200 - pass * 50; i++) {
        Vec3 p = vec3(.01 + ((i + pass) % 31) * .03, .025, .025);
        double a = 0, b = 0;
        v->config.disable_camera_cache = true;
        double ref = spectral_reconstruction_source(v, p, 0, &a);
        v->config.disable_camera_cache = false;
        double value = spectral_reconstruction_source(v, p, 0, &b);
        assert(value == ref && a == b);
      }
    }
    assert(v->reconstruction->source_overflows > 0);
    begin(v);
    assert(v->reconstruction->source_cache);
    for (int band = 0; band < 2; band++) {
      double a = .125, b = .125;
      v->config.disable_camera_cache = true;
      double ref = spectral_reconstruction_source(v, vec3(.025, .025, .025), band, &a);
      v->config.disable_camera_cache = false;
      begin(v);
      double value = spectral_reconstruction_source(v, vec3(.025, .025, .025), band, &b);
      assert(value > 0 && value == ref && a == b);
    }
    bool prepared_nonzero = false;
    for (int i = 0; i < SPECTRAL_SOURCE_CACHE_SIZE; i++) {
      SpectralSourceStencil *c = &v->reconstruction->source_cache[i];
      if (c->valid && c->position.x == .025 && c->position.y == .025 && c->position.z == .025)
        prepared_nonzero = c->count > 0;
    }
    assert(prepared_nonzero);
    printf("source cache mode=%d hits=%llu misses=%llu bytes=%zu\n", mode,
           (unsigned long long)v->reconstruction->source_hits,
           (unsigned long long)v->reconstruction->source_misses,
           SPECTRAL_SOURCE_CACHE_SIZE * sizeof(SpectralSourceStencil));
    RuntimeSpectralVolume_Free(v);
    /* A new map cannot inherit stencils; insufficient optional cache budget
     * retains exact scalar evaluation without setting a fatal error. */
    v = volume(mode); bind(v, &scene); event(v, vec3(.449, .449, .449)); begin(v);
    v->config.disable_camera_cache = true;
    double ref = sample(v, vec3(.44, .44, .44));
    v->reconstruction->budget_bytes = v->reconstruction->owned_bytes;
    v->config.disable_camera_cache = false;
    assert(sample(v, vec3(.44, .44, .44)) == ref && ref > 0);
    assert(!v->reconstruction->source_cache && !v->reconstruction->error[0]);
    RuntimeSpectralVolume_Free(v);
  }
  puts("PASS source stencil: exact band/direction parity, sequence mismatch/overflow, map lifetime and budget fallback");
}

#include "spectral_independent_support_case.h"

#include "spectral_kernel_case.h"
#include "spectral_wide_surface_case.h"
int main(void) {
  kernel_cases();
  wide_surface_cases();
  independent_support();
  source_stencil_cache();
  mesh_boundaries();
  prism_boundaries();
  mesh_camera_quadrature();
  camera_segment_cache();
  rectangular();
  occupied();
  angular();
  camera_quadrature();
  cache_order();
  refusals();
  puts("PASS spectral reconstruction native qualification");
  return 0;
}
