#include "tools/ray_tracing_headless_spectral.h"
#include "tools/ray_tracing_headless_spectral_map.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_spectral_observer.h"
#include <json-c/json.h>
#include <math.h>
#include <limits.h>
#include <string.h>

/* Reject unsupported scene-level lighting, authoring layers and distance units
 * before loading their resources. Native scene preparation owns all geometry. */
static bool receiver_materials(json_object *ext, const RuntimeSpectralPencil *p) {
    if (!ext) return true;
    if (!json_object_is_type(ext, json_type_object)) return false;
    if (!json_object_object_length(ext)) return true;
    if (!p->use_scene_texture || json_object_object_length(ext) != 1) return false;
    json_object *rt = json_object_object_get(ext, "ray_tracing"),
                *auth = json_object_object_get(rt, "authoring");
    json_object *items = json_object_object_get(auth, "object_materials");
    if (!json_object_is_type(rt, json_type_object) || json_object_object_length(rt) != 1 ||
        !json_object_is_type(auth, json_type_object) || json_object_object_length(auth) != 1 ||
        !json_object_is_type(items, json_type_array) || json_object_array_length(items) != 1)
        return false;
    json_object *entry = json_object_array_get_idx(items, 0);
    json_object *id = json_object_object_get(entry, "object_id");
    if (!json_object_is_type(entry, json_type_object) || !json_object_is_type(id, json_type_string) ||
        strcmp(json_object_get_string(id), p->receiver_id))
        return false;
    /* Limit authoring to scalar matte texture controls consumed by the
     * scene bridge. Glass and light behavior are owned by the spectral request. */
    static const struct {
        const char *key;
        double lo, hi;
        bool integer;
    } fields[] = {{"object_color", 0, 0xffffff, true},
                  {"material_id", 0, 0, true},
                  {"texture_id", 0, 2, true},
                  {"texture_pattern_mode", 0, 3, true},
                  {"texture_seed", 0, INT_MAX, true},
                  {"texture_scale", .0001, 10000, false},
                  {"texture_offset_u", -10000, 10000, false},
                  {"texture_offset_v", -10000, 10000, false},
                  {"texture_strength", 0, 1, false},
                  {"texture_coverage", 0, 1, false},
                  {"texture_grain", 0, 1, false},
                  {"texture_edge_softness", 0, 1, false},
                  {"texture_contrast", 0, 1, false},
                  {"texture_flow", 0, 1, false},
                  {"texture_color_depth", 0, 1, false},
                  {"texture_surface_damage", 0, 1, false}};
    json_object_object_foreach(entry, key, value) {
        if (!strcmp(key, "object_id")) continue;
        size_t n = 0;
        while (n < sizeof(fields) / sizeof(fields[0]) && strcmp(key, fields[n].key))
            ++n;
        if (n == sizeof(fields) / sizeof(fields[0]) ||
            !(json_object_is_type(value, json_type_int) ||
              (!fields[n].integer && json_object_is_type(value, json_type_double))))
            return false;
        double v = json_object_get_double(value);
        if (!(v >= fields[n].lo && v <= fields[n].hi)) return false;
    }
    return true;
}
bool RayTracingHeadlessSpectral_SceneSupported(const char *path, const RuntimeSpectralPencil *p) {
    bool mapping = p->map.source_count > 0;
    json_object *scene = json_object_from_file(path);
    if (!scene) return false;
    json_object *ext = json_object_object_get(scene, "extensions");
    json_object *scale = json_object_object_get(scene, "world_scale");
    json_object *units = json_object_object_get(scene, "unit_system");
    json_object *objects = json_object_object_get(scene, "objects");
    json_object *lights = json_object_object_get(scene, "lights");
    bool ok =
        json_object_is_type(units, json_type_string) && !strcmp(json_object_get_string(units), "meters") &&
        (json_object_is_type(scale, json_type_int) || json_object_is_type(scale, json_type_double)) &&
        json_object_get_double(scale) == 1 && receiver_materials(ext, p) &&
        json_object_is_type(objects, json_type_array) &&
        (mapping ? json_object_array_length(objects) >= (p->glass_id[0] ? 2u : 1u) && json_object_array_length(objects) <= 64
                 : json_object_array_length(objects) == 2) &&
        (!lights || (json_object_is_type(lights, json_type_array) && !json_object_array_length(lights)));
    for (size_t i = 0; ok && i < json_object_array_length(objects); ++i) {
        json_object *obj = json_object_array_get_idx(objects, i);
        json_object *extensions = json_object_object_get(obj, "extensions");
        if (!extensions) continue;
        json_object *ld = json_object_object_get(extensions, "line_drawing");
        ok = json_object_is_type(extensions, json_type_object) &&
             (!json_object_object_length(extensions) ||
              (json_object_object_length(extensions) == 1 && json_object_is_type(ld, json_type_object) &&
               json_object_object_length(ld) == 1 &&
               json_object_is_type(json_object_object_get(ld, "runtime_mesh_path"), json_type_string)));
    }
    json_object_put(scene);
    return ok;
}

static bool closed_mesh(const char *id) {
    const RayTracingRuntimeMeshAssetSet *assets = ray_tracing_runtime_mesh_assets_last();
    for (int i = 0; assets && i < assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance *instance = &assets->instances[i];
        if (strcmp(instance->object_id, id) || instance->asset_index < 0 ||
            instance->asset_index >= assets->asset_count)
            continue;
        return assets->assets[instance->asset_index].document.contract.topology_closed_volume;
    }
    return false;
}

static bool supported(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p) {
    bool glass = p->volume.kind && !p->glass_id[0], receiver = false;
    if ((!p->map.source_count && scene->primitiveCount != 2) || scene->volume.enabled ||
        scene->curveInstanceCount)
        return false;
    for (int i = 0; i < scene->lightSet.lightCount; ++i) {
        const RuntimeLightSource3D *l = &scene->lightSet.lights[i];
        if (l->enabled && l->origin != RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT) return false;
    }
    for (int i = 0; i < scene->primitiveCount; ++i) {
        const RuntimePrimitive3D *prim = &scene->primitives[i];
        if (!strcmp(prim->source.objectId, p->glass_id))
            glass = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH && closed_mesh(p->glass_id);
        else if (prim->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            return false;
        if (!strcmp(prim->source.objectId, p->receiver_id))
            receiver = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    }
    return glass && receiver;
}

static uint8_t display(double linear) {
    double c = fmin(1, fmax(0, linear));
    return (uint8_t)lround(255 * (c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1 / 2.4) - 0.055));
}

void RayTracingHeadlessSpectral_Pixel(uint8_t *rgb, Vec3 xyz, double exposure) {
    xyz = vec3_scale(xyz, exposure);
    rgb[0] = display(3.2404542 * xyz.x - 1.5371385 * xyz.y - 0.4985314 * xyz.z);
    rgb[1] = display(-0.9692660 * xyz.x + 1.8760108 * xyz.y + 0.0415560 * xyz.z);
    rgb[2] = display(0.0556434 * xyz.x - 0.2040259 * xyz.y + 1.0572252 * xyz.z);
    rgb[3] = 255;
}

bool RayTracingHeadlessSpectral_Run(const RayTracingAgentRenderRequest *request,
                                    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                    RuntimeSpectralReadout *out, char *diag, size_t size) {
    const RuntimeSpectralPencil *p = &request->spectral;
    const RuntimeScene3D *scene = &frame->scene;
    if (!supported(scene, p)) {
        snprintf(
            diag, size,
            "spectral V3 requires supported dielectric geometry, a named matte receiver, compatible planes, and only the "
            "spectral source");
        return false;
    }
    if (p->map.source_count) {
        bool previous = RuntimeRay3D_SetInclusiveTriangleEdges(true);
        bool ok = RayTracingHeadlessSpectralMap_Run(p, frame, pixels, out, diag, size);
        RuntimeRay3D_SetInclusiveTriangleEdges(previous);
        return ok;
    }
    Ray3D source = RuntimeRay3D_Make(p->origin_m, p->direction);
    Ray3D center = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, (frame->width - 1) * 0.5,
                                                           (frame->height - 1) * 0.5);
    Vec3 response[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    double radius2 = p->receiver_radius_m * p->receiver_radius_m;
    for (int i = 0; i < p->count; ++i) {
        out->ior[i] = RuntimeSpectralPencil_Index(p, p->lines[i].vacuum_nm);
        if (!RuntimeSpectralPencil_Trace(scene, p, &p->lines[i], out->ior[i], source, false,
                                         &out->forward[i]) ||
            !RuntimeSpectralPencil_Trace(scene, p, &p->lines[i], out->ior[i], center, true,
                                         &out->camera[i]))
            goto invalid_boundary;
        /* Unit-albedo diffuse detector, normalized circular spatial response. */
        response[i] = vec3_scale(RuntimeSpectral_CIE1931(p->lines[i].vacuum_nm),
                                 out->forward[i].weight / (M_PI * M_PI * radius2));
    }
    if (!pixels) return true;
    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) {
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, x, y);
            Vec3 xyz = vec3(0, 0, 0);
            for (int i = 0; i < p->count; ++i) {
                if (!out->forward[i].receiver) continue;
                RuntimeSpectralPathReadout hit;
                if (!RuntimeSpectralPencil_Trace(scene, p, &p->lines[i], out->ior[i], ray, true, &hit))
                    goto invalid_boundary;
                Vec3 delta = vec3_sub(hit.position, out->forward[i].position);
                if (hit.receiver && hit.receiver_front == out->forward[i].receiver_front &&
                    vec3_dot(delta, delta) <= radius2)
                    xyz = vec3_add(xyz, vec3_scale(response[i], hit.weight));
            }
            if (x == frame->width / 2 && y == frame->height / 2) out->center_pixel_xyz = xyz;
            RayTracingHeadlessSpectral_Pixel(pixels + ((size_t)y * frame->width + x) *
                                                          RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES,
                                             xyz, p->exposure);
        }
    }
    return true;
invalid_boundary:
    snprintf(diag, size,
             "spectral boundary mismatch: source and camera must start in air, glass must be closed and "
             "outward wound");
    return false;
}

bool RayTracingHeadlessSpectral_Render(const RayTracingAgentRenderRequest *request,
                                       const RayEvaluatedSceneSnapshot *scene, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, char *diag, size_t size) {
    RuntimeNative3DPreparedFrame frame = {0};
    bool ok = RuntimeNative3DPrepareFrameWithSamplingForEvaluatedSceneAndMutation(
        &frame, request->width, request->height, scene, NULL, NULL, NULL);
    if (ok) ok = RayTracingHeadlessSpectral_Run(request, &frame, pixels, out, diag, size);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}

static void write_path(FILE *f, const RuntimeSpectralPathReadout *hit, const char *weight_unit) {
    fprintf(f,
            "{\"vacuum_nm\":%.17g,\"depth_limited\":%s,\"receiver_hit\":%s,\"interfaces\":%d,\"glass_"
            "m\":%.17g,\"%s\":%.17g,"
            "\"position_m\":[%.17g,%.17g,%.17g],\"direction\":[%.17g,%.17g,%.17g]}",
            hit->vacuum_nm, hit->depth_limited ? "true" : "false", hit->receiver ? "true" : "false",
            hit->interfaces, hit->glass_m, weight_unit, hit->weight, hit->position.x, hit->position.y,
            hit->position.z, hit->direction.x, hit->direction.y, hit->direction.z);
}

void RayTracingHeadlessSpectral_Write(FILE *f, const RayTracingAgentRenderRequest *request,
                                      const RuntimeSpectralReadout *out, bool rendered) {
    const RuntimeSpectralPencil *p = &request->spectral;
    if (rendered)
        fprintf(f, "  \"spectral_center_pixel_xyz\":[%.17g,%.17g,%.17g],\n", out->center_pixel_xyz.x,
                out->center_pixel_xyz.y, out->center_pixel_xyz.z);
    if (rendered && p->sensor_count) {
        fprintf(f, "  \"spectral_center_sensor_signals\":[");
        for (int i = 0; i < p->sensor_count; ++i)
            fprintf(f, "%s%.17g", i ? "," : "", out->sensor_signal[i]);
        fprintf(f, "],\n");
    }
    if (p->map.source_count) {
        fprintf(f,"  \"spectral_source_models\":[");
        for(int i=0;i<p->map.source_count;++i) {
            const RuntimeSpectralLamp *lamp=&p->map.sources[i];
            fprintf(f,"%s\"%s\"",i ? "," : "",lamp->spectrum_model ? lamp->spectrum_model : "tabulated_and_lines");
        }
        fprintf(f,"],\n");
        if(p->use_scene_texture) fprintf(f,"  \"spectral_receiver_texture\":\"scene_rgb_approximation\",\n");
        RayTracingHeadlessSpectralMap_Write(f, &out->map, rendered);
        return;
    }
    fprintf(f, "  \"spectral_pencil\": [");
    for (int i = 0; i < p->count; ++i) {
        double cosine = vec3_dot(p->direction, out->forward[i].direction);
        fprintf(f,
                "%s{\"vacuum_nm\":%.17g,\"source_watts\":%.17g,\"ior\":%.17g,"
                "\"deviation_deg\":%.17g,\"forward\":",
                i ? "," : "", p->lines[i].vacuum_nm, p->lines[i].watts, out->ior[i],
                acos(fmax(-1, fmin(1, cosine))) * 180 / M_PI);
        write_path(f, &out->forward[i], "path_watts");
        fprintf(f, ",\"camera_axis\":");
        write_path(f, &out->camera[i], "throughput");
        fprintf(f, "}");
    }
    fprintf(f, "],\n");
}
