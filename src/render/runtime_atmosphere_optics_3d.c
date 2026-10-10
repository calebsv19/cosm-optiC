#include "render/runtime_atmosphere_optics_3d.h"
#include <math.h>

static const double pi = 3.14159265358979323846;

bool RuntimeAtmosphereOptics3D_Valid(RuntimeAtmosphereOptics3D o) {
    return isfinite(o.scattering_albedo) && o.scattering_albedo >= 0 &&
        o.scattering_albedo <= 1 && isfinite(o.phase_g) && fabs(o.phase_g) <= .95;
}

double RuntimeAtmosphereOptics3D_SegmentWeight(double transmittance,
    double extinction, double length, double albedo) {
    if (!isfinite(transmittance) || transmittance < 0 || transmittance > 1 ||
        !isfinite(extinction) || extinction <= 0 || !isfinite(length) || length <= 0 ||
        !isfinite(albedo) || albedo < 0 || albedo > 1) return 0;
    return transmittance * albedo * -expm1(-extinction * length);
}

double RuntimeAtmosphereOptics3D_Phase(double cosine, double g) {
    if (!isfinite(cosine) || !isfinite(g) || fabs(g) > .95) return 0;
    cosine = fmax(-1, fmin(1, cosine));
    return (1 - g*g) / (4*pi*pow(1 + g*g + 2*g*cosine, 1.5));
}

void RuntimeAtmosphereOptics3D_SkyWeights(const double view[3], double g,
    double directions[12][3], double weights[12]) {
    const double golden = 1.6180339887498948482;
    const double inverse_length = 1 / sqrt(1 + golden*golden);
    double sum = 0;
    for (int i = 0; i < 12; ++i) {
        const int group = i / 4;
        const double a = (i % 2 ? -1 : 1) * inverse_length;
        const double b = (i % 4 >= 2 ? -golden : golden) * inverse_length;
        directions[i][0] = group == 0 ? 0 : group == 1 ? a : b;
        directions[i][1] = group == 0 ? a : group == 1 ? b : 0;
        directions[i][2] = group == 0 ? b : group == 1 ? 0 : a;
        double cosine = 0;
        for (int j = 0; j < 3; ++j) cosine += view[j] * directions[i][j];
        weights[i] = RuntimeAtmosphereOptics3D_Phase(cosine, g);
        sum += weights[i];
    }
    for (int i = 0; i < 12; ++i) weights[i] = sum > 0 ? weights[i] / sum : 1.0 / 12;
}
