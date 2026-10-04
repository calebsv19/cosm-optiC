#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_timeline.h"
#include "app/scene_atmosphere_assets.h"
#include "app/scene_atmosphere_runtime.h"
#include "import/fluid_volume_import_3d.h"
#include "render/runtime_volume_3d_transform.h"
#include "scene/object_manager.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct {
    RuntimeVolumeAttachment3D volume;
    char path[4096],source[1024],diagnostic[512];
    unsigned long long revision;
    int64_t frame;
    bool attempted,valid;
} cache;
void SceneEditorAtmosphereGeometryReset(void) {
    RuntimeVolumeAttachment3D_Free(&cache.volume);memset(&cache,0,sizeof(cache));
}
void SceneEditorAtmospherePlaceGeometry(RuntimeVolumeAttachment3D* v,
    const SceneAtmosphereAttachment* a,double w) {
    (void)RuntimeVolume3D_SetPlacement(v,
        vec3(a->translation[0]*w,a->translation[1]*w,a->translation[2]*w),
        vec3(a->rotation_degrees[0],a->rotation_degrees[1],a->rotation_degrees[2]),
        vec3(a->scale[0],a->scale[1],a->scale[2]));
}
Vec3 SceneEditorAtmosphereCentre(const RuntimeVolumeAttachment3D* v) {
    return RuntimeVolume3D_ToWorldPoint(v,vec3_scale(vec3_add(v->grid.boundsMin,v->grid.boundsMax),.5));
}
bool SceneEditorAtmosphereSetCentre(SceneAtmosphereAttachment* a,
    const RuntimeVolumeAttachment3D* source,Vec3 centre,double w) {
    if(!a || !source || !isfinite(w) || w<=0 || !isfinite(centre.x) || !isfinite(centre.y) || !isfinite(centre.z)) return false;
    RuntimeVolumeAttachment3D placed=*source;
    SceneEditorAtmospherePlaceGeometry(&placed,a,w);
    Vec3 delta=vec3_scale(vec3_sub(centre,SceneEditorAtmosphereCentre(&placed)),1/w);
    a->translation[0]+=delta.x;a->translation[1]+=delta.y;a->translation[2]+=delta.z;
    return SceneAtmosphereAttachmentValidate(a,NULL,0);
}
static bool load(const SceneAtmosphereAttachment* a,int64_t frame,RuntimeVolumeAttachment3D* out,char* d,size_t n) {
    char root[4096],path[4096],source[4096];int kind;
    snprintf(root,sizeof(root),"%s",SceneEditorDocumentPath());char* slash=strrchr(root,'/');
    if(!slash) {if(d && n) snprintf(d,n,"Scene has no asset root.");return false;}*slash=0;
    if(frame<0 || frame>INT_MAX ||
       !SceneAtmosphereAssetPath(SceneEditorDocumentPath(),a->source_path,root,path,sizeof(path),d,n) ||
       !SceneAtmosphereAssetVolumeSource(SceneEditorDocumentPath(),path,source,sizeof(source),&kind,d,n)) return false;
    /* A single file carries its own ID even if its stored ID is not zero. */
    if(kind==RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D || kind==RUNTIME_VOLUME_3D_SOURCE_PACK) {
        uint64_t actual;
        if(!fluid_volume_import_3d_single_frame_index(source,kind,&actual) || actual>INT_MAX) return false;
        frame=(int64_t)actual;
    }
    if(!fluid_volume_import_3d_load_source_at_frame(source,kind,(int)frame,out,d,n)) return false;
    if(out->grid.frameIndex!=(uint64_t)frame) {
        if(d && n) snprintf(d,n,"Cache frame ID does not match its volume header.");return false;
    }
    return true;
}
bool SceneEditorAtmosphereLoadGeometry(const SceneAtmosphereAttachment* a,
    RuntimeVolumeAttachment3D* out,char* d,size_t n) {
    RuntimeVolumeAttachment3D_Init(out);
    TimelineEvaluationContext context;int64_t selected;
    if(!SceneAtmosphereRuntimeContext(0,&context,d,n) ||
       !SceneAtmosphereFrameForAttachment(a,SceneEditorDocumentPath(),&context,&selected,d,n)) return false;
    return load(a,selected,out,d,n);
}
bool SceneEditorAtmosphereGeometry(RuntimeVolumeAttachment3D* out,char* d,size_t n) {
    SceneAtmosphereAttachment a;TimelineSample sample;TimelineEvaluationContext context;int64_t frame=0;
    if(!SceneEditorAtmosphereRead(&a) || !a.present) return false;
    if(!SceneEditorTimelineCurrentSample(&sample) ||
       !SceneAtmosphereRuntimeContext((int)sample.absolute_frame,&context,d,n) ||
       !SceneAtmosphereRuntimeFrame(SceneEditorDocumentPath(),&context,&frame,d,n)) return false;
    unsigned long long rev=SceneEditorDocumentRevision();
    if(!cache.attempted || cache.revision!=rev || cache.frame!=frame ||
       strcmp(cache.path,SceneEditorDocumentPath()) || strcmp(cache.source,a.source_path)) {
        SceneEditorAtmosphereGeometryReset();cache.attempted=true;cache.revision=rev;cache.frame=frame;
        snprintf(cache.path,sizeof(cache.path),"%s",SceneEditorDocumentPath());
        snprintf(cache.source,sizeof(cache.source),"%s",a.source_path);
        cache.valid=load(&a,frame,&cache.volume,cache.diagnostic,sizeof(cache.diagnostic));
    }
    if(!cache.valid) {if(d && n) snprintf(d,n,"%s",cache.diagnostic);return false;}
    *out=cache.volume;out->ownsChannelBuffers=false;
    SceneEditorAtmospherePlaceGeometry(out,&a,SceneEditorDocumentWorldScale());return true;
}
bool SceneEditorAtmosphereFit(SceneAtmosphereAttachment* a,const RuntimeVolumeAttachment3D* v,const char* preset) {
    RuntimeSceneBridge3DDigestState digest;
    Vec3 lo=vec3(INFINITY,INFINITY,INFINITY),hi=vec3(-INFINITY,-INFINITY,-INFINITY);bool any=false;
    if(SceneEditorDigestOverlayResolve(&digest)) for(int i=0;i<sceneSettings.objectCount;++i) {
        if(SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[i])) continue;
        SceneEditorDocumentObjectInfo info;char id[128];
        if(runtime_scene_bridge_get_last_object_id_for_scene_index(i,id,sizeof(id)) &&
           SceneEditorDocumentObjectById(id,&info) && !info.visible) continue;
        double x,y,z,X,Y,Z,span;
        if(!SceneEditorDigestOverlayResolveObjectExtents(&digest,i,&x,&y,&z,&X,&Y,&Z,&span)) continue;
        lo=vec3(fmin(lo.x,x),fmin(lo.y,y),fmin(lo.z,z));
        hi=vec3(fmax(hi.x,X),fmax(hi.y,Y),fmax(hi.z,Z));any=true;
    }
    if(!any) {lo=vec3(-5,-5,0);hi=vec3(5,5,5);}
    Vec3 extent=vec3_sub(hi,lo),grid=vec3_sub(v->grid.boundsMax,v->grid.boundsMin);
    double span=fmax(extent.x,fmax(extent.y,extent.z));
    if(!isfinite(span) || span<=1e-9 || grid.x<=0 || grid.y<=0 || grid.z<=0) return false;
    double factor=.4*span/fmax(grid.x,fmax(grid.y,grid.z));
    a->scale[0]=a->scale[1]=a->scale[2]=factor;
    Vec3 centre=vec3_scale(vec3_add(lo,hi),.5);
    if(preset && (!strcmp(preset,"room_haze_v1") || !strcmp(preset,"low_floor_fog_v1"))) {
        a->scale[0]=fmax(extent.x,.1*span)*.95/grid.x;
        a->scale[1]=fmax(extent.y,.1*span)*.95/grid.y;
        double height=!strcmp(preset,"low_floor_fog_v1") ? .08*span : fmax(extent.z,.3*span)*.95;
        a->scale[2]=height/grid.z;
        centre.z=lo.z+height*.5;
    }
    memset(a->rotation_degrees,0,sizeof(a->rotation_degrees));
    return SceneEditorAtmosphereSetCentre(a,v,centre,SceneEditorDocumentWorldScale());
}
