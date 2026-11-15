#pragma once
#include "app/ray_tracing_sha256.h"

typedef struct SpectralPhase SpectralPhase;

SpectralPhase *spectral_phase_load(const char *path, RayTracingSha256Context *identity);
void spectral_phase_free(SpectralPhase *);
double spectral_phase_value(const SpectralPhase *, double nm, double mu);
double spectral_phase_sample(const SpectralPhase *, double nm, double u);
