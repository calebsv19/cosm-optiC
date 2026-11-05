#ifndef RUNTIME_SPECTRAL_SAMPLING_H
#define RUNTIME_SPECTRAL_SAMPLING_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {
  bool enabled;
  int min_passes,
      max_passes; /* Multiples of eight; zero maximum is unlimited. */
  double visible_error, edge_error, energy_error;
} RuntimeSpectralSamplingConfig;
typedef struct {
  bool enabled, converged, canceled, pass_limit;
  int passes, full_passes, checkpoints, consecutive;
  uint64_t camera_samples, edge_pixels;
  double visible_error, edge_error, energy_error;
} RuntimeSpectralSamplingStats;
RuntimeSpectralSamplingConfig RuntimeSpectralSampling_Defaults(void);
bool RuntimeSpectralSampling_Parse(json_object *value,
                                   RuntimeSpectralSamplingConfig *out);
json_object *
RuntimeSpectralSampling_JSON(const RuntimeSpectralSamplingStats *stats);
void RuntimeSpectralSampling_Jitter(unsigned seed, int pass, double *x,
                                    double *y);
#endif
