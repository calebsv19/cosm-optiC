#include "render/runtime_light_radiometry_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double radiometry_nonnegative(double value) {
    return isfinite(value) && value > 0.0 ? value : 0.0;
}

static Vec3 radiometry_normalize_or(Vec3 value, Vec3 fallback) {
    if (!isfinite(value.x) || !isfinite(value.y) || !isfinite(value.z) ||
        !(vec3_length(value) > 1.0e-12)) {
        return fallback;
    }
    return vec3_normalize(value);
}

static void radiometry_basis(Vec3 normal, Vec3* out_u, Vec3* out_v) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0)
                                          : vec3(0.0, 1.0, 0.0);
    Vec3 u = vec3_cross(guide, normal);
    if (!(vec3_length(u) > 1.0e-12)) u = vec3(1.0, 0.0, 0.0);
    u = vec3_normalize(u);
    if (out_u) *out_u = u;
    if (out_v) *out_v = vec3_normalize(vec3_cross(normal, u));
}

RuntimeLightRadiometryMode3D RuntimeLightRadiometryMode3D_FromLabel(
    const char* label) {
    return label && (strcmp(label, "lambertian_radiance") == 0 ||
                     strcmp(label, "physical_radiance") == 0)
               ? RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE
               : RUNTIME_LIGHT_RADIOMETRY_LEGACY_INTENSITY;
}

const char* RuntimeLightRadiometryMode3D_Label(
    RuntimeLightRadiometryMode3D mode) {
    return mode == RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE
               ? "lambertian_radiance"
               : "legacy_intensity";
}

bool RuntimeLightRadiometry3D_Evaluate(
    const RuntimeLightSource3D* source,
    RuntimeLightRadiometry3DEvaluation* out_evaluation) {
    RuntimeLightRadiometry3DEvaluation result = {0};
    double sides = 0.0;
    if (out_evaluation) *out_evaluation = result;
    if (!source ||
        source->radiometryMode != RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE ||
        source->kind != RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
        return false;
    }
    result.areaM2 = radiometry_nonnegative(source->width) *
                    radiometry_nonnegative(source->height);
    result.radiance = radiometry_nonnegative(source->radiance);
    if (source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED) {
        sides = 1.0;
    } else if (source->emissionProfile ==
               RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED) {
        sides = 2.0;
    }
    if (!(result.areaM2 > 0.0) || !(result.radiance > 0.0) || !(sides > 0.0) ||
        !isfinite(source->color.x) || !isfinite(source->color.y) ||
        !isfinite(source->color.z)) {
        return false;
    }
    result.angularIntegralSr = sides * M_PI;
    result.spectralRadiance = vec3_scale(source->color, result.radiance);
    result.totalEmittedPower = vec3_scale(
        result.spectralRadiance, result.areaM2 * result.angularIntegralSr);
    result.valid = true;
    if (out_evaluation) *out_evaluation = result;
    return true;
}

double RuntimeLightRadiometry3D_DirectionPdf(
    const RuntimeLightSource3D* source,
    Vec3 direction) {
    RuntimeLightRadiometry3DEvaluation evaluation;
    Vec3 normal;
    double cosine = 0.0;
    if (!RuntimeLightRadiometry3D_Evaluate(source, &evaluation)) return 0.0;
    normal = radiometry_normalize_or(source->normal, vec3(0.0, -1.0, 0.0));
    cosine = vec3_dot(normal, radiometry_normalize_or(direction, normal));
    if (source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED) {
        return cosine > 0.0 ? cosine / M_PI : 0.0;
    }
    return fabs(cosine) / (2.0 * M_PI);
}

bool RuntimeLightRadiometry3D_SampleDirection(
    const RuntimeLightSource3D* source,
    double u0,
    double u1,
    Vec3* out_direction,
    double* out_direction_pdf) {
    RuntimeLightRadiometry3DEvaluation evaluation;
    Vec3 normal;
    Vec3 axis_u;
    Vec3 axis_v;
    Vec3 direction;
    double side = 1.0;
    double hemisphere_u = fmin(fmax(u0, 0.0), 1.0 - 1.0e-15);
    double phi = 0.0;
    double radius = 0.0;
    double z = 0.0;
    if (out_direction) *out_direction = vec3(0.0, 0.0, 0.0);
    if (out_direction_pdf) *out_direction_pdf = 0.0;
    if (!RuntimeLightRadiometry3D_Evaluate(source, &evaluation)) return false;
    if (source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED) {
        side = hemisphere_u < 0.5 ? 1.0 : -1.0;
        hemisphere_u = hemisphere_u < 0.5 ? hemisphere_u * 2.0
                                           : (hemisphere_u - 0.5) * 2.0;
    }
    normal = vec3_scale(
        radiometry_normalize_or(source->normal, vec3(0.0, -1.0, 0.0)), side);
    radiometry_basis(normal, &axis_u, &axis_v);
    radius = sqrt(hemisphere_u);
    z = sqrt(fmax(0.0, 1.0 - hemisphere_u));
    phi = 2.0 * M_PI * fmin(fmax(u1, 0.0), 1.0 - 1.0e-15);
    direction = vec3_add(vec3_scale(axis_u, radius * cos(phi)),
                         vec3_add(vec3_scale(axis_v, radius * sin(phi)),
                                  vec3_scale(normal, z)));
    direction = vec3_normalize(direction);
    if (out_direction) *out_direction = direction;
    if (out_direction_pdf) {
        *out_direction_pdf = RuntimeLightRadiometry3D_DirectionPdf(source,
                                                                   direction);
    }
    return true;
}

double RuntimeLightRadiometry3D_RectIrradianceScale(
    const RuntimeLightSource3D* source,
    Vec3 direction_from_receiver_to_source,
    double distance_m) {
    RuntimeLightRadiometry3DEvaluation evaluation;
    Vec3 normal;
    Vec3 direction;
    double emitter_cosine = 0.0;
    if (!RuntimeLightRadiometry3D_Evaluate(source, &evaluation) ||
        !(distance_m > 1.0e-12) || !isfinite(distance_m)) {
        return 0.0;
    }
    normal = radiometry_normalize_or(source->normal, vec3(0.0, -1.0, 0.0));
    direction = radiometry_normalize_or(direction_from_receiver_to_source,
                                        vec3_scale(normal, -1.0));
    emitter_cosine = vec3_dot(normal, vec3_scale(direction, -1.0));
    if (source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED) {
        emitter_cosine = fmax(0.0, emitter_cosine);
    } else {
        emitter_cosine = fabs(emitter_cosine);
    }
    return evaluation.radiance * evaluation.areaM2 * emitter_cosine /
           (distance_m * distance_m);
}

static bool radiometry_intersect_receiver_plane(
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    Vec3 receiver_position,
    Vec3 receiver_normal,
    Vec3* out_position) {
    Ray3D ray;
    double denominator = 0.0;
    double distance = 0.0;
    if (!projector || !out_position) return false;
    ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    denominator = vec3_dot(ray.direction, receiver_normal);
    if (!isfinite(denominator) || fabs(denominator) <= 1.0e-12) return false;
    distance = vec3_dot(vec3_sub(receiver_position, ray.origin), receiver_normal) /
               denominator;
    if (!isfinite(distance) || !(distance > 0.0)) return false;
    *out_position = vec3_add(ray.origin, vec3_scale(ray.direction, distance));
    return true;
}

bool RuntimeLightRadiometry3D_PerspectivePixelFootprintArea(
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    Vec3 receiver_position,
    Vec3 receiver_geometric_normal,
    double* out_area_m2) {
    Vec3 normal;
    Vec3 p00;
    Vec3 p10;
    Vec3 p11;
    Vec3 p01;
    double area = 0.0;
    if (out_area_m2) *out_area_m2 = 0.0;
    if (!projector || !out_area_m2 ||
        !isfinite(receiver_position.x) || !isfinite(receiver_position.y) ||
        !isfinite(receiver_position.z) ||
        !isfinite(receiver_geometric_normal.x) ||
        !isfinite(receiver_geometric_normal.y) ||
        !isfinite(receiver_geometric_normal.z) ||
        !(vec3_length(receiver_geometric_normal) > 1.0e-12)) {
        return false;
    }
    normal = vec3_normalize(receiver_geometric_normal);
    if (!radiometry_intersect_receiver_plane(projector, pixel_x - 0.5,
                                              pixel_y - 0.5, receiver_position,
                                              normal, &p00) ||
        !radiometry_intersect_receiver_plane(projector, pixel_x + 0.5,
                                              pixel_y - 0.5, receiver_position,
                                              normal, &p10) ||
        !radiometry_intersect_receiver_plane(projector, pixel_x + 0.5,
                                              pixel_y + 0.5, receiver_position,
                                              normal, &p11) ||
        !radiometry_intersect_receiver_plane(projector, pixel_x - 0.5,
                                              pixel_y + 0.5, receiver_position,
                                              normal, &p01)) {
        return false;
    }
    area = 0.5 * vec3_length(vec3_cross(vec3_sub(p10, p00),
                                        vec3_sub(p11, p00))) +
           0.5 * vec3_length(vec3_cross(vec3_sub(p11, p00),
                                        vec3_sub(p01, p00)));
    if (!isfinite(area) || !(area > 0.0)) return false;
    *out_area_m2 = area;
    return true;
}
