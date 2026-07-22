#include "render/runtime_caustic_photon_path_weight_3d.h"

#include <math.h>

bool RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
    Vec3 weight_before,
    Vec3 throughput_before,
    Vec3 throughput_after,
    Vec3* out_weight) {
    const double epsilon = 1.0e-18;
    double before[3] = {throughput_before.x, throughput_before.y,
                        throughput_before.z};
    double after[3] = {throughput_after.x, throughput_after.y,
                       throughput_after.z};
    double weight[3] = {weight_before.x, weight_before.y, weight_before.z};
    double resolved[3] = {0.0, 0.0, 0.0};
    if (!out_weight) return false;
    for (int channel = 0; channel < 3; ++channel) {
        if (!isfinite(before[channel]) || before[channel] < 0.0 ||
            !isfinite(after[channel]) || after[channel] < 0.0 ||
            !isfinite(weight[channel]) || weight[channel] < 0.0) {
            return false;
        }
        if (before[channel] > epsilon) {
            resolved[channel] = weight[channel] *
                                (after[channel] / before[channel]);
        } else if (after[channel] > epsilon) {
            return false;
        }
    }
    *out_weight = vec3(resolved[0], resolved[1], resolved[2]);
    return true;
}
