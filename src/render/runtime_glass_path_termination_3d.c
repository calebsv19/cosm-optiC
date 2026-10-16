#include "render/runtime_glass_path_termination_3d.h"
#include <math.h>

double RuntimeGlassPathTermination3D_Survival(int prefix, int next, Vec3 beta, double eta_scale) {
    if (prefix <= 0 || next < 1) return 0;
    if (next <= prefix) return 1;
    double importance = fmax(beta.x, fmax(beta.y, beta.z)) * eta_scale;
    if (!isfinite(importance) || importance <= 0) return 0;
    /* High survival protects long, nearly lossless internal reflections.
     * The strict upper bound also terminates trapped unit-weight paths. */
    return fmin(.98, fmax(.05, importance));
}
bool RuntimeGlassPathTermination3D_Apply(double survival, double sample, Vec3 *beta) {
    if (!beta || !(survival > 0 && survival <= 1) ||
        !(sample >= 0 && sample < 1) || sample >= survival) return false;
    *beta = vec3_scale(*beta, 1 / survival);
    return true;
}
static uint64_t mix(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
double RuntimeGlassPathTermination3D_Sample(const RuntimeNative3DSamplingContext *sampling,
                                            uint32_t seed, int index, uint32_t boundary) {
    uint64_t stream = ((uint64_t)seed << 32) | (sampling ? sampling->sampleSequence : 1u);
    stream = mix(stream ^ UINT64_C(0x6a09e667f3bcc909));
    stream ^= mix(((uint64_t)(sampling ? sampling->temporalSubpassIndex : 0u) << 32) |
                  (uint32_t)index);
    uint64_t bits = mix(stream + UINT64_C(0x9e3779b97f4a7c15) * ((uint64_t)boundary + 1));
    return (double)(bits >> 11) * 0x1p-53;
}
