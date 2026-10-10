#ifndef RUNTIME_ATMOSPHERE_OPTICS_3D_H
#define RUNTIME_ATMOSPHERE_OPTICS_3D_H

#include <stdbool.h>

/* Dimensionless appearance policy, not a smoke mass-concentration contract.
 * Extinction is supplied by the existing density/placement sampler. */
typedef struct {
    bool enabled;
    double scattering_albedo;
    double phase_g;
} RuntimeAtmosphereOptics3D;

bool RuntimeAtmosphereOptics3D_Valid(RuntimeAtmosphereOptics3D optics);
/* Analytic constant-coefficient segment integral. Camera transmittance is
 * measured at segment entry; incoming radiance is evaluated at its midpoint. */
double RuntimeAtmosphereOptics3D_SegmentWeight(double camera_transmittance,
    double extinction, double segment_length, double scattering_albedo);
/* Both directions point away from the sample: toward camera and toward light.
 * Positive g therefore peaks at cosine -1 (forward scattering). */
double RuntimeAtmosphereOptics3D_Phase(double cosine, double g);
/* Bounded deterministic sky quadrature. Discrete weights sum to one, giving
 * exact uniform-sky radiance before geometry/medium visibility. */
void RuntimeAtmosphereOptics3D_SkyWeights(const double view_direction[3],
    double g, double directions[12][3], double weights[12]);

#endif
