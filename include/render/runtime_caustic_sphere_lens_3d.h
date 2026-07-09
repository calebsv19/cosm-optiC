#ifndef RENDER_RUNTIME_CAUSTIC_SPHERE_LENS_3D_H
#define RENDER_RUNTIME_CAUSTIC_SPHERE_LENS_3D_H

#include <stdbool.h>

#include "math/vec3.h"

typedef struct {
    Vec3 center;
    double radius;
    double outsideIor;
    double ior;
    double fresnelScale;
    double transmissionScale;
    Vec3 tint;
    double absorptionDistance;
    double apertureRadiusScale;
} RuntimeCausticSphereLens3DDescriptor;

typedef struct {
    Vec3 position;
    double radius;
    double intensity;
    Vec3 color;
} RuntimeCausticSphereLens3DLight;

typedef struct {
    double apertureU;
    double apertureV;
    double lensU;
    double lensV;
    double sampleWeight;
    double receiverPlaneZ;
} RuntimeCausticSphereLens3DSample;

typedef struct {
    bool valid;
    bool totalInternalReflection;
    Vec3 lightSamplePosition;
    Vec3 lensTargetPosition;
    Vec3 entryPosition;
    Vec3 entryNormal;
    Vec3 entryDirection;
    Vec3 insideDirection;
    Vec3 exitPosition;
    Vec3 exitNormal;
    Vec3 exitDirection;
    double entryDistance;
    double insideDistance;
    double exitReceiverT;
    Vec3 receiverCrossing;
    double entryFresnel;
    double exitFresnel;
    Vec3 throughput;
} RuntimeCausticSphereLens3DPath;

void RuntimeCausticSphereLens3D_DefaultDescriptor(
    RuntimeCausticSphereLens3DDescriptor* descriptor);
void RuntimeCausticSphereLens3D_DefaultLight(RuntimeCausticSphereLens3DLight* light);
void RuntimeCausticSphereLens3D_DefaultSample(RuntimeCausticSphereLens3DSample* sample);
bool RuntimeCausticSphereLens3D_IntersectRay(const RuntimeCausticSphereLens3DDescriptor* sphere,
                                             Vec3 origin,
                                             Vec3 direction,
                                             double min_t,
                                             double* out_t);
bool RuntimeCausticSphereLens3D_Refract(Vec3 incident,
                                        Vec3 normal,
                                        double eta_from,
                                        double eta_to,
                                        Vec3* out_direction,
                                        bool* out_total_internal_reflection);
bool RuntimeCausticSphereLens3D_SolvePath(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    RuntimeCausticSphereLens3DPath* out_path);

#endif
