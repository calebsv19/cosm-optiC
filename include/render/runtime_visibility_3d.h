#ifndef RENDER_RUNTIME_VISIBILITY_3D_H
#define RENDER_RUNTIME_VISIBILITY_3D_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"

bool RuntimeVisibility3D_TraceToLight(const RuntimeScene3D* scene,
                                      Vec3 surface_position,
                                      Vec3 surface_normal,
                                      Vec3 light_position,
                                      HitInfo3D* out_blocker_hit,
                                      double* out_light_distance);

bool RuntimeVisibility3D_HasLineOfSightToLight(const RuntimeScene3D* scene,
                                               Vec3 surface_position,
                                               Vec3 surface_normal,
                                               Vec3 light_position);

bool RuntimeVisibility3D_HasLineOfSightFromHit(const RuntimeScene3D* scene,
                                               const HitInfo3D* surface_hit,
                                               const RuntimeLight3D* light);

#endif
