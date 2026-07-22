#include "render/runtime_caustic_photon_surface_kernel_3d.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double RuntimeCausticPhotonSurfaceKernel3D_Weight(double distance,
                                                  double support_radius) {
    double normalized_distance;
    if (!(support_radius > 1.0e-12) || !(distance >= 0.0) ||
        distance >= support_radius) {
        return 0.0;
    }
    normalized_distance = distance / support_radius;
    return (2.0 / (M_PI * support_radius * support_radius)) *
           (1.0 - normalized_distance * normalized_distance);
}

double RuntimeCausticPhotonSurfaceKernel3D_BoundedAdaptiveRadius(
    double farthest_distance,
    double maximum_radius,
    bool* out_adaptive) {
    double radius;
    if (out_adaptive) *out_adaptive = false;
    if (!(maximum_radius > 0.0)) return 0.001;
    if (!(farthest_distance >= 0.0) || farthest_distance >= maximum_radius) {
        return maximum_radius;
    }
    radius = fmax(0.001, farthest_distance * (1.0 + 1.0e-6));
    if (radius > maximum_radius) radius = maximum_radius;
    if (out_adaptive) *out_adaptive = radius < maximum_radius;
    return radius;
}

double RuntimeCausticPhotonSurfaceKernel3D_Density(uint64_t sample_count,
                                                   double support_radius) {
    const double area = M_PI * support_radius * support_radius;
    return area > 1.0e-12 ? (double)sample_count / area : 0.0;
}
