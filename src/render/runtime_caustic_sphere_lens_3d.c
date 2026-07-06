#include "render/runtime_caustic_sphere_lens_3d.h"

#include <math.h>
#include <string.h>

static double sphere_lens_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double sphere_lens_saturate(double value) {
    return sphere_lens_clamp(value, 0.0, 1.0);
}

static bool sphere_lens_finite_vec3(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static void sphere_lens_build_basis(Vec3 axis, Vec3* out_u, Vec3* out_v) {
    Vec3 up = vec3(0.0, 0.0, 1.0);
    Vec3 u = vec3(0.0, 0.0, 0.0);
    Vec3 v = vec3(0.0, 0.0, 0.0);
    if (fabs(vec3_dot(vec3_normalize(axis), up)) > 0.92) {
        up = vec3(0.0, 1.0, 0.0);
    }
    u = vec3_normalize(vec3_cross(up, axis));
    if (!(vec3_length(u) > 1.0e-9)) {
        u = vec3(1.0, 0.0, 0.0);
    }
    v = vec3_normalize(vec3_cross(axis, u));
    if (!(vec3_length(v) > 1.0e-9)) {
        v = vec3(0.0, 1.0, 0.0);
    }
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
}

static double sphere_lens_fresnel_schlick(Vec3 incident,
                                          Vec3 normal,
                                          double eta_from,
                                          double eta_to) {
    double r0 = 0.0;
    double cos_i = 0.0;
    Vec3 i = vec3_normalize(incident);
    Vec3 n = vec3_normalize(normal);
    if (!(eta_from > 0.0) || !(eta_to > 0.0)) return 1.0;
    r0 = (eta_from - eta_to) / (eta_from + eta_to);
    r0 *= r0;
    cos_i = fabs(vec3_dot(i, n));
    cos_i = sphere_lens_saturate(cos_i);
    return sphere_lens_saturate(r0 + (1.0 - r0) * pow(1.0 - cos_i, 5.0));
}

void RuntimeCausticSphereLens3D_DefaultDescriptor(
    RuntimeCausticSphereLens3DDescriptor* descriptor) {
    if (!descriptor) return;
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->center = vec3(0.0, 0.0, 0.0);
    descriptor->radius = 1.0;
    descriptor->ior = 1.5;
    descriptor->tint = vec3(1.0, 1.0, 1.0);
    descriptor->absorptionDistance = 0.0;
}

void RuntimeCausticSphereLens3D_DefaultLight(RuntimeCausticSphereLens3DLight* light) {
    if (!light) return;
    memset(light, 0, sizeof(*light));
    light->position = vec3(0.0, 0.0, 4.0);
    light->radius = 0.0;
    light->intensity = 1.0;
    light->color = vec3(1.0, 1.0, 1.0);
}

void RuntimeCausticSphereLens3D_DefaultSample(RuntimeCausticSphereLens3DSample* sample) {
    if (!sample) return;
    memset(sample, 0, sizeof(*sample));
    sample->sampleWeight = 1.0;
    sample->receiverPlaneZ = -2.0;
}

bool RuntimeCausticSphereLens3D_IntersectRay(const RuntimeCausticSphereLens3DDescriptor* sphere,
                                             Vec3 origin,
                                             Vec3 direction,
                                             double min_t,
                                             double* out_t) {
    Vec3 oc = vec3(0.0, 0.0, 0.0);
    Vec3 dir = vec3_normalize(direction);
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double disc = 0.0;
    double root_disc = 0.0;
    double t0 = 0.0;
    double t1 = 0.0;
    if (out_t) *out_t = 0.0;
    if (!sphere || !out_t || !(sphere->radius > 1.0e-9) ||
        !(vec3_length(dir) > 1.0e-9) || !sphere_lens_finite_vec3(origin) ||
        !sphere_lens_finite_vec3(direction)) {
        return false;
    }
    if (!(min_t >= 0.0)) min_t = 0.0;
    oc = vec3_sub(origin, sphere->center);
    a = vec3_dot(dir, dir);
    b = 2.0 * vec3_dot(oc, dir);
    c = vec3_dot(oc, oc) - sphere->radius * sphere->radius;
    disc = b * b - 4.0 * a * c;
    if (!(disc >= 0.0)) return false;
    root_disc = sqrt(disc);
    t0 = (-b - root_disc) / (2.0 * a);
    t1 = (-b + root_disc) / (2.0 * a);
    if (t0 > min_t) {
        *out_t = t0;
        return true;
    }
    if (t1 > min_t) {
        *out_t = t1;
        return true;
    }
    return false;
}

bool RuntimeCausticSphereLens3D_Refract(Vec3 incident,
                                        Vec3 normal,
                                        double eta_from,
                                        double eta_to,
                                        Vec3* out_direction,
                                        bool* out_total_internal_reflection) {
    Vec3 i = vec3_normalize(incident);
    Vec3 n = vec3_normalize(normal);
    double cos_i = 0.0;
    double eta = 0.0;
    double k = 0.0;
    Vec3 refracted = vec3(0.0, 0.0, 0.0);
    if (out_direction) *out_direction = vec3(0.0, 0.0, 0.0);
    if (out_total_internal_reflection) *out_total_internal_reflection = false;
    if (!out_direction || !(vec3_length(i) > 1.0e-9) ||
        !(vec3_length(n) > 1.0e-9) || !(eta_from > 0.0) || !(eta_to > 0.0)) {
        return false;
    }
    cos_i = -vec3_dot(n, i);
    if (cos_i < 0.0) {
        n = vec3_scale(n, -1.0);
        cos_i = -vec3_dot(n, i);
    }
    cos_i = sphere_lens_saturate(cos_i);
    eta = eta_from / eta_to;
    k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
    if (k < 0.0) {
        if (out_total_internal_reflection) *out_total_internal_reflection = true;
        return false;
    }
    refracted = vec3_add(vec3_scale(i, eta),
                         vec3_scale(n, eta * cos_i - sqrt(k)));
    if (!(vec3_length(refracted) > 1.0e-9)) return false;
    *out_direction = vec3_normalize(refracted);
    return true;
}

bool RuntimeCausticSphereLens3D_SolvePath(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    RuntimeCausticSphereLens3DPath* out_path) {
    RuntimeCausticSphereLens3DSample default_sample;
    RuntimeCausticSphereLens3DPath path;
    Vec3 optical_axis = vec3(0.0, 0.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 basis_v = vec3(0.0, 1.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    double entry_t = 0.0;
    double exit_t = 0.0;
    double receiver_t = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double absorption = 1.0;
    double sample_weight = 1.0;
    bool tir = false;
    const RuntimeCausticSphereLens3DSample* active_sample = sample;

    if (out_path) memset(out_path, 0, sizeof(*out_path));
    if (!sphere || !light || !out_path || !(sphere->radius > 1.0e-9) ||
        !(sphere->ior > 1.0) || !sphere_lens_finite_vec3(sphere->center) ||
        !sphere_lens_finite_vec3(light->position)) {
        return false;
    }
    if (!active_sample) {
        RuntimeCausticSphereLens3D_DefaultSample(&default_sample);
        active_sample = &default_sample;
    }
    memset(&path, 0, sizeof(path));
    optical_axis = vec3_sub(sphere->center, light->position);
    if (!(vec3_length(optical_axis) > sphere->radius + 1.0e-6)) return false;
    optical_axis = vec3_normalize(optical_axis);
    sphere_lens_build_basis(optical_axis, &basis_u, &basis_v);

    aperture_u = sphere_lens_clamp(active_sample->apertureU, -1.0, 1.0);
    aperture_v = sphere_lens_clamp(active_sample->apertureV, -1.0, 1.0);
    lens_u = sphere_lens_clamp(active_sample->lensU, -0.98, 0.98);
    lens_v = sphere_lens_clamp(active_sample->lensV, -0.98, 0.98);
    if (lens_u * lens_u + lens_v * lens_v > 0.98 * 0.98) {
        const double scale = 0.98 / sqrt(lens_u * lens_u + lens_v * lens_v);
        lens_u *= scale;
        lens_v *= scale;
    }

    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u, aperture_u * fmax(light->radius, 0.0)),
                 vec3_scale(basis_v, aperture_v * fmax(light->radius, 0.0))));
    path.lightSamplePosition = ray_origin;
    path.lensTargetPosition = vec3_add(
        sphere->center,
        vec3_add(vec3_scale(basis_u, lens_u * sphere->radius),
                 vec3_scale(basis_v, lens_v * sphere->radius)));
    ray_dir = vec3_normalize(vec3_sub(path.lensTargetPosition, ray_origin));
    if (!(vec3_length(ray_dir) > 1.0e-9)) return false;

    if (!RuntimeCausticSphereLens3D_IntersectRay(sphere, ray_origin, ray_dir, 1.0e-6, &entry_t)) {
        return false;
    }
    path.entryDistance = entry_t;
    path.entryPosition = vec3_add(ray_origin, vec3_scale(ray_dir, entry_t));
    path.entryNormal = vec3_normalize(vec3_sub(path.entryPosition, sphere->center));
    path.entryDirection = ray_dir;
    path.entryFresnel = sphere_lens_fresnel_schlick(ray_dir, path.entryNormal, 1.0, sphere->ior);
    if (!RuntimeCausticSphereLens3D_Refract(ray_dir,
                                            path.entryNormal,
                                            1.0,
                                            sphere->ior,
                                            &path.insideDirection,
                                            &tir)) {
        path.totalInternalReflection = tir;
        *out_path = path;
        return false;
    }

    if (!RuntimeCausticSphereLens3D_IntersectRay(sphere,
                                                 path.entryPosition,
                                                 path.insideDirection,
                                                 1.0e-5,
                                                 &exit_t)) {
        *out_path = path;
        return false;
    }
    path.insideDistance = exit_t;
    path.exitPosition = vec3_add(path.entryPosition, vec3_scale(path.insideDirection, exit_t));
    path.exitNormal = vec3_normalize(vec3_sub(path.exitPosition, sphere->center));
    path.exitFresnel =
        sphere_lens_fresnel_schlick(path.insideDirection, path.exitNormal, sphere->ior, 1.0);
    if (!RuntimeCausticSphereLens3D_Refract(path.insideDirection,
                                            path.exitNormal,
                                            sphere->ior,
                                            1.0,
                                            &path.exitDirection,
                                            &tir)) {
        path.totalInternalReflection = tir;
        *out_path = path;
        return false;
    }

    if (sphere->absorptionDistance > 1.0e-9) {
        absorption = exp(-path.insideDistance / sphere->absorptionDistance);
    }
    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    path.throughput =
        vec3(light->color.x * light->intensity * sample_weight *
                 (1.0 - path.entryFresnel) * (1.0 - path.exitFresnel) *
                 sphere_lens_saturate(sphere->tint.x) * absorption,
             light->color.y * light->intensity * sample_weight *
                 (1.0 - path.entryFresnel) * (1.0 - path.exitFresnel) *
                 sphere_lens_saturate(sphere->tint.y) * absorption,
             light->color.z * light->intensity * sample_weight *
                 (1.0 - path.entryFresnel) * (1.0 - path.exitFresnel) *
                 sphere_lens_saturate(sphere->tint.z) * absorption);

    if (fabs(path.exitDirection.z) > 1.0e-9) {
        receiver_t = (active_sample->receiverPlaneZ - path.exitPosition.z) /
                     path.exitDirection.z;
        if (receiver_t > 0.0 && isfinite(receiver_t)) {
            path.exitReceiverT = receiver_t;
            path.receiverCrossing =
                vec3_add(path.exitPosition, vec3_scale(path.exitDirection, receiver_t));
        }
    }
    path.valid = true;
    *out_path = path;
    return true;
}
