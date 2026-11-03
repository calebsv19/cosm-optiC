#include "render/runtime_spectral_pencil.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_physical_glass_3d.h"
#include <math.h>
#include <string.h>

double RuntimeSpectralCurve_Value(const RuntimeSpectralCurve *curve, double nm) {
    for (int j = 1; j < curve->count; ++j) {
        const double *lo = curve->points[j - 1], *hi = curve->points[j];
        if (nm <= hi[0]) {
            double t = (nm - lo[0]) / (hi[0] - lo[0]);
            return (1 - t) * lo[1] + t * hi[1];
        }
    }
    return 1; /* empty curve denotes a neutral response */
}
double RuntimeSpectralPencil_Index(const RuntimeSpectralPencil *p, double vacuum_nm) {
    if (!p->use_n_bk7) return p->constant_ior;
    /* SCHOTT N-BK7, relative to standard air. Dispersion constants use um^2. */
    double um = RuntimeSpectral_StandardAirNm(vacuum_nm) * 0.001;
    double l2 = um * um;
    return sqrt(1 + 1.03961212 * l2 / (l2 - 0.00600069867) + 0.231792344 * l2 / (l2 - 0.0200179144) +
                1.01046945 * l2 / (l2 - 103.560653));
}

/* Both paths start in the exterior medium. Smooth paths track ballistic
 * transmission and TIR. Rough interfaces sample the native GGX random walk. */
bool RuntimeSpectralPencil_Trace(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p,
                                 const RuntimeSpectralPencilLine *line, double index, Ray3D ray,
                                 bool camera, RuntimeSpectralPathReadout *out) {
    const RuntimeMaterialPayload3D material = {.hasGlassTransportOverride = true,
                                               .bsdf.roughness = p->glass_roughness};
    const double exterior = p->map.source_count ? p->exterior_ior : 1;
    bool inside = false;
    *out = (RuntimeSpectralPathReadout){.weight = camera ? 1 : line->watts};
    out->vacuum_nm = line->vacuum_nm;
    for (int depth = 0; depth < 32; ++depth) {
        HitInfo3D hit;
        out->direction = ray.direction;
        if (!RuntimeRay3D_TraceSceneFirstHit(scene, &ray, 1e-8, RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE,
                                             &hit))
            return !inside;
        out->position = hit.position;
        if (inside) {
            out->glass_m += hit.t;
            out->weight *= exp(-line->absorption_per_m * hit.t);
        }
        if (hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE &&
            (p->map.source_count || !strcmp(hit.source.objectId, p->receiver_id))) {
            if (camera && p->use_scene_texture && !strcmp(hit.source.objectId, p->receiver_id)) {
                RuntimeMaterialPayload3D payload;
                if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload)) return false;
                out->receiver_rgb = vec3(payload.baseColorR, payload.baseColorG, payload.baseColorB);
            } else
                out->receiver_rgb = vec3(1, 1, 1);
            out->primitive = hit.primitiveIndex;
            out->receiver = true;
            out->receiver_front =
                vec3_dot(ray.direction, scene->primitives[hit.primitiveIndex].shape.plane.normal) < 0;
            return !inside;
        }
        Vec3 normal = HitInfo3D_BoundaryNormal(&hit);
        double from = inside ? index : exterior, to = inside ? exterior : index;
        RuntimeDielectricTransport3D event;
        if (strcmp(hit.source.objectId, p->glass_id) ||
            !RuntimeDielectricTransport3D_ResolveInterface(&material, normal, ray.direction, from, to,
                                                           &event) ||
            event.entering == inside)
            return false;
        ++out->interfaces;
        if (p->glass_roughness > 0 && from != to) {
            RuntimePhysicalGlass3DSample sample;
            uint64_t id = (line->sample_id * UINT64_C(0x9e3779b97f4a7c15)) ^ ((uint64_t)depth << 3);
            if (!RuntimePhysicalGlass3D_Sample(
                    &material, normal, ray.direction, from, to, RuntimeSpectral_Uniform64(id),
                    RuntimeSpectral_Uniform64(id + 1), RuntimeSpectral_Uniform64(id + 2), &sample))
                return true; /* native microsurface roulette, zero contribution */
            out->weight *= sample.weight;
            if (!sample.reflected) {
                if (!camera) out->weight *= (to / from) * (to / from); /* radiance to photon flux */
                inside = !inside;
            }
            ray = RuntimeRay3D_Make(hit.position, sample.direction);
        } else if (event.totalInternalReflection) {
            ray = RuntimeRay3D_Make(hit.position, event.reflectionDir);
        } else {
            inside = !inside;
            out->weight *= 1 - event.fresnel;
            if (camera) out->weight *= (from / to) * (from / to);
            ray = RuntimeRay3D_Make(hit.position, event.refractionDir);
        }
    }
    /* A depth-limited path contributes no flux to the receiver. */
    out->direction = ray.direction;
    out->depth_limited = true;
    return true;
}
