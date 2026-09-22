/* Surface mapping's prepared sampling adapter; shares its validated transforms. */
#include "core_authored_surface_sampling.h"
#include "render/runtime_surface_sampling.h"
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#define SAMPLING_MAX_BYTES (128u * 1024u * 1024u)
#define SAMPLING_RESOLUTION 256u
#include "runtime_surface_resource_cache.inc"
typedef struct SurfaceSampling {
    bool declared, valid, composition;
    double period[2], normal_strength, height_m;
    RuntimeMaterialPayload3D base;
    Vec3 coordinate_gradient[3];
    double coordinate_offset[3];
    CoreAuthoredSurfacePyramid material, images[4];
    SamplingResource *material_resource, *image_resources[4];
} SurfaceSampling;
static SurfaceSampling sampling[MAX_OBJECTS];
static size_t sampling_bytes;
static unsigned long long sampling_builds;
static bool sampling_preparing;
static RuntimeSurfacePreparationFailure surface_failure;
void RuntimeSurfacePreparationFailNextForTests(RuntimeSurfacePreparationFailure failure) {
    surface_failure = failure;
}
static bool surface_preparation_fail(RuntimeSurfacePreparationFailure failure) {
    if (surface_failure != failure)
        return false;
    surface_failure = RUNTIME_SURFACE_FAIL_NONE;
    return true;
}
static const char *sampling_channels[] = {"base_color", "roughness", "normal", "height"};
static void sampling_free(SurfaceSampling *s) {
    sampling_resource_release(s->material_resource);
    for (int c = 0; c < 4; ++c)
        sampling_resource_release(s->image_resources[c]);
    memset(s, 0, sizeof(*s));
}
static json_object *sampling_declaration(json_object *o) {
    return field(field(field(o, "extensions"), "ray_tracing"), "surface_sampling");
}
static bool sampling_image(json_object *entry, int channel, CoreAuthoredSurfacePyramid *out,
                           SamplingResource **resource, bool composition) {
    if (!json_object_is_type(entry, json_type_object))
        return false;
    const char *declared = json_object_get_string(field(entry, "path")),
               *expected = json_object_get_string(field(entry, "sha256"));
    char path[PATH_MAX];
    if (!json_object_is_type(field(entry, "path"), json_type_string) ||
        !sampling_resource_path(declared, path) || !ray_tracing_sha256_is_valid_hex(expected))
        return false;
    if (sampling_preparing && surface_preparation_fail(RUNTIME_SURFACE_FAIL_IMAGE_READ))
        return false;
    struct stat st;
    char digest[65];
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size > 16 * 1024 * 1024 ||
        !ray_tracing_sha256_file(path, digest) || strcmp(digest, expected))
        return false;
    bool srgb = token(entry, "color_space", "srgb");
    if (channel == 0 ? (!srgb && !token(entry, "color_space", "linear"))
                     : !token(entry, "color_space", "data"))
        return false;
    /* Faults remain preparation-boundary failures even for warm cache hits. */
    if (sampling_preparing && surface_preparation_fail(RUNTIME_SURFACE_FAIL_IMAGE_DECODE))
        return false;
    char key[96];
    snprintf(key, sizeof(key), "image-v%d:%d:%d:%s", composition && channel==2 ? 2 : 1, channel, srgb, expected);
    SamplingResource *cached = sampling_resource_find(key);
    if (cached) {
        *resource = cached;
        *out = cached->pyramid;
        return true;
    }
    unsigned char *bytes = NULL;
    int w = 0, h = 0;
    ++sampling_cache_stats.decodes;
    if (!RuntimeMaterialAuthoredTextureReadPNGData(path, 1024, &bytes, &w, &h))
        return false;
    if (w < 1 || h < 1 || (w & (w - 1)) || (h & (h - 1)) ||
        !ray_tracing_sha256_file(path, digest) || strcmp(digest, expected)) {
        free(bytes);
        return false;
    }
    SamplingResource *slot =
        sampling_resource_slot(sampling_pyramid_bytes((unsigned)w, (unsigned)h, 4));
    if (!slot) {
        free(bytes);
        return false;
    }
    float *pixels = calloc((size_t)w * h * 4, sizeof(float));
    if (!pixels) {
        free(bytes);
        return false;
    }
    bool valid = true;
    for (size_t p = 0; p < (size_t)w * h; ++p) {
        double alpha = bytes[4 * p + 3] / 255.0;
        for (int c = 0; c < 4; ++c)
            pixels[4 * p + c] = bytes[4 * p + c] / 255.0f;
        if (channel == 0)
            for (int c = 0; c < 3; ++c)
                pixels[4 * p + c] =
                    (float)((srgb ? core_authored_surface_srgb_to_linear(pixels[4 * p + c])
                                  : pixels[4 * p + c]) *
                            alpha);
        if (channel == 1)
            pixels[4 * p] *= pixels[4 * p]; /* RMS roughness */
        if (channel == 2) {
            double n[3];
            for (int c = 0; c < 3; ++c)
                n[c] = composition && c<2 ? fmax(-1,((double)bytes[4*p+c]-128)/127) : 2 * pixels[4 * p + c] - 1;
            double length = hypot(hypot(n[0], n[1]), n[2]);
            if (length < 1e-6 || n[2] < 0) {
                valid = false;
                break;
            }
            for (int c = 0; c < 3; ++c)
                pixels[4 * p + c] = (float)(n[c] / length);
        }
    }
    CoreAuthoredSurfacePyramid built = {0};
    if (valid)
        valid = core_authored_surface_pyramid_build(&built, (unsigned)w, (unsigned)h, 4, pixels);
    free(pixels);
    free(bytes);
    if (valid) {
        *resource = sampling_resource_publish(slot, key, &built);
        *out = (*resource)->pyramid;
        ++sampling_cache_stats.image_builds;
    } else
        core_authored_surface_pyramid_free(&built);
    return valid;
}
static bool sampling_read(json_object *o, json_object *row, SurfaceSampling *s) {
    json_object *decl = sampling_declaration(o);
    if (!decl)
        return true;
    s->declared = true;
    s->composition = composition_source(row);
    if (!json_object_is_type(decl, json_type_object) ||
        !token(decl, "required_capability", "optic.surface_sampling_v1") ||
        !json_object_is_type(field(decl, "version"), json_type_int) ||
        json_object_get_int(field(decl, "version")) != 1 || !token(decl, "filter", "trilinear") ||
        !token(decl, "address", "repeat") || !token(decl, "color_space", "linear") ||
        !vector(decl, "period_tiles", s->period, 2) || !mapping(o) ||
        field(o, "procedural_solid_material_ref") || field(row, "authored_texture") ||
        field(row, "material_graph") || field(row, "materialGraph") ||
        field(row, "surface_material_binding"))
        return false;
    for (int c = 0; c < 2; ++c)
        if (s->period[c] < 1 || s->period[c] > 32 || floor(s->period[c]) != s->period[c])
            return false;
    CoreAuthoredSurfaceMapping map;
    if (!parse(mapping(o), &map) || (s->composition && map.version!=1 && map.version!=3))
        return false;
    /* Axial U wraps exactly once per declared circumference; no chart-edge seam. */
    if (map.version == 2 &&
        s->period[0] != round(6.2831853071795864769 * map.reference_radius_m / map.tile_m[0]))
        return false;
    s->normal_strength = 1;
    s->height_m = 0;
    if (field(decl, "normal_strength") &&
        (!number(field(decl, "normal_strength"), &s->normal_strength) || s->normal_strength < 0 ||
         s->normal_strength > 2))
        return false;
    if (field(decl, "height_m") &&
        (!number(field(decl, "height_m"), &s->height_m) || s->height_m < 0 || s->height_m > 1))
        return false;
    json_object *channels = field(decl, "channels");
    if (channels && !json_object_is_type(channels, json_type_object))
        return false;
    if (channels) {
        json_object_object_foreach(channels, key, value) {
            int c = 0;
            for (; c < 4; ++c)
                if (!strcmp(key, sampling_channels[c]))
                    break;
            if (c == 4 || !sampling_image(value, c, &s->images[c], &s->image_resources[c], s->composition))
                return false;
        }
    }
    if (s->images[2].values && s->images[3].values)
        return false;
    if (s->height_m > 0 && !s->images[3].values)
        return false;
    return true;
}
static size_t sampling_unique_bytes(const SurfaceSampling *samples) {
    bool seen[SAMPLING_CACHE_MAX_ENTRIES] = {false};
    size_t bytes = 0;
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        SamplingResource *resources[] = {
            samples[i].material_resource, samples[i].image_resources[0],
            samples[i].image_resources[1], samples[i].image_resources[2],
            samples[i].image_resources[3]};
        for (size_t c = 0; c < 5; ++c)
            if (resources[c]) {
                size_t slot = (size_t)(resources[c] - sampling_resources);
                if (!seen[slot]) {
                    seen[slot] = true;
                    bytes += sampling_resource_bytes(resources[c]);
                }
            }
    }
    return bytes;
}
static bool sampling_validate_scene(json_object *root, char *diagnostic, size_t size) {
    json_object *objects = field(root, "objects");
    size_t object_count =
        json_object_is_type(objects, json_type_array) ? json_object_array_length(objects) : 0;
    char (*programs)[65] = object_count ? calloc(object_count, sizeof(*programs)) : NULL;
    if (object_count && !programs)
        return false;
    size_t program_count = 0;
    bool valid = true;
    sampling_validation_begin();
    for (size_t i = 0; i < object_count; ++i) {
        json_object *o = json_object_array_get_idx(objects, i);
        SurfaceSampling s = {0};
        const char *id = json_object_get_string(field(o, "object_id"));
        json_object *row = id ? material_row(root, id) : NULL;
        bool ok = sampling_read(o, row, &s);
        if (ok && s.declared && !s.composition) {
            char key[65];
            ok = sampling_program_estimate_key(o, row, s.period, key);
            bool duplicate = false;
            for (size_t p = 0; ok && p < program_count; ++p)
                if (!strcmp(key, programs[p]))
                    duplicate = true;
            if (ok && !duplicate)
                snprintf(programs[program_count++], 65, "%s", key);
        }
        sampling_free(&s);
        size_t estimate =
            sampling_validation_bytes() +
            program_count * sampling_pyramid_bytes(SAMPLING_RESOLUTION, SAMPLING_RESOLUTION, 8);
        if (!ok || estimate > sampling_cache_budget) {
            if (diagnostic && size)
                snprintf(
                    diagnostic, size,
                    "surface_sampling object %zu: unsupported declaration, missing/stale image, "
                    "color/data encoding, conflicting response, path escape or unique-resource "
                    "budget",
                    i);
            valid = false;
            break;
        }
    }
    sampling_validation_end();
    free(programs);
    return valid;
}
static bool sampling_prepare_scene(json_object *root, const Binding *bindings,
                                   SurfaceSampling *sampling, size_t *bytes,
                                   unsigned long long *builds) {
    json_object *objects = field(root, "objects");
    for (size_t j = 0;
         json_object_is_type(objects, json_type_array) && j < json_object_array_length(objects);
         ++j) {
        json_object *o = json_object_array_get_idx(objects, j);
        if (!sampling_declaration(o))
            continue;
        for (int i = 0; i < sceneSettings.objectCount; ++i) {
            char id[64] = {0};
            runtime_scene_bridge_get_last_object_id_for_scene_index(i, id, sizeof(id));
            if (!token(o, "object_id", id))
                continue;
            SurfaceSampling *s = &sampling[i];
            if (!sampling_read(o, material_row(root, id), s))
                goto failure;
            RuntimeMaterialPayload3D payload;
            RuntimeMaterialTextureStack stack;
            if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(i, &payload) ||
                (!s->composition && !SceneEditorMaterialStackGetEffectiveObjectStack(&sceneSettings.sceneObjects[i], i,
                                                                 &stack)))
                goto failure;
            s->base = payload;
            const Binding *binding = &bindings[i];
            for (int axis = 0; axis < 3; ++axis) {
                Vec3 gradient = vec3(axis == 0, axis == 1, axis == 2);
                gradient = vec3_scale(gradient, 1 / binding->world_scale);
                Vec3 origin = vec3(0, 0, 0);
                if (binding->map.space == CORE_AUTHORED_SURFACE_OBJECT_REST && binding->mesh) {
                    gradient = vec3_scale(vec3(binding->basis[axis][0], binding->basis[axis][1],
                                               binding->basis[axis][2]),
                                          1 / (binding->world_scale * binding->scale[axis]));
                    origin = vec3_scale(
                        vec3(binding->position[0], binding->position[1], binding->position[2]),
                        binding->world_scale);
                } else if (binding->map.space == CORE_AUTHORED_SURFACE_OBJECT_REST) {
                    const RuntimeSceneBridgePrimitiveSeed *p = &binding->primitive;
                    Vec3 axes[] = {vec3(p->axis_u_x, p->axis_u_y, p->axis_u_z),
                                   vec3(p->axis_v_x, p->axis_v_y, p->axis_v_z),
                                   vec3(p->normal_x, p->normal_y, p->normal_z)};
                    double sizes[] = {p->width, p->height, p->depth};
                    gradient = vec3_scale(axes[axis], sizes[axis] > 1e-12
                                                          ? binding->source_size[axis] / sizes[axis]
                                                          : 0);
                    origin = vec3(p->origin_x, p->origin_y, p->origin_z);
                }
                s->coordinate_gradient[axis] = gradient;
                s->coordinate_offset[axis] = -vec3_dot(origin, gradient);
            }
            if(s->composition) {s->valid=true;continue;}
            RuntimeMaterialSurfaceEval base = RuntimeMaterialSurfaceEvalMakeBase(
                payload.baseColorR, payload.baseColorG, payload.baseColorB, payload.bsdf.roughness,
                payload.bsdf.reflectivity, payload.bsdf.specWeight, payload.bsdf.diffuseWeight,
                payload.transparency);
            if (surface_preparation_fail(RUNTIME_SURFACE_FAIL_PYRAMID_ALLOCATION))
                goto failure;
            char program_key[96];
            if (!sampling_program_key(&stack, &base, s->period, bindings[i].map.seed, program_key))
                goto failure;
            s->material_resource = sampling_resource_find(program_key);
            if (!s->material_resource) {
                SamplingResource *slot = sampling_resource_slot(
                    sampling_pyramid_bytes(SAMPLING_RESOLUTION, SAMPLING_RESOLUTION, 8));
                if (!slot)
                    goto failure;
                float *pixels =
                    malloc((size_t)SAMPLING_RESOLUTION * SAMPLING_RESOLUTION * 8 * sizeof(float));
                if (!pixels)
                    goto failure;
                bool ok = true;
                for (unsigned y = 0; y < SAMPLING_RESOLUTION && ok; ++y)
                    for (unsigned x = 0; x < SAMPLING_RESOLUTION; ++x) {
                        RuntimeMaterialSurfaceEval value;
                        ok = RuntimeMaterialTextureStackEvaluateBrickCellsPeriodic(
                            &stack, (x + .5) * s->period[0] / SAMPLING_RESOLUTION,
                            (y + .5) * s->period[1] / SAMPLING_RESOLUTION, bindings[i].map.seed,
                            (int)s->period[0], &base, &value);
                        if (!ok)
                            break;
                        double v[] = {value.colorR,        value.colorG,
                                      value.colorB,        value.roughness * value.roughness,
                                      value.reflectivity,  value.specWeight,
                                      value.diffuseWeight, value.transparency};
                        for (int c = 0; c < 8; ++c)
                            pixels[((size_t)y * SAMPLING_RESOLUTION + x) * 8 + c] = (float)v[c];
                    }
                if (ok)
                    ok = core_authored_surface_pyramid_build(&s->material, SAMPLING_RESOLUTION,
                                                             SAMPLING_RESOLUTION, 8, pixels);
                free(pixels);
                if (!ok)
                    goto failure;
                s->material_resource = sampling_resource_publish(slot, program_key, &s->material);
                ++sampling_cache_stats.program_builds;
                ++*builds;
            }
            s->material = s->material_resource->pyramid;
            s->valid = true;
        }
    }
    *bytes = sampling_unique_bytes(sampling);
    return true;
failure:
    for (int i = 0; i < MAX_OBJECTS; ++i)
        sampling_free(&sampling[i]);
    *bytes = 0;
    return false;
}
bool RuntimeSurfaceSamplingActive(int i) {
    return RuntimeSurfaceMappingActive(i) && sampling[i].declared && sampling[i].valid;
}
unsigned long long RuntimeSurfaceSamplingBuildCount(void) { return sampling_builds; }
size_t RuntimeSurfaceSamplingPreparedBytes(void) { return sampling_bytes; }
static bool sampling_gradient_basis(Vec3 du, Vec3 dv, Vec3 *gu, Vec3 *gv) {
    double a = vec3_dot(du, du), b = vec3_dot(du, dv), c = vec3_dot(dv, dv), det = a * c - b * b;
    if (!isfinite(det) || det <= 1e-20 * fmax(1, a * c))
        return false;
    *gu = vec3_scale(vec3_sub(vec3_scale(du, c), vec3_scale(dv, b)), 1 / det);
    *gv = vec3_scale(vec3_sub(vec3_scale(dv, a), vec3_scale(du, b)), 1 / det);
    return true;
}
static bool sampling_coordinates(const HitInfo3D *hit, double uv[2], double dx[2], double dy[2],
                                 Vec3 *du, Vec3 *dv, double *source_weight) {
    if (!hit || !RuntimeSurfaceSamplingActive(hit->sceneObjectIndex))
        return false;
    int i = hit->sceneObjectIndex;
    SurfaceSampling *s = &sampling[i];
    const CoreAuthoredSurfaceMapping *m = &bindings[i].map;
    CoreAuthoredSurfaceCoordinates q;
    if (!coordinates_for(hit, m, &q))
        return false;
    if (source_weight)
        *source_weight = q.source_weight;
    for (int k = 0; k < 2; ++k) {
        uv[k] = q.uv_tiles[k] / s->period[k];
        dx[k] = dy[k] = 0;
    }
    Vec3 gu = vec3(0, 0, 0), gv = gu;
    bool frame = true;
    if (m->version == 3) {
        Vec3 a, b;
        frame = hit->hasSurfaceDifferentials &&
                sampling_gradient_basis(hit->surfaceDpDu, hit->surfaceDpDv, &a, &b);
        if (frame) {
            double co = cos(m->rotation_rad), si = sin(m->rotation_rad);
            gu = vec3_scale(
                vec3_sub(vec3_scale(a, co * m->uv_scale[0]), vec3_scale(b, si * m->uv_scale[1])),
                1 / s->period[0]);
            gv = vec3_scale(
                vec3_add(vec3_scale(a, si * m->uv_scale[0]), vec3_scale(b, co * m->uv_scale[1])),
                1 / s->period[1]);
        }
    } else {
        /* Analytic chart gradients transformed by the prepared inverse frame. */
        double local_u[3], local_v[3];
        if (m->version == 1) {
            double co = cos(m->rotation_rad), si = sin(m->rotation_rad);
            for (int k = 0; k < 3; ++k) {
                local_u[k] = (co * m->axis_u[k] - si * m->axis_v[k]) / m->tile_m[0];
                local_v[k] = (si * m->axis_u[k] + co * m->axis_v[k]) / m->tile_m[1];
            }
        } else {
            Vec3 axis_u = vec3(m->axis_u[0], m->axis_u[1], m->axis_u[2]),
                 axis_v = vec3(m->axis_v[0], m->axis_v[1], m->axis_v[2]);
            Vec3 tangent = vec3_cross(axis_v, axis_u);
            double point[3];
            for (int k = 0; k < 3; ++k)
                point[k] = vec3_dot(s->coordinate_gradient[k], hit->position) +
                           s->coordinate_offset[k] - m->origin_m[k];
            Vec3 point3 = vec3(point[0], point[1], point[2]);
            double x = vec3_dot(point3, axis_u), y = vec3_dot(point3, tangent),
                   radius2 = x * x + y * y;
            double tangent_components[] = {tangent.x, tangent.y, tangent.z};
            for (int k = 0; k < 3; ++k) {
                local_u[k] = q.singular ? 0
                                        : (x * tangent_components[k] - y * m->axis_u[k]) / radius2 *
                                              q.repeats_u / 6.2831853071795864769;
                local_v[k] = m->axis_v[k] / m->tile_m[1];
            }
        }
        for (int k = 0; k < 3; ++k) {
            gu = vec3_add(gu, vec3_scale(s->coordinate_gradient[k], local_u[k] / s->period[0]));
            gv = vec3_add(gv, vec3_scale(s->coordinate_gradient[k], local_v[k] / s->period[1]));
        }
        frame = !q.singular;
    }
    if (frame && hit->hasPixelFootprint) {
        dx[0] = vec3_dot(gu, hit->pixelDpDx);
        dx[1] = vec3_dot(gv, hit->pixelDpDx);
        dy[0] = vec3_dot(gu, hit->pixelDpDy);
        dy[1] = vec3_dot(gv, hit->pixelDpDy);
    }
    if (hit->footprintUnbounded) {
        dx[0] = dy[1] = 1;
        dx[1] = dy[0] = 0;
    }
    Vec3 n = vec3_normalize(hit->shadingNormal);
    gu = vec3_sub(gu, vec3_scale(n, vec3_dot(gu, n)));
    gv = vec3_sub(gv, vec3_scale(n, vec3_dot(gv, n)));
    if (!frame || !sampling_gradient_basis(gu, gv, du, dv)) {
        *du = *dv = vec3(0, 0, 0);
    }
    return true;
}
bool RuntimeSurfaceSamplingCoordinates(const HitInfo3D *hit, double uv[2], double dx[2],
                                       double dy[2], Vec3 *du, Vec3 *dv) {
    return sampling_coordinates(hit, uv, dx, dy, du, dv, NULL);
}
static bool sampling_finish_response(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload,
                                      RuntimeMaterialSurfaceEval e) {
    HitInfo3D unperturbed=*hit;
    if(hit->hasUnperturbedShadingNormal) {
        unperturbed.shadingNormal=unperturbed.normal=hit->unperturbedShadingNormal;hit=&unperturbed;
    }
    RuntimeMaterialSurfaceEval base=RuntimeMaterialSurfaceEvalMakeBase(
        payload->baseColorR,payload->baseColorG,payload->baseColorB,payload->bsdf.roughness,
        payload->bsdf.reflectivity,payload->bsdf.specWeight,payload->bsdf.diffuseWeight,payload->transparency);
    SurfaceSampling *s=&sampling[hit->sceneObjectIndex];
    double uv[2],dx[2],dy[2],v[8],lod,source_weight=1;Vec3 du,dv;
    if(!sampling_coordinates(hit,uv,dx,dy,&du,&dv,&source_weight))return false;
    double n[] = {hit->shadingNormal.x, hit->shadingNormal.y, hit->shadingNormal.z},
           t[] = {du.x, du.y, du.z}, out[3];
    bool perturbed = false;
    bool frame = vec3_length(du) > 1e-12 && vec3_length(dv) > 1e-12;
    if (s->images[2].values && frame) {
        if (!core_authored_surface_pyramid_sample(&s->images[2], uv, dx, dy, v, NULL))
            return false;
        double coherence = fmin(1, hypot(hypot(v[0], v[1]), v[2]));
        e.roughness = sqrt(fmin(1, e.roughness * e.roughness +
                                       s->normal_strength * s->normal_strength * (1 - coherence)));
        v[0] *= s->normal_strength;
        v[1] *= s->normal_strength;
        double sign = vec3_dot(vec3_cross(hit->shadingNormal, du), dv) < 0 ? -1 : 1;
        if(!(s->composition && (s->normal_strength==0 || (v[0]==0 && v[1]==0 && v[2]>0))))
            perturbed = core_authored_surface_normal(n, t, sign, v, out);
    }
    if (s->images[3].values && s->height_m > 0 && frame) {
        CoreAuthoredSurfacePyramid *p = &s->images[3];
        if (!core_authored_surface_pyramid_sample(p, uv, dx, dy, v, &lod))
            return false;
        double gradient[2], value[2][8];
        for (int k = 0; k < 2; ++k) {
            double step = exp2(lod) / (k ? p->height : p->width), a[2] = {uv[0], uv[1]},
                   b[2] = {uv[0], uv[1]};
            a[k] -= step;
            b[k] += step;
            if (!core_authored_surface_pyramid_sample(p, a, dx, dy, value[0], NULL) ||
                !core_authored_surface_pyramid_sample(p, b, dx, dy, value[1], NULL))
                return false;
            gradient[k] = (value[1][0] - value[0][0]) * s->height_m / (2 * step);
        }
        double u[] = {du.x, du.y, du.z}, vworld[] = {dv.x, dv.y, dv.z};
        perturbed = core_authored_surface_bump(n, u, vworld, gradient, out);
    }
    RuntimeSurfaceMappingBlendPole(&base, source_weight, &e);
    if (!RuntimeMaterialPayload3D_ApplySurfaceEval(&e, payload))
        return false;
    if (perturbed) {
        Vec3 normal =
            vec3_normalize(vec3_add(vec3_scale(hit->shadingNormal, 1 - source_weight),
                                    vec3_scale(vec3(out[0], out[1], out[2]), source_weight)));
        if (vec3_dot(normal, hit->geometricNormal) > 1e-6) {
            payload->hasMicrodetailNormal = true;
            payload->microdetailShadingNormal = normal;
        }
    }
    return true;
}

bool RuntimeSurfaceSamplingResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload) {
    if (!hit || !payload || !RuntimeSurfaceSamplingActive(hit->sceneObjectIndex))
        return false;
    if(sampling[hit->sceneObjectIndex].composition)return RuntimeSurfaceGraphResolve(hit,payload);
    *payload = sampling[hit->sceneObjectIndex].base;
    payload->valid = RuntimeSurfaceSamplingApply(hit, payload);
    return payload->valid;
}
bool RuntimeSurfaceSamplingApply(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload) {
    if(hit && RuntimeSurfaceSamplingActive(hit->sceneObjectIndex) && sampling[hit->sceneObjectIndex].composition)
        return RuntimeSurfaceSamplingApplyResponse(hit,payload);
    if (!hit || !payload || !RuntimeSurfaceSamplingActive(hit->sceneObjectIndex))
        return true;
    HitInfo3D unperturbed = *hit;
    if (hit->hasUnperturbedShadingNormal) {
        unperturbed.shadingNormal = unperturbed.normal = hit->unperturbedShadingNormal;
        hit = &unperturbed;
    }
    double source_weight = 1;
    SurfaceSampling *s = &sampling[hit->sceneObjectIndex];
    double uv[2], dx[2], dy[2], v[8], lod;
    Vec3 du, dv;
    if (!sampling_coordinates(hit, uv, dx, dy, &du, &dv, &source_weight) ||
        !core_authored_surface_pyramid_sample(&s->material, uv, dx, dy, v, &lod))
        return false;
    RuntimeMaterialSurfaceEval e = RuntimeMaterialSurfaceEvalMakeBase(
        v[0], v[1], v[2], sqrt(fmax(0, v[3])), v[4], v[5], v[6], v[7]);
    e.active = true;
    if (s->images[0].values) {
        if (!core_authored_surface_pyramid_sample(&s->images[0], uv, dx, dy, v, NULL))
            return false;
        e.colorR = v[0] + e.colorR * (1 - v[3]);
        e.colorG = v[1] + e.colorG * (1 - v[3]);
        e.colorB = v[2] + e.colorB * (1 - v[3]);
    }
    if (s->images[1].values) {
        if (!core_authored_surface_pyramid_sample(&s->images[1], uv, dx, dy, v, NULL))
            return false;
        e.roughness = sqrt(fmax(0, v[0]));
    }
    return sampling_finish_response(hit,payload,e);
}

bool RuntimeSurfaceSamplingApplyResponse(const HitInfo3D *hit,RuntimeMaterialPayload3D *payload) {
    if(!hit || !payload || !RuntimeSurfaceSamplingActive(hit->sceneObjectIndex))return false;
    RuntimeMaterialSurfaceEval e=RuntimeMaterialSurfaceEvalMakeBase(
        payload->baseColorR,payload->baseColorG,payload->baseColorB,payload->bsdf.roughness,
        payload->bsdf.reflectivity,payload->bsdf.specWeight,payload->bsdf.diffuseWeight,payload->transparency);
    e.active=true;
    return sampling_finish_response(hit,payload,e);
}
bool RuntimeSurfaceSamplingGraphInputs(const HitInfo3D *hit,const CoreSurfaceGraph *program,
                                      CoreSurfaceGraphInputs *inputs) {
    if(!hit || !program || !inputs || !RuntimeSurfaceSamplingActive(hit->sceneObjectIndex))return false;
    memset(inputs,0,sizeof(*inputs));
    SurfaceSampling *s=&sampling[hit->sceneObjectIndex];
    if(!s->composition)return false;
    HitInfo3D unperturbed=*hit;
    if(hit->hasUnperturbedShadingNormal){unperturbed.shadingNormal=unperturbed.normal=hit->unperturbedShadingNormal;hit=&unperturbed;}
    double uv[2],dx[2],dy[2],sampled[4][8]={{0}};bool ready[4]={0};Vec3 du,dv;
    if(!sampling_coordinates(hit,uv,dx,dy,&du,&dv,NULL))return false;
    for(size_t i=0;i<program->count;++i) {
        const CoreSurfaceGraphNode *node=&program->nodes[i];
        if(node->kind!=CORE_SG_IMAGE_COLOR && node->kind!=CORE_SG_IMAGE_SCALAR)continue;
        int channel=node->resource==CORE_SG_RESOURCE_BASE_COLOR || node->resource==CORE_SG_RESOURCE_BASE_COLOR_ALPHA ? 0 :
                    node->resource==CORE_SG_RESOURCE_ROUGHNESS ? 1 : node->resource==CORE_SG_RESOURCE_HEIGHT ? 3 : -1;
        if(channel<0 || !s->images[channel].values)return false;
        if(!ready[channel]) {
            if(!core_authored_surface_pyramid_sample(&s->images[channel],uv,dx,dy,sampled[channel],NULL))return false;
            ready[channel]=true;
        }
        CoreSurfaceGraphInput *out=&inputs->nodes[i];out->kind=node->kind;out->valid=true;
        for(int c=0;c<3;++c) {
            double value=node->kind==CORE_SG_IMAGE_COLOR ? (sampled[0][3]>1e-12?sampled[0][c]/sampled[0][3]:0) :
                node->resource==CORE_SG_RESOURCE_ROUGHNESS ? sqrt(fmax(0,sampled[1][0])) :
                node->resource==CORE_SG_RESOURCE_BASE_COLOR_ALPHA ? sampled[0][3] : sampled[3][0];
            out->value[c]=fmax(0,fmin(1,value));
        }
    }
    return true;
}
