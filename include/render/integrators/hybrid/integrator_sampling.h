#ifndef INTEGRATOR_SAMPLING_H
#define INTEGRATOR_SAMPLING_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float renderQualityScale;
} IntegratorSamplingSettings;

double HashDouble(int x, int y, int s);
double PixelFeelerJitter(int px, int py, int idx);
void NormalizeVector(double* x, double* y);
void OrientNormalForIncoming(double inX, double inY,
                             double nx, double ny,
                             double* outNx, double* outNy);

void FeelerDirection(int index, int total, double jitter,
                     double baseNx, double baseNy,
                     double* outX, double* outY);

int DetermineFeelerCount(float directLimit, float qualityScale);

#ifdef __cplusplus
}
#endif

#endif
