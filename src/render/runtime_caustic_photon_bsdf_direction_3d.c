#include "render/runtime_caustic_photon_bsdf_direction_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_principled_bsdf_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double photon_bsdf_direction_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool photon_bsdf_direction_finite_vec(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static Vec3 photon_bsdf_direction_orient_normal(Vec3 incident, Vec3 normal) {
    Vec3 oriented = vec3_normalize(normal);
    if (vec3_dot(incident, oriented) > 0.0) {
        oriented = vec3_scale(oriented, -1.0);
    }
    return oriented;
}

static Vec3 photon_bsdf_direction_reflect(Vec3 incident, Vec3 normal) {
    return vec3_normalize(
        vec3_sub(incident, vec3_scale(normal, 2.0 * vec3_dot(incident, normal))));
}

static void photon_bsdf_direction_basis(Vec3 normal,
                                        Vec3* out_tangent,
                                        Vec3* out_bitangent) {
    Vec3 axis = fabs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0)
                                      : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_normalize(vec3_cross(axis, normal));
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));
    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

bool RuntimeCausticPhotonBsdfDirection3D_Sample(
    RuntimeCausticPhotonBsdfLobe3D lobe,
    const RuntimeMaterialPayload3D* material,
    Vec3 incident_direction,
    Vec3 surface_normal,
    const RuntimeCausticPhotonBsdfDirectionSample3D* sample,
    RuntimeCausticPhotonBsdfDirection3D* out_direction) {
    return RuntimeCausticPhotonBsdfDirection3D_SampleInterface(lobe,
                                                               material,
                                                               incident_direction,
                                                               surface_normal,
                                                               sample,
                                                               0.0,
                                                               0.0,
                                                               out_direction);
}

bool RuntimeCausticPhotonBsdfDirection3D_SampleInterface(
    RuntimeCausticPhotonBsdfLobe3D lobe,
    const RuntimeMaterialPayload3D* material,
    Vec3 incident_direction,
    Vec3 surface_normal,
    const RuntimeCausticPhotonBsdfDirectionSample3D* sample,
    double eta_from,
    double eta_to,
    RuntimeCausticPhotonBsdfDirection3D* out_direction) {
    RuntimeCausticPhotonBsdfDirection3D result;
    RuntimePrincipledBSDF3D principled;
    Vec3 incident;
    Vec3 normal;
    Vec3 tangent;
    Vec3 bitangent;
    double u;
    double v;

    if (!out_direction) return false;
    memset(&result, 0, sizeof(result));
    result.attempted = true;
    result.lobe = lobe;
    *out_direction = result;
    if (!material || !material->valid || !sample ||
        !photon_bsdf_direction_finite_vec(incident_direction) ||
        !photon_bsdf_direction_finite_vec(surface_normal) ||
        !isfinite(sample->unitU) || !isfinite(sample->unitV) ||
        vec3_length(incident_direction) <= 1.0e-12 ||
        vec3_length(surface_normal) <= 1.0e-12) {
        return false;
    }

    incident = vec3_normalize(incident_direction);
    normal = photon_bsdf_direction_orient_normal(incident, surface_normal);
    u = photon_bsdf_direction_clamp01(sample->unitU);
    v = photon_bsdf_direction_clamp01(sample->unitV);
    photon_bsdf_direction_basis(normal, &tangent, &bitangent);

    if (lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE) {
        double phi = 2.0 * M_PI * u;
        double radius = sqrt(v);
        double z = sqrt(fmax(0.0, 1.0 - v));
        result.outgoingDirection = vec3_normalize(
            vec3_add(vec3_add(vec3_scale(tangent, radius * cos(phi)),
                              vec3_scale(bitangent, radius * sin(phi))),
                     vec3_scale(normal, z)));
        result.cosine = photon_bsdf_direction_clamp01(
            vec3_dot(normal, result.outgoingDirection));
        result.angularPdf = fmax(result.cosine / M_PI, 1.0e-12);
    } else if (lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR) {
        result.outgoingDirection = photon_bsdf_direction_reflect(incident, normal);
        result.cosine = photon_bsdf_direction_clamp01(
            vec3_dot(normal, result.outgoingDirection));
        result.angularPdf = 1.0;
    } else if (lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY) {
        Vec3 reflection = photon_bsdf_direction_reflect(incident, normal);
        Vec3 jitter;
        double phi = 2.0 * M_PI * u;
        double radius = sqrt(v);
        double blend = fmin(fmax(material->bsdf.roughness * 0.35, 0.0), 0.35);
        double cos_half;
        double dot_i_h;

        jitter = vec3_normalize(
            vec3_add(vec3_add(vec3_scale(tangent, radius * cos(phi)),
                              vec3_scale(bitangent, radius * sin(phi))),
                     reflection));
        result.outgoingDirection = vec3_normalize(
            vec3_add(vec3_scale(reflection, 1.0 - blend),
                     vec3_scale(jitter, blend)));
        if (vec3_dot(result.outgoingDirection, normal) <= 1.0e-9) {
            result.outgoingDirection = reflection;
        }
        result.cosine = photon_bsdf_direction_clamp01(
            vec3_dot(normal, result.outgoingDirection));
        principled = RuntimePrincipledBSDF3D_FromMaterialPayload(material);
        cos_half = sqrt(photon_bsdf_direction_clamp01(result.cosine));
        dot_i_h = photon_bsdf_direction_clamp01(
            fabs(vec3_dot(result.outgoingDirection, vec3_scale(incident, -1.0))));
        result.angularPdf = fmax(
            RuntimePrincipledBSDF3D_GGXHalfVectorPdf(&principled, cos_half, dot_i_h),
            1.0e-12);
    } else if (lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION) {
        bool explicit_interface = eta_from > 0.0 && eta_to > 0.0;
        bool resolved = explicit_interface
                            ? RuntimeDielectricTransport3D_ResolveInterface(
                                  material,
                                  surface_normal,
                                  incident,
                                  eta_from,
                                  eta_to,
                                  &result.dielectric)
                            : RuntimeDielectricTransport3D_Resolve(
                                  material,
                                  surface_normal,
                                  incident,
                                  &result.dielectric);
        if (!resolved) {
            *out_direction = result;
            return false;
        }
        result.totalInternalReflection = result.dielectric.totalInternalReflection;
        if (!result.dielectric.hasRefraction) {
            *out_direction = result;
            return false;
        }
        result.outgoingDirection = vec3_normalize(result.dielectric.refractionDir);
        result.cosine = photon_bsdf_direction_clamp01(
            fabs(vec3_dot(result.dielectric.orientedNormal,
                          result.outgoingDirection)));
        result.angularPdf = 1.0;
    } else {
        return false;
    }

    result.valid = photon_bsdf_direction_finite_vec(result.outgoingDirection) &&
                   vec3_length(result.outgoingDirection) > 1.0e-9 &&
                   isfinite(result.angularPdf) && result.angularPdf > 0.0;
    *out_direction = result;
    return result.valid;
}
