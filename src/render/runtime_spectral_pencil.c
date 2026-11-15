#include "render/runtime_spectral_pencil.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_physical_glass_3d.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <float.h>
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

/* An ordinary rectangular emitter also occludes a forward path that returns
 * to it after refraction or reflection. The launch point itself is excluded. */
static double return_to_area_distance(const RuntimeSpectralSurfacePathConfig *a, Ray3D ray, double limit) {
    double cosine = vec3_dot(ray.direction,a->normal);
    if (fabs(cosine) < 1e-12) return INFINITY;
    double t = vec3_dot(vec3_sub(a->origin,ray.origin),a->normal)/cosine;
    if (!(t > 1e-7 && t < limit)) return INFINITY;
    Vec3 offset = vec3_sub(vec3_add(ray.origin,vec3_scale(ray.direction,t)),a->origin);
    return fabs(vec3_dot(offset,a->axis_u)) <= a->width*.5 &&
           fabs(vec3_dot(offset,a->axis_v)) <= a->height*.5 ? t : INFINITY;
}

/* Both paths start in the exterior medium. Smooth interfaces can sample
 * Fresnel reflection and transmission. The ballistic profile is retained for
 * existing requests. Rough interfaces use the native GGX random walk. */
static bool trace_closed_dielectric(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p,
                                 const RuntimeSpectralPencilLine *line, double index, Ray3D ray,
                                 bool camera, RuntimeSpectralPathReadout *out) {
    const RuntimeMaterialPayload3D material = {.hasGlassTransportOverride = true,
                                               .bsdf.roughness = p->glass_roughness};
    const double exterior = p->map.source_count ? p->exterior_ior : 1;
    bool inside = false;
    *out = (RuntimeSpectralPathReadout){.weight = camera ? 1 : line->watts};
    out->vacuum_nm = line->vacuum_nm;
    for (int step = 0; out->interfaces < 32; ++step) {
        HitInfo3D hit;
        out->direction = ray.direction;
        /* Secondary interfaces can be nanometers apart near a prism corner.
         * Bound self-intersection error by coordinate precision instead of
         * discarding every segment shorter than a fixed geometric distance. */
        double scale = fmax(1, fmax(fabs(ray.origin.x), fmax(fabs(ray.origin.y), fabs(ray.origin.z))));
        double min_distance = 64 * DBL_EPSILON * scale;
        bool surface = RuntimeRay3D_TraceSceneFirstHit(scene, &ray, min_distance,
                                                       RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE, &hit);
        double limit = surface ? hit.t : INFINITY;
        if (p->surface_path.enabled && step) {
            double emitter = return_to_area_distance(&p->surface_path, ray, limit);
            if (isfinite(emitter)) limit = emitter;
        }
        if (p->active_volume && !inside && p->surface_path.enabled && !camera) {
            if (!RuntimeSpectralVolume_SpecularSegment(p->active_volume, line, ray, limit, step, out))
                return false;
        }
        if (limit < (surface ? hit.t : INFINITY)) return !inside;
        if (p->active_volume && !inside && !p->surface_path.enabled) {
            bool restart;
            if (step >= 1024 ||
                !RuntimeSpectralVolume_Segment(p->active_volume, line, &ray, surface ? hit.t : INFINITY,
                                               camera, step, out, &restart))
                return false;
            if (out->volume_absorbed) return true;
            if (restart) continue;
        }
        if (!surface) return !inside;
        out->position = hit.position;
        if (inside) {
            out->glass_m += hit.t;
            double before = out->weight;
            out->weight *= exp(-line->absorption_per_m * hit.t);
            if (!camera && p->fresnel_camera_samples)
                out->glass_absorbed_w += before - out->weight;
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
            uint64_t id = (line->sample_id * UINT64_C(0x9e3779b97f4a7c15)) ^ ((uint64_t)step << 3);
            if (!RuntimePhysicalGlass3D_Sample(
                    &material, normal, ray.direction, from, to, RuntimeSpectral_Uniform64(id),
                    RuntimeSpectral_Uniform64(id + 1), RuntimeSpectral_Uniform64(id + 2), &sample)) {
                out->weight = 0;
                return true; /* native microsurface roulette, zero contribution */
            }
            out->weight *= sample.weight;
            if (!sample.reflected) {
                if (!camera) out->weight *= (to / from) * (to / from); /* radiance to photon flux */
                inside = !inside;
            }
            ray = RuntimeRay3D_Make(hit.position, sample.direction);
        } else if (event.totalInternalReflection) {
            ++out->total_internal_reflections;
            ray = RuntimeRay3D_Make(hit.position, event.reflectionDir);
        } else {
            /* The selected branch probability cancels its Fresnel factor.
             * Photon power is unchanged. Camera transmission still needs the
             * radiance-mode index factor across the interface. */
            uint64_t id = line->sample_id ^
                ((uint64_t)out->interfaces * UINT64_C(0xd1b54a32d192ed03)) ^
                UINT64_C(0x7ca04d663658e9cb);
            if (p->fresnel_camera_samples && RuntimeSpectral_Uniform64(id) < event.fresnel) {
                ++out->fresnel_reflections;
                ray = RuntimeRay3D_Make(hit.position, event.reflectionDir);
            } else {
                inside = !inside;
                if (!p->fresnel_camera_samples) out->weight *= 1 - event.fresnel;
                if (camera) out->weight *= (from / to) * (from / to);
                ray = RuntimeRay3D_Make(hit.position, event.refractionDir);
            }
        }
        /* At grazing angles a rounded hit point can meet its own face again
         * beyond min_distance. Move across the boundary in the outgoing
         * normal direction using the same coordinate-precision bound. */
        double side = vec3_dot(ray.direction, normal) < 0 ? -1 : 1;
        ray.origin = vec3_add(ray.origin, vec3_scale(normal, side * min_distance));
    }
    /* A depth-limited path contributes no flux to the receiver. */
    out->direction = ray.direction;
    out->depth_limited = true;
    return true;
}

bool RuntimeSpectralPencil_Trace(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p,
                                 const RuntimeSpectralPencilLine *line, double index, Ray3D ray,
                                 bool camera, RuntimeSpectralPathReadout *out) {
    /* Closed surfaces include their shared edges in every caller, including
     * the standalone monochromatic path oracle. The policy is thread-local. */
    bool previous = RuntimeRay3D_SetInclusiveTriangleEdges(true);
    bool ok = trace_closed_dielectric(scene, p, line, index, ray, camera, out);
    RuntimeRay3D_SetInclusiveTriangleEdges(previous);
    return ok;
}
