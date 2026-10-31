#include "import/runtime_event_camera.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_inspection.h"
#include "import/sequence_clock_json.h"
#include "import/scene_timeline_document_io.h"
#include "import/runtime_scene_timeline.h"
#include "app/evaluated_camera_route.h"
#include "animation/timeline_property_registry.h"
#include "config/config_manager.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct CameraBase { TimelineVec3 position;double yaw,pitch,fov,zoom; } CameraBase;
typedef struct CameraRig { CameraSequence sequence;CameraBase bases[CAMERA_SEQUENCE_CAPACITY];uint64_t revision; } CameraRig;
static CameraRig runtime;
static bool loaded;
static json_object *get(json_object *o,const char *k){json_object *v=NULL;if(o)json_object_object_get_ex(o,k,&v);return v;}
static json_object *rig(json_object *s){return get(get(get(get(s,"extensions"),"ray_tracing"),"authoring"),"camera_rig");}
static bool object(json_object *o,int n){return json_object_is_type(o,json_type_object)&&json_object_object_length(o)==n;}
static bool integer(json_object *o,int64_t *out){if(!json_object_is_type(o,json_type_int))return false;errno=0;char *end;long long n=strtoll(json_object_get_string(o),&end,10);if(errno||*end)return false;*out=n;return true;}
static bool natural(json_object *o,uint64_t *out){int64_t n;if(!integer(o,&n)||n<0)return false;*out=(uint64_t)n;return true;}
static bool text(json_object *o,char *out,size_t n){if(!json_object_is_type(o,json_type_string))return false;const char *s=json_object_get_string(o);size_t len=json_object_get_string_len(o);if(!len||len>=n||strlen(s)!=len)return false;memcpy(out,s,len+1);return true;}
static bool number(json_object *o,double *v){if(!json_object_is_type(o,json_type_int)&&!json_object_is_type(o,json_type_double))return false;*v=json_object_get_double(o);return isfinite(*v);}
static bool ratio(json_object *o,SequenceTime *v){return object(o,2)&&natural(get(o,"numerator"),&v->numerator)&&natural(get(o,"denominator"),&v->denominator);}
static bool parse(json_object *root,double scale,CameraRig *out){
    CameraRig v={0};CameraSequence *s=&v.sequence;char name[64];int64_t version;SequenceTime rate;
    if(!isfinite(scale)||scale<=0||!object(root,6)||!text(get(root,"schema"),name,sizeof(name))||strcmp(name,"ray_camera_rig_v1")||!integer(get(root,"version"),&version)||version!=1||!ratio(get(root,"rate"),&rate)||rate.numerator>UINT32_MAX||rate.denominator>UINT32_MAX)return false;
    s->rate=(TimelineRate){(uint32_t)rate.numerator,(uint32_t)rate.denominator};json_object *range=get(root,"range"),*cameras=get(root,"cameras"),*shots=get(root,"shots");
    if(!object(range,2)||!integer(get(range,"start_frame"),&s->range.start_frame)||!natural(get(range,"frame_count"),&s->range.frame_count)||!json_object_is_type(cameras,json_type_array)||!json_object_is_type(shots,json_type_array))return false;
    s->camera_count=json_object_array_length(cameras);s->shot_count=json_object_array_length(shots);
    if(s->camera_count>CAMERA_SEQUENCE_CAPACITY||s->shot_count>CAMERA_SHOT_CAPACITY)return false;
    for(size_t i=0;i<s->camera_count;i++){
        json_object *o=json_object_array_get_idx(cameras,i),*clock=get(o,"clock"),*position=get(o,"position");CameraClock *c=&s->cameras[i];CameraBase *b=&v.bases[i];
        if(!object(o,7)||!text(get(o,"id"),c->id,sizeof(c->id))||!json_object_is_type(position,json_type_array)||json_object_array_length(position)!=3||!number(json_object_array_get_idx(position,0),&b->position.x)||!number(json_object_array_get_idx(position,1),&b->position.y)||!number(json_object_array_get_idx(position,2),&b->position.z)||!number(get(o,"yaw"),&b->yaw)||!number(get(o,"pitch"),&b->pitch)||!number(get(o,"fov_y"),&b->fov)||!number(get(o,"zoom"),&b->zoom)||b->fov<1||b->fov>179||fabs(b->pitch)>1.5707963267948966||b->zoom<=0||!object(clock,5)||!text(get(clock,"mode"),name,sizeof(name)))return false;
        c->policy=!strcmp(name,"continuous")?CAMERA_CLOCK_CONTINUOUS:!strcmp(name,"selected")?CAMERA_CLOCK_SELECTED:!strcmp(name,"clip")?CAMERA_CLOCK_CLIP:0;
        if(!ratio(get(clock,"offset"),&c->offset)||!ratio(get(clock,"speed"),&c->speed)||!natural(get(clock,"duration_frames"),&c->duration_frames)||!text(get(clock,"end_behavior"),name,sizeof(name))||strcmp(name,"hold"))return false;
        b->position.x*=scale;b->position.y*=scale;b->position.z*=scale;
        if(!isfinite(b->position.x)||!isfinite(b->position.y)||!isfinite(b->position.z))return false;
    }
    for(size_t i=0;i<s->shot_count;i++){
        json_object *o=json_object_array_get_idx(shots,i);CameraShot *shot=&s->shots[i];
        if(!object(o,5)||!text(get(o,"id"),shot->id,sizeof(shot->id))||!text(get(o,"camera_id"),shot->camera_id,sizeof(shot->camera_id))||!natural(get(o,"start_frame"),&shot->start_frame)||!natural(get(o,"frame_count"),&shot->frame_count)||!ratio(get(o,"clip_start"),&shot->clip_start))return false;
    }
    if(!CameraSequenceValidate(s))return false;
    const unsigned char *bytes=(const unsigned char*)json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN);v.revision=1469598103934665603ULL;
    for(;*bytes;bytes++)v.revision=(v.revision^*bytes)*1099511628211ULL;
    *out=v;return true;
}
bool RuntimeCameraRigTargetExists(json_object *scene,const char *target){
    if(!rig(scene))return !strcmp(target,"camera/main");
    json_object *list=get(rig(scene),"cameras");size_t count=0;
    for(size_t i=0;json_object_is_type(list,json_type_array)&&i<json_object_array_length(list);i++){
        char id[64];if(text(get(json_object_array_get_idx(list,i),"id"),id,sizeof(id))&&!strcmp(target,id))count++;
    }return count==1;
}
bool RuntimeCameraRigValidateScene(json_object *scene,char *d,size_t n){
    json_object *a=get(get(get(scene,"extensions"),"ray_tracing"),"authoring"),*root=NULL;
    if(!a||!json_object_object_get_ex(a,"camera_rig",&root))return true;
    CameraRig v;json_object *scale=get(scene,"world_scale");
    bool ok=parse(root,scale?json_object_get_double(scale):1,&v);
    TimelineDocument *doc=malloc(sizeof(*doc));json_object *timeline=get(a,"scene_timeline");
    if(ok&&timeline){
        ok=doc&&SceneTimelineDocumentFromJson(timeline,doc)==TIMELINE_STATUS_OK&&doc->rate.frames_per_second_numerator==v.sequence.rate.frames_per_second_numerator&&doc->rate.frames_per_second_denominator==v.sequence.rate.frames_per_second_denominator&&doc->range.start_frame==v.sequence.range.start_frame&&doc->range.frame_count==v.sequence.range.frame_count;
        for(size_t i=0;ok&&i<doc->track_count;i++)if(doc->tracks[i].enabled&&!strncmp(doc->tracks[i].target_id,"camera/",7)){
            size_t found=0;for(size_t j=0;j<v.sequence.camera_count;j++)found+=!strcmp(doc->tracks[i].target_id,v.sequence.cameras[j].id);ok=found==1;
            if(!strcmp(doc->tracks[i].property_id,"camera/path_progress"))ok=false;
        }
    }
    free(doc);json_object *sequence=get(a,"sequence");
    if(ok&&sequence){SequenceClock s;ok=SequenceClockFromJson(sequence,&s)&&s.rate.frames_per_second_numerator==v.sequence.rate.frames_per_second_numerator&&s.rate.frames_per_second_denominator==v.sequence.rate.frames_per_second_denominator&&s.range.start_frame==v.sequence.range.start_frame&&s.range.frame_count==v.sequence.range.frame_count;}
    /* Every requested camera clock must be representable by native timeline samples. */
    if(ok)for(size_t i=0;i<v.sequence.camera_count&&ok;i++)for(size_t j=0;j<v.sequence.shot_count&&ok;j++){
        uint64_t points[2]={v.sequence.shots[j].start_frame,v.sequence.shots[j].start_frame+v.sequence.shots[j].frame_count-1};
        for(size_t k=0;k<2&&ok;k++){CameraSequenceSample sample;ok=CameraSequenceEvaluate(&v.sequence,(TimelineSample){v.sequence.range.start_frame+(int64_t)points[k],0,1},v.sequence.cameras[i].id,&sample);}
    }
    if(!ok&&d&&n)snprintf(d,n,"invalid camera_rig: identity, clocks, shots or native sample range");return ok;
}
void RuntimeCameraRigReset(void){RuntimeEventCameraReset();RuntimeInspectionReset();loaded=false;memset(&runtime,0,sizeof(runtime));}
bool RuntimeCameraRigLoad(json_object *scene,double scale){RuntimeCameraRigReset();json_object *root=rig(scene);if(!root)return true;CameraRig v;if(!parse(root,scale,&v))return false;runtime=v;loaded=true;return RuntimeInspectionLoad(scene)&&RuntimeEventCameraLoad(scene);}
uint64_t RuntimeCameraRigRevision(void){return loaded?runtime.revision:0;}
bool RuntimeCameraRigClock(TimelineRate *rate,TimelineRange *range){if(!loaded||!rate||!range)return false;*rate=runtime.sequence.rate;*range=runtime.sequence.range;return true;}
bool RuntimeCameraRigEvaluateBase(const TimelineEvaluationContext *context,const char *id,RayEvaluatedCamera *out,bool *handled){
    if(!context||!out||!handled)return false;
    if(!loaded){*handled=false;return !id||!strcmp(id,"camera/main");}
    const CameraSequence *s=&runtime.sequence;
    if(context->rate.frames_per_second_numerator!=s->rate.frames_per_second_numerator||context->rate.frames_per_second_denominator!=s->rate.frames_per_second_denominator||context->range.start_frame!=s->range.start_frame||context->range.frame_count!=s->range.frame_count)return false;
    CameraSequenceSample mapping;if(!CameraSequenceEvaluate(s,context->sample,id,&mapping))return false;
    const CameraClock *clock=&s->cameras[mapping.camera_index];const CameraBase *base=&runtime.bases[mapping.camera_index];
    Camera camera={0};camera.x=base->position.x;camera.y=base->position.y;camera.rotation=base->yaw;camera.zoom=base->zoom;
    PreviewCameraSample sample;TimelineFrameSnapshot snapshot;
    TimelineStatus status=RuntimeSceneTimelineSample(mapping.motion_sample,&snapshot);
    if(status==TIMELINE_STATUS_OK){
        /* A converted main-camera route may explicitly retain legacy yaw/pitch.
           Other named views never inherit the global legacy carrier. */
        const Path *legacy_path=NULL;const CameraPath3D *legacy_depth=NULL;MotionPathBinding binding;
        if(!strcmp(clock->id,"camera/main")&&MotionPathsRuntimeBinding(clock->id,&binding)){
            const TimelineDocument *doc=RuntimeSceneTimelineRead();
            for(size_t k=0;k<binding.restore_count;k++)for(size_t j=0;doc&&j<doc->track_count;j++)
                if(!strcmp(binding.restore_xyz_tracks[k],doc->tracks[j].track_id)&&!strcmp(doc->tracks[j].property_id,"camera/path_progress")){legacy_path=&sceneSettings.cameraPath;legacy_depth=&sceneSettings.cameraPath3D;}
        }
        if(!EvaluatedCameraRouteSampleTarget(clock->id,&camera,base->position.z,legacy_path,legacy_depth,snapshot.context.normalized_t,sceneSettings.windowWidth,sceneSettings.windowHeight,&snapshot,&sample))return false;
        /* Static base lens/pitch are overridden only by their authored channels. */
        TimelineCameraChannels channels={0};
        for(size_t i=0;i<snapshot.property_count;i++)if(!strcmp(snapshot.properties[i].track.target_id,clock->id)){
            channels.has_pitch|=!strcmp(snapshot.properties[i].track.property_id,"camera/pitch");channels.has_fov|=!strcmp(snapshot.properties[i].track.property_id,"camera/fov_y");
        }
        if(!channels.has_pitch&&!sample.has_orientation_frame&&!legacy_path)sample.pitch_radians=base->pitch;
        if(!channels.has_fov)sample.fov_y_degrees=base->fov;
    }else if(status==TIMELINE_STATUS_TARGET_NOT_FOUND){
        if(!PreviewCameraSampleEvaluate(&camera,base->position.z,NULL,NULL,0,sceneSettings.windowWidth,sceneSettings.windowHeight,&sample))return false;
        sample.pitch_radians=base->pitch;sample.fov_y_degrees=base->fov;
    }else return false;
    EvaluatedCameraApplyFocusTargetFor(clock->id,&sample);
    RayEvaluatedCamera value={0};value.valid=sample.valid;value.uses_authored_path=sample.uses_authored_path;
    value.position=(TimelineVec3){sample.position_x,sample.position_y,sample.position_z};value.has_orientation_frame=sample.has_orientation_frame;value.orientation_frame=sample.orientation_frame;value.orientation_fallback=sample.orientation_fallback;
    value.yaw_radians=sample.yaw_radians;value.pitch_radians=sample.pitch_radians;value.fov_y_degrees=sample.fov_y_degrees;value.aspect_ratio=sample.aspect_ratio;value.zoom=base->zoom;
    snprintf(value.target_id,sizeof(value.target_id),"%s",clock->id);snprintf(value.shot_id,sizeof(value.shot_id),"%s",s->shots[mapping.shot_index].id);
    value.clock_policy=clock->policy;value.motion_time=mapping.motion_time;value.rig_revision=runtime.revision;value.selected=mapping.selected;
    *out=value;*handled=true;return true;
}
bool RuntimeCameraRigSnapshotMatches(const RayEvaluatedSceneSnapshot *s){
    if(!s)return false;if(!loaded)return !s->camera.rig_revision;
    RayEvaluatedCamera expected;bool handled;
    if(!s->camera.rig_revision||!(RuntimeEventCameraOwns(s->camera.target_id)?RuntimeCameraRigEvaluateForAspect(&s->frame,s->camera.target_id,s->camera.aspect_ratio,&expected,&handled):RuntimeCameraRigEvaluate(&s->frame,s->camera.target_id,&expected,&handled))||!handled)return false;
    return !strcmp(expected.target_id,s->camera.target_id)&&!strcmp(expected.shot_id,s->camera.shot_id)&&expected.rig_revision==s->camera.rig_revision&&!strcmp(expected.inspection_id,s->camera.inspection_id)&&expected.inspection_time.numerator==s->camera.inspection_time.numerator&&expected.inspection_time.denominator==s->camera.inspection_time.denominator&&expected.clock_policy==s->camera.clock_policy&&expected.selected==s->camera.selected&&expected.motion_time.numerator==s->camera.motion_time.numerator&&expected.motion_time.denominator==s->camera.motion_time.denominator&&expected.position.x==s->camera.position.x&&expected.position.y==s->camera.position.y&&expected.position.z==s->camera.position.z&&expected.yaw_radians==s->camera.yaw_radians&&expected.pitch_radians==s->camera.pitch_radians&&expected.fov_y_degrees==s->camera.fov_y_degrees&&expected.zoom==s->camera.zoom&&expected.aspect_ratio==s->camera.aspect_ratio&&expected.has_orientation_frame==s->camera.has_orientation_frame&&expected.orientation_fallback==s->camera.orientation_fallback&&(!expected.has_orientation_frame||!memcmp(&expected.orientation_frame,&s->camera.orientation_frame,sizeof(MotionFrame)));
}

json_object *RuntimeCameraRigPhysicalScene(json_object *scene){
    if(!scene)return NULL;
    bool has_rig=rig(scene)!=NULL;
    if(has_rig&&(!RuntimeCameraRigValidateScene(scene,NULL,0)||!RuntimeSceneTimelineValidateScene(scene,NULL,0)))return NULL;
    json_object *copy=NULL;if(json_object_deep_copy(scene,&copy,NULL)!=0)return NULL;
    if(!has_rig)return copy;
    json_object *bindings=get(get(get(get(get(copy,"extensions"),"ray_tracing"),"authoring"),"motion_paths"),"bindings");
    if(json_object_is_type(bindings,json_type_array))for(size_t i=json_object_array_length(bindings);i>0;i--){
        json_object *binding=json_object_array_get_idx(bindings,i-1),*target=get(binding,"target_id");
        if(json_object_is_type(target,json_type_string)&&!strncmp(json_object_get_string(target),"camera/",7)&&strcmp(json_object_get_string(target),"camera/main"))json_object_array_del_idx(bindings,i-1,1);
    }
    return copy;
}

static bool evaluate_for_aspect(const TimelineEvaluationContext*c,const char*id,double aspect,RayEvaluatedCamera*out,bool*handled){
 RayEvaluatedCamera value;bool present=false;if(!RuntimeCameraRigEvaluateBase(c,id,&value,&present))return false;
 if(!present){*handled=false;return true;}
 if(RuntimeEventCameraOwns(value.target_id)&&aspect>0){if(aspect+1e-12<RuntimeEventCameraMinimumAspect(value.target_id))return false;value.aspect_ratio=aspect;}
 if(!RuntimeInspectionApply(c,&value)||!RuntimeEventCameraApply(c,&value))return false;*out=value;*handled=true;return true;
}
bool RuntimeCameraRigEvaluate(const TimelineEvaluationContext*c,const char*id,RayEvaluatedCamera*out,bool*handled){return evaluate_for_aspect(c,id,0,out,handled);}
bool RuntimeCameraRigEvaluateForAspect(const TimelineEvaluationContext*c,const char*id,double aspect,RayEvaluatedCamera*out,bool*handled){return isfinite(aspect)&&aspect>0&&evaluate_for_aspect(c,id,aspect,out,handled);}

bool RuntimeCameraRigDeclaredClock(json_object *scene,const char *id,TimelineSample sample,CameraSequenceSample *out){CameraRig value;return parse(rig(scene),1,&value)&&CameraSequenceEvaluate(&value.sequence,sample,id,out);}
