#define _XOPEN_SOURCE 700
#include "app/scene_atmosphere_runtime.h"
#include "app/scene_atmosphere_assets.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"
#include "app/ray_tracing_sha256.h"
#include "import/fluid_volume_import_3d.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SceneAtmosphereAttachment active;
static double active_world_scale=1;
static bool owns_tuning;
static RuntimeVolume3DMaterialTuning previous_material;
static RuntimeVolume3DScatterTuning previous_scatter;
static json_object* get(json_object* o,const char* key) {
    json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;
}
static bool fail(char* d,size_t n,const char* why) {
    if(d && n) snprintf(d,n,"%s",why);return false;
}
json_object* SceneAtmosphereAuthoring(json_object* scene) {
    return get(get(get(scene,"extensions"),"ray_tracing"),"authoring");
}
bool SceneAtmosphereValidateScene(json_object* scene,char* d,size_t n) {
    SceneAtmosphereAttachment v;
    return SceneAtmosphereAttachmentParse(SceneAtmosphereAuthoring(scene),&v,d,n);
}
void SceneAtmosphereRuntimeReset(void) {
    if(owns_tuning) {
        RuntimeVolume3DMaterial_RestoreTuning(previous_material);
        RuntimeVolume3DScatter_RestoreTuning(previous_scatter);
        owns_tuning=false;
    }
    SceneAtmosphereAttachmentDefaults(&active);active_world_scale=1;
}
bool SceneAtmosphereRuntimeLoad(json_object* scene,double world_scale) {
    SceneAtmosphereAttachment v;
    if(!SceneAtmosphereAttachmentParse(SceneAtmosphereAuthoring(scene),&v,NULL,0)) return false;
    if(v.present && !owns_tuning) {
        previous_material=RuntimeVolume3DMaterial_GetTuning();
        previous_scatter=RuntimeVolume3DScatter_GetTuning();owns_tuning=true;
    }
    active=v;active_world_scale=world_scale;SceneAtmosphereRuntimeApplyTuning();return true;
}
const SceneAtmosphereAttachment* SceneAtmosphereRuntimeRead(void) {return &active;}
double SceneAtmosphereRuntimeWorldScale(void) {return active_world_scale;}
void SceneAtmosphereRuntimeApplyTuning(void) {
    if(!active.present || !active.enabled) return;
    /* Clear the previous retained profile without changing legacy clamp policy. */
    RuntimeVolume3DMaterialTuning material=RuntimeVolume3DMaterial_GetTuning();
    material.physical_smoke=false;material.sky_error_budget=0;
    if(active.optics.present) material.cell_centered=active.optics.physical && active.optics.cell_centered;
    RuntimeVolume3DMaterial_RestoreTuning(material);
    if(active.optics.present && active.optics.physical) {
        RuntimeVolume3DMaterial_SetSmokeOptics(active.optics.reference_concentration_kg_m3,
            active.optics.mass_extinction_m2_kg,active.optics.scattering_albedo,
            active.optics.anisotropy,active.optics.sky_illumination);
        RuntimeVolume3DMaterial_SetSkyErrorBudget(active.optics.sky_error_budget);
    }
    RuntimeVolume3DMaterial_SetDensityScale(active.density_scale);
    RuntimeVolume3DMaterial_SetDensityGamma(active.density_gamma);
    RuntimeVolume3DMaterial_SetAbsorptionGain(active.absorption_gain);
    RuntimeVolume3DMaterial_SetOpacity(active.opacity);
    RuntimeVolume3DScatter_SetStrengthGain(active.scatter_gain);
    RuntimeVolume3DScatter_SetStepScale(active.step_scale);
    RuntimeVolume3DScatter_SetTint(active.tint[0],active.tint[1],active.tint[2]);
}
static bool source_for(const SceneAtmosphereAttachment* value,const char* scene_path,char* out,size_t n,char* d,size_t dn) {
    char parent[PATH_MAX],resolved_parent[PATH_MAX],candidate[PATH_MAX],resolved[PATH_MAX];
    if(!value || !value->present || !scene_path || !out || !n || strlen(scene_path)>=sizeof(parent)) return fail(d,dn,"attachment has no scene root");
    strcpy(parent,scene_path);char* slash=strrchr(parent,'/');
    if(slash) *slash=0;else strcpy(parent,".");
    if(!parent[0]) strcpy(parent,"/");
    if(!realpath(parent,resolved_parent) || snprintf(candidate,sizeof(candidate),"%s/%s",resolved_parent,value->source_path)>=(int)sizeof(candidate) ||
        !realpath(candidate,resolved)) return fail(d,dn,"atmosphere source is missing; reattach or restore project assets");
    size_t plen=strlen(resolved_parent);
    if(strncmp(resolved,resolved_parent,plen) || resolved[plen]!='/') return fail(d,dn,"atmosphere source escapes its project root");
    if(strlen(resolved)>=n) return fail(d,dn,"atmosphere path exceeds capacity");
    return SceneAtmosphereAssetPath(scene_path,value->source_path,resolved_parent,out,n,d,dn);
}
bool SceneAtmosphereRuntimeSource(const char* scene_path,char* out,size_t n,char* d,size_t dn) {
    return source_for(&active,scene_path,out,n,d,dn);
}
static bool source_ids(const char* path,int depth,int64_t** ids,size_t* count,char* d,size_t n) {
    if(depth>4) return fail(d,n,"cache bundle nesting exceeds supported depth");
    json_object* root=json_object_from_file(path);
    if(!root) return fail(d,n,"cannot read cache manifest");
    json_object* bundle=get(root,"fluid_source"),*frames=get(root,"frames");
    bool ok=false;
    if(bundle) {
        json_object* p=get(bundle,"path");
        const char* rel=p && json_object_is_type(p,json_type_string)?json_object_get_string(p):NULL;
        if(!rel || rel[0]=='/' || strchr(rel,'\\') || strstr(rel,"..")) goto done;
        char child[PATH_MAX];const char* slash=strrchr(path,'/');
        size_t plen=slash?(size_t)(slash-path):0;
        if(snprintf(child,sizeof(child),"%.*s/%s",(int)plen,path,rel)>=(int)sizeof(child)) goto done;
        char base[PATH_MAX],verified[PATH_MAX];
        snprintf(base,sizeof(base),"%.*s",(int)plen,path);
        if(!SceneAtmosphereAssetPath(path,rel,base,verified,sizeof(verified),d,n)) goto done;
        RuntimeVolume3DSourceKind kind;
        if(fluid_volume_import_3d_classify_path(verified,&kind) &&
           (kind==RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D || kind==RUNTIME_VOLUME_3D_SOURCE_PACK)) {
            uint64_t actual;
            if(!fluid_volume_import_3d_single_frame_index(verified,kind,&actual) || actual>INT_MAX) goto done;
            *ids=malloc(sizeof(**ids));if(!*ids) goto done;
            **ids=(int64_t)actual;*count=1;ok=true;goto done;
        }
        ok=source_ids(verified,depth+1,ids,count,d,n);goto done;
    }
    if(!frames || !json_object_is_type(frames,json_type_array)) goto done;
    *count=json_object_array_length(frames);
    if(!*count || *count>1000000) goto done;
    *ids=malloc(*count*sizeof(**ids));if(!*ids) goto done;
    for(size_t i=0;i<*count;++i) {
        json_object* index=get(json_object_array_get_idx(frames,i),"frame_index");
        if(!index || !json_object_is_type(index,json_type_int)) {free(*ids);*ids=NULL;goto done;}
        (*ids)[i]=json_object_get_int64(index);
    }
    ok=true;
done:
    json_object_put(root);
    if(!ok && (!d || !n || !d[0])) fail(d,n,"cache manifest needs explicit sorted frame_index values");
    return ok;
}
bool SceneAtmosphereFrameForAttachment(const SceneAtmosphereAttachment* value,const char* scene_path,const TimelineEvaluationContext* frame,int64_t* out,char* d,size_t n) {
    char path[PATH_MAX];int64_t requested;int64_t* ids=NULL;size_t count=0;
    if(!value || !frame || !out) return fail(d,n,"missing cache frame selection context");
    if(d && n) d[0]=0;
    if(!SceneAtmosphereAttachmentRequestedFrame(value,frame,&requested,d,n) ||
        !source_for(value,scene_path,path,sizeof(path),d,n)) return false;
    if(!strcmp(value->source_kind,"vf3d") || !strcmp(value->source_kind,"pack")) {
        uint64_t actual;
        RuntimeVolume3DSourceKind kind=!strcmp(value->source_kind,"vf3d")?RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:RUNTIME_VOLUME_3D_SOURCE_PACK;
        if(!fluid_volume_import_3d_single_frame_index(path,kind,&actual) || actual>INT_MAX)
            return fail(d,n,"Single-file cache has no supported actual frame ID.");
        *out=(int64_t)actual;return true;
    }
    if(!source_ids(path,0,&ids,&count,d,n)) return false;
    bool ok=SceneAtmosphereAttachmentSelectFrame(value,requested,ids,count,out,d,n);
    free(ids);return ok;
}
bool SceneAtmosphereRuntimeFrame(const char* scene_path,const TimelineEvaluationContext* frame,int64_t* out,char* d,size_t n) {
    return SceneAtmosphereFrameForAttachment(&active,scene_path,frame,out,d,n);
}
bool SceneAtmosphereRuntimeSimulation(const char* scene_path,const TimelineEvaluationContext* frame,
    RayEvaluatedSimulationIdentity* out,char* d,size_t n) {
    if(!out) return fail(d,n,"missing evaluated cache identity");
    RayEvaluatedSimulationIdentity identity={0};
    if(!active.present || !active.enabled) {*out=identity;return true;}
    char path[PATH_MAX],selected_path[PATH_MAX],digest[65],manifest_digest[65];int64_t selected;
    if(!SceneAtmosphereRuntimeSource(scene_path,path,sizeof(path),d,n) ||
        !SceneAtmosphereRuntimeFrame(scene_path,frame,&selected,d,n) || selected>INT_MAX)
        return false;
    char volume_source[4096];int resolved_kind;
    if(!SceneAtmosphereAssetVolumeSource(scene_path,path,volume_source,sizeof(volume_source),&resolved_kind,d,n)) return false;
    RuntimeVolume3DSourceKind kind=(RuntimeVolume3DSourceKind)resolved_kind;
    if(!fluid_volume_import_3d_resolve_source_frame_path(volume_source,kind,(int)selected,
        selected_path,sizeof(selected_path),d,n)) return false;
    char project[PATH_MAX],resolved_project[PATH_MAX],verified[PATH_MAX];
    snprintf(project,sizeof(project),"%s",scene_path);char* slash=strrchr(project,'/');
    if(slash) *slash=0;else strcpy(project,".");
    if(!realpath(project,resolved_project)) return fail(d,n,"Cache project directory is unavailable.");
    size_t root_length=strlen(resolved_project);
    if(strncmp(selected_path,resolved_project,root_length) || selected_path[root_length]!='/' ||
       !SceneAtmosphereAssetPath(scene_path,selected_path+root_length+1,resolved_project,verified,sizeof(verified),d,n))
        return fail(d,n,"Selected cache frame escapes the project or is a symlink.");
    if(!ray_tracing_sha256_file(verified,digest) ||
        !ray_tracing_sha256_file(path,manifest_digest)) return fail(d,n,"cannot identify selected atmosphere cache bytes");
    char water_digest[65],water_manifest[65],combined[129];
    if(!SceneAtmosphereRuntimeWaterDigest(scene_path,frame,water_digest,water_manifest,d,n)) return false;
    if(water_digest[0]) {
        snprintf(combined,sizeof(combined),"%s%s",digest,water_digest);
        if(!ray_tracing_sha256_bytes(combined,128,digest)) return false;
        snprintf(combined,sizeof(combined),"%s%s",manifest_digest,water_manifest);
        if(!ray_tracing_sha256_bytes(combined,128,manifest_digest)) return false;
    }
    identity.source=RAY_EVALUATED_SIMULATION_CACHE;identity.valid=true;
    snprintf(identity.cache_id,sizeof(identity.cache_id),"%s",active.id);
    char prefix[17];memcpy(prefix,manifest_digest,16);prefix[16]=0;
    identity.cache_revision=strtoull(prefix,NULL,16);
    identity.frame_index=frame->sample.absolute_frame;identity.source_frame_index=selected;
    identity.source_rate=active.source_rate;identity.frame_offset=active.timeline_start;
    identity.frame_stride=1;identity.subframe_denominator=1;
    identity.interpolation=RAY_EVALUATED_SIMULATION_INTERPOLATION_STEP;
    snprintf(identity.content_digest,sizeof(identity.content_digest),"%s",digest);
    *out=identity;return true;
}
