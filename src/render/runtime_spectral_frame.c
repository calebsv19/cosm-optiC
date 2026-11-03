#include "render/runtime_direct_light_3d.h"
#include "render/runtime_spectral_film.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void RuntimeSpectralFrame_Free(RuntimeSpectralFrame *s) {
    if (!s)
        return;
    RuntimeSpectralSurfaceMap_Free(s->map);
    RuntimeSpectralVolume_Free(s->profile.active_volume);
    free(s);
}

RuntimeSpectralFrame *RuntimeSpectralFrame_Create(const RuntimeSpectralPencil *profile,
                                                  const RuntimeScene3D *scene, char *diag, size_t size) {
    if (!RuntimeSpectralFilm_Supported(scene, profile)) {
        snprintf(diag, size,
                 "V3 photon deposition requires a closed dielectric, named receiver planes and spectral "
                 "sources.");
        return NULL;
    }
    RuntimeSpectralFrame *s = calloc(1, sizeof(*s));
    if (!s) {
        snprintf(diag, size, "Cannot allocate spectral frame.");
        return NULL;
    }
    s->profile = *profile;
    if (profile->volume.kind) {
        s->profile.active_volume = RuntimeSpectralVolume_Load(&profile->volume, diag, size);
        if (!s->profile.active_volume) {
            free(s);
            return NULL;
        }
        RuntimeSpectralVolume_Identity(s->profile.active_volume, &s->stats);
    }
    bool previous = RuntimeRay3D_SetInclusiveTriangleEdges(true);
    s->map = RuntimeSpectralSurfaceMap_Build(&s->profile, scene, &s->stats);
    RuntimeRay3D_SetInclusiveTriangleEdges(previous);
    if (!s->map) {
        snprintf(diag, size, "V3 spectral photon map construction failed.");
        RuntimeSpectralFrame_Free(s);
        return NULL;
    }
    return s;
}

/* For existing RGB lights and matte surfaces, reconstruct incident and reflected
 * spectra separately. Normalize the equal-energy reference to unit luminance. */
static Vec3 direct_response(Vec3 incident, Vec3 albedo) {
    double gain = fmax(incident.x, fmax(incident.y, incident.z));
    if (!(gain > 0))
        return vec3(0, 0, 0);
    Vec3 source = vec3_scale(incident, 1 / gain), xyz = vec3(0, 0, 0);
    double normalization = 0;
    for (int nm = 390; nm < 780; nm += 20) {
        Vec3 observer = RuntimeSpectral_CIE1931(nm);
        normalization += observer.y;
        xyz = vec3_add(xyz, vec3_scale(observer, RuntimeSpectral_RGBBasis(source, nm) *
                                                     RuntimeSpectral_RGBBasis(albedo, nm)));
    }
    return RuntimeSpectral_LinearRGB(vec3_scale(xyz, gain / normalization));
}

bool RuntimeSpectralFrame_Shade(const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                float *radiance, int stride, RuntimeNative3DRenderStats *stats) {
    if (stats)
        *stats = (RuntimeNative3DRenderStats){0};
    const RuntimeScene3D *scene = frame->traceScene ? frame->traceScene : &frame->scene;
    if (scene->volume.enabled)
        return false;
    if (frame->spectralFrame) {
        const RuntimeSpectralFrame *s = frame->spectralFrame;
        RuntimeSpectralReadout out = {.map = s->stats};
        char diag[160];
        bool previous = RuntimeRay3D_SetInclusiveTriangleEdges(true);
        bool ok = RuntimeSpectralFilm_Region(&s->profile, s->map, frame, x0, y0, x1, y1, NULL, radiance,
                                             stride, &out, diag, sizeof(diag));
        RuntimeRay3D_SetInclusiveTriangleEdges(previous);
        return ok;
    }
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            Vec3 rgb = vec3(0, 0, 0);
            HitInfo3D hit;
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, x, y);
            if (RuntimeRay3D_TraceSceneFirstHit(scene, &ray, frame->projector.nearPlane,
                                                RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE, &hit)) {
                RuntimeMaterialPayload3D material = {0};
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &material);
                Vec3 albedo = material.valid
                                  ? vec3(material.baseColorR, material.baseColorG, material.baseColorB)
                                  : vec3(1, 1, 1);
                /* Ask the native source/visibility sampler for irradiance on white. */
                material.valid = true;
                material.baseColorR = material.baseColorG = material.baseColorB = 1;
                RuntimeDirectLight3DResult light = {0};
                RuntimeDirectLight3D_ShadeHitWithPayload(scene, &hit, &material, &frame->sampling, &light);
                rgb = direct_response(vec3(light.radianceR + material.emissive,
                                           light.radianceG + material.emissive,
                                           light.radianceB + material.emissive),
                                      albedo);
                if (stats) {
                    ++stats->hitPixelCount;
                    double peak = fmax(rgb.x, fmax(rgb.y, rgb.z));
                    if (peak > 0)
                        ++stats->visiblePixelCount;
                    stats->maxRadiance = fmax(stats->maxRadiance, peak);
                }
            }
            size_t i = ((size_t)(y - y0) * stride + x - x0) * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            radiance[i] = fmax(0, rgb.x);
            radiance[i + 1] = fmax(0, rgb.y);
            radiance[i + 2] = fmax(0, rgb.z);
            radiance[i + 3] = 0;
        }
    return true;
}
