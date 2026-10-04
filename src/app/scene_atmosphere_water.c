#include "app/scene_atmosphere_runtime.h"
#include "app/scene_atmosphere_assets.h"
#include "app/ray_tracing_sha256.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_object* get(json_object* root,const char* key) {
    json_object* out=NULL;if(root) json_object_object_get_ex(root,key,&out);return out;
}
int SceneAtmosphereRuntimeWater(const char* scene_path,const TimelineEvaluationContext* context,
    char* path,size_t size,int* frame_index,char* d,size_t n) {
    const SceneAtmosphereAttachment* spec=SceneAtmosphereRuntimeRead();
    if(!spec->present) return 0;
    path[0]=0;
    if(!spec->enabled || strcmp(spec->source_kind,"scene_bundle")) return 1;
    TimelineEvaluationContext fallback;
    if(!context) {
        if(!SceneAtmosphereRuntimeContext(*frame_index,&fallback,d,n)) return -1;
        context=&fallback;
    }
    char bundle_path[4096];
    if(!SceneAtmosphereRuntimeSource(scene_path,bundle_path,sizeof(bundle_path),d,n)) return -1;
    json_object* bundle=json_object_from_file(bundle_path);
    if(!bundle) return -1;
    json_object* water=get(bundle,"water_source"),*relative=get(water,"path");
    if(!water) {json_object_put(bundle);return 1;}
    char project[4096];snprintf(project,sizeof(project),"%s",scene_path);
    char* slash=strrchr(project,'/');if(slash) *slash=0;else strcpy(project,".");
    bool ok=relative && json_object_is_type(relative,json_type_string) &&
        SceneAtmosphereAssetPath(bundle_path,json_object_get_string(relative),project,path,size,d,n);
    json_object_put(bundle);
    if(!ok) return -1;
    json_object* manifest=json_object_from_file(path),*frames=get(manifest,"frames");
    size_t count=frames && json_object_is_type(frames,json_type_array)?json_object_array_length(frames):0;
    if(!count || count>1000000) {if(manifest) json_object_put(manifest);goto fail;}
    int64_t* ids=malloc(count*sizeof(*ids));if(!ids) {json_object_put(manifest);goto fail;}
    for(size_t i=0;i<count;++i) {
        json_object* frame=get(json_object_array_get_idx(frames,i),"frame_index");
        if(!frame || !json_object_is_type(frame,json_type_int)) {
            free(ids);json_object_put(manifest);goto fail;
        }
        ids[i]=json_object_get_int64(frame);
    }
    json_object_put(manifest);int64_t requested,selected;
    ok=SceneAtmosphereAttachmentRequestedFrame(spec,context,&requested,d,n) &&
        SceneAtmosphereAttachmentSelectFrame(spec,requested,ids,count,&selected,d,n) && selected<=INT_MAX;
    free(ids);if(!ok) return -1;
    *frame_index=(int)selected;return 1;
fail:
    if(d && n) snprintf(d,n,"Water cache needs explicit sorted actual frame IDs.");return -1;
}

bool SceneAtmosphereRuntimeWaterDigest(const char* scene,const TimelineEvaluationContext* context,
    char content[65],char manifest_digest[65],char* d,size_t n) {
    char path[4096],selected_path[4096],project[4096];int selected=0;
    content[0]=manifest_digest[0]=0;
    int status=SceneAtmosphereRuntimeWater(scene,context,path,sizeof(path),&selected,d,n);
    if(status<0) return false;
    if(!path[0]) return true;
    snprintf(project,sizeof(project),"%s",scene);
    char* slash=strrchr(project,'/');if(slash) *slash=0;else strcpy(project,".");
    json_object* manifest=json_object_from_file(path),*frames=get(manifest,"frames");bool ok=false;
    for(size_t i=0;frames && i<json_object_array_length(frames);++i) {
        json_object* item=json_object_array_get_idx(frames,i),*index=get(item,"frame_index");
        json_object* relative=get(item,"path");
        if(index && json_object_get_int64(index)==selected && relative && json_object_is_type(relative,json_type_string)) {
            ok=SceneAtmosphereAssetPath(path,json_object_get_string(relative),project,selected_path,sizeof(selected_path),d,n) &&
                ray_tracing_sha256_file(selected_path,content) && ray_tracing_sha256_file(path,manifest_digest);
            break;
        }
    }
    if(manifest) json_object_put(manifest);
    if(!ok && d && n) snprintf(d,n,"Cannot identify selected water cache bytes.");
    return ok;
}
