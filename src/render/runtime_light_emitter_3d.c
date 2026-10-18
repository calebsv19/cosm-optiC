#include <fisics/extensions.h>

#include "render/runtime_light_emitter_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_light_set_3d.h"
#include "render/runtime_light_radiometry_3d.h"

static double runtime_light_emitter_3d_zero_length(void) {
    double zero FISICS_DIM(length) FISICS_UNIT(meter) = 0.0;
    return zero;
}

static double runtime_light_emitter_3d_unit_length(void) {
    double unit_length FISICS_DIM(length) FISICS_UNIT(meter) = 1.0;
    return unit_length;
}

static double runtime_light_emitter_3d_length_epsilon(void) {
    double epsilon FISICS_DIM(length) FISICS_UNIT(meter) = 1e-9;
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
                                                   double distance FISICS_DIM(length) FISICS_UNIT(meter)) {
    double falloff FISICS_DIM(length) FISICS_UNIT(meter) = runtime_light_emitter_3d_unit_length();
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

static bool runtime_light_emitter_3d_intersect_light(
    const RuntimeLight3D* light,
    const Ray3D* ray,
    double t_min FISICS_DIM(length) FISICS_UNIT(meter),
    double t_max FISICS_DIM(length) FISICS_UNIT(meter),
    RuntimeLightEmitterHit3DResult* out_result) {
    RuntimeLightEmitterHit3DResult result = {0};
    Vec3 sphere_offset = vec3(0.0, 0.0, 0.0);
    double radius FISICS_DIM(length) FISICS_UNIT(meter) = 0.0;
    double b = 0.0;
    double c = 0.0;
    double discriminant = 0.0;
    double sqrt_disc = 0.0;
    double roots[2] FISICS_DIM(length) FISICS_UNIT(meter) = {0.0, 0.0};
    double hit_t FISICS_DIM(length) FISICS_UNIT(meter) = 0.0;
    Vec3 view_facing = vec3(0.0, 0.0, 0.0);
    double zero_length = runtime_light_emitter_3d_zero_length();
    double epsilon = runtime_light_emitter_3d_length_epsilon();

    if (!light || !ray || !out_result) return false;
    *out_result = result;

    radius = light->radius;
    if (!(radius > epsilon)) {
        return false;
    }

    sphere_offset = vec3_sub(ray->origin, light->position);
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
    result.normal = vec3_normalize(vec3_sub(result.position, light->position));
    if (vec3_length(result.normal) <= epsilon) {
        result.normal = vec3_scale(ray->direction, -1.0);
    }
    view_facing = vec3_scale(ray->direction, -1.0);
    result.radialFalloff =
        runtime_light_emitter_3d_clamp(vec3_dot(result.normal, view_facing), 0.0, 1.0);
    result.attenuation = runtime_light_emitter_3d_attenuation(light, hit_t);
    result.radiance = light->intensity * result.attenuation * result.radialFalloff;
    *out_result = result;
    return true;
}

/* The compatibility MIS PDFs do not describe a rectangle. Until source-aware
 * PDFs are available, use a disjoint estimator: opaque non-delta receivers
 * sample authored rectangles by NEE only, with unit direct weight. Camera,
 * delta and glass-random-walk continuations have no competing direct sample. */
double RuntimeLightEmitter3D_BsdfHitWeight(const RuntimeLightEmitterHit3DResult* hit,
                                            bool exclusive_path, double mis_weight) {
    if (exclusive_path) return 1;
    return hit && hit->directSampledRectangle ? 0 : mis_weight;
}

double RuntimeLightEmitter3D_DirectEstimate(double total, double direct_only, double mis_weight) {
    return total * mis_weight + direct_only * (1 - mis_weight);
}

Vec3 RuntimeLightEmitter3D_RadianceRGB(const RuntimeLightEmitterHit3DResult* hit) {
    if (!hit) return vec3(0, 0, 0);
    return hit->hasRadianceRGB ? hit->radianceRGB
                              : vec3(hit->radiance, hit->radiance, hit->radiance);
}

void RuntimeLightEmitter3D_ApplyTransmittance(RuntimeLightEmitterHit3DResult* hit,
                                              Vec3 tr) {
    if (!hit) return;
    Vec3 rgb = RuntimeLightEmitter3D_RadianceRGB(hit);
    hit->radianceRGB = vec3(rgb.x * tr.x, rgb.y * tr.y, rgb.z * tr.z);
    hit->hasRadianceRGB = true;
    hit->radiance *= .2126 * tr.x + .7152 * tr.y + .0722 * tr.z;
}

/* Use the same normalized authored axes and width/height as the direct-light
 * sampler. Solve the Gram system so bounds also work for a skew basis. */
static bool runtime_light_emitter_3d_intersect_rect(
    const RuntimeLightSource3D* source, const Ray3D* ray,
    double t_min, double t_max, RuntimeLightEmitterHit3DResult* out) {
    if (!(source->width > 1e-9) || !isfinite(source->width) ||
        !(source->height > 1e-9) || !isfinite(source->height)) return false;
    Vec3 u = vec3_length(source->axisU) > 1e-9 ? vec3_normalize(source->axisU) : vec3(1,0,0);
    Vec3 v = vec3_length(source->axisV) > 1e-9 ? vec3_normalize(source->axisV) : vec3(0,0,1);
    Vec3 cross = vec3_cross(u, v);
    double cross_length = vec3_length(cross);
    if (!isfinite(cross_length) || !(cross_length > 1e-9)) return false;
    Vec3 plane_normal = vec3_scale(cross, 1.0 / cross_length);
    double denominator = vec3_dot(plane_normal, ray->direction);
    if (!isfinite(denominator) || fabs(denominator) < 1e-12) return false;
    double t = vec3_dot(vec3_sub(source->position, ray->origin), plane_normal) / denominator;
    if (!isfinite(t) || t < t_min || t > t_max) return false;
    Vec3 position = vec3_add(ray->origin, vec3_scale(ray->direction, t));
    Vec3 offset = vec3_sub(position, source->position);
    double uv = vec3_dot(u, v), determinant = 1.0 - uv * uv;
    double du = vec3_dot(offset, u), dv = vec3_dot(offset, v);
    double x = (du - uv * dv) / determinant;
    double y = (dv - uv * du) / determinant;
    if (!isfinite(x) || !isfinite(y) || fabs(x) > .5 * source->width ||
        fabs(y) > .5 * source->height) return false;
    Vec3 normal = vec3_length(source->normal) > 1e-9 ? vec3_normalize(source->normal) : plane_normal;
    double facing = vec3_dot(normal, vec3_scale(ray->direction, -1));
    if (!isfinite(facing)) return false;
    RuntimeLightEmitterHit3DResult hit = {0};
    hit.hit = true; hit.t = t; hit.position = position; hit.normal = normal;
    hit.attenuation = 1;
    /* A back face is still geometry: it blocks lights behind it, but emits zero. */
    hit.radialFalloff = source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED
        ? fmax(0, facing) : source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED
        ? fabs(facing) : 1;
    if (source->radiometryMode == RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE) {
        RuntimeLightRadiometry3DEvaluation evaluation;
        if (!RuntimeLightRadiometry3D_Evaluate(source, &evaluation)) return false;
        /* Lambertian radiance is angle/distance invariant on the emitting side.
         * Do not apply the irradiance sampler's area, cosine or inverse-square. */
        hit.radiance = hit.radialFalloff > 0 ? evaluation.radiance : 0;
        hit.radianceRGB = vec3_scale(source->color, hit.radiance);
    } else {
        RuntimeLight3D compatibility = {0};
        compatibility.falloffDistance = source->falloffDistance;
        compatibility.falloffMode = source->falloffMode;
        hit.attenuation = runtime_light_emitter_3d_attenuation(&compatibility, t);
        hit.radiance = source->intensity * hit.attenuation * hit.radialFalloff;
        hit.radianceRGB = vec3_scale(source->color, hit.radiance);
    }
    hit.hasRadianceRGB = true;
    hit.directSampledRectangle = true;
    *out = hit;
    return true;
}

static RuntimeLight3D runtime_light_emitter_3d_light_from_source(
    const RuntimeLightSource3D* source) {
    RuntimeLight3D light = {0};
    if (!source) return light;
    light.position = source->position;
    light.radius = source->radius;
    light.intensity = source->intensity;
    light.falloffDistance = source->falloffDistance;
    light.falloffMode = source->falloffMode;
    return light;
}

static bool runtime_light_emitter_3d_intersect_ray(bool camera_query, const RuntimeScene3D* scene,
                                        const Ray3D* ray,
                                        double t_min FISICS_DIM(length) FISICS_UNIT(meter),
                                        double t_max FISICS_DIM(length) FISICS_UNIT(meter),
                                        RuntimeLightEmitterHit3DResult* out_result) {
    RuntimeLightEmitterHit3DResult best = {0};
    bool found = false;

    if (!scene || !ray || !out_result) return false;
    *out_result = best;

    for (int i = 0; i < scene->lightSet.lightCount; ++i) {
        const RuntimeLightSource3D* source = &scene->lightSet.lights[i];
        RuntimeLightEmitterHit3DResult candidate = {0};
        RuntimeLight3D light = {0};
        if (!source->enabled) continue;
        /* Legacy rectangles are lighting helpers, not camera-visible panels.
         * Keep their transport hits for reflections/transmission and leave
         * calibrated radiance emitters camera-visible. Never filter by ID. */
        if (camera_query && source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT &&
            source->radiometryMode == RUNTIME_LIGHT_RADIOMETRY_LEGACY_INTENSITY) continue;
        if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
            /* Material emitters already have an exact geometry/emission owner;
             * their sampling proxies must not become a second visible surface. */
            if (source->origin == RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER ||
                !runtime_light_emitter_3d_intersect_rect(source, ray, t_min, t_max, &candidate)) continue;
            if (!found || candidate.t < best.t) { best = candidate; found = true; }
            continue;
        }
        if (source->radiometryMode != RUNTIME_LIGHT_RADIOMETRY_LEGACY_INTENSITY) continue;
        if (source->kind != RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE &&
            source->kind != RUNTIME_LIGHT_SOURCE_3D_KIND_POINT) {
            continue;
        }
        light = runtime_light_emitter_3d_light_from_source(source);
        if (!runtime_light_emitter_3d_intersect_light(&light,
                                                      ray,
                                                      t_min,
                                                      t_max,
                                                      &candidate)) {
            continue;
        }
        candidate.radianceRGB = vec3_scale(source->color, candidate.radiance);
        candidate.hasRadianceRGB = true;
        if (!found || candidate.t < best.t) {
            best = candidate;
            found = true;
        }
    }

    if (!found && scene->lightSet.lightCount == 0 && scene->hasLight) {
        found = runtime_light_emitter_3d_intersect_light(&scene->light,
                                                        ray,
                                                        t_min,
                                                        t_max,
                                                        &best);
    }

    if (found) {
        *out_result = best;
    }
    return found;
}

bool RuntimeLightEmitter3D_IntersectRay(const RuntimeScene3D* scene, const Ray3D* ray,
                                        double t_min, double t_max,
                                        RuntimeLightEmitterHit3DResult* out_result) {
    return runtime_light_emitter_3d_intersect_ray(false, scene, ray, t_min, t_max, out_result);
}

bool RuntimeLightEmitter3D_IntersectCameraRay(const RuntimeScene3D* scene, const Ray3D* ray,
                                              double t_min, double t_max,
                                              RuntimeLightEmitterHit3DResult* out_result) {
    return runtime_light_emitter_3d_intersect_ray(true, scene, ray, t_min, t_max, out_result);
}

static bool runtime_light_emitter_3d_resolve_first_hit(bool camera_query, const RuntimeScene3D* scene,
                                           const Ray3D* ray,
                                           double t_min FISICS_DIM(length) FISICS_UNIT(meter),
                                           double t_max FISICS_DIM(length) FISICS_UNIT(meter),
                                           RuntimeLightEmitterTrace3DResult* out_result) {
    RuntimeLightEmitterTrace3DResult result = {0};

    if (!scene || !ray || !out_result) return false;

    HitInfo3D_Reset(&result.geometryHitInfo);
    result.geometryHit = RuntimeRay3D_TraceSceneFirstHit(scene,
                                                         ray,
                                                         t_min,
                                                         t_max,
                                                         &result.geometryHitInfo);
    result.emitterHit = runtime_light_emitter_3d_intersect_ray(camera_query, scene,
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

bool RuntimeLightEmitter3D_ResolveFirstHit(const RuntimeScene3D* scene, const Ray3D* ray,
                                           double t_min, double t_max,
                                           RuntimeLightEmitterTrace3DResult* out_result) {
    return runtime_light_emitter_3d_resolve_first_hit(false, scene, ray, t_min, t_max, out_result);
}

bool RuntimeLightEmitter3D_ResolveCameraFirstHit(const RuntimeScene3D* scene, const Ray3D* ray,
                                                 double t_min, double t_max,
                                                 RuntimeLightEmitterTrace3DResult* out_result) {
    return runtime_light_emitter_3d_resolve_first_hit(true, scene, ray, t_min, t_max, out_result);
}
