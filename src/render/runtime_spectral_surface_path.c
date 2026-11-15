#include "runtime_spectral_surface_path_internal.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include "app/ray_tracing_sha256.h"
#include "render/runtime_incident_light_3d.h"
#include "render/runtime_specular_bsdf_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_scene_accel_3d_internal.h"
#include <float.h>
#include <math.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

static bool supported(const RuntimeSpectralPencil *profile,const RuntimeScene3D *scene,
    int *mapping,int *glass,char *diag,size_t size) {
    const RuntimeSpectralSurfacePathConfig *p=&profile->surface_path;
    bool hybrid=profile->glass_id[0]!=0;
    *glass=-1;
    if(scene->volume.enabled || scene->curveInstanceCount || scene->primitiveCount<1 ||
       scene->primitiveCount>SPECTRAL_PATH_MATERIALS) {
        snprintf(diag,size,"surface_path_v1: unsupported geometry/medium (planes, boxes, meshes; at most32 primitives; explicit homogeneous box only)");return false;
    }
    if(p->native_sources && RuntimeLightSet3D_EnabledCount(&scene->lightSet)>32) {
        snprintf(diag,size,"surface_path_v1: at most32 native sources are qualified");return false;
    }
    bool used[SPECTRAL_PATH_MATERIALS]={0};
    for(int i=0;i<scene->primitiveCount;++i) {
        const RuntimePrimitive3D *prim=&scene->primitives[i];
        if(prim->kind!=RUNTIME_PRIMITIVE_3D_KIND_PLANE && prim->kind!=RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM &&
           prim->kind!=RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH) {
            snprintf(diag,size,"surface_path_v1: unsupported primitive %s",prim->source.objectId);return false;
        }
        if (hybrid && !strcmp(prim->source.objectId,profile->glass_id)) {
            if (*glass>=0 || prim->kind!=RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH ||
                !RuntimeSpectralFilm_IsClosedMesh(profile->glass_id)) {
                snprintf(diag,size,"surface_path_v1: smooth glass requires one closed runtime mesh");return false;
            }
            *glass=i;mapping[i]=-1;continue;
        }
        if (hybrid && prim->kind!=RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            snprintf(diag,size,"surface_path_v1: smooth glass currently admits matte planes around the mesh");return false;
        }
        if(p->native_materials) {
            RuntimeMaterialPayload3D payload;
            if(!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(prim->source.sceneObjectIndex,&payload) ||
               !payload.valid || payload.transparency>0 || payload.emissive>0 ||
               payload.hasGlassTransportOverride || payload.hasMicrodetailNormal) {
                snprintf(diag,size,"surface_path_v1: object %s needs an explicit supported opaque material; glass, emission and microdetail normals require another qualified route",prim->source.objectId);return false;
            }
            if(RuntimePrincipledBSDF3D_FromMaterialPayload(&payload).transmissionWeight>0) {
                snprintf(diag,size,"surface_path_v1: object %s has unsupported transmission",prim->source.objectId);return false;
            }
            mapping[i]=-1;continue;
        }
        int j=0;
        while(j<p->material_count && strcmp(prim->source.objectId,p->materials[j].object_id))++j;
        if(j==p->material_count) {snprintf(diag,size,"surface_path_v1: missing spectral material for %s",prim->source.objectId);return false;}
        if (hybrid && p->materials[j].ggx) {
            snprintf(diag,size,"surface_path_v1: hybrid photon receivers must be diffuse");return false;
        }
        used[j]=true;mapping[i]=j;
    }
    if (hybrid && (*glass<0 || scene->primitiveCount<2 || p->native_sources ||
                   p->native_materials || p->sky)) {
        snprintf(diag,size,"surface_path_v1: smooth glass requires a closed mesh, matte planes and one explicit area source");return false;
    }
    for(int j=0;j<p->material_count;++j)if(!used[j]) {
        snprintf(diag,size,"surface_path_v1: unused spectral material %s",p->materials[j].object_id);return false;
    }
    for(int i=0;p->native_sources && i<RuntimeLightSet3D_EnabledCount(&scene->lightSet);++i) {
        const RuntimeLightSource3D *source=RuntimeLightSet3D_GetEnabled(&scene->lightSet,i);
        if(!RuntimeIncidentLight3D_SampleCount(source) || source->origin==RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER) {
            snprintf(diag,size,"surface_path_v1: source %s has no qualified native incident-light adapter",source->id);return false;
        }
    }
    return true;
}

void RuntimeSpectralSurfacePath_Defaults(RuntimeSpectralPencil *p) {
    p->surface_path=(RuntimeSpectralSurfacePathConfig){.enabled=true,.native_sources=true,.native_materials=true,.linear_components=true,
        .samples=512,.max_depth=12,.seed=19,.caustic_cell_m=.04,.knot_count=2,.knots={{380,1},{780,1}}};
    p->exposure=1;p->soft_display=true;
}

static double default_caustic_radius(const RuntimeScene3D *scene,int glass) {
    RuntimeSceneAcceleration3DInstanceBounds *bounds=NULL;
    int count=0;double radius=0;
    if (!RuntimeSceneAcceleration3D_CaptureInstanceBounds(scene,&bounds,&count,NULL)) return 0;
    for (int i=0;i<count;++i) if (bounds[i].primitiveIndex==glass) {
        Vec3 extents=vec3_sub(bounds[i].max,bounds[i].min);
        radius=.03*fmax(extents.x,fmax(extents.y,extents.z));
        break;
    }
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds,count);
    free(bounds);
    return radius;
}

static bool setup_caustics(RuntimeSpectralSurfacePath *state,char *diag,size_t size) {
    const RuntimeSpectralPencil *profile=state->profile;
    const RuntimeSpectralSurfacePathConfig *p=&profile->surface_path;
    if (state->glass_primitive<0 || p->caustic_passes==0) return true;
    RuntimeSpectralPencil *map=state->photon_profile=malloc(sizeof(*map));
    if (!map) {snprintf(diag,size,"surface_path_v1: cannot allocate caustic profile");return false;}
    *map=*profile;
    if (!(map->receiver_radius_m>0))
        map->receiver_radius_m=default_caustic_radius(state->scene,state->glass_primitive);
    if (!(map->receiver_radius_m>0)) {
        snprintf(diag,size,"surface_path_v1: cannot resolve caustic radius from glass bounds");return false;
    }
    map->exterior_ior=1;
    map->fresnel_camera_samples=1; /* Sample the smooth Fresnel branches in the forward tracer. */
    map->map=(RuntimeSpectralMapConfig){.enabled=true,.samples=p->caustic_passes*p->caustic_photons_per_pass,
        .bins=p->caustic_bands,.source_count=1,.seed=(unsigned)p->caustic_seed,.absorption_per_m=p->caustic_absorption};
    RuntimeSpectralLamp *lamp=&map->map.sources[0];
    lamp->origin=p->origin;lamp->direction=p->normal;lamp->nlines=p->line_count;lamp->nknots=p->knot_count;
    double projected_area=M_PI*p->width*p->height;
    for (int i=0;i<p->line_count;++i)
        lamp->lines[i]=(RuntimeSpectralLine){p->lines[i].nm,p->lines[i].watts*projected_area};
    for (int i=0;i<p->knot_count;++i)
        lamp->knots[i]=(RuntimeSpectralKnot){p->knots[i].nm,p->knots[i].watts_per_nm*projected_area};
    return true;
}

static bool nonzero_curve(const RuntimeSpectralCurve *curve) {
    for (int i=0;i<curve->count;++i) if (curve->points[i][1]>0) return true;
    return false;
}

static bool prepare_caustic_medium(RuntimeSpectralSurfacePath *state,char *diag,size_t size) {
    RuntimeSpectralPencil *map=state->photon_profile;
    const RuntimeSpectralSurfacePathConfig *p=&map->surface_path;
    if (!p->medium) return true;
    state->volume_caustics=nonzero_curve(&p->sigma_s);
    if (!state->volume_caustics && !nonzero_curve(&p->sigma_a)) return true;
    RuntimeSpectralVolumeConfig cfg={.kind=SPECTRAL_VOLUME_BOX,.lo=p->medium_lo,.hi=p->medium_hi,
        .absorption=p->sigma_a,.scattering=p->sigma_s,.g=p->g,.cell_m=p->caustic_cell_m};
    RayTracingSha256Context hash;
    ray_tracing_sha256_init(&hash);
    const char tag[]="ordinary-glass-homogeneous-v1";
    ray_tracing_sha256_update(&hash,tag,sizeof(tag));
    ray_tracing_sha256_update(&hash,&cfg.cell_m,sizeof(cfg.cell_m));
    ray_tracing_sha256_update(&hash,&cfg.g,sizeof(cfg.g));
    ray_tracing_sha256_update(&hash,&cfg.absorption.count,sizeof(cfg.absorption.count));
    ray_tracing_sha256_update(&hash,cfg.absorption.points,(size_t)cfg.absorption.count*sizeof(cfg.absorption.points[0]));
    ray_tracing_sha256_update(&hash,&cfg.scattering.count,sizeof(cfg.scattering.count));
    ray_tracing_sha256_update(&hash,cfg.scattering.points,(size_t)cfg.scattering.count*sizeof(cfg.scattering.points[0]));
    ray_tracing_sha256_finish_hex(&hash,cfg.profile_hash);
    map->active_volume=RuntimeSpectralVolume_Load(&cfg,diag,size);
    return map->active_volume!=NULL;
}

static bool build_caustics(RuntimeSpectralSurfacePath *state,RuntimeSpectralReadout *out,char *diag,size_t size) {
    if (state->photon_profile && !state->caustic_map) {
        memset(&state->caustic_stats,0,sizeof(state->caustic_stats));
        if (!prepare_caustic_medium(state,diag,size)) return false;
        bool old=RuntimeRay3D_SetInclusiveTriangleEdges(true);
        state->caustic_map=RuntimeSpectralSurfaceMap_Build(state->photon_profile,state->scene,&state->caustic_stats);
        RuntimeRay3D_SetInclusiveTriangleEdges(old);
        if (!state->caustic_map) {
            RuntimeSpectralVolume_Free(state->photon_profile->active_volume);
            state->photon_profile->active_volume=NULL;
            snprintf(diag,size,"surface_path_v1: could not build smooth glass caustic map");return false;
        }
        RuntimeSpectralSurfaceMap_Key(state->photon_profile,state->scene,state->caustic_stats.key);
        if (state->photon_profile->active_volume)
            RuntimeSpectralSurfaceMap_LaunchKey(state->photon_profile,state->scene,state->caustic_stats.launch_key);
    }
    if (state->photon_profile) {
        out->map=state->caustic_stats;
        out->surface_path.caustic_radius_m=state->photon_profile->receiver_radius_m;
    }
    return true;
}

RuntimeSpectralSurfacePath *RuntimeSpectralSurfacePath_Create(const RuntimeSpectralPencil *profile,
    const RuntimeScene3D *scene,int width,int height,char *diag,size_t size) {
    const RuntimeSpectralSurfacePathConfig *p=&profile->surface_path;
    if(width<1 || height<1 || width>4096 || height>4096 || p->samples<1 || p->samples>8192 ||
       p->max_depth<1 || p->max_depth>12 || (int64_t)width*height*p->samples*p->max_depth*spectral_path_max_branches(p)>INT64_C(1200000000)) {
        snprintf(diag,size,"surface_path_v1: invalid film or work budget (film samples, depth and branch budget exceed1200000000)");return NULL;
    }
    for(int i=0;i<p->probe_count;++i)if(p->probes[i].x>=width || p->probes[i].y>=height) {
        snprintf(diag,size,"surface_path_v1: probe %s lies outside the effective film",p->probes[i].name);return NULL;
    }
    if(p->trace_enabled && (p->trace_x>=width || p->trace_y>=height || p->trace_sample>=p->samples)) {
        snprintf(diag,size,"surface_path_v1: path_trace lies outside the effective film/sample range");return NULL;
    }
    if(p->sample_audit_enabled && (p->sample_audit_x<0 || p->sample_audit_y<0 ||
       p->sample_audit_x>=width || p->sample_audit_y>=height ||
       p->sample_audit_limit<1 || p->sample_audit_limit>SPECTRAL_SAMPLE_AUDIT_LIMIT)) {
        snprintf(diag,size,"surface_path_v1: sample_audit lies outside effective film or retained limit");return NULL;
    }
    RuntimeSpectralSurfacePath *state=calloc(1,sizeof(*state));
    if(!state) {snprintf(diag,size,"surface_path_v1: cannot allocate frame transport");return NULL;}
    state->profile=profile;state->scene=scene;
    if(!supported(profile,scene,state->mapping,&state->glass_primitive,diag,size)) {free(state);return NULL;}
    state->source_count=RuntimeLightSet3D_EnabledCount(&scene->lightSet);
    state->spectrum=RuntimeSpectral_Prepare(p->knots,p->knot_count,p->lines,p->line_count);
    if(!state->spectrum) {snprintf(diag,size,"surface_path_v1: invalid spectrum or unrepresentable CDF mass");free(state);return NULL;}
    if (!setup_caustics(state,diag,size)) {RuntimeSpectralSurfacePath_Free(state);return NULL;}
    for(int i=380;i<780;++i)state->native_normalization+=RuntimeSpectral_CIE1931(i+.5).y;
    return state;
}
void RuntimeSpectralSurfacePath_GetMetadata(const RuntimeSpectralSurfacePath *state,RuntimeSpectralReadout *out) {
    RuntimeSpectralSurfacePathReadout *s=&out->surface_path;
    s->source_count=state->profile->surface_path.native_sources?state->source_count:1;
    for(int i=0;i<s->source_count;++i) {
        const RuntimeLightSource3D *source=state->profile->surface_path.native_sources?
            RuntimeLightSet3D_GetEnabled(&state->scene->lightSet,i):NULL;
        snprintf(s->sources[i].id,sizeof(s->sources[i].id),"%s",source?source->id:state->profile->surface_path.sky?"sky":"area");
        s->sources[i].kind=source?source->kind:-1;s->sources[i].radiometry=source?source->radiometryMode:-1;
    }
}
void RuntimeSpectralSurfacePath_Free(RuntimeSpectralSurfacePath *state) {
    if(state) {
        spectral_components_abort(state->component_writer);
        RuntimeSpectralSurfaceMap_Free(state->caustic_map);
        if (state->photon_profile)
            RuntimeSpectralVolume_Free(state->photon_profile->active_volume);
        free(state->photon_profile);
        RuntimeSpectral_Free(state->spectrum);free(state);
    }
}

bool RuntimeSpectralSurfacePath_Region(RuntimeSpectralSurfacePath *state,
    const RuntimeNative3DPreparedFrame *frame,int x0,int y0,int x1,int y1,uint8_t *pixels,float *radiance,
    int stride,RuntimeSpectralReadout *out,const RuntimeNative3DTileSchedulerCancelToken *cancel,
    char *diag,size_t size) {
    state->scene=frame->traceScene?frame->traceScene:&frame->scene;
    const RuntimeSpectralPencil *profile=state->profile;
    const RuntimeSpectralSurfacePathConfig *p=&profile->surface_path;
    if(frame->physicalAtmosphere || x0<0 || y0<0 || x1>frame->width || y1>frame->height) {
        snprintf(diag,size,"surface_path_v1: unsupported atmosphere or film region");return false;
    }
    if (RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel)) {
        snprintf(diag,size,"surface_path_v1: canceled before caustic or film work");return false;
    }
    if (!build_caustics(state,out,diag,size)) return false;
    bool previous=RuntimeRay3D_SetInclusiveTriangleEdges(true),ok=true;
    double total=RuntimeSpectral_TotalWatts(state->spectrum);
    for(int y=y0;y<y1 && ok;++y)for(int x=x0;x<x1 && ok;++x) {
        RuntimeSpectralPathProbe value={0};double sum_y2=0;
        uint64_t pixel=(uint64_t)y*frame->width+x;
        double shift=RuntimeSpectral_Uniform64(pixel^((uint64_t)p->seed<<32));
        for(int sample=0;sample<p->samples && total>0;++sample) {
            if((sample%64)==0 && RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel)) {
                snprintf(diag,size,"surface_path_v1: canceled before pixel commitment");ok=false;break;
            }
            RuntimeSpectralDraw draw;double u=(sample+shift)/p->samples;
            if(!RuntimeSpectral_Sample(state->spectrum,u,&draw)) {snprintf(diag,size,"surface_path_v1: wavelength proposal failed");ok=false;break;}
            PathRandom rng=spectral_path_random_init(pixel,(unsigned)p->seed,(unsigned)sample,p->camera_qmc);
            double px=x+spectral_path_random(&rng)-.5,py=y+spectral_path_random(&rng)-.5;
            Ray3D ray=RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector,px,py);
            bool traced=p->trace_enabled && x==p->trace_x && y==p->trace_y && sample==p->trace_sample;
            bool audit=p->sample_audit_enabled && x==p->sample_audit_x && y==p->sample_audit_y;
            unsigned long long hits_before=out->surface_path.source_hit,volume_before=out->surface_path.volume_events;
            PathSignal signal=spectral_path_trace(state,ray,draw.nm,draw.measure,&rng,&out->surface_path,traced?&out->surface_path:NULL);
            if(signal.failed) {snprintf(diag,size,"surface_path_v1: invalid material or dielectric crossing at pixel(%d,%d), sample %d",x,y,sample);ok=false;break;}
            double weight=RuntimeSpectral_Value(state->spectrum,draw.nm,draw.measure)/draw.probability;
            if(traced && out->surface_path.trace_count) {
                RuntimeSpectralPathEvent *first=&out->surface_path.trace[0];
                first->wavelength_pdf=draw.probability;first->wavelength_weight=weight;first->wavelength_is_line=draw.measure==SPECTRAL_LINE;
            }

            Vec3 color=vec3_scale(RuntimeSpectral_CIE1931(draw.nm),weight);
            if(audit)spectral_path_audit_sample(&out->surface_path,p->sample_audit_limit,sample,draw.nm,weight,px,py,color,&signal,
                out->surface_path.source_hit-hits_before,out->surface_path.volume_events-volume_before);
            value.direct=vec3_add(value.direct,vec3_scale(color,signal.direct/p->samples));
            value.indirect=vec3_add(value.indirect,vec3_scale(color,signal.indirect/p->samples));
            value.volume=vec3_add(value.volume,vec3_scale(color,signal.volume/p->samples));
            value.volume_caustic=vec3_add(value.volume_caustic,vec3_scale(color,signal.volume_caustic/p->samples));
            if (signal.replaced_source>0)
                out->surface_path.film_replaced_source=vec3_add(out->surface_path.film_replaced_source,
                    vec3_scale(color,signal.replaced_source/p->samples));
            value.caustic=vec3_add(value.caustic,vec3_scale(color,signal.caustic/p->samples));
            value.caustic_bounce=vec3_add(value.caustic_bounce,vec3_scale(color,signal.caustic_bounce/p->samples));
            double sy=color.y*(signal.direct+signal.indirect+signal.volume+signal.caustic+signal.caustic_bounce);
            sum_y2+=sy*sy;
            for(int k=0;k<4;++k) {
                value.volume_order[k]=vec3_add(value.volume_order[k],vec3_scale(color,signal.volume_order[k]/p->samples));
                if(signal.volume_order[k]!=0)++out->surface_path.volume_order_contributions[k];
            }
            for(int k=0;k<5;++k)value.nonscattered_indirect[k]=vec3_add(value.nonscattered_indirect[k],vec3_scale(color,signal.nonscattered_indirect[k]/p->samples));
            value.mean_nm+=draw.nm/p->samples;
            for(int k=0;k<SPECTRAL_PATH_DEPTHS;++k)value.depth_radiance[k]+=color.y*signal.depth[k]/p->samples;
        }
        if(!ok)break;
        value.total=vec3_add(vec3_add(value.direct,value.indirect),value.volume);
        if (state->photon_profile)
            value.total=vec3_add(value.total,vec3_add(value.caustic,value.caustic_bounce));
        if(p->samples>1)value.luminance_variance=fmax(0,sum_y2-p->samples*value.total.y*value.total.y)/(p->samples*(p->samples-1.));
        for(int i=0;i<p->probe_count;++i)if(x==p->probes[i].x && y==p->probes[i].y)out->surface_path.probes[i]=value;
        if(x==frame->width/2 && y==frame->height/2)out->center_pixel_xyz=value.total;
        out->surface_path.film_direct=vec3_add(out->surface_path.film_direct,value.direct);
        for(int k=0;k<5;++k)out->surface_path.film_nonscattered_indirect[k]=vec3_add(out->surface_path.film_nonscattered_indirect[k],value.nonscattered_indirect[k]);
        out->surface_path.film_indirect=vec3_add(out->surface_path.film_indirect,value.indirect);
        out->surface_path.film_volume=vec3_add(out->surface_path.film_volume,value.volume);++out->surface_path.pixels;
        out->surface_path.film_caustic=vec3_add(out->surface_path.film_caustic,value.caustic);
        out->surface_path.film_caustic_bounce=vec3_add(out->surface_path.film_caustic_bounce,value.caustic_bounce);
        out->surface_path.film_volume_caustic=vec3_add(out->surface_path.film_volume_caustic,value.volume_caustic);
        for(int k=0;k<4;++k)out->surface_path.film_volume_order[k]=vec3_add(out->surface_path.film_volume_order[k],value.volume_order[k]);
        if(state->component_writer && !spectral_components_pixel(state->component_writer,&value)) {
            snprintf(diag,size,"surface_path_v1: linear component write failed");ok=false;break;
        }
        Vec3 components[]={value.total,value.direct,value.indirect,value.volume,value.caustic,value.caustic_bounce,
                           value.volume_caustic};
        Vec3 rgb=RuntimeSpectral_LinearRGB(vec3_scale(components[p->output_component],profile->exposure));
        if(pixels)RuntimeSpectral_DisplayPixelWithTransform(pixels+((size_t)y*frame->width+x)*4,rgb,profile->soft_display);
        if(radiance) {size_t i=((size_t)(y-y0)*stride+x-x0)*RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            radiance[i]=rgb.x;radiance[i+1]=rgb.y;radiance[i+2]=rgb.z;radiance[i+3]=0;}
    }
    RuntimeRay3D_SetInclusiveTriangleEdges(previous);
    return ok;
}

bool RuntimeSpectralSurfacePath_Render(RuntimeSpectralSurfacePath *state,
    const RuntimeNative3DPreparedFrame *frame,uint8_t *pixels,RuntimeSpectralReadout *out,
    const RuntimeNative3DTileSchedulerCancelToken *cancel,RuntimeNative3DTileSchedulerProgressCallback callback,
    void *user,char *diag,size_t size) {
    /* Prepared frames move into native job storage after creation. Rebind the
     * borrowed scene before either metadata or traversal reads it. */
    state->scene=frame->traceScene?frame->traceScene:&frame->scene;
    out->surface_path=(RuntimeSpectralSurfacePathReadout){0};
    RuntimeSpectralSurfacePath_GetMetadata(state,out);
    if(state->component_path[0]) {
        state->component_writer=spectral_components_begin(state->component_path,frame->width,frame->height);
        if(!state->component_writer) {snprintf(diag,size,"surface_path_v1: cannot open linear component output");return false;}
    }
    int bands=(frame->height+7)/8;
    for(int band=0;band<bands;++band) {
        int y=band*8,end=y+8<frame->height?y+8:frame->height;
        if(!RuntimeSpectralSurfacePath_Region(state,frame,0,y,frame->width,end,pixels,NULL,0,out,cancel,diag,size))goto abort;
        IntegratorTile tile={.originX=0,.originY=y,.width=frame->width,.height=end-y};
        RuntimeNative3DTileSchedulerProgress progress={.dirtyTiles=&tile,.dirtyTileCount=1,.startedSubpasses=1,
            .completedSubpasses=band+1==bands?1:0,.totalSubpasses=1,.completedTilesInSubpass=band+1,.totalTilesInSubpass=bands};
        if(callback && !callback(&progress,user)) {snprintf(diag,size,"surface_path_v1: progress consumer refused update");goto abort;}
    }
    if(RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel)) {snprintf(diag,size,"surface_path_v1: canceled before final commitment");goto abort;}
    if(state->component_writer) {
        bool written=spectral_components_finish(state->component_writer,state->component_path,state->component_overwrite,&out->surface_path);
        state->component_writer=NULL;
        if(!written) {snprintf(diag,size,"surface_path_v1: linear component publication failed");return false;}
    }
    snprintf(diag,size,"ok");return true;
abort:
    spectral_components_abort(state->component_writer);state->component_writer=NULL;return false;
}

bool RuntimeSpectralSurfacePath_Run(const RuntimeSpectralPencil *profile,
    const RuntimeNative3DPreparedFrame *frame,uint8_t *pixels,RuntimeSpectralReadout *out,char *diag,size_t size) {
    const RuntimeScene3D *scene=frame->traceScene?frame->traceScene:&frame->scene;
    RuntimeSpectralSurfacePath *state=RuntimeSpectralSurfacePath_Create(profile,scene,frame->width,frame->height,diag,size);
    if(!state)return false;
    RuntimeSpectralSurfacePath_GetMetadata(state,out);
    bool ok=!frame->physicalAtmosphere && (!pixels || RuntimeSpectralSurfacePath_Render(state,frame,pixels,out,NULL,NULL,NULL,diag,size));
    if(frame->physicalAtmosphere)snprintf(diag,size,"surface_path_v1: physical atmosphere is not qualified");
    RuntimeSpectralSurfacePath_Free(state);return ok;
}
