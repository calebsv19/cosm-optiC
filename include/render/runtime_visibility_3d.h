#ifndef RENDER_RUNTIME_VISIBILITY_3D_H
#define RENDER_RUNTIME_VISIBILITY_3D_H

#include <stdbool.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"

typedef struct {
    double luma;
    double r;
    double g;
    double b;
} RuntimeVisibility3DTransmittance;

RuntimeVisibility3DTransmittance RuntimeVisibility3D_UnitTransmittance(void);

void RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(
    const RuntimeMaterialPayload3D* payload,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double segment_distance,
    RuntimeVisibility3DTransmittance* io_transmittance);

bool RuntimeVisibility3D_TraceToLight(const RuntimeScene3D* scene,
                                      Vec3 surface_position,
                                      Vec3 surface_normal,
                                      Vec3 light_position,
                                      HitInfo3D* out_blocker_hit,
                                      [[fisics::dim(length)]] [[fisics::unit(meter)]] double* out_light_distance);

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceToLightRGB(
    const RuntimeScene3D* scene,
    Vec3 surface_position,
    Vec3 surface_normal,
    Vec3 light_position);

double RuntimeVisibility3D_TransmittanceToLight(const RuntimeScene3D* scene,
                                                Vec3 surface_position,
                                                Vec3 surface_normal,
                                                Vec3 light_position);

bool RuntimeVisibility3D_HasLineOfSightToLight(const RuntimeScene3D* scene,
                                               Vec3 surface_position,
                                               Vec3 surface_normal,
                                               Vec3 light_position);

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceFromHitRGB(
    const RuntimeScene3D* scene,
    const HitInfo3D* surface_hit,
    const RuntimeLight3D* light);

double RuntimeVisibility3D_TransmittanceFromHit(const RuntimeScene3D* scene,
                                                const HitInfo3D* surface_hit,
                                                const RuntimeLight3D* light);

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceFromHitToPointRGB(
    const RuntimeScene3D* scene,
    const HitInfo3D* surface_hit,
    Vec3 target_position,
    int target_scene_object_index,
    int target_triangle_index);

double RuntimeVisibility3D_TransmittanceFromHitToPoint(const RuntimeScene3D* scene,
                                                       const HitInfo3D* surface_hit,
                                                       Vec3 target_position,
                                                       int target_scene_object_index,
                                                       int target_triangle_index);

bool RuntimeVisibility3D_HasLineOfSightFromHit(const RuntimeScene3D* scene,
                                               const HitInfo3D* surface_hit,
                                               const RuntimeLight3D* light);

#endif
