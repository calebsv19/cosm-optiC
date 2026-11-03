#include "render/runtime_spectral_pencil.h"
#include <math.h>
#include <string.h>

/* Request parser validates the bounded model before this one-time preparation. */
bool RuntimeSpectralLamp_PrepareModel(RuntimeSpectralLamp *lamp, double watts, double temperature,
                                      int peaks, const double center[4], const double sigma[4],
                                      const double weight[4]) {
    if (!lamp || !(watts >= 0) || !isfinite(watts) || peaks < 0 || peaks > 4 ||
        (temperature > 0 && peaks) || (!temperature && !peaks))
        return false;
    double grid[129] = {380, 780};
    int nodes = 2;
    if (temperature > 0) {
        nodes = 101;
        for (int i = 0; i < nodes; ++i)
            grid[i] = 380 + 4 * i;
    }
    /* Per-peak nodes prevent sub-nm lineshapes from falling between grid points. */
    for (int j = 0; j < peaks; ++j)
        for (int k = -13; k <= 13; ++k) {
            double nm = center[j] + .5 * k * sigma[j];
            if (nm <= 380 || nm >= 780) continue;
            int i = 0;
            while (i < nodes && grid[i] < nm)
                ++i;
            if ((i < nodes && grid[i] - nm < 1e-10) || (i > 0 && nm - grid[i - 1] < 1e-10)) continue;
            memmove(grid + i + 1, grid + i, (nodes - i) * sizeof(double));
            grid[i] = nm;
            ++nodes;
        }
    double integral = 0;
    for (int i = 0; i < nodes; ++i) {
        double nm = grid[i], v = 0;
        if (temperature) v = pow(550 / nm, 5) / expm1(1.438776877e7 / (nm * temperature));
        for (int j = 0; j < peaks; ++j) {
            double z = (nm - center[j]) / sigma[j];
            if (fabs(z) <= 6) v += weight[j] * exp(-.5 * z * z);
        }
        lamp->knots[i] = (RuntimeSpectralKnot){nm, v};
        if (i) integral += (nm - grid[i - 1]) * .5 * (v + lamp->knots[i - 1].watts_per_nm);
    }
    if (!(integral > 0) && watts > 0) return false;
    for (int i = 0; i < nodes; ++i) {
        double relative = integral > 0 ? lamp->knots[i].watts_per_nm / integral : 0;
        lamp->knots[i].watts_per_nm = watts * relative;
    }
    lamp->nknots = nodes;
    return true;
}
