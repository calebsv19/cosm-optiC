/* Shared native UI/MCP commands. Patch exact retained JSON, never reserialize
 * unrelated channels, legacy light timing or unknown producer properties. */
#include "editor/scene_editor_timeline_manage.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_object_timeline.h"
#include "editor/scene_editor_command_guard.h"
#include "import/runtime_scene_object_timeline.h"
#include "scene_editor_document_transaction.h"
#include <stdio.h>
#include <string.h>
static json_object* member(json_object* o,const char* key) {json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;}
static json_object* retained(void) {return member(member(member(member(document_authoring_root(),"extensions"),"ray_tracing"),"authoring"),"scene_timeline");}
static bool refuse(char* message,size_t size,const char* reason) {snprintf(message,size,"%s",reason);return false;}
static bool ready(unsigned long long revision,TimelineDocument* doc,char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision()) return refuse(message,size,"Scene changed; read the current revision.");
    if(SceneEditorCommandInteractionActive()) return refuse(message,size,"Finish or cancel the active editor operation first.");
    if(SceneEditorDocumentGetTimeline(doc)!=TIMELINE_STATUS_OK) return refuse(message,size,"Activate a valid scene timeline first.");
    return true;
}
static bool finish(TimelineSample before,TimelineRange range,char* message,size_t size) {
    static TimelineDocument candidate;
    if(SceneEditorDocumentGetTimeline(&candidate)!=TIMELINE_STATUS_OK) {
        document_rollback_command();return refuse(message,size,"Clock edit excludes existing keys or violates timeline bounds; no keys were changed.");
    }
    if(!document_finish_command(message,size)) return false;
    SceneEditorTimelinePause();SceneEditorTimelineClearKeys();int64_t end;TimelineRangeEndFrame(range,&end);
    if(before.absolute_frame<range.start_frame) before=(TimelineSample){range.start_frame,0,1};
    if(before.absolute_frame>end) before=(TimelineSample){end,0,1};
    SceneEditorTimelineSeekSample(before);return true;
}
bool SceneEditorTimelineSetClock(TimelineRate rate,TimelineRange range,unsigned long long revision,char* message,size_t size) {
    static TimelineDocument doc;int64_t end;
    if(!ready(revision,&doc,message,size)) return false;
    if(!TimelineRateIsValid(rate) || rate.frames_per_second_numerator>1000000 || rate.frames_per_second_denominator>1000000 ||
       range.start_frame < -1000000000 || range.start_frame>1000000000 || range.frame_count>1000000001 ||
       TimelineRangeEndFrame(range,&end)!=TIMELINE_STATUS_OK || end>1000000000) return refuse(message,size,"Invalid rational rate or inclusive frame range.");
    if(doc.rate.frames_per_second_numerator==rate.frames_per_second_numerator && doc.rate.frames_per_second_denominator==rate.frames_per_second_denominator && doc.range.start_frame==range.start_frame && doc.range.frame_count==range.frame_count) return true;
    TimelineSample before={doc.range.start_frame,0,1};SceneEditorTimelineCurrentSample(&before);
    if(!document_begin_command(message,size)) return false;
    json_object* clock=member(retained(),"rate"),*window=member(retained(),"range");
    json_object_object_add(clock,"numerator",json_object_new_int64(rate.frames_per_second_numerator));
    json_object_object_add(clock,"denominator",json_object_new_int64(rate.frames_per_second_denominator));
    json_object_object_add(window,"start_frame",json_object_new_int64(range.start_frame));
    json_object_object_add(window,"frame_count",json_object_new_int64(range.frame_count));
    return finish(before,range,message,size);
}
static bool channel(const char* id,int operation,bool enabled,unsigned long long revision,char* message,size_t size) {
    static TimelineDocument doc;if(!id || !ready(revision,&doc,message,size)) return false;
    size_t index=SIZE_MAX;
    for(size_t i=0;i<doc.track_count;++i) if(!strcmp(id,doc.tracks[i].track_id)) index=i;
    if(index==SIZE_MAX) return refuse(message,size,"Channel ID is unavailable.");
    const TimelineTrack* selected=&doc.tracks[index];
    if(!SceneEditorObjectTimelineEditable(selected->target_id,message,size)) return false;
    bool group=RuntimeObjectTimelineAxis(selected->property_id)>=0 && !strncmp(selected->target_id,"object/",7);
    bool affected[TIMELINE_DOCUMENT_TRACK_CAPACITY]={0},changed=operation==1;
    for(size_t i=0;i<doc.track_count;++i) {
        affected[i]=i==index || (group && !strcmp(selected->target_id,doc.tracks[i].target_id) && RuntimeObjectTimelineAxis(doc.tracks[i].property_id)>=0);
        if(affected[i] && doc.tracks[i].enabled!=enabled) changed=true;
    }
    if(!changed) return true;
    TimelineSample before={doc.range.start_frame,0,1};SceneEditorTimelineCurrentSample(&before);
    if(!document_begin_command(message,size)) return false;
    json_object* tracks=member(retained(),"tracks");
    for(size_t i=doc.track_count;i>0;--i) if(affected[i-1]) {
        if(operation==1) json_object_array_del_idx(tracks,i-1,1);
        else json_object_object_add(json_object_array_get_idx(tracks,i-1),"enabled",json_object_new_boolean(enabled));
    }
    /* Full scene validation in finish refuses required path/binding channels and
     * missing targets. Rollback restores exact source and history atomically. */
    return finish(before,doc.range,message,size);
}
bool SceneEditorTimelineSetChannelEnabled(const char* id,bool enabled,unsigned long long revision,char* message,size_t size) {return channel(id,0,enabled,revision,message,size);}
bool SceneEditorTimelineRemoveChannel(const char* id,unsigned long long revision,char* message,size_t size) {return channel(id,1,false,revision,message,size);}
