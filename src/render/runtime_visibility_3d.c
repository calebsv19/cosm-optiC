#include "render/runtime_visibility_3d.h"

#include <math.h>

static const double kRuntimeVisibility3DEpsilon = 1e-4;

bool RuntimeVisibility3D_TraceToLight(const RuntimeScene3D* scene,
                                      Vec3 surface_position,
                                      Vec3 surface_normal,
                                      Vec3 light_position,
                                      HitInfo3D* out_blocker_hit,
                                      double* out_light_distance) {
    Vec3 to_light;
    double light_distance = 0.0;
    Ray3D shadow_ray = {0};
    HitInfo3D blocker_hit = {0};
    bool blocked = false;

    if (!scene) return false;

    to_light = vec3_sub(light_position, surface_position);
    light_distance = vec3_length(to_light);
    if (out_light_distance) {
        *out_light_distance = light_distance;
    }
    if (light_distance <= kRuntimeVisibility3DEpsilon) {
        if (out_blocker_hit) {
            HitInfo3D_Reset(out_blocker_hit);
        }
        return false;
    }

    shadow_ray = RuntimeRay3D_MakeOffset(surface_position,
                                         surface_normal,
                                         to_light,
                                         kRuntimeVisibility3DEpsilon);
    blocked = RuntimeRay3D_TraceSceneFirstHit(scene,
                                              &shadow_ray,
                                              kRuntimeVisibility3DEpsilon,
                                              fmax(light_distance - kRuntimeVisibility3DEpsilon,
                                                   kRuntimeVisibility3DEpsilon),
                                              &blocker_hit);
    if (out_blocker_hit) {
        if (blocked) {
            *out_blocker_hit = blocker_hit;
        } else {
            HitInfo3D_Reset(out_blocker_hit);
        }
    }
    return blocked;
}

bool RuntimeVisibility3D_HasLineOfSightToLight(const RuntimeScene3D* scene,
                                               Vec3 surface_position,
                                               Vec3 surface_normal,
                                               Vec3 light_position) {
    return !RuntimeVisibility3D_TraceToLight(scene,
                                             surface_position,
                                             surface_normal,
                                             light_position,
                                             NULL,
                                             NULL);
}

bool RuntimeVisibility3D_HasLineOfSightFromHit(const RuntimeScene3D* scene,
                                               const HitInfo3D* surface_hit,
                                               const RuntimeLight3D* light) {
    if (!surface_hit || !light) return false;
    return RuntimeVisibility3D_HasLineOfSightToLight(scene,
                                                     surface_hit->position,
                                                     surface_hit->normal,
                                                     light->position);
}
