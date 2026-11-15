#include "render/runtime_spectral_diffuse.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The segment test excludes the destination and treats all planes as opaque. */
static bool visible(const RuntimeScene3D *scene, Vec3 from, Vec3 dir, double distance) {
    /* Test rectangles analytically, including blockers seen from either side. */
    bool mesh = false;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (scene->primitives[i].kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            mesh = true;
            continue;
        }
        const RuntimePlane3D *plane = &scene->primitives[i].shape.plane;
        double nd = vec3_dot(dir, plane->normal);
        if (fabs(nd) < 1e-12) continue;
        Vec3 offset = vec3_sub(from, plane->origin);
        double t = -vec3_dot(offset, plane->normal) / nd;
        if (t <= 1e-7 || t >= distance - 1e-7) continue;
        Vec3 at = vec3_add(offset, vec3_scale(dir, t));
        if (fabs(vec3_dot(at, plane->axisU)) <= plane->width * .5 &&
            fabs(vec3_dot(at, plane->axisV)) <= plane->height * .5)
            return false;
    }
    if (mesh) {
        HitInfo3D hit;
        Ray3D ray = RuntimeRay3D_Make(from, dir);
        double limit = isfinite(distance) ? distance - 1e-7 : RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE;
        if (RuntimeRay3D_TraceSceneFirstHit(scene, &ray, 1e-7, limit, &hit) &&
            hit.source.kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            return false;
    }
    return true;
}

static double point_irradiance(const RuntimeScene3D *scene, Vec3 pos, Vec3 normal, Vec3 source) {
    Vec3 offset = vec3_sub(source, pos);
    double r2 = vec3_dot(offset, offset);
    if (r2 <= 1e-12) return 0;
    double r = sqrt(r2);
    Vec3 direction = vec3_scale(offset, 1 / r);
    double cosine = vec3_dot(normal, direction);
    if (cosine <= 0 || !visible(scene, pos, direction, r)) return 0;
    return cosine / (4 * M_PI * r2);
}

/* Equal solid-angle cells in azimuth and cos(theta). Their weights sum
 * to the cap solid angle, with no distance falloff for a source at infinity. */
static void sky_directions(const RuntimeSpectralDiffuseConfig *config, Vec3 *directions,
                           double *weight) {
    Vec3 axis = config->sky_axis;
    Vec3 guide = fabs(axis.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 u = vec3_normalize(vec3_cross(guide, axis));
    Vec3 v = vec3_cross(axis, u);
    int grid = config->sky_grid;
    double extent = 1 - cos(config->sky_half_angle_deg * M_PI / 180);
    *weight = 2 * M_PI * extent / (grid * grid);
    for (int j = 0; j < grid; ++j) {
        double mu = 1 - extent * (j + .5) / grid;
        double radial = sqrt(1 - mu * mu);
        for (int i = 0; i < grid; ++i) {
            double phi = 2 * M_PI * (i + .5) / grid;
            directions[j * grid + i] = vec3_add(vec3_scale(axis, mu),
                vec3_add(vec3_scale(u, radial * cos(phi)), vec3_scale(v, radial * sin(phi))));
        }
    }
}

static double irradiance(const RuntimeScene3D *scene, Vec3 pos, Vec3 normal, Vec3 point,
                         const Vec3 *sky, int sky_count, double weight,
                         const RuntimeSpectralVolume *volume, double nm) {
    if (!sky) return point_irradiance(scene, pos, normal, point);
    double sum = 0;
    for (int i = 0; i < sky_count; ++i) {
        double cosine = vec3_dot(normal, sky[i]);
        if (cosine > 0 && visible(scene, pos, sky[i], INFINITY))
            sum += cosine * RuntimeSpectralVolume_Transmission(volume,
                RuntimeRay3D_Make(pos, sky[i]), INFINITY, nm);
    }
    return sum * weight;
}

static Vec3 panel_node(const RuntimePlane3D *panel, int grid, int i, int j) {
    return vec3_add(panel->origin,
        vec3_add(vec3_scale(panel->axisU, panel->width * ((i + .5) / grid - .5)),
                 vec3_scale(panel->axisV, panel->height * ((j + .5) / grid - .5))));
}

static double bounce(const RuntimeScene3D *scene, const RuntimePlane3D *panel,
                     const double *incident, Vec3 pos, Vec3 normal, int grid,
                     RuntimeSpectralProbeReadout *probe, const RuntimeSpectralVolume *volume, double nm) {
    double sum = 0;
    double area = panel->width * panel->height / ((double)grid * grid);
    for (int j = 0; j < grid; ++j) {
        for (int i = 0; i < grid; ++i) {
            Vec3 q = panel_node(panel, grid, i, j);
            Vec3 delta = vec3_sub(q, pos);
            double r2 = vec3_dot(delta, delta);
            if (r2 <= 1e-12) continue;
            double r = sqrt(r2);
            Vec3 direction = vec3_scale(delta, 1 / r);
            double receiver_cos = vec3_dot(normal, direction);
            double panel_cos = -vec3_dot(panel->normal, direction);
            if (receiver_cos <= 0 || panel_cos <= 0) continue;
            double incoming = incident[j * grid + i];
            if (!incoming || !visible(scene, pos, direction, r)) {
                if (probe) ++probe->panel_occluded;
                continue;
            }
            if (probe) ++probe->panel_accepted;
            double attenuation = RuntimeSpectralVolume_Transmission(volume,
                RuntimeRay3D_Make(pos, direction), r, nm);
            sum += incoming * receiver_cos * panel_cos * area / (M_PI * M_PI * r2) * attenuation;
        }
    }
    return sum;
}

bool RuntimeSpectralDiffuse_Run(const RuntimeSpectralPencil *p, const RuntimeNative3DPreparedFrame *frame,
                                uint8_t *pixels, RuntimeSpectralReadout *out, char *diag, size_t size) {
    const RuntimeScene3D *scene = frame->traceScene ? frame->traceScene : &frame->scene;
    int panel = -1, receiver = -1;
    if (scene->volume.enabled || scene->curveInstanceCount || frame->physicalAtmosphere ||
        scene->primitiveCount > 8) goto unsupported;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        const RuntimePrimitive3D *prim = &scene->primitives[i];
        const RuntimePlane3D *plane = &prim->shape.plane;
        if (prim->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE ||
            !(plane->width >= 1e-4 && plane->width <= 100 &&
              plane->height >= 1e-4 && plane->height <= 100) ||
            (!p->diffuse.sky_grid &&
             fabs(vec3_dot(vec3_sub(p->origin_m, plane->origin), plane->normal)) < 1e-6))
            goto unsupported;
        if (!strcmp(prim->source.objectId, p->diffuse.panel_id)) {
            if (panel >= 0) goto unsupported;
            panel = i;
        } else if (!strcmp(prim->source.objectId, p->receiver_id)) {
            if (receiver >= 0) goto unsupported;
            receiver = i;
        }
    }
    if (panel < 0 || receiver < 0) goto unsupported;
    if (!pixels) return true;

    int sky_count = p->diffuse.sky_grid * p->diffuse.sky_grid;
    Vec3 *sky = NULL;
    double sky_weight = 0;
    if (sky_count) {
        sky = malloc((size_t)sky_count * sizeof(*sky));
        if (!sky) {
            snprintf(diag, size, "cannot allocate spectral sky directions");
            return false;
        }
        sky_directions(&p->diffuse, sky, &sky_weight);
    }
    const RuntimePlane3D *panel_plane = &scene->primitives[panel].shape.plane;
    double panel_incident[64 * 64];
    int grid = p->diffuse.grid;
    if (p->diffuse.indirect) {
        for (int j = 0; j < grid; ++j)
            for (int i = 0; i < grid; ++i)
                panel_incident[j * grid + i] = irradiance(scene, panel_node(panel_plane, grid, i, j),
                    panel_plane->normal, p->origin_m, sky, sky_count, sky_weight, NULL, 0);
    }

    Vec3 observer[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    double panel_rho[RUNTIME_SPECTRAL_PENCIL_MAX_LINES], receiver_rho[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    for (int b = 0; b < p->count; ++b) {
        double nm = p->lines[b].vacuum_nm;
        observer[b] = RuntimeSpectral_CIE1931(nm);
        panel_rho[b] = RuntimeSpectralCurve_Value(&p->diffuse.panel_reflectance, nm);
        receiver_rho[b] = RuntimeSpectralCurve_Value(&p->reflectance, nm);
    }
    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) {
            RuntimeSpectralProbeReadout *probe =
                p->probe.enabled && x == p->probe.x && y == p->probe.y ? &out->probe : NULL;
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, x, y);
            HitInfo3D hit;
            bool found = RuntimeRay3D_TraceSceneFirstHit(scene, &ray, frame->projector.nearPlane,
                                                        RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE, &hit);
            if (probe) {
                *probe = (RuntimeSpectralProbeReadout){.valid=true, .hit=found,
                    .primitive=found ? hit.primitiveIndex : -1};
                if (found) probe->position = hit.position;
            }
            Vec3 xyz = vec3(0, 0, 0);
            if (found && (hit.primitiveIndex == receiver || hit.primitiveIndex == panel)) {
                const RuntimePlane3D *plane = &scene->primitives[hit.primitiveIndex].shape.plane;
                if (vec3_dot(plane->normal, ray.direction) < 0) {
                    double d = irradiance(scene, hit.position, plane->normal, p->origin_m,
                                          sky, sky_count, sky_weight, NULL, 0) / M_PI;
                    double s = hit.primitiveIndex == receiver && p->diffuse.indirect
                        ? bounce(scene, panel_plane, panel_incident, hit.position, plane->normal, grid, probe, NULL, 0) : 0;
                    if (probe) { probe->direct_geometry = d; probe->indirect_geometry = s; }
                    for (int b = 0; b < p->count; ++b) {
                        double rho = hit.primitiveIndex == receiver ? receiver_rho[b] : panel_rho[b];
                        double source = sky_count ? p->diffuse.sky_radiance[b] : p->lines[b].watts;
                        double direct_radiance = source * rho * d;
                        double indirect_radiance = source * rho * panel_rho[b] * s;
                        if (probe) {
                            probe->bands[b].direct = direct_radiance;
                            probe->bands[b].indirect = indirect_radiance;
                            probe->bands[b].camera_throughput = 1;
                            probe->bands[b].samples = 1;
                        }
                        xyz = vec3_add(xyz, vec3_scale(observer[b], direct_radiance + indirect_radiance));
                    }
                }
            }
            if (x == frame->width / 2 && y == frame->height / 2) out->center_pixel_xyz = xyz;
            if (probe) { probe->xyz = xyz; probe->signed_rgb = RuntimeSpectral_LinearRGB(xyz); }
            RuntimeSpectral_DisplayPixelWithTransform(pixels + ((size_t)y * frame->width + x) * 4,
                RuntimeSpectral_LinearRGB(vec3_scale(xyz, p->exposure)), p->soft_display);
        }
    }
    free(sky);
    return true;
unsupported:
    snprintf(diag, size, "diffuse_bounce_v1 needs distinct finite named panel/receiver planes, at most "
             "eight planes, no volume, and any point source off each plane");
    return false;
}

struct RuntimeSpectralSkyLighting {
    const RuntimeSpectralPencil *profile;
    const RuntimeScene3D *scene;
    int panel, receiver, directions;
    double weight;
    Vec3 sky[64 * 64];
    double incident[];
};

RuntimeSpectralSkyLighting *RuntimeSpectralSkyLighting_Create(const RuntimeSpectralPencil *p,
                                                               const RuntimeScene3D *scene) {
    int panel = -1, receiver = -1, grid = p->diffuse.grid;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        const RuntimePrimitive3D *prim = &scene->primitives[i];
        if (prim->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) continue;
        if (!strcmp(prim->source.objectId, p->diffuse.panel_id)) panel = i;
        if (!strcmp(prim->source.objectId, p->receiver_id)) receiver = i;
    }
    if (panel < 0 || receiver < 0) return NULL;
    RuntimeSpectralSkyLighting *s = calloc(1, sizeof(*s) + (size_t)p->count * grid * grid * sizeof(double));
    if (!s) return NULL;
    s->profile = p;
    s->scene = scene;
    s->panel = panel;
    s->receiver = receiver;
    s->directions = p->diffuse.sky_grid * p->diffuse.sky_grid;
    sky_directions(&p->diffuse, s->sky, &s->weight);
    if (p->diffuse.indirect) {
        const RuntimePlane3D *plane = &scene->primitives[panel].shape.plane;
        for (int b = 0; b < p->count; ++b) {
            if (p->diffuse.sky_radiance[b] == 0) continue;
            for (int j = 0; j < grid; ++j)
                for (int i = 0; i < grid; ++i)
                    s->incident[(b * grid + j) * grid + i] = irradiance(scene,
                        panel_node(plane, grid, i, j), plane->normal, p->origin_m,
                        s->sky, s->directions, s->weight, p->active_volume, p->lines[b].vacuum_nm);
        }
    }
    return s;
}

void RuntimeSpectralSkyLighting_Free(RuntimeSpectralSkyLighting *sky) { free(sky); }

void RuntimeSpectralSkyLighting_Evaluate(const RuntimeSpectralSkyLighting *s,
    const RuntimeSpectralPathReadout *hit, double nm, double *direct, double *indirect) {
    if (!s || !hit->receiver || !hit->receiver_front ||
        (hit->primitive != s->panel && hit->primitive != s->receiver)) return;
    const RuntimeSpectralPencil *p = s->profile;
    int b = 0;
    while (b < p->count && p->lines[b].vacuum_nm != nm) ++b;
    if (b == p->count || p->diffuse.sky_radiance[b] == 0) return;
    const RuntimePlane3D *plane = &s->scene->primitives[hit->primitive].shape.plane;
    double source = p->diffuse.sky_radiance[b];
    *direct = source * irradiance(s->scene, hit->position, plane->normal, p->origin_m,
        s->sky, s->directions, s->weight, p->active_volume, nm) / M_PI;
    if (p->diffuse.indirect && hit->primitive == s->receiver) {
        int grid = p->diffuse.grid;
        double rho = RuntimeSpectralCurve_Value(&p->diffuse.panel_reflectance, nm);
        *indirect = source * rho * bounce(s->scene, &s->scene->primitives[s->panel].shape.plane,
            s->incident + b * grid * grid, hit->position, plane->normal, grid, NULL,
            p->active_volume, nm);
    }
}
