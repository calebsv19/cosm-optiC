#include "render/runtime_caustic_photon_receiver_bsdf_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double receiver_bsdf_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

bool RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
    const RuntimeMaterialPayload3D* material,
    const RuntimeCausticPhotonMapQueryResult3D* query,
    Vec3* out_radiance,
    RuntimeCausticPhotonReceiverBsdfReadback3D* out_readback) {
    RuntimeCausticPhotonReceiverBsdfReadback3D readback;
    Vec3 response;
    double diffuse_weight;
    const double inv_pi = 1.0 / M_PI;

    memset(&readback, 0, sizeof(readback));
    readback.attempted = true;
    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (out_readback) *out_readback = readback;
    if (!material || !material->valid || !query || !query->hit || !out_radiance) {
        return false;
    }
    diffuse_weight = receiver_bsdf_clamp01(material->bsdf.diffuseWeight);
    readback.baseColor = vec3(receiver_bsdf_clamp01(material->baseColorR),
                              receiver_bsdf_clamp01(material->baseColorG),
                              receiver_bsdf_clamp01(material->baseColorB));
    readback.diffuseWeight = diffuse_weight;
    readback.roughness = receiver_bsdf_clamp01(material->bsdf.roughness);
    readback.meanIncidentCosine = receiver_bsdf_clamp01(query->meanIncidentCosine);
    readback.lambertianNormalization = inv_pi;
    readback.inputPhysicalFlux = query->physicalFlux;
    response = vec3_scale(readback.baseColor, diffuse_weight * inv_pi);
    readback.response = response;
    *out_radiance = vec3(query->physicalFlux.x * response.x,
                         query->physicalFlux.y * response.y,
                         query->physicalFlux.z * response.z);
    readback.outputRadiance = *out_radiance;
    readback.applied = out_radiance->x > 0.0 || out_radiance->y > 0.0 ||
                       out_radiance->z > 0.0;
    if (out_readback) *out_readback = readback;
    return readback.applied;
}
