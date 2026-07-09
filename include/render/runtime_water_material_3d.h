#ifndef RENDER_RUNTIME_WATER_MATERIAL_3D_H
#define RENDER_RUNTIME_WATER_MATERIAL_3D_H

#include <stdbool.h>

#include "render/runtime_material_payload_3d.h"

typedef struct {
    bool valid;
    double ior;
    double absorptionDistance;
    double absorptionR;
    double absorptionG;
    double absorptionB;
    double transparency;
    double reflectivity;
    double roughness;
} RuntimeWaterMaterial3DOverride;

void RuntimeWaterMaterial3D_ClearAll(void);
void RuntimeWaterMaterial3D_Clear(int scene_object_index);
bool RuntimeWaterMaterial3D_Set(int scene_object_index,
                                const RuntimeWaterMaterial3DOverride* override);
bool RuntimeWaterMaterial3D_Get(int scene_object_index,
                                RuntimeWaterMaterial3DOverride* out_override);
void RuntimeWaterMaterial3D_ComputeTransmittanceTint(double absorption_distance,
                                                     double absorption_r,
                                                     double absorption_g,
                                                     double absorption_b,
                                                     double* out_r,
                                                     double* out_g,
                                                     double* out_b);
bool RuntimeWaterMaterial3D_ApplyToPayload(int scene_object_index,
                                           RuntimeMaterialPayload3D* payload);

#endif
