#ifndef RENDER_RUNTIME_LIGHT_RADIOMETRY_3D_H
#define RENDER_RUNTIME_LIGHT_RADIOMETRY_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_light_set_3d.h"

typedef struct {
    bool valid;
    double areaM2;
    double radiance;
    double angularIntegralSr;
    Vec3 spectralRadiance;
    Vec3 totalEmittedPower;
} RuntimeLightRadiometry3DEvaluation;

RuntimeLightRadiometryMode3D RuntimeLightRadiometryMode3D_FromLabel(
    const char* label);
const char* RuntimeLightRadiometryMode3D_Label(
    RuntimeLightRadiometryMode3D mode);
bool RuntimeLightRadiometry3D_Evaluate(
    const RuntimeLightSource3D* source,
    RuntimeLightRadiometry3DEvaluation* out_evaluation);
bool RuntimeLightRadiometry3D_SampleDirection(
    const RuntimeLightSource3D* source,
    double u0,
    double u1,
    Vec3* out_direction,
    double* out_direction_pdf);
double RuntimeLightRadiometry3D_DirectionPdf(
    const RuntimeLightSource3D* source,
    Vec3 direction);
double RuntimeLightRadiometry3D_RectIrradianceScale(
    const RuntimeLightSource3D* source,
    Vec3 direction_from_receiver_to_source,
    double distance_m);
bool RuntimeLightRadiometry3D_PerspectivePixelFootprintArea(
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    Vec3 receiver_position,
    Vec3 receiver_geometric_normal,
    double* out_area_m2);

#endif
