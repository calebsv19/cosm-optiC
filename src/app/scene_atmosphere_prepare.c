#include "app/scene_atmosphere_assets.h"
#include "app/scene_atmosphere_runtime.h"
#include "import/runtime_scene_timeline.h"
#include "import/fluid_volume_import_3d.h"
#include "render/runtime_volume_3d_transform.h"
#include "config/config_manager.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

bool SceneAtmosphereRuntimeContext(int frame_index,TimelineEvaluationContext* context,char* d,size_t n) {
    TimelineRate rate;TimelineRange range;
    if(RuntimeSceneTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK) {
        rate=(TimelineRate){(uint32_t)(animSettings.fps>0?animSettings.fps:24),1};
        range=(TimelineRange){0,(uint64_t)(frame_index>=0?(int64_t)frame_index+1:1)};
    }
    if(TimelineEvaluationContextBuild(rate,range,(TimelineSample){frame_index,0,1},context)!=TIMELINE_STATUS_OK) {
        if(d && n) snprintf(d,n,"Invalid cache timeline sample.");return false;
    }
    return true;
}
bool SceneAtmosphereRuntimeSnapshotMatches(const char* scene,const RayEvaluatedSceneSnapshot* snapshot,char* d,size_t n) {
    if(!snapshot || !SceneAtmosphereRuntimeRead()->present) return true;
    RayEvaluatedSimulationIdentity identity;
    if(!SceneAtmosphereRuntimeSimulation(scene,&snapshot->frame,&identity,d,n)) return false;
    const RayEvaluatedSimulationIdentity* recorded=&snapshot->simulation;
    if(identity.valid!=recorded->valid || (identity.valid &&
       (identity.source_frame_index!=recorded->source_frame_index || identity.cache_revision!=recorded->cache_revision ||
        strcmp(identity.cache_id,recorded->cache_id) || strcmp(identity.content_digest,recorded->content_digest)))) {
        if(d && n) snprintf(d,n,"Cache changed after scene evaluation; reevaluate the frame.");return false;
    }
    return true;
}

int SceneAtmosphereRuntimeAttach(RuntimeVolumeAttachment3D* volume,const char* scene_path,
    int frame_index,const TimelineEvaluationContext* evaluated,const RayEvaluatedSimulationIdentity* identity,
    char* diagnostics,size_t diagnostics_size) {
    const SceneAtmosphereAttachment* attachment=SceneAtmosphereRuntimeRead();
    if(!attachment->present) return 0;

    if(!attachment->enabled || !attachment->visible) return 1;
    char path[4096];int64_t selected=0;
    TimelineEvaluationContext context;
    if(!evaluated) {
        if(!SceneAtmosphereRuntimeContext(frame_index,&context,diagnostics,diagnostics_size)) return -1;
        evaluated=&context;
    }
    if(!SceneAtmosphereRuntimeSource(scene_path,path,sizeof(path),diagnostics,diagnostics_size) ||
        !SceneAtmosphereRuntimeFrame(scene_path,evaluated,&selected,diagnostics,diagnostics_size) ||
        selected<0 || selected>INT_MAX) {
        return -1;
    }
    RayEvaluatedSimulationIdentity before,after;
    if(!SceneAtmosphereRuntimeSimulation(scene_path,evaluated,&before,diagnostics,diagnostics_size)) return -1;
    if(identity && (!identity->valid || identity->source_frame_index!=before.source_frame_index ||
        identity->cache_revision!=before.cache_revision || strcmp(identity->content_digest,before.content_digest))) {
        if(diagnostics && diagnostics_size) snprintf(diagnostics,diagnostics_size,"Cache changed after scene evaluation; reevaluate the frame.");
        return -1;
    }
    char volume_source[4096];int resolved_kind;
    if(!SceneAtmosphereAssetVolumeSource(scene_path,path,volume_source,sizeof(volume_source),&resolved_kind,diagnostics,diagnostics_size)) return -1;
    RuntimeVolume3DSourceKind kind=(RuntimeVolume3DSourceKind)resolved_kind;
    if(!fluid_volume_import_3d_load_source_at_frame(volume_source,kind,(int)selected,volume,diagnostics,diagnostics_size)) {
        return -1;
    }
    if(volume->grid.frameIndex!=(uint64_t)selected) {
        if(diagnostics && diagnostics_size) snprintf(diagnostics,diagnostics_size,"Cache manifest frame ID does not match the selected volume header.");
        return -1;
    }
    if(!SceneAtmosphereRuntimeSimulation(scene_path,evaluated,&after,diagnostics,diagnostics_size) ||
       after.cache_revision!=before.cache_revision || strcmp(before.content_digest,after.content_digest)) {
        if(diagnostics && diagnostics_size) snprintf(diagnostics,diagnostics_size,"Cache changed during frame preparation.");
        return -1;
    }
    volume->affectsLighting=attachment->affects_lighting;
    volume->debugOverlayEnabled=animSettings.volumeDebugOverlayEnabled;
    double w=SceneAtmosphereRuntimeWorldScale();
    return RuntimeVolume3D_SetPlacement(volume,
        vec3(attachment->translation[0]*w,attachment->translation[1]*w,attachment->translation[2]*w),
        vec3(attachment->rotation_degrees[0],attachment->rotation_degrees[1],attachment->rotation_degrees[2]),
        vec3(attachment->scale[0],attachment->scale[1],attachment->scale[2]))?1:-1;
}
