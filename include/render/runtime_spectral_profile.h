#ifndef RENDER_RUNTIME_SPECTRAL_PROFILE_H
#define RENDER_RUNTIME_SPECTRAL_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

enum { RUNTIME_SPECTRAL_MIN_NM = 380, RUNTIME_SPECTRAL_MAX_NM = 780 };

typedef struct {
    double nm, watts_per_nm;
} RuntimeSpectralKnot;
typedef struct {
    double nm, watts;
} RuntimeSpectralLine;
typedef enum { SPECTRAL_CONTINUUM, SPECTRAL_LINE } RuntimeSpectralMeasure;
typedef struct {
    double nm;
    RuntimeSpectralMeasure measure;
    /* Proposal density per nm for continuum, dimensionless mass for a line. */
    double probability;
} RuntimeSpectralDraw;
typedef struct RuntimeSpectralProfile RuntimeSpectralProfile;

/* Copies sorted, unique knots and sorted, unique lines. Outside the knot
 * interval the continuum is zero. Empty/zero power is valid but unsampleable.
 * Returns NULL for invalid input, unrepresentable CDF or allocation failure. */
RuntimeSpectralProfile *RuntimeSpectral_Prepare(const RuntimeSpectralKnot *knots, size_t knot_count,
                                                const RuntimeSpectralLine *lines, size_t line_count);
void RuntimeSpectral_Free(RuntimeSpectralProfile *profile);
double RuntimeSpectral_TotalWatts(const RuntimeSpectralProfile *profile);
/* Returns W/nm (continuum) or integrated W (line). */
double RuntimeSpectral_Value(const RuntimeSpectralProfile *profile, double nm,
                             RuntimeSpectralMeasure measure);
/* One independent variate in [0,1). Evaluate physical source and proposal
 * separately when importance sampling. Proposal must cover source support. */
bool RuntimeSpectral_Sample(const RuntimeSpectralProfile *proposal, double u, RuntimeSpectralDraw *draw);
/* Check once at preparation. All positive continuum intervals and all positive
 * atoms of the source must be sampleable under the chosen proposal. */
bool RuntimeSpectral_Covers(const RuntimeSpectralProfile *proposal,
                            const RuntimeSpectralProfile *source);
/* Spectral and emitter-selection factors only. Spatial/angular weights belong
 * to the geometric sampler. trials counts all launches, including misses. */
bool RuntimeSpectral_EmissionWatts(const RuntimeSpectralProfile *source, RuntimeSpectralDraw draw,
                                   double emitter_probability, size_t trials, double *watts);
#endif
