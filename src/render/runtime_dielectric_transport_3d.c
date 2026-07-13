#include "render/runtime_dielectric_transport_3d.h"

#include <math.h>

#include "render/material_bsdf.h"

static double runtime_dielectric_transport_3d_clamp(double value,
                                                    double min_value,
                                                    double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 runtime_dielectric_transport_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    return vec3_normalize(
        vec3_sub(incident_dir, vec3_scale(normal, 2.0 * vec3_dot(incident_dir, normal))));
}

bool RuntimeDielectricTransport3D_Resolve(const RuntimeMaterialPayload3D* payload,
                                          Vec3 surface_normal,
                                          Vec3 incident_dir,
                                          RuntimeDielectricTransport3D* out_transport) {
    double ior = 1.45;
    double eta_i = 1.0;
    double eta_t = 1.45;

    if (!payload || !out_transport) return false;
    if (payload->opticalIor >= 1.0) {
        ior = payload->opticalIor;
    } else if (payload->bsdf.ior > 1.0 + 1e-6) {
        ior = payload->bsdf.ior;
    }
    if (vec3_dot(vec3_normalize(incident_dir),
                 vec3_normalize(surface_normal)) >= 0.0) {
        eta_i = ior;
        eta_t = 1.0;
    } else {
        eta_t = ior;
    }
    return RuntimeDielectricTransport3D_ResolveInterface(payload,
                                                         surface_normal,
                                                         incident_dir,
                                                         eta_i,
                                                         eta_t,
                                                         out_transport);
}

bool RuntimeDielectricTransport3D_ResolveInterface(
    const RuntimeMaterialPayload3D* payload,
    Vec3 surface_normal,
    Vec3 incident_dir,
    double eta_from,
    double eta_to,
    RuntimeDielectricTransport3D* out_transport) {
    RuntimeDielectricTransport3D transport = {0};
    Vec3 normal = vec3_normalize(surface_normal);
    Vec3 incident = vec3_normalize(incident_dir);
    double reflectivity = 0.0;
    double cos_i = runtime_dielectric_transport_3d_clamp(
        vec3_dot(incident, normal), -1.0, 1.0);
    double eta = 1.0;
    double sin2_t = 0.0;
    double cos_t = 0.0;
    double dielectric_f0 = 0.0;
    double authored_f0 = 0.0;
    double f0 = 0.0;

    if (!payload || !out_transport || !isfinite(eta_from) ||
        !isfinite(eta_to) || eta_from <= 0.0 || eta_to <= 0.0 ||
        vec3_length(surface_normal) <= 1.0e-12 ||
        vec3_length(incident_dir) <= 1.0e-12) {
        return false;
    }

    reflectivity = runtime_dielectric_transport_3d_clamp(payload->bsdf.reflectivity, 0.0, 1.0);
    transport.entering = (cos_i < 0.0);
    if (transport.entering) {
        cos_i = -cos_i;
    } else {
        normal = vec3_scale(normal, -1.0);
        cos_i = runtime_dielectric_transport_3d_clamp(vec3_dot(vec3_scale(incident, -1.0), normal),
                                                      0.0,
                                                      1.0);
    }

    transport.etaFrom = eta_from;
    transport.etaTo = eta_to;
    eta = eta_from / eta_to;
    sin2_t = eta * eta * fmax(0.0, 1.0 - (cos_i * cos_i));
    transport.incidentDir = incident;
    transport.orientedNormal = normal;
    transport.reflectionDir = runtime_dielectric_transport_3d_reflect(incident, normal);

    if (fabs(eta_to - eta_from) > 1.0e-6) {
        double ior_ratio = (eta_to - eta_from) / (eta_to + eta_from);
        dielectric_f0 = runtime_dielectric_transport_3d_clamp(ior_ratio * ior_ratio, 0.0, 1.0);
    }
    authored_f0 = runtime_dielectric_transport_3d_clamp(reflectivity, 0.0, 1.0);
    f0 = fmax(dielectric_f0, authored_f0);
    transport.fresnel =
        (fabs(eta_to - eta_from) <= 1.0e-6 && reflectivity <= 1e-9)
            ? 0.0
            : FresnelSchlick(cos_i, f0);

    if (payload->thinWalled) {
        transport.totalInternalReflection = false;
        transport.hasRefraction = true;
        transport.refractionDir = incident;
        *out_transport = transport;
        return true;
    }

    if (sin2_t >= 1.0) {
        transport.totalInternalReflection = true;
        transport.hasRefraction = false;
        transport.refractionDir = transport.reflectionDir;
        *out_transport = transport;
        return true;
    }

    cos_t = sqrt(fmax(0.0, 1.0 - sin2_t));
    transport.refractionDir =
        vec3_normalize(vec3_add(vec3_scale(incident, eta),
                                vec3_scale(normal, (eta * cos_i) - cos_t)));
    transport.hasRefraction = true;
    *out_transport = transport;
    return true;
}
