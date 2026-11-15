#include "render/runtime_spectral_scene_profile.h"
#include "render/runtime_spectral_surface_path.h"
#include "app/ray_tracing_sha256.h"
#include "spectral_profile_json_fields.h"
#include <stdlib.h>
#include <stdio.h>

/* Generated emitter spectra use the same piecewise-linear preparation, launch
 * and direct-light quadrature as measured tables. */
static bool emitter_model(json_object *obj, RuntimeSpectralLamp *lamp) {
    const char *kind;
    if (!RayTracingJsonGetString(obj, "type", &kind)) return false;
    double watts;
    if (!number(json_object_object_get(obj, "visible_watts"), 0, 1e12, &watts)) return false;
    double temperature = 0, center[4], sigma[4], weight[4];
    int peaks = 0;
    if (!strcmp(kind, "blackbody")) {
        lamp->spectrum_model = "blackbody";
        if (!ONLY(obj, "type", "visible_watts", "kelvin") ||
            !number(json_object_object_get(obj, "kelvin"), 500, 100000, &temperature))
            return false;
    } else if (!strcmp(kind, "gaussian_mixture")) {
        lamp->spectrum_model = "gaussian_mixture";
        json_object *a = json_object_object_get(obj, "peaks");
        if (!ONLY(obj, "type", "visible_watts", "peaks") || !json_object_is_type(a, json_type_array) ||
            json_object_array_length(a) < 1 || json_object_array_length(a) > 4)
            return false;
        peaks = (int)json_object_array_length(a);
        for (int j = 0; j < peaks; ++j) {
            json_object *v = json_object_array_get_idx(a, j);
            if (!json_object_is_type(v, json_type_array) || json_object_array_length(v) != 3 ||
                !number(json_object_array_get_idx(v, 0), 380, 780, &center[j]) ||
                !number(json_object_array_get_idx(v, 1), 0.01, 400, &sigma[j]) ||
                !number(json_object_array_get_idx(v, 2), 0, 1e12, &weight[j]))
                return false;
        }
    } else if (!strcmp(kind, "rgb_approximation")) {
        lamp->spectrum_model = "rgb_approximation";
        Vec3 rgb;
        if (!ONLY(obj, "type", "visible_watts", "linear_rgb") ||
            !vector(json_object_object_get(obj, "linear_rgb"), &rgb) ||
            fmin(rgb.x, fmin(rgb.y, rgb.z)) < 0 || fmax(rgb.x, fmax(rgb.y, rgb.z)) > 1)
            return false;
        peaks = 3;
        center[0] = 610;
        center[1] = 545;
        center[2] = 455;
        weight[0] = rgb.x;
        weight[1] = rgb.y;
        weight[2] = rgb.z;
        sigma[0] = sigma[1] = sigma[2] = 20;
    } else
        return false;
    return RuntimeSpectralLamp_PrepareModel(lamp, watts, temperature, peaks, center, sigma, weight);
}
static bool optics(json_object *s, RuntimeSpectralPencil *out) {
    json_object *v;
    out->exterior_ior = 1;
    out->display_sensor = -1;
    if (json_object_object_get_ex(s, "exterior_ior", &v) && !number(v, 1, 3, &out->exterior_ior))
        return false;
    if (json_object_object_get_ex(s, "glass_roughness", &v) && !number(v, 0, 1, &out->glass_roughness))
        return false;
    if (json_object_object_get_ex(s, "receiver_reflectance", &v) && !bounded_curve(v, &out->reflectance))
        return false;
    if (json_object_object_get_ex(s, "receiver_texture", &v)) {
        const char *type;
        if (!RayTracingJsonGetString(s, "receiver_texture", &type) ||
            strcmp(type, "scene_rgb_approximation"))
            return false;
        out->use_scene_texture = true;
    }
    if (json_object_object_get_ex(s, "sensors", &v)) {
        if (!json_object_is_type(v, json_type_array) || json_object_array_length(v) < 1 ||
            json_object_array_length(v) > 4)
            return false;
        out->sensor_count = (int)json_object_array_length(v);
        for (int i = 0; i < out->sensor_count; ++i) {
            json_object *sensor = json_object_array_get_idx(v, i);
            const char *kind;
            if (!ONLY(sensor, "type", "response") || !RayTracingJsonGetString(sensor, "type", &kind) ||
                !bounded_curve(json_object_object_get(sensor, "response"), &out->sensors[i].response))
                return false;
            out->sensors[i].quantum_efficiency = !strcmp(kind, "quantum_efficiency");
            if (!out->sensors[i].quantum_efficiency && strcmp(kind, "radiometric")) return false;
        }
    }
    return !json_object_object_get_ex(s, "display_sensor", &v) ||
           integer(s, "display_sensor", 0, out->sensor_count - 1, &out->display_sensor);
}
static bool parse_map(json_object *obj, RuntimeSpectralMapConfig *m) {
    if (!ONLY(obj, "enabled", "photons_per_source", "bins", "seed", "absorption_per_m", "sources", "gather", "scattered_support_m", "scattered_neighbors"))
        return false;
    json_object *gather;
    if (json_object_object_get_ex(obj, "gather", &gather)) {
        if (!json_object_is_type(gather, json_type_string)) return false;
        const char *name = json_object_get_string(gather);
        if (strlen(name) != (size_t)json_object_get_string_len(gather)) return false;
        m->adaptive_scattered = !strcmp(name, "adaptive_scattered");
        if (!m->adaptive_scattered && strcmp(name, "fixed")) return false;
    }
    json_object *support=NULL,*neighbors=NULL;
    bool has_support=json_object_object_get_ex(obj,"scattered_support_m",&support);
    bool has_neighbors=json_object_object_get_ex(obj,"scattered_neighbors",&neighbors);
    if ((has_support || has_neighbors) && (!m->adaptive_scattered || !has_support)) return false;
    if (has_support && !number(support,1e-6,1,&m->scattered_support_m)) return false;
    m->scattered_neighbors=16;
    if (has_neighbors && !integer(obj,"scattered_neighbors",4,64,&m->scattered_neighbors)) return false;
    json_object *on = json_object_object_get(obj, "enabled"),
                *lamps = json_object_object_get(obj, "sources");
    int seed;
    if (!json_object_is_type(on, json_type_boolean) ||
        !integer(obj, "photons_per_source", 1, 262144, &m->samples) ||
        !integer(obj, "bins", 1, SPECTRAL_MAP_BINS, &m->bins) || !integer(obj, "seed", 0, INT_MAX, &seed) ||
        !json_object_is_type(lamps, json_type_array) || json_object_array_length(lamps) < 1 ||
        json_object_array_length(lamps) > SPECTRAL_MAP_LIGHTS)
        return false;
    json_object *absorption = json_object_object_get(obj, "absorption_per_m");
    if (json_object_is_type(absorption, json_type_array)) {
        if (!spectrum_pairs(absorption, m->absorption_curve, SPECTRAL_MAP_KNOTS, &m->absorption_knots) ||
            m->absorption_knots < 2 || m->absorption_curve[0][0] != 380 ||
            m->absorption_curve[m->absorption_knots - 1][0] != 780)
            return false;
    } else if (!number(absorption, 0, 1e12, &m->absorption_per_m))
        return false;
    m->enabled = json_object_get_boolean(on);
    m->seed = (unsigned)seed;
    m->source_count = (int)json_object_array_length(lamps);
    for (int i = 0; i < m->source_count; ++i) {
        json_object *lamp = json_object_array_get_idx(lamps, i);
        RuntimeSpectralLamp *l = &m->sources[i];
        double pairs[SPECTRAL_MAP_KNOTS][2];
        if (!ONLY(lamp, "origin_m", "direction", "aperture_m", "lines", "continuum", "spectrum",
                  "angular_radius_deg") ||
            !vector(json_object_object_get(lamp, "origin_m"), &l->origin) ||
            !vector(json_object_object_get(lamp, "direction"), &l->direction) ||
            vec3_length(l->direction) <= 1e-9 ||
            !number(json_object_object_get(lamp, "aperture_m"), 1e-8, 1e4, &l->aperture_m))
            return false;
        l->direction = vec3_normalize(l->direction);
        json_object *angle, *model;
        if (json_object_object_get_ex(lamp, "angular_radius_deg", &angle)) {
            if (!number(angle, 0, 45, &l->angular_radius_rad)) return false;
            l->angular_radius_rad *= M_PI / 180;
        }
        if (json_object_object_get_ex(lamp, "spectrum", &model)) {
            if (json_object_object_get_ex(lamp, "lines", NULL) ||
                json_object_object_get_ex(lamp, "continuum", NULL) || !emitter_model(model, l))
                return false;
            continue;
        }
        if (!spectrum_pairs(json_object_object_get(lamp, "lines"), pairs, RUNTIME_SPECTRAL_PENCIL_MAX_LINES,
                            &l->nlines))
            return false;

        for (int j = 0; j < l->nlines; ++j)
            l->lines[j] = (RuntimeSpectralLine){pairs[j][0], pairs[j][1]};
        if (!spectrum_pairs(json_object_object_get(lamp, "continuum"), pairs, SPECTRAL_MAP_KNOTS,
                            &l->nknots) ||
            l->nknots == 1 || (!l->nlines && !l->nknots))
            return false;
        for (int j = 0; j < l->nknots; ++j)
            l->knots[j] = (RuntimeSpectralKnot){pairs[j][0], pairs[j][1]};
    }
    const char *json = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    return json && ray_tracing_sha256_bytes(json, strlen(json), m->config_hash);
}

static bool volume_config(json_object *obj, const char *dir, RuntimeSpectralVolumeConfig *v) {
    const char *kind;
    if (!RayTracingJsonGetString(obj, "kind", &kind)) return false;
    if (!strcmp(kind, "homogeneous")) {
        if (!ONLY(obj, "kind", "bounds_min_m", "bounds_max_m", "sigma_a_per_m", "sigma_s_per_m", "g", "mie_phase_path",
                  "cell_m", "reconstruction", "phase_tolerance", "camera_spatial_cache", "reconstruction_cell_m", "higher_scatter_support_m", "local_refinement_v1") ||
            !vector(json_object_object_get(obj, "bounds_min_m"), &v->lo) ||
            !vector(json_object_object_get(obj, "bounds_max_m"), &v->hi) ||
            !(v->lo.x < v->hi.x && v->lo.y < v->hi.y && v->lo.z < v->hi.z))
            return false;
        v->kind = SPECTRAL_VOLUME_BOX;
    } else {
        const char *source;
        if (!ONLY(obj, "kind", "source_path", "frame_index", "sigma_a_per_m", "sigma_s_per_m", "g",
                  "cell_m", "reconstruction", "phase_tolerance", "camera_spatial_cache", "reconstruction_cell_m", "higher_scatter_support_m", "local_refinement_v1") ||
            !RayTracingJsonGetString(obj, "source_path", &source) || !*source ||
            strlen(source) !=
                (size_t)json_object_get_string_len(json_object_object_get(obj, "source_path")) ||
            !RayTracingResolveExistingRequestPath(dir, source, v->path, sizeof(v->path)) ||
            !integer(obj, "frame_index", 0, INT_MAX, &v->frame_index))
            return false;
        if (!strcmp(kind, "vf3d_density"))
            v->kind = SPECTRAL_VOLUME_VF3D;
        else if (!strcmp(kind, "water_heightfield"))
            v->kind = SPECTRAL_VOLUME_WATER;
        else
            return false;
    }
    v->reconstruction = SPECTRAL_RECONSTRUCTION_BOX;
    json_object *mode = json_object_object_get(obj, "reconstruction");
    if (mode) {
        if (json_object_get_type(mode) != json_type_string) return false;
        const char *name = json_object_get_string(mode);
        if (strlen(name) != (size_t)json_object_get_string_len(mode)) return false;
        if (!strcmp(name, "continuous")) v->reconstruction = SPECTRAL_RECONSTRUCTION_CONTINUOUS;
        else if (!strcmp(name, "adaptive")) v->reconstruction = SPECTRAL_RECONSTRUCTION_ADAPTIVE;
        else if (!strcmp(name, "kernel")) v->reconstruction = SPECTRAL_RECONSTRUCTION_KERNEL;
        else if (strcmp(name, "box")) return false;
    }
    json_object *cache = json_object_object_get(obj, "camera_spatial_cache");
    if (cache && !json_object_is_type(cache, json_type_boolean)) return false;
    v->disable_camera_cache = cache && !json_object_get_boolean(cache);
    v->phase_tolerance = .01;
    json_object *tolerance = json_object_object_get(obj, "phase_tolerance");
    if (tolerance && (!v->reconstruction || !number(tolerance, 0, .05, &v->phase_tolerance))) return false;
    RuntimeSpectralCurve *curves[] = {&v->absorption, &v->scattering};
    const char *curve_keys[] = {"sigma_a_per_m", "sigma_s_per_m"};
    for (int i = 0; i < 2; ++i) {
        RuntimeSpectralCurve *c = curves[i];
        if (!spectrum_pairs(json_object_object_get(obj, curve_keys[i]), c->points, SPECTRAL_MAP_KNOTS,
                            &c->count) ||
            c->count < 2 || c->points[0][0] != 380 || c->points[c->count - 1][0] != 780)
            return false;
    }
    json_object *grid = NULL;
    bool has_grid = json_object_object_get_ex(obj, "reconstruction_cell_m", &grid);
    if (has_grid && (!v->reconstruction || !number(grid, 1e-6, 1e6, &v->reconstruction_cell_m))) return false;
    json_object *higher=NULL,*local=NULL;
    bool has_higher=json_object_object_get_ex(obj,"higher_scatter_support_m",&higher);
    bool has_local=json_object_object_get_ex(obj,"local_refinement_v1",&local);
    if ((has_higher || has_local) && v->reconstruction!=SPECTRAL_RECONSTRUCTION_KERNEL) return false;
    if (v->reconstruction==SPECTRAL_RECONSTRUCTION_KERNEL && has_grid) return false;
    if (has_higher && !number(higher,1e-6,1e6,&v->higher_scatter_support_m)) return false;
    if (has_local && (!local || !ONLY(local,"bounds_min_m","bounds_max_m","step_m") ||
        !vector(json_object_object_get(local,"bounds_min_m"),&v->local_refinement_lo) ||
        !vector(json_object_object_get(local,"bounds_max_m"),&v->local_refinement_hi) ||
        !number(json_object_object_get(local,"step_m"),1e-6,1e6,&v->local_refinement_step_m) ||
        !(v->local_refinement_lo.x<v->local_refinement_hi.x &&
          v->local_refinement_lo.y<v->local_refinement_hi.y &&
          v->local_refinement_lo.z<v->local_refinement_hi.z))) return false;
    const char *json = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    json_object *mie = NULL, *hg = NULL;
    bool has_mie = json_object_object_get_ex(obj, "mie_phase_path", &mie);
    bool has_hg = json_object_object_get_ex(obj, "g", &hg);
    if (has_mie) {
        const char *path;
        if (has_hg || v->reconstruction != SPECTRAL_RECONSTRUCTION_BOX ||
            !RayTracingJsonGetString(obj, "mie_phase_path", &path) || !*path ||
            strlen(path) != (size_t)json_object_get_string_len(mie) ||
            !RayTracingResolveExistingRequestPath(dir, path, v->mie_phase_path, sizeof(v->mie_phase_path)))
            return false;
    } else if (!number(hg, -.99, .99, &v->g)) {
        return false;
    }
    return
           number(json_object_object_get(obj, "cell_m"), 1e-6, 1e6, &v->cell_m) &&
           (!has_grid || (v->reconstruction_cell_m <= v->cell_m && v->reconstruction_cell_m >= .5 * v->cell_m)) &&
           (!has_higher || (v->higher_scatter_support_m>=v->cell_m && v->higher_scatter_support_m<=64*v->cell_m)) &&
           (!has_local || v->local_refinement_step_m<=v->cell_m) && json &&
           ray_tracing_sha256_bytes(json, strlen(json), v->profile_hash);
}
#include "spectral_scene_surface_profile.h"

static bool parse_diffuse(json_object *s, int64_t pixels, bool mixed, RuntimeSpectralPencil *out) {
    json_object *bounce = json_object_object_get(s, "diffuse_bounce_v1");
    json_object *indirect = json_object_object_get(bounce, "indirect");
    json_object *sky = NULL;
    bool use_sky = json_object_object_get_ex(s, "sky", &sky);
    double lines[RUNTIME_SPECTRAL_PENCIL_MAX_LINES][2];
    int count;
    if (!ONLY(bounce, "panel_id", "panel_reflectance", "grid", "indirect") ||
        !json_object_is_type(indirect, json_type_boolean) ||
        !object_id(bounce, "panel_id", out->diffuse.panel_id) ||
        !object_id(s, "receiver_id", out->receiver_id) ||
        !strcmp(out->diffuse.panel_id, out->receiver_id) ||
        !bounded_curve(json_object_object_get(bounce, "panel_reflectance"), &out->diffuse.panel_reflectance) ||
        !bounded_curve(json_object_object_get(s, "receiver_reflectance"), &out->reflectance) ||
        !integer(bounce, "grid", 4, 64, &out->diffuse.grid) ||
        !number(json_object_object_get(s, "exposure"), 1e-20, 1e12, &out->exposure))
        return false;
    json_object *source_lines = json_object_object_get(s, "lines");
    int panel_nodes = out->diffuse.grid * out->diffuse.grid;
    if (pixels * panel_nodes > 4194304) return false;
    if (mixed && (!use_sky || !out->fresnel_camera_samples || out->sampling.enabled ||
                  !strcmp(out->diffuse.panel_id, out->glass_id))) return false;
    if (use_sky) {
        if (json_object_object_get_ex(s, "origin_m", NULL) ||
            json_object_object_get_ex(s, "lines", NULL) ||
            !ONLY(sky, "axis", "half_angle_deg", "grid", "radiance_lines") ||
            !vector(json_object_object_get(sky, "axis"), &out->diffuse.sky_axis) ||
            vec3_length(out->diffuse.sky_axis) < 1e-12 ||
            !number(json_object_object_get(sky, "half_angle_deg"), 1, 90,
                    &out->diffuse.sky_half_angle_deg) ||
            !integer(sky, "grid", 4, 64, &out->diffuse.sky_grid) ||
            (!mixed &&
             (pixels + panel_nodes) * out->diffuse.sky_grid * out->diffuse.sky_grid > 4194304))
            return false;
        out->diffuse.sky_axis = vec3_normalize(out->diffuse.sky_axis);
        source_lines = json_object_object_get(sky, "radiance_lines");
    } else if (!vector(json_object_object_get(s, "origin_m"), &out->origin_m)) {
        return false;
    }
    if (!spectrum_pairs(source_lines, lines, RUNTIME_SPECTRAL_PENCIL_MAX_LINES, &count) || !count)
        return false;
    if (mixed) {
        int64_t directions = out->diffuse.sky_grid * out->diffuse.sky_grid;
        int grid = out->pixel_grid > 0 ? out->pixel_grid : 1;
        int64_t camera_paths = pixels * out->fresnel_camera_samples * grid * grid;
        if (((camera_paths + panel_nodes) * directions + camera_paths * panel_nodes) * count > 536870912)
            return false;
        /* Match exact line atoms without changing map bands or cache identity. */
        for (int i = 0; i < count; ++i) {
            bool found = false;
            for (int j = 0; j < out->map.source_count; ++j)
                for (int k = 0; k < out->map.sources[j].nlines; ++k)
                    if (out->map.sources[j].lines[k].nm == lines[i][0]) found = true;
            if (!found) return false;
        }
    }
    out->diffuse.enabled = true;
    out->diffuse.indirect = json_object_get_boolean(indirect);
    out->count = count;
    for (int i = 0; i < count; ++i) {
        out->lines[i].vacuum_nm = lines[i][0];
        if (use_sky)
            out->diffuse.sky_radiance[i] = lines[i][1];
        else
            out->lines[i].watts = lines[i][1];
    }
    return true;
}

bool RuntimeSpectralProfile_ParseFilm(json_object *s,const char *dir,int width,int height,RuntimeSpectralPencil *out) {
    if (json_object_object_get_ex(s, "surface_path_v1", NULL))
        return surface_path_parse(s, width,
                                  height, out);
    if (!s) return true; /* Ordinary V3 resolves its saved/default profile during frame preparation. */
    bool mixed = json_object_object_get_ex(s, "surface_map", NULL);
    bool diffuse = json_object_object_get_ex(s, "diffuse_bounce_v1", NULL);
    if (mixed || !diffuse) {
        if (!RuntimeSpectralProfile_Parse(s, dir, out)) return false;
    } else if (!ONLY(s, "diffuse_bounce_v1", "selected_pixel", "receiver_id", "receiver_reflectance",
                     "origin_m", "lines", "sky", "exposure", "display_transform")) {
        return false;
    }
    json_object *display = NULL;
    if (json_object_object_get_ex(s, "display_transform", &display)) {
        if (out->sampling.enabled || !json_object_is_type(display, json_type_string) ||
            json_object_get_string_len(display) != (int)sizeof("spectral_soft_v1") - 1 ||
            strcmp(json_object_get_string(display), "spectral_soft_v1")) return false;
        out->soft_display = true;
    }
    if (json_object_object_get_ex(s, "pixel_grid", NULL) &&
        (!out->map.source_count || out->sampling.enabled ||
         !integer(s, "pixel_grid", 1, 4, &out->pixel_grid))) return false;
    json_object *observer = NULL, *strip = NULL;
    if (json_object_object_get_ex(s, "surface_observer", &observer)) {
        if (!out->map.enabled || out->sampling.enabled || !json_object_is_type(observer, json_type_string) ||
            json_object_get_string_len(observer) != (int)sizeof("deposit_wavelength") - 1 ||
            strcmp(json_object_get_string(observer), "deposit_wavelength")) return false;
        out->exact_surface_observer = true;
    }
    if (json_object_object_get_ex(s, "linear_strip", &strip)) {
        if (!out->map.source_count || out->sampling.enabled || !ONLY(strip, "x", "y", "count") ||
            !integer(strip, "x", 0, width - 1, &out->strip_x) ||
            !integer(strip, "y", 0, height - 1, &out->strip_y) ||
            !integer(strip, "count", 1, 512, &out->strip_count) ||
            out->strip_x + out->strip_count > width) return false;
    }
    if (json_object_object_get_ex(s, "sky", NULL) && !diffuse) return false;
    if (diffuse) {
        int64_t pixels = (int64_t)width * height;
        if (!parse_diffuse(s, pixels, mixed, out)) return false;
    }
    if (json_object_object_get_ex(s, "selected_pixel", NULL)) {
        json_object *pixel = json_object_object_get(s, "selected_pixel");
        if ((!out->diffuse.enabled && (!out->map.source_count || out->sampling.enabled)) ||
            !ONLY(pixel, "x", "y") ||
            !integer(pixel, "x", 0, width - 1, &out->probe.x) ||
            !integer(pixel, "y", 0, height - 1, &out->probe.y))
            return false;
        out->probe.enabled = true;
    }
    return true;
}

bool RuntimeSpectralProfile_Parse(json_object *s, const char *dir, RuntimeSpectralPencil *out) {
    if (json_object_object_get_ex(s,"surface_path_v1",NULL))
        return surface_path_parse(s,0,0,out);
    if (ONLY(s,"quality_profile")) {
        const char *quality=NULL;
        if (!RayTracingJsonGetString(s,"quality_profile",&quality)) return false;
        if (!strcmp(quality,"surface_path_v1")) {RuntimeSpectralSurfacePath_Defaults(out);return true;}
        if (strcmp(quality,"legacy_direct_v1")) return false;
        out->legacy_direct=true;return true;
    }

    json_object *map = NULL;
    bool mapping = json_object_object_get_ex(s, "surface_map", &map);
    json_object *sampling_value;
    if(json_object_object_get_ex(s,"sampling",&sampling_value) && !sampling_value) return false;
    if (!(mapping
              ? ONLY(s, "glass_id", "receiver_id", "glass", "constant_ior", "receiver_radius_m", "exposure",
                     "surface_map", "exterior_ior", "glass_roughness", "receiver_reflectance",
                     "receiver_texture", "sensors", "display_sensor", "participating_volume_v1", "sampling", "selected_pixel", "smooth_glass_transport", "glass_camera_samples", "diffuse_bounce_v1", "sky", "display_transform", "surface_observer", "linear_strip", "pixel_grid")
              : ONLY(s, "glass_id", "receiver_id", "glass", "constant_ior", "origin_m", "direction",
                     "receiver_radius_m", "exposure", "lines", "display_transform")))
        return false;
    bool no_glass = mapping && json_object_object_get(s, "participating_volume_v1") &&
                    !json_object_object_get_ex(s, "glass_id", NULL);
    const char *glass = NULL;
    if (no_glass && (json_object_object_get_ex(s, "glass", NULL) ||
                     json_object_object_get_ex(s, "glass_roughness", NULL) ||
                     json_object_object_get_ex(s, "constant_ior", NULL)))
        return false;
    if ((!no_glass && !object_id(s, "glass_id", out->glass_id)) ||
        !object_id(s, "receiver_id", out->receiver_id) || !strcmp(out->glass_id, out->receiver_id) ||
        (!no_glass && !RayTracingJsonGetString(s, "glass", &glass)) ||
        (!mapping && (!vector(json_object_object_get(s, "origin_m"), &out->origin_m) ||
                      !vector(json_object_object_get(s, "direction"), &out->direction) ||
                      vec3_length(out->direction) < 1e-12)) ||
        !number(json_object_object_get(s, "receiver_radius_m"), 1e-8, 1e4, &out->receiver_radius_m) ||
        !number(json_object_object_get(s, "exposure"), 1e-20, 1e12, &out->exposure))
        return false;
    if (!mapping) out->direction = vec3_normalize(out->direction);
    out->use_n_bk7 = glass && strcmp(glass, "schott_n_bk7") == 0;
    json_object *ior = json_object_object_get(s, "constant_ior");
    if (no_glass)
        out->constant_ior = 1;
    else if (out->use_n_bk7 ? ior != NULL
                            : (strcmp(glass, "constant") || !number(ior, 1, 5, &out->constant_ior)))
        return false;
    if (mapping) {
        json_object *volume;
        if (!optics(s, out) || !parse_map(map, &out->map)) return false;
        if (json_object_object_get_ex(s, "smooth_glass_transport", NULL)) {
            const char *mode;
            if (no_glass || out->glass_roughness != 0 || !out->map.enabled ||
                !RayTracingJsonGetString(s, "smooth_glass_transport", &mode) ||
                strcmp(mode, "fresnel")) return false;
            out->fresnel_camera_samples = 16;
        }
        if (json_object_object_get_ex(s, "glass_camera_samples", NULL) &&
            (!out->fresnel_camera_samples ||
             !integer(s, "glass_camera_samples", 1, 64, &out->fresnel_camera_samples)))
            return false;
        return RuntimeSpectralSampling_Parse(json_object_object_get(s, "sampling"), &out->sampling) &&
               (!out->map.scattered_support_m || (out->map.scattered_support_m>=out->receiver_radius_m && out->map.scattered_support_m<=64*out->receiver_radius_m)) &&
               (!json_object_object_get_ex(s, "participating_volume_v1", &volume) ||
                (volume_config(volume, dir, &out->volume) && out->map.enabled));
    }
    json_object *a = json_object_object_get(s, "lines");
    if (!json_object_is_type(a, json_type_array)) return false;
    size_t count = json_object_array_length(a);
    if (!count || count > RUNTIME_SPECTRAL_PENCIL_MAX_LINES) return false;
    for (size_t i = 0; i < count; ++i) {
        RuntimeSpectralPencilLine *line = &out->lines[i];
        json_object *l = json_object_array_get_idx(a, i);
        if (!ONLY(l, "vacuum_nm", "watts", "absorption_per_m") ||
            !number(json_object_object_get(l, "vacuum_nm"), RUNTIME_SPECTRAL_MIN_NM,
                    RUNTIME_SPECTRAL_MAX_NM, &line->vacuum_nm) ||
            !number(json_object_object_get(l, "watts"), 0, 1e12, &line->watts) ||
            !number(json_object_object_get(l, "absorption_per_m"), 0, 1e12, &line->absorption_per_m))
            return false;
        if (i && line->vacuum_nm <= out->lines[i - 1].vacuum_nm) return false;
    }
    out->count = (int)count;
    return true;
}

/* Both editor jobs and the ordinary renderer use the document-relative profile.
 * Surface-only jobs must also work with the optional volume asset offline. */
bool RuntimeSpectralProfile_FromScene(json_object *doc,const char *path,bool surface_only,
                                      RuntimeSpectralPencil *out) {
    json_object *ray=json_object_object_get(json_object_object_get(doc,"extensions"),"ray_tracing");
    json_object *profile=NULL;
    if (!json_object_object_get_ex(ray,"spectral_transport_v1",&profile)) return true;
    const char *legacy=NULL;
    if(RayTracingJsonGetString(profile,"quality_profile",&legacy) && !strcmp(legacy,"legacy_direct_v1"))
        return RuntimeSpectralProfile_Parse(profile,".",out);
    json_object *lights=json_object_object_get(doc,"lights");
    if (lights && !json_object_is_type(lights,json_type_array)) return false;
    json_object *units=json_object_object_get(doc,"unit_system");
    double scale;
    if (!json_object_is_type(units,json_type_string) || strcmp(json_object_get_string(units),"meters") ||
        !number(json_object_object_get(doc,"world_scale"),1,1,&scale) ||
        !json_object_is_type(profile,json_type_object)) return false;
    char dir[4096];
    if (!RayTracingCopyString(dir,sizeof(dir),path)) return false;
    char *slash=strrchr(dir,'/');
    if (slash) slash[1]=0; else strcpy(dir,".");
    json_object *selected=json_object_new_object();
    if (!selected) return false;
    json_object_object_foreach(profile,key,value) {
        if (!surface_only || strcmp(key,"participating_volume_v1"))
            json_object_object_add(selected,key,json_object_get(value));
    }
    bool surface=json_object_object_get_ex(selected,"surface_path_v1",NULL);
    bool ok=surface ? RuntimeSpectralProfile_Parse(selected,dir,out) :
        !json_object_object_get_ex(selected,"selected_pixel",NULL) &&
        !json_object_object_get_ex(selected,"diffuse_bounce_v1",NULL) &&
        !json_object_object_get_ex(selected,"sky",NULL) &&
        !json_object_object_get_ex(selected,"display_transform",NULL) &&
        !json_object_object_get_ex(selected,"surface_observer",NULL) &&
        !json_object_object_get_ex(selected,"linear_strip",NULL) &&
        !json_object_object_get_ex(selected,"pixel_grid",NULL) &&
        RuntimeSpectralProfile_Parse(selected,dir,out) && (out->map.enabled || out->legacy_direct || out->surface_path.enabled);
    if (lights && json_object_array_length(lights) && (!out->legacy_direct && (!out->surface_path.enabled || !out->surface_path.native_sources))) ok=false;
    json_object_put(selected);
    return ok;
}
