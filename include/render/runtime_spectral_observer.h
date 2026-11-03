#ifndef RENDER_RUNTIME_SPECTRAL_OBSERVER_H
#define RENDER_RUNTIME_SPECTRAL_OBSERVER_H

#include "math/vec3.h"

/* Raw CIE 1931 2-degree response, linear interpolation, zero outside the
 * optiC visible domain. Multiply by compensated spectral power/radiance once. */
double RuntimeSpectral_StandardAirNm(double vacuum_nm);
Vec3 RuntimeSpectral_CIE1931(double vacuum_nm);
#endif
