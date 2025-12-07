#include "render/integrators/hybrid/integrator_sampling.h"
#include <stdint.h>

static inline double fast_atan2(double y, double x) {
    return atan2(y, x);
}

void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-12) {
        *x /= len;
        *y /= len;
    }
}

double HashDouble(int x, int y, int s) {
    uint32_t h = (uint32_t)(x * 73856093) ^
                 (uint32_t)(y * 19349663) ^
                 (uint32_t)(s * 83492791);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return (double)(h & 0xFFFFFF) / (double)0x1000000;
}

double PixelFeelerJitter(int px, int py, int idx) {
    double base = HashDouble(px, py, idx);
    return base * 2.0 - 1.0;
}

void OrientNormalForIncoming(double inX, double inY,
                             double nx, double ny,
                             double* outNx, double* outNy)
{
    NormalizeVector(&inX, &inY);
    double dot = inX * nx + inY * ny;

    if (fabs(dot) < 1e-8) {
        *outNx = nx;
        *outNy = ny;
        return;
    }

    if (dot > 0.0) {
        nx = -nx;
        ny = -ny;
    }

    *outNx = nx;
    *outNy = ny;
}

void FeelerDirection(int index, int total, double jitter,
                     double baseNx, double baseNy,
                     double* outX, double* outY)
{
    if (total < 1) total = 1;
    double baseAngle = fast_atan2(baseNy, baseNx);

    double hemiAngle = (M_PI * (double)index) / (double)total;
    double jitterAngle = 0.5 * (M_PI / (double)total) * jitter;

    double angle = baseAngle + hemiAngle + jitterAngle;

    *outX = cos(angle);
    *outY = sin(angle);
}

int DetermineFeelerCount(float directLimit, float qualityScale)
{
    int base = 16;
    if (directLimit < 4.0f) base = 24;
    if (directLimit < 1.5f) base = 32;
    if (directLimit < 0.3f) base = 48;

    float scaled = base * qualityScale;
    int out = (int)lround(scaled);
    if (out < 8) out = 8;
    return out;
}
