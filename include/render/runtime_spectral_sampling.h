#ifndef RUNTIME_SPECTRAL_SAMPLING_H
#define RUNTIME_SPECTRAL_SAMPLING_H
#include "render/runtime_native_3d_sampling.h"
#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {
  bool enabled, full_camera, progressive_camera, camera_seed_set;
  uint32_t camera_seed;
  unsigned camera_samples_per_map; /* 0 is legacy internal default; JSON 1/2/4/8. */
  int min_passes,
      max_passes; /* Multiples of eight; zero maximum is unlimited. */
  double visible_error, edge_error, energy_error, local_error;
} RuntimeSpectralSamplingConfig;
typedef struct {
  uint32_t camera_seed;
  bool progressive_camera;
  bool enabled, converged, canceled, pass_limit, full_camera,
      validation_pending;
  int passes, full_passes, checkpoints, consecutive, full_checkpoints;
  unsigned camera_samples_per_map;
  uint32_t min_pixel_samples, max_pixel_samples;
  uint32_t min_pixel_maps, max_pixel_maps;
  uint64_t camera_samples, edge_pixels;
  double visible_error, edge_error, energy_error, local_error;
} RuntimeSpectralSamplingStats;
RuntimeSpectralSamplingConfig RuntimeSpectralSampling_Defaults(void);
bool RuntimeSpectralSampling_Parse(json_object *value,
                                   RuntimeSpectralSamplingConfig *out);
json_object *
RuntimeSpectralSampling_JSON(const RuntimeSpectralSamplingStats *stats);
void RuntimeSpectralSampling_Jitter(unsigned seed, int pass, double *x,
                                    double *y);
RuntimeNative3DSamplingContext RuntimeSpectralSampling_Camera(uint32_t seed,
                                                              int pass);
RuntimeNative3DSamplingContext RuntimeSpectralSampling_CameraForMap(
    uint32_t seed, int pass, unsigned sample, unsigned samples_per_map);
#endif
