#include "render/runtime_spectral_pencil.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_spectral_observer.h"
#include <math.h>
#include <string.h>

double RuntimeSpectralPencil_Index(const RuntimeSpectralPencil *p, double vacuum_nm) {
    if (!p->use_n_bk7) return p->constant_ior;
    /* SCHOTT N-BK7, relative to standard air. Dispersion constants use um^2. */
    double um = RuntimeSpectral_StandardAirNm(vacuum_nm) * 0.001;
    double l2 = um * um;
    return sqrt(1 + 1.03961212 * l2 / (l2 - 0.00600069867) + 0.231792344 * l2 / (l2 - 0.0200179144) +
                1.01046945 * l2 / (l2 - 103.560653));
}

/* The source and camera start in air. Follow specular transmission and TIR.
 * Fresnel-reflected branches at partial interfaces leave this ballistic probe. */
bool RuntimeSpectralPencil_Trace(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p, const RuntimeSpectralPencilLine *line,
                                 double index, Ray3D ray, bool camera, RuntimeSpectralPathReadout *out) {
    const RuntimeMaterialPayload3D material = {.hasGlassTransportOverride = true};
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
            out->primitive = hit.primitiveIndex;
            out->receiver = true;
            out->receiver_front =
                vec3_dot(ray.direction, scene->primitives[hit.primitiveIndex].shape.plane.normal) < 0;
            return !inside;
        }
        Vec3 normal = HitInfo3D_BoundaryNormal(&hit);
        double from = inside ? index : 1, to = inside ? 1 : index;
        RuntimeDielectricTransport3D event;
        if (strcmp(hit.source.objectId, p->glass_id) ||
            !RuntimeDielectricTransport3D_ResolveInterface(&material, normal, ray.direction, from, to,
                                                           &event) ||
            event.entering == inside)
            return false;
        ++out->interfaces;
        if (event.totalInternalReflection) {
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
