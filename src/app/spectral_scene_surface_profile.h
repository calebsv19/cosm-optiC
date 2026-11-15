/* Included by the spectral parser to share its strict typed field helpers. */
static bool surface_glass_parse(json_object *m, RuntimeSpectralPencil *out) {
    const char *model = "schott_n_bk7";
    if (out->glass_id[0] || !ONLY(m, "object_id", "model", "glass", "ior", "absorption_per_m") ||
        !object_id(m, "object_id", out->glass_id) ||
        (json_object_object_get_ex(m, "glass", NULL) && !RayTracingJsonGetString(m, "glass", &model)) ||
        (json_object_object_get_ex(m, "absorption_per_m", NULL) &&
         !number(json_object_object_get(m, "absorption_per_m"), 0, 10000, &out->surface_path.caustic_absorption)))
        return false;
    if (!strcmp(model, "schott_n_bk7")) {
        out->use_n_bk7 = true;
        return !json_object_object_get_ex(m, "ior", NULL);
    }
    return !strcmp(model, "constant") &&
        number(json_object_object_get(m, "ior"), 1.0001, 3, &out->constant_ior);
}

static bool surface_sampling_choice(json_object *options,const char *key,
                                    const char *base,const char *enhanced,bool *enabled) {
    json_object *value=NULL;
    if (!json_object_object_get_ex(options,key,&value)) return true;
    if (!json_object_is_type(value,json_type_string)) return false;
    const char *s=json_object_get_string(value);
    if (strlen(s)!=(size_t)json_object_get_string_len(value)) return false;
    if (!strcmp(s,enhanced)) {*enabled=true;return true;}
    if (!strcmp(s,base)) {*enabled=false;return true;}
    return false;
}

static bool surface_volume_direction_choice(json_object *options,RuntimeSpectralSurfacePathConfig *p) {
    json_object *v=NULL;
    p->volume_rectangle_mis=false;
    if(json_object_object_get_ex(options,"volume_direction_sampling",&v) &&
       json_object_is_type(v,json_type_string) && json_object_get_string_len(v)==13 &&
       !strcmp(json_object_get_string(v),"rectangle_mis")) {
        p->volume_rectangle_mis=true;p->volume_light_mis=false;p->volume_light_only=false;return true;
    }
    if(json_object_object_get_ex(options,"volume_direction_sampling",&v) &&
       json_object_is_type(v,json_type_string) && json_object_get_string_len(v)==10 &&
       !strcmp(json_object_get_string(v),"light_only")) {
        p->volume_light_only=true;p->volume_light_mis=false;return true;
    }
    p->volume_light_only=false;
    return surface_sampling_choice(options,"volume_direction_sampling","phase","light_mis",&p->volume_light_mis);
}

static bool surface_fresnel_choice(json_object *options,RuntimeSpectralSurfacePathConfig *p) {
    json_object *v=NULL;
    if(!json_object_object_get_ex(options,"camera_fresnel",&v))return true;
    if(json_object_is_type(v,json_type_string) && json_object_get_string_len(v)==13 &&
       !strcmp(json_object_get_string(v),"split_bounded")) {
        p->glass_split_first=p->glass_split_bounded=true;return true;
    }
    if(json_object_is_type(v,json_type_string) && json_object_get_string_len(v)==17 &&
       !strcmp(json_object_get_string(v),"split_camera_exit")) {
        p->glass_split_first=p->glass_split_camera_exit=true;return true;
    }
    return surface_sampling_choice(options,"camera_fresnel","roulette","split_first",&p->glass_split_first);
}
static bool surface_caustics_parse(json_object *q, RuntimeSpectralPencil *out) {
    RuntimeSpectralSurfacePathConfig *p = &out->surface_path;
    json_object *options = NULL;
    bool has_options = json_object_object_get_ex(q, "caustics", &options);
    if (!out->glass_id[0]) return !has_options;
    if (p->native_sources || p->sky) return false;
    p->caustic_passes = 4;
    p->caustic_photons_per_pass = 16384;
    p->caustic_seed = 19;
    p->caustic_bands = 48;
    if (!has_options) return true;
    if (!ONLY(options, "passes", "photons_per_pass", "seed", "bins", "radius_m", "photon_launch", "camera_fresnel") ||
        !surface_sampling_choice(options,"photon_launch","cosine","glass_guided",&p->caustic_guided) ||
        !surface_fresnel_choice(options,p) ||
        (json_object_object_get_ex(options, "passes", NULL) && !integer(options, "passes", 0, 64, &p->caustic_passes)) ||
        (json_object_object_get_ex(options, "photons_per_pass", NULL) &&
         !integer(options, "photons_per_pass", 256, 262144, &p->caustic_photons_per_pass)) ||
        (json_object_object_get_ex(options, "seed", NULL) && !integer(options, "seed", 0, INT_MAX, &p->caustic_seed)) ||
        (json_object_object_get_ex(options, "bins", NULL) && !integer(options, "bins", 8, 128, &p->caustic_bands)) ||
        (json_object_object_get_ex(options, "radius_m", NULL) &&
         !number(json_object_object_get(options, "radius_m"), .00001, 10, &out->receiver_radius_m)))
        return false;
    return (int64_t)p->caustic_passes * p->caustic_photons_per_pass <= 262144;
}
static bool surface_path_parse(json_object *s, int width, int height,
                               RuntimeSpectralPencil *out) {
    RuntimeSpectralSurfacePathConfig *p = &out->surface_path;
    json_object *q = json_object_object_get(s, "surface_path_v1");
    json_object *light = json_object_object_get(q, "light");
    json_object *spectrum = json_object_object_get(light, "spectrum");
    json_object *materials = json_object_object_get(q, "materials");
    json_object *probes = json_object_object_get(q, "probes");
    json_object *display = NULL;
    const char *kind;
    RuntimeSpectralSurfacePath_Defaults(out);
    p->enabled=false;p->knot_count=p->line_count=0;
    p->native_sources=!json_object_object_get_ex(q,"light",NULL);
    p->native_materials=!json_object_object_get_ex(q,"materials",NULL);
    json_object *quality = NULL;
    if (json_object_object_get_ex(s, "quality_profile", &quality) && (!json_object_is_type(quality, json_type_string) ||
        strcmp(json_object_get_string(quality), "surface_path_v1") ||
        json_object_get_string_len(quality) != (int)sizeof("surface_path_v1") - 1)) return false;
    if (!ONLY(s, "surface_path_v1", "exposure", "display_transform", "quality_profile") ||
        !ONLY(q, "samples", "max_depth", "seed", "light", "materials", "probes", "medium", "output_component", "path_trace", "caustics", "linear_components", "camera_sampler", "sample_audit", "medium_sampling", "continuation_roulette", "volume_direction_sampling") ||
        !surface_volume_direction_choice(q,p) ||
        !surface_sampling_choice(q,"continuation_roulette","throughput","fog_survival",&p->fog_survival) ||
        !surface_sampling_choice(q,"medium_sampling","analog","split_first",&p->medium_split_first) ||
        !surface_sampling_choice(q,"camera_sampler","hashed_v1","scrambled_path_v1",&p->camera_qmc) ||
        (json_object_object_get_ex(q, "samples", NULL) && !integer(q, "samples", 1, 8192, &p->samples)) ||
        (json_object_object_get_ex(q, "max_depth", NULL) && !integer(q, "max_depth", 1, 12, &p->max_depth)) ||
        (json_object_object_get_ex(q, "seed", NULL) && !integer(q, "seed", 0, INT_MAX, &p->seed)) ||
        (width > 0 && height > 0 && (int64_t)width * height * p->samples * p->max_depth > INT64_C(1200000000)) ||
        (json_object_object_get_ex(s, "exposure", NULL) &&
         !number(json_object_object_get(s, "exposure"), 1e-20, 1e12, &out->exposure)) ||
        (!p->native_sources && (!RayTracingJsonGetString(light, "type", &kind) ||
        !ONLY(spectrum, "lines", "continuum"))))
        return false;
    if (json_object_object_get_ex(s, "display_transform", &display)) {
        if (!json_object_is_type(display, json_type_string)) return false;
        const char *name = json_object_get_string(display);
        if (strlen(name) != (size_t)json_object_get_string_len(display)) return false;
        if (!strcmp(name, "legacy_clip")) out->soft_display = false;
        else if (strcmp(name, "spectral_soft_v1")) return false;
    }
    json_object *export = NULL;
    if (json_object_object_get_ex(q,"linear_components",&export)) {
        if (!json_object_is_type(export,json_type_boolean)) return false;
        p->linear_components=json_object_get_boolean(export);
    }
    json_object *component = NULL;
    if (json_object_object_get_ex(q, "output_component", &component)) {
        if (!json_object_is_type(component, json_type_string)) return false;
        const char *name = json_object_get_string(component);
        if (strlen(name) != (size_t)json_object_get_string_len(component)) return false;
        const char *choices[] = {"total", "direct", "surface_indirect", "volume", "caustic", "caustic_bounce", "volume_caustic"};
        int i = 0;
        int count = (int)(sizeof(choices) / sizeof(choices[0]));
        while (i < count && strcmp(name, choices[i])) ++i;
        if (i == count) return false;
        p->output_component = i;
    }
    json_object *audit=NULL;
    if(json_object_object_get_ex(q,"sample_audit",&audit)) {
        p->sample_audit_limit=8;
        if(!ONLY(audit,"x","y","limit") ||
           !integer(audit,"x",0,width>0?width-1:4095,&p->sample_audit_x) ||
           !integer(audit,"y",0,height>0?height-1:4095,&p->sample_audit_y) ||
           (json_object_object_get_ex(audit,"limit",NULL) && !integer(audit,"limit",1,SPECTRAL_SAMPLE_AUDIT_LIMIT,&p->sample_audit_limit)))return false;
        p->sample_audit_enabled=true;
    }
    double pairs[32][2];
    json_object *values = NULL;
    if (json_object_object_get_ex(spectrum, "lines", &values)) {
        if (!spectrum_pairs(values, pairs, 16, &p->line_count)) return false;
        for (int i = 0; i < p->line_count; ++i)
            p->lines[i] = (RuntimeSpectralLine){pairs[i][0], pairs[i][1]};
    }
    if (json_object_object_get_ex(spectrum, "continuum", &values)) {
        if (!spectrum_pairs(values, pairs, 32, &p->knot_count) || p->knot_count < 2) return false;
        for (int i = 0; i < p->knot_count; ++i)
            p->knots[i] = (RuntimeSpectralKnot){pairs[i][0], pairs[i][1]};
    }
    if (p->native_sources) {
        p->knot_count=2;p->knots[0]=(RuntimeSpectralKnot){380,1};p->knots[1]=(RuntimeSpectralKnot){780,1};
    } else if (!p->line_count && !p->knot_count) return false;
    if (p->native_sources) {
        /* Native illumination is resolved once against the prepared scene. */
    } else if (!strcmp(kind, "area")) {
        if (!ONLY(light, "type", "origin_m", "axis_u", "axis_v", "width_m", "height_m", "spectrum") ||
            !vector(json_object_object_get(light, "origin_m"), &p->origin) ||
            !vector(json_object_object_get(light, "axis_u"), &p->axis_u) ||
            !vector(json_object_object_get(light, "axis_v"), &p->axis_v) ||
            !number(json_object_object_get(light, "width_m"), .0001, 100, &p->width) ||
            !number(json_object_object_get(light, "height_m"), .0001, 100, &p->height) ||
            vec3_length(p->axis_u) < 1e-9 || vec3_length(p->axis_v) < 1e-9)
            return false;
        p->axis_u = vec3_normalize(p->axis_u);
        p->axis_v = vec3_normalize(p->axis_v);
        if (fabs(vec3_dot(p->axis_u, p->axis_v)) > 1e-8) return false;
        p->normal = vec3_cross(p->axis_u, p->axis_v);
    } else if (!strcmp(kind, "sky_cap")) {
        if (!ONLY(light, "type", "axis", "half_angle_deg", "spectrum") ||
            !vector(json_object_object_get(light, "axis"), &p->sky_axis) ||
            vec3_length(p->sky_axis) < 1e-9 ||
            !number(json_object_object_get(light, "half_angle_deg"), 1, 180, &p->sky_angle))
            return false;
        p->sky = true;
        p->sky_axis = vec3_normalize(p->sky_axis);
    } else {
        return false;
    }
    if (!p->native_materials && (!json_object_is_type(materials, json_type_array) ||
        json_object_array_length(materials) < 1 ||
        json_object_array_length(materials) > SPECTRAL_PATH_MATERIALS)) return false;
    int count = materials ? (int)json_object_array_length(materials) : 0;
    for (int i = 0; i < count; ++i) {
        json_object *m = json_object_array_get_idx(materials, i), *two = NULL;
        const char *model;
        if (!RayTracingJsonGetString(m, "model", &model)) return false;
        if (!strcmp(model, "smooth_glass")) {
            if (!surface_glass_parse(m, out)) return false;
            continue;
        }
        RuntimeSpectralPathMaterial *mat = &p->materials[p->material_count];
        if (!ONLY(m, "object_id", "reflectance", "model", "roughness", "two_sided") ||
            !object_id(m, "object_id", mat->object_id) ||
            !bounded_curve(json_object_object_get(m, "reflectance"), &mat->reflectance)) return false;
        for (int j = 0; j < p->material_count; ++j)
            if (!strcmp(mat->object_id, p->materials[j].object_id)) return false;
        if (!strcmp(model, "ggx")) {
            mat->ggx = true;
            if (!number(json_object_object_get(m, "roughness"), .03, 1, &mat->roughness)) return false;
        } else if (strcmp(model, "diffuse") || json_object_object_get_ex(m, "roughness", NULL)) {
            return false;
        }
        if (json_object_object_get_ex(m, "two_sided", &two)) {
            if (!json_object_is_type(two, json_type_boolean)) return false;
            mat->two_sided = json_object_get_boolean(two);
        }
        ++p->material_count;
    }
    for (int i = 0; i < p->material_count; ++i)
        if (!strcmp(p->materials[i].object_id, out->glass_id)) return false;
    if (probes) {
        if (!json_object_is_type(probes, json_type_array) ||
            json_object_array_length(probes) > SPECTRAL_PATH_PROBES) return false;
        p->probe_count = (int)json_object_array_length(probes);
        for (int i = 0; i < p->probe_count; ++i) {
            json_object *v = json_object_array_get_idx(probes, i);
            const char *name;
            RuntimeSpectralPathProbeConfig *probe = &p->probes[i];
            if (!ONLY(v, "name", "x", "y") || !RayTracingJsonGetString(v, "name", &name) ||
                !*name || !RayTracingCopyString(probe->name, sizeof(probe->name), name) ||
                !integer(v, "x", 0, width > 0 ? width - 1 : 4095, &probe->x) ||
                !integer(v, "y", 0, height > 0 ? height - 1 : 4095, &probe->y)) return false;
            for (int j = 0; j < i; ++j)
                if (!strcmp(probe->name, p->probes[j].name)) return false;
        }
    }
    json_object *medium = NULL;
    if (json_object_object_get_ex(q, "medium", &medium)) {
        if (!ONLY(medium, "bounds_min_m", "bounds_max_m", "sigma_a_per_m", "sigma_s_per_m", "g", "caustic_cell_m") ||
            !vector(json_object_object_get(medium, "bounds_min_m"), &p->medium_lo) ||
            !vector(json_object_object_get(medium, "bounds_max_m"), &p->medium_hi) ||
            p->medium_lo.x >= p->medium_hi.x || p->medium_lo.y >= p->medium_hi.y ||
            p->medium_lo.z >= p->medium_hi.z ||
            !number(json_object_object_get(medium, "g"), -.95, .95, &p->g)) return false;
        RuntimeSpectralCurve *curves[] = {&p->sigma_a, &p->sigma_s};
        const char *keys[] = {"sigma_a_per_m", "sigma_s_per_m"};
        for (int i = 0; i < 2; ++i) {
            RuntimeSpectralCurve *c = curves[i];
            if (!spectrum_pairs(json_object_object_get(medium, keys[i]), c->points, 32, &c->count) ||
                c->count < 2 || c->points[0][0] != 380 || c->points[c->count-1][0] != 780)
                return false;
        }
        if (json_object_object_get_ex(medium, "caustic_cell_m", NULL) &&
            (!out->glass_id[0] || !number(json_object_object_get(medium, "caustic_cell_m"), .005, 1,
                                          &p->caustic_cell_m))) return false;
        p->medium = true;
    }
    json_object *trace=NULL;
    if (json_object_object_get_ex(q,"path_trace",&trace)) {
        if (!ONLY(trace,"x","y","sample") ||
            !integer(trace,"x",0,width>0?width-1:4095,&p->trace_x) ||
            !integer(trace,"y",0,height>0?height-1:4095,&p->trace_y) ||
            !integer(trace,"sample",0,p->samples-1,&p->trace_sample)) return false;
        p->trace_enabled=true;
    }
    if (!surface_caustics_parse(q, out) || (p->output_component >= 4 && !out->glass_id[0]) ||
        (p->output_component == 6 && !p->medium)) return false;
    p->enabled = true;
    return true;
}
