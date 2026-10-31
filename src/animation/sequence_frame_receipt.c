#include "animation/sequence_frame_receipt.h"
#include <inttypes.h>
#include "import/runtime_event_camera.h"
#include "app/ray_tracing_sha256.h"
#include <stdio.h>
#include <string.h>
static void quoted(FILE *f,const char *text) {
    fputc('"',f);
    for(const unsigned char *p=(const unsigned char*)text;*p;p++) {
        if(*p=='"'||*p=='\\'){fputc('\\',f);fputc(*p,f);}
        else if(*p<32)fprintf(f,"\\u%04x",*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static void ratio(FILE *f,const char *key,SequenceTime t) {
    fprintf(f,"\"%s\":[%" PRIu64 ",%" PRIu64 "]",key,t.numerator,t.denominator);
}
bool SequenceFrameReceiptWrite(const char *path,const RayEvaluatedSceneSnapshot *s,const char *scene_path,bool overwrite) {
    if(!s||!path)return false;
    if(!s->has_sequence)return true;
    if(RayEvaluatedSceneSnapshotValidate(s)!=TIMELINE_STATUS_OK)return false;
    char sidecar[4096];int n=snprintf(sidecar,sizeof(sidecar),"%s.sequence.json",path);
    if(n<0||(size_t)n>=sizeof(sidecar))return false;
    char scene_hash[65],image_hash[65];
    if(!scene_path || !ray_tracing_sha256_file(scene_path,scene_hash) || !ray_tracing_sha256_file(path,image_hash))return false;
    FILE *f=fopen(sidecar,overwrite?"w":"wx");if(!f)return false;
    const SequenceEvaluation *q=&s->sequence;
    fprintf(f,"{\"schema\":\"ray_sequence_frame_v1\",\"frame\":%" PRId64 ",\"rate\":[%u,%u],",
        s->frame.sample.absolute_frame,s->frame.rate.frames_per_second_numerator,s->frame.rate.frames_per_second_denominator);
    fprintf(f,"\"scene_sha256\":\"%s\",\"image_sha256\":\"%s\",",scene_hash,image_hash);
    if(!strncmp(s->camera.inspection_id,"event_",6))fprintf(f,"\"event_validation_policy\":\"%s\",",RAY_EVENT_GEOMETRY_VALIDATION_POLICY);
    if(s->camera.rig_revision){
        fprintf(f,"\"camera_id\":\"%s\",\"shot_id\":\"%s\",\"camera_clock_policy\":%d,\"camera_selected\":%s,",s->camera.target_id,s->camera.shot_id,s->camera.clock_policy,s->camera.selected?"true":"false");
        fprintf(f,"\"camera_yaw\":%.17g,\"camera_pitch\":%.17g,\"camera_fov_y\":%.17g,\"camera_zoom\":%.17g,\"camera_aspect\":%.17g,\"camera_has_orientation_frame\":%s,\"camera_frame\":[",s->camera.yaw_radians,s->camera.pitch_radians,s->camera.fov_y_degrees,s->camera.zoom,s->camera.aspect_ratio,s->camera.has_orientation_frame?"true":"false");
        for(int i=0;i<3;i++)fprintf(f,"%s%.17g",i?",":"",s->camera.orientation_frame.forward[i]);
        for(int i=0;i<3;i++)fprintf(f,",%.17g",s->camera.orientation_frame.up[i]);
        for(int i=0;i<3;i++)fprintf(f,",%.17g",s->camera.orientation_frame.right[i]);
        fputs("],",f);
        if(s->camera.inspection_id[0]){fprintf(f,"\"inspection_id\":\"%s\",",s->camera.inspection_id);ratio(f,"inspection_time",s->camera.inspection_time);fputs(",",f);}
        ratio(f,"camera_motion_time",s->camera.motion_time);fputs(",",f);
    }
    ratio(f,"presentation_time",q->presentation_time);fputs(",",f);ratio(f,"simulation_time",q->simulation_time);
    fprintf(f,",\"segment\":%zu,\"source_tick\":%" PRIu64 ",\"source_revision\":\"%s\",\"camera\":[%.17g,%.17g,%.17g],\"bodies\":[",
        q->segment_index,q->source_tick,q->source.revision,s->camera.position.x,s->camera.position.y,s->camera.position.z);
    size_t count=0;
    for(size_t i=0;i<s->object_transform_count;++i) {
        const RayEvaluatedObjectTransform *t=&s->object_transforms[i];
        if(t->source!=RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT)continue;
        fprintf(f,"%s{\"body_id\":%u,\"tick\":%" PRIu64 ",\"position\":[%.17g,%.17g,%.17g],\"q\":[%.17g,%.17g,%.17g,%.17g]}",
          count++?",":"",t->rigid_body_id,t->source_tick,t->position.x,t->position.y,t->position.z,
          t->orientation_quaternion.w,t->orientation_quaternion.x,t->orientation_quaternion.y,t->orientation_quaternion.z);
    }
    fputs("]",f);
    if(s->simulation.valid){fputs(",\"cache\":{\"id\":",f);quoted(f,s->simulation.cache_id);fprintf(f,",\"tick\":%" PRId64 ",\"content_digest\":\"%s\"}",s->simulation.source_frame_index,s->simulation.content_digest);}
    fputs("}\n",f);bool ok=!ferror(f);if(fclose(f))ok=false;
    if(!ok)remove(sidecar);return ok;
}
