/* Stable-ID MCP adapter. Native timeline commands own all edits and evaluation. */
#include "editor/scene_editor_agent_animation.h"
#include "editor/scene_editor_timeline_manage.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_object_timeline.h"
#include "import/scene_timeline_document_io.h"
#include "scene_editor_timeline_commands.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object* member(json_object* p,const char* key) {
    json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;
}
static const char* text(json_object* p,const char* key) {
    json_object* v=member(p,key);if(!json_object_is_type(v,json_type_string)) return NULL;
    const char* s=json_object_get_string(v);return strlen(s)==(size_t)json_object_get_string_len(v)?s:NULL;
}
static bool fields(json_object* p,const char* const* keys,size_t n) {
    if(!json_object_is_type(p,json_type_object) || json_object_object_length(p)!=(int)n) return false;
    for(size_t i=0;i<n;++i) if(!member(p,keys[i])) return false;return true;
}
static bool integer(json_object* p,const char* k,int64_t* v) {
    json_object* x=member(p,k);if(!json_object_is_type(x,json_type_int)) return false;*v=json_object_get_int64(x);return true;
}
static bool number(json_object* p,const char* k,double* v) {
    json_object* x=member(p,k);if(!json_object_is_type(x,json_type_int) && !json_object_is_type(x,json_type_double)) return false;
    *v=json_object_get_double(x);return isfinite(*v);
}
static const TimelineTrack* track_by_id(const TimelineDocument* d,const char* id,size_t* index) {
    if(d && id) for(size_t i=0;i<d->track_count;++i) if(!strcmp(id,d->tracks[i].track_id)) {*index=i;return &d->tracks[i];}return NULL;
}
static bool output(json_object* response,const char* key,json_object* value,char* message,size_t size) {
    if(!value) return false;
    if(strlen(json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN))>42000) {
        json_object_put(value);snprintf(message,size,"Animation readback exceeds control capacity; read individual tracks.");return false;
    }
    json_object_object_add(response,key,value);return true;
}
bool SceneEditorAgentAnimationReadOnly(const char* name) {return !strcmp(name,"animation.read") || !strcmp(name,"animation.track_read");}
bool SceneEditorAgentAnimationDispatch(const char* name,json_object* p,json_object* response,char* message,size_t size) {
    static TimelineDocument saved;
    if(!strcmp(name,"animation.read")) {
        if(!fields(p,NULL,0)) goto invalid;
        TimelineStatus status=SceneEditorDocumentGetTimeline(&saved);
        json_object* info=json_object_new_object();
        json_object_object_add(info,"present",json_object_new_boolean(status!=TIMELINE_STATUS_TARGET_NOT_FOUND));
        json_object_object_add(info,"valid",json_object_new_boolean(status==TIMELINE_STATUS_OK));
        json_object_object_add(info,"world_scale",json_object_new_double(SceneEditorDocumentWorldScale()));
        json_object* tracks=json_object_new_array();json_object_object_add(info,"tracks",tracks);
        if(status==TIMELINE_STATUS_OK) {
            json_object* encoded=SceneTimelineDocumentToJson(&saved);
            json_object_object_add(info,"rate",json_object_get(member(encoded,"rate")));
            json_object_object_add(info,"range",json_object_get(member(encoded,"range")));json_object_put(encoded);
            for(size_t i=0;i<saved.track_count;++i) {
                const TimelineTrack* t=&saved.tracks[i];json_object* row=json_object_new_object();
                json_object_object_add(row,"track_id",json_object_new_string(t->track_id));
                json_object_object_add(row,"target_id",json_object_new_string(t->target_id));
                json_object_object_add(row,"property_id",json_object_new_string(t->property_id));
                json_object_object_add(row,"unit",json_object_new_string(TimelineUnitLabel(t->unit)));
                json_object_object_add(row,"enabled",json_object_new_boolean(t->enabled));
                json_object_object_add(row,"key_count",json_object_new_int64(t->key_count));json_object_array_add(tracks,row);
            }
        }
        return output(response,"animation",info,message,size);
    }
    if(!strcmp(name,"animation.set_clock")) {
        const char* keys[]={"rate_numerator","rate_denominator","start_frame","frame_count","expected_revision"};int64_t n,d,start,count;
        if(!fields(p,keys,5) || !integer(p,keys[0],&n) || !integer(p,keys[1],&d) || !integer(p,keys[2],&start) || !integer(p,keys[3],&count) || n<1 || n>1000000 || d<1 || d>1000000 || count<1) goto invalid;
        return SceneEditorTimelineSetClock((TimelineRate){(uint32_t)n,(uint32_t)d},(TimelineRange){start,(uint64_t)count},SceneEditorDocumentRevision(),message,size);
    }
    if(!strcmp(name,"animation.set_channel_enabled")) {
        const char* keys[]={"track_id","enabled","expected_revision"};json_object* v=member(p,"enabled");
        if(!fields(p,keys,3) || !text(p,"track_id") || !json_object_is_type(v,json_type_boolean)) goto invalid;
        return SceneEditorTimelineSetChannelEnabled(text(p,"track_id"),json_object_get_boolean(v),SceneEditorDocumentRevision(),message,size);
    }
    if(!strcmp(name,"animation.remove_channel")) {
        const char* keys[]={"track_id","expected_revision"};if(!fields(p,keys,2) || !text(p,"track_id")) goto invalid;
        return SceneEditorTimelineRemoveChannel(text(p,"track_id"),SceneEditorDocumentRevision(),message,size);
    }
    const TimelineDocument* doc=SceneEditorTimelineDocumentView(NULL);size_t index=SIZE_MAX;
    const TimelineTrack* track=track_by_id(doc,text(p,"track_id"),&index);
    if(!strcmp(name,"animation.track_read")) {
        const char* keys[]={"track_id"};if(!fields(p,keys,1) || !track) goto invalid;
        /* One track bounds the socket response, even for a full document. */
        static TimelineDocument one;one=*doc;one.track_count=1;one.tracks[0]=*track;
        json_object* encoded=SceneTimelineDocumentToJson(&one);
        json_object* value=json_object_get(json_object_array_get_idx(member(encoded,"tracks"),0));json_object_put(encoded);
        return output(response,"track",value,message,size);
    }
    if(!strcmp(name,"animation.activate")) {
        const char* keys[]={"expected_revision"};if(!fields(p,keys,1)) goto invalid;
        bool ok=SceneEditorTimelineActivate();if(!ok) snprintf(message,size,"%s",SceneEditorTimelineStatus());return ok;
    }
    if(!strcmp(name,"animation.object_channels")) {
        const char* keys[]={"object_id","expected_revision"};
        if(!fields(p,keys,2) || !text(p,"object_id") || !doc) goto invalid;
        /* Explicit activation avoids two history commands or partial setup. */
        return SceneEditorObjectTimelineAdd(text(p,"object_id"),message,size);
    }
    if(!strcmp(name,"animation.add_channel")) {
        const char* keys[]={"track_id","property_id","expected_revision"};const char* property=text(p,"property_id");
        if(!fields(p,keys,3) || !track || !property || !track->enabled ||
           (strcmp(property,"camera/yaw") && strcmp(property,"camera/pitch") && strcmp(property,"light/intensity")) ||
           (!strncmp(property,"camera/",7)?strcmp(track->target_id,"camera/main"):strncmp(track->target_id,"light/",6))) goto invalid;
        SceneEditorTimelinePause();bool ok=SceneEditorTimelineSelectTrack(index) && SceneEditorTimelineAddChannel(property);
        if(!ok) snprintf(message,size,"%s",SceneEditorTimelineStatus());return ok;
    }
    if(!track || !track->enabled || track->source!=TIMELINE_CHANNEL_SOURCE_AUTHORED || track->value_type!=TIMELINE_VALUE_SCALAR ||
       !SceneEditorObjectTimelineEditable(track->target_id,message,size)) goto invalid;
    int64_t frame,end;double value=0,handles[4]={0};int mode=-1;
    if(!integer(p,"frame",&frame) || TimelineRangeEndFrame(doc->range,&end)!=TIMELINE_STATUS_OK || frame<doc->range.start_frame || frame>end) goto invalid;
    const char* key_keys[]={"track_id","frame","value","expected_revision"};
    const char* delete_keys[]={"track_id","frame","expected_revision"};
    const char* move_keys[]={"track_id","frame","destination_frame","expected_revision"};
    const char* mode_keys[]={"track_id","frame","mode","expected_revision"};
    const char* handle_keys[]={"track_id","frame","incoming_frames","incoming_value","outgoing_frames","outgoing_value","expected_revision"};
    int64_t destination=0;bool existing=false;
    for(size_t k=0;k<track->key_count;++k) if(track->keys[k].frame==frame) existing=true;
    if(!strcmp(name,"animation.set_key")) {if(!fields(p,key_keys,4) || !number(p,"value",&value)) goto invalid;}
    else if(!strcmp(name,"animation.delete_key")) {if(!fields(p,delete_keys,3) || !existing || track->key_count<2) goto invalid;}
    else if(!strcmp(name,"animation.move_key")) {
        if(!fields(p,move_keys,4) || !existing || !integer(p,"destination_frame",&destination) || destination<doc->range.start_frame || destination>end) goto invalid;
        for(size_t k=0;k<track->key_count;++k) if(track->keys[k].frame==destination && destination!=frame) goto invalid;
    } else if(!strcmp(name,"animation.interpolation") || !strcmp(name,"animation.tangent")) {
        const char* label=text(p,"mode");if(!fields(p,mode_keys,4) || !existing || !label) goto invalid;
        int count=!strcmp(name,"animation.interpolation")?3:4;
        for(int m=0;m<count;++m) if(!strcmp(label,count==3?TimelineInterpolationLabel(m):TimelineTangentModeLabel(m))) mode=m;
        if(mode<0) goto invalid;
    } else if(!strcmp(name,"animation.handles")) {
        if(!fields(p,handle_keys,7) || !existing) goto invalid;
        for(int i=0;i<4;++i) if(!number(p,handle_keys[i+2],&handles[i])) goto invalid;
    } else goto invalid;
    SceneEditorTimelinePause();if(!SceneEditorTimelineSelectTrack(index)) goto invalid;
    bool ok=false;
    if(!strcmp(name,"animation.set_key")) {
        TimelineSample original;SceneEditorTimelineCurrentSample(&original);
        ok=SceneEditorTimelineSeek(frame) && SceneEditorTimelineSetKey(value);SceneEditorTimelineSeekSample(original);
        if(ok) SceneEditorTimelineSelectKey(frame,false);
    } else if(SceneEditorTimelineSelectKey(frame,false)) {
        if(!strcmp(name,"animation.delete_key")) ok=SceneEditorTimelineDeleteSelectedKeys();
        else if(!strcmp(name,"animation.move_key")) ok=SceneEditorTimelineMoveSelectedKeys(destination);
        else if(!strcmp(name,"animation.interpolation")) ok=SceneEditorTimelineSelectedInterpolation(mode);
        else if(!strcmp(name,"animation.tangent")) ok=SceneEditorTimelineSelectedTangentMode(mode);
        else ok=SceneEditorTimelineSelectedHandles(handles[0],handles[1],handles[2],handles[3]);
    }
    if(!ok) snprintf(message,size,"Animation edit rejected: %s %s",SceneEditorTimelineStatus(),SceneEditorTimelineSelectionStatus());return ok;
invalid:
    if(!message[0]) snprintf(message,size,"Invalid animation arguments, unavailable/disabled track, or frame outside range. Activate before adding object channels.");return false;
}
