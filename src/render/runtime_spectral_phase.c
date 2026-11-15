#define _POSIX_C_SOURCE 200809L
#include "runtime_spectral_phase.h"

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_PHASE_BANDS = 16, MAX_PHASE_NODES = 32769 };
static const double phase_pi = 3.14159265358979323846;

struct SpectralPhase {
    int bands, nodes;
    double wavelengths[MAX_PHASE_BANDS];
    double *mu, *density, *cdf;
};

void spectral_phase_free(SpectralPhase *phase) {
    if (!phase) return;
    free(phase->mu);
    free(phase->density);
    free(phase->cdf);
    free(phase);
}

SpectralPhase *spectral_phase_load(const char *path, RayTracingSha256Context *identity) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return NULL;
    struct stat info;
    FILE *file = NULL;
    if (!fstat(fd, &info) && S_ISREG(info.st_mode) && info.st_size <= 32L*1024*1024)
        file = fdopen(fd, "rb");
    if (!file) {
        close(fd);
        return NULL;
    }
    SpectralPhase *phase = calloc(1, sizeof(*phase));
    char tag[32], trailing;
    if (!phase ||
        fscanf(file, "%31s %8d %8d", tag, &phase->bands, &phase->nodes) != 3 ||
        strcmp(tag, "optic_mie_phase_v1") || phase->bands < 2 || phase->bands > MAX_PHASE_BANDS ||
        phase->nodes < 3 || phase->nodes > MAX_PHASE_NODES)
        goto fail;

    int nodes = phase->nodes;
    size_t count = (size_t)nodes * phase->bands;
    phase->mu = malloc((size_t)nodes * sizeof(double));
    phase->density = malloc(count * sizeof(double));
    phase->cdf = malloc(count * sizeof(double));
    if (!phase->mu || !phase->density || !phase->cdf) goto fail;
    for (int i = 0; i < nodes; ++i)
        phase->mu[i] = -cos(phase_pi * i / (nodes - 1));

    for (int band = 0; band < phase->bands; ++band) {
        double nm;
        if (fscanf(file, "%64lf", &nm) != 1 || !isfinite(nm) || nm < 380 || nm > 780 ||
            (band && nm <= phase->wavelengths[band - 1]))
            goto fail;
        phase->wavelengths[band] = nm;
        double *density = phase->density + (size_t)band * nodes;
        double *cdf = phase->cdf + (size_t)band * nodes;
        cdf[0] = 0;
        for (int i = 0; i < nodes; ++i) {
            if (fscanf(file, "%64lf", &density[i]) != 1 || !isfinite(density[i]) ||
                density[i] < 0 || density[i] > 1e7)
                goto fail;
            if (i)
                cdf[i] = cdf[i - 1] + phase_pi * (density[i - 1] + density[i]) *
                                      (phase->mu[i] - phase->mu[i - 1]);
        }
        double integral = cdf[nodes - 1];
        if (fabs(integral - 1) > .005) goto fail;
        for (int i = 0; i < nodes; ++i) {
            density[i] /= integral;
            cdf[i] /= integral;
        }
    }
    if (phase->wavelengths[0] != 380 || phase->wavelengths[phase->bands - 1] != 780 ||
        fscanf(file, " %c", &trailing) != EOF || ferror(file))
        goto fail;
    fclose(file);
    ray_tracing_sha256_update(identity, tag, strlen(tag));
    ray_tracing_sha256_update(identity, &phase->bands, sizeof(phase->bands));
    ray_tracing_sha256_update(identity, &phase->nodes, sizeof(phase->nodes));
    ray_tracing_sha256_update(identity, phase->wavelengths, phase->bands * sizeof(double));
    ray_tracing_sha256_update(identity, phase->density, count * sizeof(double));
    return phase;
fail:
    fclose(file);
    spectral_phase_free(phase);
    return NULL;
}

static int wavelength_interval(const SpectralPhase *phase, double nm, double *fraction) {
    int band = 0;
    while (band + 2 < phase->bands && nm > phase->wavelengths[band + 1]) ++band;
    double low = phase->wavelengths[band], high = phase->wavelengths[band + 1];
    *fraction = fmax(0, fmin(1, (nm - low) / (high - low)));
    return band;
}

double spectral_phase_value(const SpectralPhase *phase, double nm, double mu) {
    mu = fmax(-1, fmin(1, mu));
    int nodes = phase->nodes;
    int i = (int)(acos(-mu) * (nodes - 1) / phase_pi);
    if (i >= nodes - 1) i = nodes - 2;
    double t = fmax(0, fmin(1, (mu - phase->mu[i]) / (phase->mu[i + 1] - phase->mu[i])));
    double fraction;
    int band = wavelength_interval(phase, nm, &fraction);
    const double *a = phase->density + (size_t)band * nodes + i;
    const double *b = a + nodes;
    return (1 - fraction) * ((1 - t)*a[0] + t*a[1]) + fraction * ((1 - t)*b[0] + t*b[1]);
}

double spectral_phase_sample(const SpectralPhase *phase, double nm, double u) {
    double fraction;
    int band = wavelength_interval(phase, nm, &fraction);
    /* Sample the same wavelength mixture used for evaluation. */
    if (fraction > 0 && u >= 1 - fraction) {
        ++band;
        u = (u - (1 - fraction)) / fraction;
    } else {
        u /= 1 - fraction;
    }
    const double *cdf = phase->cdf + (size_t)band * phase->nodes;
    const double *density = phase->density + (size_t)band * phase->nodes;
    int lo = 0, hi = phase->nodes - 1;
    while (hi - lo > 1) {
        int mid = lo + (hi - lo) / 2;
        if (cdf[mid] <= u) lo = mid;
        else hi = mid;
    }
    double width = phase->mu[hi] - phase->mu[lo];
    double area = fmax(0, u - cdf[lo]) / (2*phase_pi*width);
    double start = density[lo], slope = density[hi] - start;
    double denom = start + sqrt(fmax(0, start*start + 2*slope*area));
    double t = denom > 0 ? fmin(1, 2*area/denom) : 0;
    return phase->mu[lo] + t*width;
}
