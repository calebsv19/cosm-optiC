#include "render/runtime_spectral_transport.h"
#include "render/runtime_spectral_surface_path.h"
#include "render/runtime_spectral_diffuse.h"
#include "render/runtime_spectral_film.h"
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
    if (p->surface_path.enabled || p->legacy_direct || (!p->map.source_count && !p->count && !p->diffuse.enabled)) {
        json_object *scene=json_object_from_file(path);
        bool ok=scene && json_object_is_type(scene,json_type_object);
        if(scene)json_object_put(scene);
        return ok; /* Native preparation owns ordinary scene/feature capability admission. */
    }
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
        (p->surface_path.enabled ? json_object_array_length(objects) >= 1 &&
             json_object_array_length(objects) <= SPECTRAL_PATH_MATERIALS
         : p->diffuse.enabled ? json_object_array_length(objects) >= 2 && json_object_array_length(objects) <= 8
         : mapping ? json_object_array_length(objects) >= (p->glass_id[0] ? 2u : 1u) && json_object_array_length(objects) <= 64
                 : json_object_array_length(objects) == 2) &&
        (!lights || (json_object_is_type(lights, json_type_array) && !json_object_array_length(lights)));
    for (size_t i = 0; ok && i < json_object_array_length(objects); ++i) {
        json_object *obj = json_object_array_get_idx(objects, i);
        json_object *extensions = json_object_object_get(obj, "extensions");
        if (!extensions) continue;
        if (p->diffuse.enabled && !mapping) { ok = false; break; }
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

void RayTracingHeadlessSpectral_Pixel(uint8_t *rgb, Vec3 xyz, double exposure) {
    RuntimeSpectral_DisplayPixel(rgb, RuntimeSpectral_LinearRGB(vec3_scale(xyz, exposure)));
}

bool RayTracingHeadlessSpectral_Run(const RayTracingAgentRenderRequest *request,
                                    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                    RuntimeSpectralReadout *out, char *diag, size_t size) {
    const RuntimeSpectralPencil *p = &request->spectral;
    const RuntimeScene3D *scene = &frame->scene;
    if (RayTracingHeadlessSpectral_IsOrdinary(request) && frame->spectralFrame) {
        if (!pixels) {*out=frame->spectralFrame->readout;return true;}
        if (frame->spectralFrame->surface_path) {
            bool ok=RuntimeSpectralTransport_Render((RuntimeNative3DPreparedFrame *)frame,pixels,NULL,NULL,NULL,NULL);
            *out=frame->spectralFrame->readout;
            snprintf(diag,size,"%s",frame->spectralFrame->diagnostic);return ok;
        }
        if (frame->spectralFrame->profile.legacy_direct)
            return RuntimeNative3DRenderPreparedRegion(pixels,RAY_TRACING_3D_INTEGRATOR_DISNEY_V3_SPECTRAL,frame,0,0,frame->width,frame->height,NULL);
    }

    if (p->surface_path.enabled)
        return RuntimeSpectralSurfacePath_Run(p, frame, pixels, out, diag, size);
    if (p->diffuse.enabled && !p->map.source_count) return RuntimeSpectralDiffuse_Run(p, frame, pixels, out, diag, size);
    if (!RuntimeSpectralFilm_Supported(scene, p)) {
        snprintf(
            diag, size,
            "spectral V3 requires supported dielectric geometry, a named matte receiver, compatible planes, and only the "
            "spectral source");
        return false;
    }
    if (p->map.source_count) {
        bool previous = RuntimeRay3D_SetInclusiveTriangleEdges(true);
        bool ok = RayTracingHeadlessSpectralMap_Run(request, frame, pixels, out, diag, size);
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
            RuntimeSpectral_DisplayPixelWithTransform(
                pixels + ((size_t)y * frame->width + x) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES,
                RuntimeSpectral_LinearRGB(vec3_scale(xyz, p->exposure)), p->soft_display);
        }
    }
    return true;
invalid_boundary:
    snprintf(diag, size,
             "spectral boundary mismatch: source and camera must start in air, glass must be closed and "
             "outward wound");
    return false;
}

bool RayTracingHeadlessSpectral_Render(const RayTracingAgentRenderRequest *request,const char *image_path,
                                       const RayEvaluatedSceneSnapshot *scene, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, RuntimeSpectralTransportSnapshot *snapshot, char *diag, size_t size) {
    if (RayTracingHeadlessSpectral_IsOrdinary(request))
        return RayTracingHeadlessSpectral_RenderNative(request,image_path,scene,pixels,out,snapshot,diag,size);
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

static void write_probe(FILE *f, const RuntimeSpectralPencil *p, const RuntimeSpectralReadout *out) {
    const RuntimeSpectralProbeReadout *q = &out->probe;
    if (!q->valid) return;
    const char *geometry_unit = p->diffuse.sky_grid ? "per_sky_radiance" : "per_watt";
    fprintf(f, "  \"spectral_selected_pixel\":{\"x\":%d,\"y\":%d,\"receiver_or_panel_hit\":%s,"
        "\"first_sample_primitive\":%d,\"first_sample_position_m\":[%.17g,%.17g,%.17g],"
        "\"xyz\":[%.17g,%.17g,%.17g],\"signed_linear_rgb\":[%.17g,%.17g,%.17g],"
        "\"direct_geometry_%s\":%.17g,\"indirect_geometry_%s\":%.17g,"
        "\"panel_nodes_accepted\":%d,\"panel_nodes_dark_or_occluded\":%d,"
        "\"base_surface_support_m\":%.17g,\"configured_scattered_support_m\":%.17g,\"bands\":[",
        p->probe.x, p->probe.y, q->hit ? "true" : "false", q->primitive,
        q->position.x, q->position.y, q->position.z, q->xyz.x, q->xyz.y, q->xyz.z,
        q->signed_rgb.x, q->signed_rgb.y, q->signed_rgb.z,
        geometry_unit, q->direct_geometry, geometry_unit, q->indirect_geometry,
        q->panel_accepted, q->panel_occluded, q->base_support_m, q->scattered_support_m);
    bool calibration = !p->map.source_count && p->diffuse.enabled;
    int count = calibration ? p->count : out->map.band_count;
    for (int b = 0; b < count; ++b) {
        const RuntimeSpectralProbeBand *r = &q->bands[b];
        double nm = calibration ? p->lines[b].vacuum_nm : out->map.bands[b].nm;
        fprintf(f, "%s{\"vacuum_nm\":%.17g,\"width_nm\":%.17g,"
            "\"direct_radiance\":%.17g,\"indirect_radiance\":%.17g,"
            "\"photon_surface_radiance\":%.17g,\"single_scatter_radiance\":%.17g,"
            "\"multiple_scatter_radiance\":%.17g,\"mean_camera_throughput\":%.17g,"
            "\"samples\":%u,\"interfaces\":%u,\"depth_limited_samples\":%u,\"gather_candidates\":%llu",
            b ? "," : "", nm, calibration ? 0 : out->map.bands[b].width_nm, r->direct, r->indirect,
            r->mapped_surface, r->single_scatter, r->multiple_scatter, r->camera_throughput,
            r->samples, r->interfaces, r->depth_limited, r->candidates);
        if (p->diffuse.enabled && !calibration)
            fprintf(f, ",\"sky_direct_radiance\":%.17g", r->sky_direct);
        if (calibration)
            fprintf(f, ",\"%s\":%.17g,\"panel_reflectance\":%.17g,\"receiver_reflectance\":%.17g",
                p->diffuse.sky_grid ? "sky_radiance" : "source_watts",
                p->diffuse.sky_grid ? p->diffuse.sky_radiance[b] : p->lines[b].watts, RuntimeSpectralCurve_Value(&p->diffuse.panel_reflectance, nm),
                RuntimeSpectralCurve_Value(&p->reflectance, nm));
        else {
            fprintf(f, ",\"mean_receiver_reflectance\":%.17g", r->mean_receiver_reflectance);
            const RuntimeSpectralDepositProbe *d = &r->largest_deposit;
            if (d->radiance > 0)
                fprintf(f, ",\"largest_surface_deposit\":{\"map_record\":%d,\"source_index\":%d,"
                    "\"vacuum_nm\":%.17g,\"deposited_watts\":%.17g,\"support_m\":%.17g,"
                    "\"kernel_per_m2\":%.17g,\"radiance_contribution\":%.17g,\"medium_scattered\":%s,"
                    "\"incoming_direction\":[%.17g,%.17g,%.17g]}",
                    d->record, d->source, d->vacuum_nm, d->deposited_watts, d->support_m, d->kernel_per_m2,
                    d->radiance, d->scattered ? "true" : "false", d->incident.x, d->incident.y, d->incident.z);
        }
        fputc('}', f);
    }
    fprintf(f, "]},\n");
}

void RayTracingHeadlessSpectral_Write(FILE *f, const RayTracingAgentRenderRequest *request,
                                      const RuntimeSpectralReadout *out, bool rendered) {
    const RuntimeSpectralPencil *p = &request->spectral;
    if (p->surface_path.enabled) {
        RuntimeSpectralSurfacePath_Write(f, p, out);
        return;
    }
    if (p->soft_display)
        fprintf(f, "  \"spectral_display_transform\":\"spectral_soft_v1\",\n");
    json_object *sampling=RuntimeSpectralSampling_JSON(&out->sampling);
    fprintf(f,"  \"spectral_sampling\":%s,\n",json_object_to_json_string_ext(sampling,JSON_C_TO_STRING_PLAIN));
    json_object_put(sampling);
    fprintf(f,"  \"spectral_execution\":{\"route\":\"local_headless\","
          "\"film_schedule\":\"serial_scanlines\",\"film_threads\":1,"
          "\"frames\":1,\"temporal_passes\":%d,\"denoise\":false,"
          "\"tile_checkpoint_resume\":false,\"worker_supported\":false,"
          "\"editor_supported\":%s,\"map_cache_scope\":\"process\"},\n", p->sampling.enabled?out->sampling.passes:1, p->map.enabled ? "true" : "false");
    if (rendered)
        fprintf(f, "  \"spectral_center_pixel_xyz\":[%.17g,%.17g,%.17g],\n", out->center_pixel_xyz.x,
                out->center_pixel_xyz.y, out->center_pixel_xyz.z);
    if (p->pixel_grid > 1)
        fprintf(f, "  \"spectral_pixel_grid\":%d,\n", p->pixel_grid);
    if (p->exact_surface_observer)
        fprintf(f, "  \"spectral_surface_observer\":\"deposit_wavelength\",\n");
    if (rendered && p->strip_count) {
        fprintf(f, "  \"spectral_linear_strip\":{\"x\":%d,\"y\":%d,\"xyz\":[", p->strip_x, p->strip_y);
        for (int i = 0; i < p->strip_count; ++i) {
            Vec3 xyz = out->strip_xyz[i];
            fprintf(f, "%s[%.17g,%.17g,%.17g]", i ? "," : "", xyz.x, xyz.y, xyz.z);
        }
        fprintf(f, "]},\n");
    }
    if (rendered) write_probe(f, p, out);
    if (p->diffuse.enabled) {
        fprintf(f, "  \"spectral_diffuse_bounce\":{\"estimator\":\"finite_panel_midpoint\","
            "\"grid\":%d,\"indirect_enabled\":%s,\"source\":\"%s\"",
            p->diffuse.grid, p->diffuse.indirect ? "true" : "false",
            p->diffuse.sky_grid ? "constant_sky_cap" : "isotropic_point");
        if (p->diffuse.sky_grid)
            fprintf(f, ",\"sky_grid\":%d,\"sky_half_angle_deg\":%.17g,\"sky_axis\":[%.17g,%.17g,%.17g]",
                p->diffuse.sky_grid, p->diffuse.sky_half_angle_deg, p->diffuse.sky_axis.x,
                p->diffuse.sky_axis.y, p->diffuse.sky_axis.z);
        fprintf(f, "},\n");
        if (!p->map.source_count) return;
    }
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
