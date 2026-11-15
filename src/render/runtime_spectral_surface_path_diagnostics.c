#include "runtime_spectral_surface_path_internal.h"
#include "render/runtime_spectral_transport.h"
#include "runtime_spectral_json.h"
#include <json-c/json.h>
#include <string.h>

static const char *const component_names[] = {
    "total", "direct", "surface_indirect", "volume", "caustic", "caustic_bounce", "volume_caustic"
};

static void write_xyz(FILE *f, const char *name, Vec3 value) {
    fprintf(f, "\"%s\":[%.17g,%.17g,%.17g]", name, value.x, value.y, value.z);
}

void RuntimeSpectralSurfacePath_Write(FILE *f, const RuntimeSpectralPencil *p,
                                     const RuntimeSpectralReadout *out) {
    const RuntimeSpectralSurfacePathReadout *s = &out->surface_path;
    fprintf(f, "  \"spectral_execution\":{\"route\":\"local_headless_surface_path_v1\","
        "\"static_frames\":1,\"progressive\":false,\"denoise\":false,\"output_component\":\"%s\"},\n",
        component_names[p->surface_path.output_component]);
    fprintf(f, "  \"spectral_quality\":{\"profile\":\"surface_path_v1\",\"seed\":%d,"
        "\"exposure\":%.17g,\"display_transform\":\"%s\",\"wavelength_observer\":\"cie1931_sample_wavelength\","
        "\"pixel_sampling\":\"%s\",\"camera_sampler\":\"%s\",\"direct_mis\":true,\"surface_bounces\":%s,"
        "\"medium_present\":%s,\"max_path_events\":32},\n",
        p->surface_path.seed, p->exposure, p->soft_display ? "spectral_soft_v1" : "legacy_clip",
        p->surface_path.camera_qmc?"owen_sobol_2d_v1":"jittered",p->surface_path.camera_qmc?"scrambled_path_v1":"hashed_v1",
        p->surface_path.max_depth > 1 ? "true" : "false",
        p->surface_path.medium ? "true" : "false");
    fprintf(f, "  \"spectral_surface_path\":{\"samples\":%d,\"max_surface_depth\":%d,\"paths\":%llu,"
        "\"surface_events\":%llu,\"volume_events\":%llu,\"absorbed_paths\":%llu,"
        "\"escaped_paths\":%llu,\"depth_limited_paths\":%llu,\"probes\":[",
        p->surface_path.samples, p->surface_path.max_depth, s->paths, s->surface_events, s->volume_events,
        s->absorbed, s->escaped, s->limited);
    for (int i = 0; i < p->surface_path.probe_count; ++i) {
        const RuntimeSpectralPathProbeConfig *q = &p->surface_path.probes[i];
        const RuntimeSpectralPathProbe *v = &s->probes[i];
        json_object *name = json_object_new_string(q->name);
        fprintf(f, "%s{\"name\":%s,\"x\":%d,\"y\":%d,", i ? "," : "",
                name ? json_object_to_json_string_ext(name, JSON_C_TO_STRING_PLAIN) : "null", q->x, q->y);
        if (name) json_object_put(name);
        write_xyz(f, "direct_xyz", v->direct);
        fputc(',', f);
        write_xyz(f, "surface_indirect_xyz", v->indirect);
        fputc(',', f);
        write_xyz(f, "volume_xyz", v->volume);
        fputc(',', f);
        if (p->glass_id[0]) {
            write_xyz(f,"caustic_xyz",v->caustic);fputc(',',f);
            write_xyz(f,"caustic_bounce_xyz",v->caustic_bounce);fputc(',',f);
            if(p->surface_path.medium) {
                write_xyz(f,"volume_caustic_xyz",v->volume_caustic);fputc(',',f);
            }
        }
        write_xyz(f, "total_xyz", v->total);
        fprintf(f, ",\"luminance_standard_error\":%.17g,\"mean_wavelength_nm\":%.17g,\"surface_order_Y\":[",
                sqrt(v->luminance_variance), v->mean_nm);
        for (int k = 0; k < SPECTRAL_PATH_DEPTHS; ++k)
            fprintf(f, "%s%.17g", k ? "," : "", v->depth_radiance[k]);
        fprintf(f, "]}");
    }
    fprintf(f, "]},\n");
}

RuntimeSpectralPathEvent *spectral_path_event(RuntimeSpectralSurfacePathReadout *out,const char *event,
    const char *object,const char *reason,Vec3 position,Vec3 direction,double nm,double before,double after,
    int depth,int volumes) {
    if(!out)return NULL;
    if(out->trace_count>=SPECTRAL_PATH_TRACE_EVENTS) {out->trace_truncated=true;return NULL;}
    RuntimeSpectralPathEvent *e=&out->trace[out->trace_count++];
    *e=(RuntimeSpectralPathEvent){.position=position,.direction=direction,.wavelength_nm=nm,.beta_in=before,
        .beta_out=after,.surface_depth=depth,.volume_events=volumes};
    snprintf(e->event,sizeof(e->event),"%s",event);snprintf(e->object_id,sizeof(e->object_id),"%s",object);
    snprintf(e->reason,sizeof(e->reason),"%s",reason);return e;
}
#define ADD(o,k,v) do{if(!spectral_json_add(o,k,v))goto fail;}while(0)
#define S(o,k,v) ADD(o,k,json_object_new_string(v))
#define N(o,k,v) ADD(o,k,json_object_new_double(v))
#define I(o,k,v) ADD(o,k,json_object_new_int64(v))
#define B(o,k,v) ADD(o,k,json_object_new_boolean(v))
static json_object *event_json(const RuntimeSpectralPathEvent *e) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    S(o,"event",e->event);S(o,"object_or_source_id",e->object_id);S(o,"reason",e->reason);
    ADD(o,"position_m",spectral_json_vec(e->position));ADD(o,"direction",spectral_json_vec(e->direction));
    S(o,"occluder_id",e->occluder_id);S(o,"component",e->component);
    ADD(o,"geometric_normal",spectral_json_vec(e->normal));
    B(o,"integrated_light_weight",e->integrated_light);B(o,"distant_source",e->distant_source);
    S(o,"wavelength_measure",e->wavelength_is_line?"line_atom":"continuum");
    N(o,"wavelength_pdf",e->wavelength_pdf);N(o,"wavelength_weight",e->wavelength_weight);
    N(o,"continuation_weight",e->continuation_weight);N(o,"source_selection_pdf",e->selection_pdf);N(o,"source_stratum_pdf",e->stratum_pdf);
    N(o,"vacuum_nm",e->wavelength_nm);N(o,"beta_in",e->beta_in);N(o,"beta_out",e->beta_out);
    N(o,"reflectance",e->reflectance);N(o,"distance_m",e->distance);N(o,"continuation_pdf",e->bsdf_pdf);
    N(o,"light_pdf",e->light_pdf);N(o,"mis_weight",e->mis_weight);N(o,"scalar_contribution",e->contribution);
    N(o,"roulette_survival",e->survival);N(o,"bsdf_f_cos_or_phase",e->bsdf_fcos);
    N(o,"source_weight",e->source_weight);N(o,"transmittance",e->transmittance);
    N(o,"sigma_a_per_m",e->absorption_per_m);N(o,"sigma_s_per_m",e->scattering_per_m);
    I(o,"additional_surface_interactions",e->surface_depth);I(o,"volume_events",e->volume_events);
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *quality_json(const RuntimeSpectralPencil *profile) {
    const RuntimeSpectralSurfacePathConfig *p=&profile->surface_path;
    json_object *o=json_object_new_object();if(!o)return NULL;
    S(o,"profile","surface_path_v1");I(o,"samples_per_pixel",p->samples);I(o,"max_surface_interactions",p->max_depth);
    I(o,"max_path_events",32);I(o,"seed",p->seed);N(o,"exposure",profile->exposure);
    S(o,"display_transform",profile->soft_display?"spectral_soft_v1":"legacy_clip");S(o,"pixel_sampler",p->camera_qmc?"owen_sobol_2d_v1":"jittered_v1");
    S(o,"volume_direction_sampling",p->volume_rectangle_mis?"rectangle_mis":p->volume_light_only?"light_only":p->volume_light_mis?"light_mis":"phase");
    S(o,"continuation_roulette",p->fog_survival?"fog_survival":"throughput");
    S(o,"medium_sampling",p->medium_split_first?"split_first":"analog");
    I(o,"maximum_camera_branches",spectral_path_max_branches(p));
    S(o,"camera_sampler",p->camera_qmc?"scrambled_path_v1":"hashed_v1");
    S(o,"sample_error_model","within_pixel_iid_heuristic; use independent seeds for uncertainty");
    S(o,"wavelength_sampler","shifted_source_cdf_v1");S(o,"observer","cie1931_sample_wavelength");
    S(o,"component",component_names[p->output_component]);
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *sources_json(const RuntimeSpectralPencil *profile,const RuntimeScene3D *scene,const RuntimeSpectralReadout *readout) {
    json_object *a=json_object_new_array();if(!a)return NULL;
    int count=profile->surface_path.native_sources ? (scene?RuntimeLightSet3D_EnabledCount(&scene->lightSet):readout?readout->surface_path.source_count:0):1;
    for(int i=0;i<count;++i) {
        json_object *o=json_object_new_object();if(!o)goto fail;
        const RuntimeLightSource3D *s=profile->surface_path.native_sources && scene?RuntimeLightSet3D_GetEnabled(&scene->lightSet,i):NULL;
        const char *id=s?s->id:profile->surface_path.native_sources && readout?readout->surface_path.sources[i].id:profile->surface_path.sky?"sky":"area";
        int radiometry=s?s->radiometryMode:readout?readout->surface_path.sources[i].radiometry:-1;
        int kind=s?s->kind:readout?readout->surface_path.sources[i].kind:-1;
        if(!spectral_json_add(o,"id",json_object_new_string(id)) ||
           !spectral_json_add(o,"representation",json_object_new_string(profile->surface_path.native_sources?"native_rgb_basis_approximation":"explicit_spectral_distribution")) ||
           (profile->surface_path.native_sources && (!spectral_json_add(o,"radiometry_mode",json_object_new_string(radiometry==RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE?"lambertian_radiance":"legacy_intensity")) ||
                  !spectral_json_add(o,"native_kind",json_object_new_int(kind)))) ||
           json_object_array_add(a,o)!=0) {json_object_put(o);goto fail;}
    }
    return a;
fail:json_object_put(a);return NULL;
}
static json_object *components_json(const RuntimeSpectralSurfacePathReadout *s,bool glass,bool medium) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    ADD(o,"direct",spectral_json_vec(s->film_direct));ADD(o,"surface_indirect",spectral_json_vec(s->film_indirect));
    ADD(o,"volume",spectral_json_vec(s->film_volume));
    if(glass && medium) ADD(o,"volume_caustic_subset",spectral_json_vec(s->film_volume_caustic));
    Vec3 total=vec3_add(vec3_add(s->film_direct,s->film_indirect),s->film_volume);
    if (glass) {
        ADD(o,"caustic",spectral_json_vec(s->film_caustic));
        ADD(o,"caustic_bounce",spectral_json_vec(s->film_caustic_bounce));
        total=vec3_add(total,vec3_add(s->film_caustic,s->film_caustic_bounce));
    }
    ADD(o,"total",spectral_json_vec(total));
    I(o,"committed_pixels",s->pixels);S(o,"measure","unexposed_XYZ_sum; not_emitted_watts");
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *orders_json(const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    S(o,"definition","ordinary camera contributions grouped by actual medium collisions; elastic surface/glass interactions do not increment order");
    S(o,"mapped_volume_policy","volume_caustic_subset excluded; a mapped first scatter plus camera continuation may contain additional scattering");
    S(o,"measure","unexposed_XYZ_sum; counts are primary samples with nonzero ordinary contribution per group, not collision counts or watts");
    const char *names[]={"scatter_1","scatter_2","scatter_3_4","scatter_5_plus"};
    json_object *groups=json_object_new_array();if(!groups)goto fail;ADD(o,"groups",groups);
    Vec3 ordinary=vec3(0,0,0);
    for(int k=0;k<4;++k) {
        json_object *g=json_object_new_object();if(!g)goto fail;
        if(json_object_array_add(groups,g)!=0) {json_object_put(g);goto fail;}
        S(g,"name",names[k]);ADD(g,"xyz",spectral_json_vec(s->film_volume_order[k]));
        I(g,"contributing_primary_samples",s->volume_order_contributions[k]);
        ordinary=vec3_add(ordinary,s->film_volume_order[k]);
    }
    ADD(o,"ordinary_volume_xyz",spectral_json_vec(ordinary));
    ADD(o,"volume_accounting_residual_xyz",spectral_json_vec(vec3_sub(s->film_volume,vec3_add(ordinary,s->film_volume_caustic))));
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *export_json(const RuntimeSpectralPencil *p,const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    S(o,"schema","optic_v3_linear_components_v1");B(o,"enabled",p->surface_path.linear_components);
    B(o,"written",s->components_written);S(o,"file",s->components_file);
    S(o,"path_base","rendered_image_directory");S(o,"sha256",s->components_sha256);I(o,"bytes",s->components_bytes);
    S(o,"format","npy_v1");S(o,"dtype","<f8");S(o,"axes","height,width,component,XYZ; top row first");
    S(o,"measure","unexposed_CIE1931_XYZ; no clamp, display transform or denoising");
    S(o,"canonical_recombination","((direct + surface_indirect) + volume) + (caustic + caustic_bounce)");
    S(o,"surface_depth_policy","direct and surface_indirect follow camera surface depth, including dielectric interfaces; not a diffuse-only split");
    S(o,"subset_policy","volume_caustic and scatter_1/2/3_4/5_plus partition volume within floating roundoff; do not add these subsets to total");
    S(o,"exactness","canonical sum reproduces stored total bit-for-bit; regrouping or RGB conversion can introduce floating roundoff");
    json_object *names=json_object_new_array();if(!names)goto fail;ADD(o,"components",names);
    const char *labels[]={"total","direct","surface_indirect","volume","caustic","caustic_bounce","volume_caustic","scatter_1","scatter_2","scatter_3_4","scatter_5_plus"};
    for(int i=0;i<11;++i) {
        json_object *label=json_object_new_string(labels[i]);
        if(!label || json_object_array_add(names,label)!=0) {if(label)json_object_put(label);goto fail;}
    }
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *termination_json(const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    I(o,"fog_roulette_events",s->fog_roulette_events);I(o,"fog_roulette_survived",s->fog_roulette_survived);
    I(o,"medium_split_branches",s->medium_splits);I(o,"additional_glass_split_branches",s->internal_interface_splits);I(o,"specular_roulette_skipped",s->specular_roulette_skipped);
    I(o,"camera_paths",s->paths);I(o,"source_hit",s->source_hit);I(o,"dark_escape",s->dark_escape);
    I(o,"zero_surface_throughput",s->surface_absorbed);I(o,"medium_absorption",s->medium_absorbed);
    I(o,"one_sided_backface",s->backface);I(o,"bsdf_rejected",s->bsdf_rejected);I(o,"roulette",s->roulette);
    I(o,"surface_depth_limit",s->depth_limited);I(o,"path_event_limit",s->event_limited);
    unsigned long long terminal=s->source_hit+s->dark_escape+s->surface_absorbed+s->medium_absorbed+s->backface+
        s->bsdf_rejected+s->roulette+s->depth_limited+s->event_limited;
    B(o,"all_started_paths_classified",terminal==s->paths);S(o,"measure","event_counts; not_energy_conservation");
    return o;
fail:json_object_put(o);return NULL;
}
static json_object *trace_json(const RuntimeSpectralPencil *p,const RuntimeSpectralSurfacePathReadout *s,bool detailed) {
    json_object *o=json_object_new_object(),*events=NULL;if(!o)return NULL;
    B(o,"enabled",p->surface_path.trace_enabled);I(o,"x",p->surface_path.trace_x);I(o,"y",p->surface_path.trace_y);
    I(o,"sample",p->surface_path.trace_sample);I(o,"event_limit",SPECTRAL_PATH_TRACE_EVENTS);B(o,"truncated",s->trace_truncated);
    S(o,"contribution_measure","scalar_before_wavelength_weight_and_sample_average");
    events=json_object_new_array();if(!events)goto fail;
    I(o,"captured_event_count",s->trace_count);
    for(int i=0;detailed && i<s->trace_count;++i) {json_object *e=event_json(&s->trace[i]);
        if(!e || json_object_array_add(events,e)!=0) {if(e)json_object_put(e);goto fail;}}
    if(!spectral_json_add(o,"events",events)) {events=NULL;goto fail;}events=NULL;
    return o;
fail:if(events)json_object_put(events);json_object_put(o);return NULL;
}
static json_object *caustics_json(const RuntimeSpectralPencil *p,const RuntimeSpectralReadout *r) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    const RuntimeSpectralSurfacePathConfig *c=&p->surface_path;
    S(o,"glass_id",p->glass_id);S(o,"glass_model",p->use_n_bk7?"schott_n_bk7":"constant");
    I(o,"passes",c->caustic_passes);I(o,"photons_per_pass",c->caustic_photons_per_pass);
    I(o,"seed",c->caustic_seed);I(o,"continuum_bins",c->caustic_bands);
    N(o,"bulk_absorption_per_m",c->caustic_absorption);
    S(o,"photon_launch",c->caustic_guided?"glass_guided":"cosine");
    S(o,"camera_fresnel",c->glass_split_camera_exit?"split_camera_exit":c->glass_split_bounded?"split_bounded":c->glass_split_first?"split_first":"roulette");
    S(o,"launch_power_measure",c->caustic_guided?"importance_weighted_estimate":"emitted_flux");
    S(o,"pass_policy","one cumulative photon map with deterministic launch prefix");
    S(o,"ownership",c->medium ?
      "area to delta chain to first matte plane or first fog scatter; camera continues from both" :
      "area to delta chain to first matte plane; camera continues from the gather");
    if (r) {
        const RuntimeSpectralMapStats *m=&r->map;
        N(o,"radius_m",r->surface_path.caustic_radius_m);S(o,"map_key",m->key);
        if(c->medium) {
            S(o,"ballistic_launch_key",m->launch_key[0]?m->launch_key:m->key);
            S(o,"volume_reconstruction","cell_average");
            N(o,"volume_cell_m",c->caustic_cell_m);
            I(o,"camera_volume_source_hits_owned_by_map",r->surface_path.replaced_volume_source_hits);
        }
        I(o,"launched",m->launched);I(o,"deposits",m->deposits);I(o,"direct_arrivals_excluded",m->direct_arrivals);
        I(o,"camera_primary_paths",r->surface_path.paths-r->surface_path.first_interface_splits-r->surface_path.medium_splits-r->surface_path.internal_interface_splits);
        I(o,"camera_first_interface_splits",r->surface_path.first_interface_splits);
        I(o,"camera_reflections",r->surface_path.glass_reflections);
        I(o,"camera_transmissions",r->surface_path.glass_transmissions);
        I(o,"camera_total_internal_reflections",r->surface_path.glass_tir);
        I(o,"camera_source_hits_owned_by_map",r->surface_path.replaced_source_hits);
        ADD(o,"camera_replaced_source_xyz",spectral_json_vec(r->surface_path.film_replaced_source));
        json_object *a=json_object_new_array();if(!a)goto fail;
        ADD(o,"bands",a);
        for (int i=0;i<m->band_count;++i) {
            json_object *band=json_object_new_object();if(!band)goto fail;
            if (json_object_array_add(a,band)!=0) {json_object_put(band);goto fail;}
            const RuntimeSpectralBand *b=&m->bands[i];
            N(band,"vacuum_nm",b->nm);N(band,"width_nm",b->width_nm);
            N(band,"launched_w",b->launched_w);N(band,"deposited_w",b->watts);
            N(band,"all_receiver_w",b->receiver_w);N(band,"escaped_w",b->escaped_w);
            N(band,"glass_absorbed_w",b->glass_absorbed_w);N(band,"depth_limited_w",b->limited_w);
            N(band,"optical_loss_w",b->optical_loss_w);
            if(c->medium) {
                N(band,"medium_absorbed_w",b->medium_absorbed_w);
                N(band,"medium_scattered_w",b->medium_scattered_w);
                N(band,"volume_caustic_scattered_w",b->first_scatter_w);
                I(band,"volume_caustic_deposits",b->collisions);
            }
            I(band,"deposits",b->count);
            ADD(band,"deposit_centroid_m",spectral_json_vec(b->watts>0?vec3_scale(b->moment,1/b->watts):vec3(0,0,0)));
        }
    }
    return o;
fail:json_object_put(o);return NULL;
}
json_object *RuntimeSpectralSurfacePath_JSON(const RuntimeSpectralPencil *p,const RuntimeSpectralReadout *readout,
    const RuntimeScene3D *scene,bool detailed,const char *status,const char *diagnostic) {
    json_object *o=json_object_new_object();if(!o)return NULL;
    const RuntimeSpectralSurfacePathConfig *c=&p->surface_path;
    S(o,"schema","optic_v3_transport_v1");S(o,"route","surface_path_v1");S(o,"status",status);S(o,"diagnostic",diagnostic);
    bool glass=p->glass_id[0]!=0, mapped=glass && c->caustic_passes>0;
    S(o,"path_direction",mapped?"camera_to_light_and_forward_photons":"camera_to_light");
    S(o,"estimator",mapped?(c->medium?"camera_path_MIS_and_disjoint_surface_volume_caustics":
      "camera_path_MIS_and_disjoint_surface_caustics"):
      c->native_sources?"native_incident_NEE_and_camera_continuation":"camera_path_light_BSDF_MIS");
    B(o,"ordinary_indirect",c->max_depth>1);B(o,"elastic_wavelengths",true);B(o,"photon_maps_used",mapped);
    if (glass) ADD(o,"smooth_glass",caustics_json(p,readout));
    S(o,"material_representation",c->native_materials?"native_rgb_basis_approximation":"explicit_spectral_reflectance");
    S(o,"environment_policy",c->sky?"explicit_spectral_sky_cap":"no_implicit_environment_illumination");
    S(o,"medium",c->medium?"homogeneous_HG_box":"vacuum");
    S(o,"component_partition",glass?(c->medium?
      "camera direct/indirect and volume; volume_caustic_subset is included in volume; caustic and caustic_bounce are surface-map contributions":
      "camera direct/indirect; caustic:first matte on camera path; caustic_bounce:after a matte camera interaction"):
      "direct:no_medium_no_prior_surface; surface_indirect:no_medium_prior_surface; volume:any_medium_scatter");
    ADD(o,"quality",quality_json(p));ADD(o,"sources",sources_json(p,scene,readout));
    if(readout) {
        ADD(o,"film_components_xyz",components_json(&readout->surface_path,glass,c->medium));
        ADD(o,"scattering_orders",orders_json(&readout->surface_path));
        ADD(o,"linear_components",export_json(p,&readout->surface_path));
        ADD(o,"termination",termination_json(&readout->surface_path));
        ADD(o,"nonscattered_indirect",spectral_nonscattered_json(c,&readout->surface_path));
        ADD(o,"volume_light_diagnostics",spectral_volume_diagnostics_json(&readout->surface_path));
        ADD(o,"path_trace",trace_json(p,&readout->surface_path,detailed));
        ADD(o,"sample_audit",spectral_path_audit_json(c,&readout->surface_path));
    }
    return o;
fail:json_object_put(o);return NULL;
}
#undef ADD
#undef S
#undef N
#undef I
#undef B
