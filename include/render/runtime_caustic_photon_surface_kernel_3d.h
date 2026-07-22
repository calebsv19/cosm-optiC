#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SURFACE_KERNEL_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SURFACE_KERNEL_3D_H

#include <stdbool.h>
#include <stdint.h>

double RuntimeCausticPhotonSurfaceKernel3D_Weight(double distance,
                                                  double support_radius);
double RuntimeCausticPhotonSurfaceKernel3D_BoundedAdaptiveRadius(
    double farthest_distance,
    double maximum_radius,
    bool* out_adaptive);
double RuntimeCausticPhotonSurfaceKernel3D_Density(uint64_t sample_count,
                                                   double support_radius);

#endif
