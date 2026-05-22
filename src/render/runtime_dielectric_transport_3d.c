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
    RuntimeDielectricTransport3D transport = {0};
    Vec3 normal = vec3_normalize(surface_normal);
    Vec3 incident = vec3_normalize(incident_dir);
    double reflectivity = 0.0;
    double ior = 1.45;
    double eta_i = 1.0;
    double eta_t = 1.45;
    double cos_i = runtime_dielectric_transport_3d_clamp(vec3_dot(incident, normal), -1.0, 1.0);
    double eta = 1.0;
    double sin2_t = 0.0;
    double cos_t = 0.0;
    double dielectric_f0 = 0.04;
    double authored_f0 = 0.04;
    double f0 = 0.04;

    if (!payload || !out_transport) return false;

    reflectivity = runtime_dielectric_transport_3d_clamp(payload->bsdf.reflectivity, 0.0, 1.0);
    if (payload->opticalIor > 1.0 + 1e-6) {
        ior = payload->opticalIor;
    } else if (payload->bsdf.ior > 1.0 + 1e-6) {
        ior = payload->bsdf.ior;
    }

    transport.entering = (cos_i < 0.0);
    if (transport.entering) {
        cos_i = -cos_i;
    } else {
        normal = vec3_scale(normal, -1.0);
        eta_i = ior;
        eta_t = 1.0;
        cos_i = runtime_dielectric_transport_3d_clamp(vec3_dot(vec3_scale(incident, -1.0), normal),
                                                      0.0,
                                                      1.0);
    }

    eta = eta_i / eta_t;
    sin2_t = eta * eta * fmax(0.0, 1.0 - (cos_i * cos_i));
    transport.incidentDir = incident;
    transport.orientedNormal = normal;
    transport.reflectionDir = runtime_dielectric_transport_3d_reflect(incident, normal);

    if (ior > 1.0 + 1e-6) {
        double ior_ratio = (ior - 1.0) / (ior + 1.0);
        dielectric_f0 = runtime_dielectric_transport_3d_clamp(ior_ratio * ior_ratio, 0.04, 1.0);
    }
    authored_f0 = runtime_dielectric_transport_3d_clamp(0.04 + (reflectivity * 0.96), 0.04, 1.0);
    f0 = fmax(dielectric_f0, authored_f0);
    transport.fresnel = FresnelSchlick(cos_i, f0);

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
