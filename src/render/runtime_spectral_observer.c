#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_profile.h"

enum { FIRST_AIR_NM = 379 };
static const Vec3 observer[] = {
#include "cie_1931_2deg_380_780.inc"
};
_Static_assert(sizeof(observer) / sizeof(observer[0]) ==
                   RUNTIME_SPECTRAL_MAX_NM - FIRST_AIR_NM + 1,
               "observer grid must cover the transport domain");

double RuntimeSpectral_StandardAirNm(double vacuum_nm) {
    /* CIE 11664-1 tabulates standard-air wavelengths. Convert the argument
     * only; observer sensitivity is dimensionless. Peck & Reeder (1972),
     * https://doi.org/10.1364/JOSA.62.000958, vacuum wavenumber in um^-1. */
    double sigma = 1000.0 / vacuum_nm;
    double sigma2 = sigma * sigma;
    double n = 1.0 + 1e-8 * (8060.51 + 2480990.0 / (132.274 - sigma2) + 17455.7 / (39.32957 - sigma2));
    return vacuum_nm / n;
}

Vec3 RuntimeSpectral_CIE1931(double vacuum_nm) {
    if (!(vacuum_nm >= RUNTIME_SPECTRAL_MIN_NM && vacuum_nm <= RUNTIME_SPECTRAL_MAX_NM))
        return vec3(0, 0, 0);
    double index = RuntimeSpectral_StandardAirNm(vacuum_nm) - FIRST_AIR_NM;
    unsigned i = (unsigned)index;
    double fraction = index - i;
    if (fraction == 0)
        return observer[i];
    return vec3_add(vec3_scale(observer[i], 1 - fraction), vec3_scale(observer[i + 1], fraction));
}
