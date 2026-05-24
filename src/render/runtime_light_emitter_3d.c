#include "render/runtime_light_emitter_3d.h"

#include <math.h>
#include <string.h>

static double runtime_light_emitter_3d_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_light_emitter_3d_unit_length(void) {
    double unit_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1.0;
    return unit_length;
}

static double runtime_light_emitter_3d_length_epsilon(void) {
    double epsilon [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1e-9;
    return epsilon;
}

static double runtime_light_emitter_3d_clamp(double value,
                                             double min_value,
                                             double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_light_emitter_3d_attenuation(const RuntimeLight3D* light,
                                                   double distance [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double falloff [[fisics::dim(length)]] [[fisics::unit(meter)]] = runtime_light_emitter_3d_unit_length();
    double normalized = 0.0;
    double zero_length = runtime_light_emitter_3d_zero_length();

    if (!light) return 0.0;

    falloff = light->falloffDistance;
    if (!(falloff > zero_length)) {
        falloff = runtime_light_emitter_3d_unit_length();
    }
    normalized = distance / falloff;

    switch (light->falloffMode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            return 1.0 / (1.0 + normalized);
        case FORWARD_FALLOFF_MODE_NONE:
            return 1.0;
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            return 1.0 / (1.0 + normalized * normalized);
    }
}

bool RuntimeLightEmitter3D_IntersectRay(const RuntimeScene3D* scene,
                                        const Ray3D* ray,
                                        double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                        double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                        RuntimeLightEmitterHit3DResult* out_result) {
    RuntimeLightEmitterHit3DResult result = {0};
    Vec3 sphere_offset = vec3(0.0, 0.0, 0.0);
    double radius [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double b = 0.0;
    double c = 0.0;
    double discriminant = 0.0;
    double sqrt_disc = 0.0;
    double roots[2] [[fisics::dim(length)]] [[fisics::unit(meter)]] = {0.0, 0.0};
    double hit_t [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    Vec3 view_facing = vec3(0.0, 0.0, 0.0);
    double zero_length = runtime_light_emitter_3d_zero_length();
    double epsilon = runtime_light_emitter_3d_length_epsilon();

    if (!scene || !ray || !out_result) return false;
    *out_result = result;
    if (!scene->hasLight) return false;

    radius = scene->light.radius;
    if (!(radius > epsilon)) {
        return false;
    }

    sphere_offset = vec3_sub(ray->origin, scene->light.position);
    b = 2.0 * vec3_dot(sphere_offset, ray->direction);
    c = vec3_dot(sphere_offset, sphere_offset) - (radius * radius);
    discriminant = (b * b) - (4.0 * c);
    if (discriminant < 0.0) {
        return false;
    }

    sqrt_disc = sqrt(discriminant);
    roots[0] = (-b - sqrt_disc) * 0.5;
    roots[1] = (-b + sqrt_disc) * 0.5;
    hit_t = -runtime_light_emitter_3d_unit_length();

    for (int i = 0; i < 2; ++i) {
        if (roots[i] < t_min || roots[i] > t_max) continue;
        if (!(hit_t > zero_length) || roots[i] < hit_t) {
            hit_t = roots[i];
        }
    }
    if (!(hit_t >= t_min) || !(hit_t <= t_max)) {
        return false;
    }

    result.hit = true;
    result.t = hit_t;
    result.position = vec3_add(ray->origin, vec3_scale(ray->direction, hit_t));
    result.normal = vec3_normalize(vec3_sub(result.position, scene->light.position));
    if (vec3_length(result.normal) <= epsilon) {
        result.normal = vec3_scale(ray->direction, -1.0);
    }
    view_facing = vec3_scale(ray->direction, -1.0);
    result.radialFalloff =
        runtime_light_emitter_3d_clamp(vec3_dot(result.normal, view_facing), 0.0, 1.0);
    result.attenuation = runtime_light_emitter_3d_attenuation(&scene->light, hit_t);
    result.radiance = scene->light.intensity * result.attenuation * result.radialFalloff;
    *out_result = result;
    return true;
}

bool RuntimeLightEmitter3D_ResolveFirstHit(const RuntimeScene3D* scene,
                                           const Ray3D* ray,
                                           double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                           double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                           RuntimeLightEmitterTrace3DResult* out_result) {
    RuntimeLightEmitterTrace3DResult result = {0};

    if (!scene || !ray || !out_result) return false;

    HitInfo3D_Reset(&result.geometryHitInfo);
    result.geometryHit = RuntimeRay3D_TraceSceneFirstHit(scene,
                                                         ray,
                                                         t_min,
                                                         t_max,
                                                         &result.geometryHitInfo);
    result.emitterHit = RuntimeLightEmitter3D_IntersectRay(scene,
                                                           ray,
                                                           t_min,
                                                           t_max,
                                                           &result.emitterHitInfo);
    result.emitterWins = result.emitterHit &&
                         (!result.geometryHit ||
                          result.emitterHitInfo.t < result.geometryHitInfo.t);
    *out_result = result;
    return result.geometryHit || result.emitterHit;
}
