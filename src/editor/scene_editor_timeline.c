#include "editor/scene_editor_object_commands.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_object_timeline.h"
#include "scene_editor_timeline_ui.h"
#include "scene_editor_timeline_commands.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_workspace_profile.h"
#include "app/scene_timeline_session.h"
#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_bridge.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static TimelineDocument document;
static SceneTimelineSession session;
static TimelineEntityBindings bindings;
static TimelinePropertyRegistry registry;
static bool available;
static size_t selected;
static char selected_track[TIMELINE_ID_CAPACITY];
static uint64_t last_ms;
static char status_line[256];
static char document_path[4096];

void SceneEditorTimelineClearSelection(void) {SceneEditorTimelineClearKeys();selected=SIZE_MAX;snprintf(selected_track,sizeof(selected_track),"@none");}
void SceneEditorTimelineReleaseFocus(void) { SceneEditorTimelineUIReleaseFocus(); }
static bool sync_document(void) {
    unsigned long long revision = SceneEditorDocumentRevision();
    const char* path = SceneEditorDocumentPath();
    bool changed_scene = strcmp(document_path, path ? path : "") != 0;
    if (changed_scene) {
        memset(&session, 0, sizeof(session));
        selected = 0;
        selected_track[0] = 0;
        SceneEditorTimelineUIReset();
        SceneEditorTimelineSelectionReset();
        snprintf(document_path, sizeof(document_path), "%s", path ? path : "");
    }
    if (available && !changed_scene && session.scene_revision == revision) return true;
    available = SceneEditorDocumentGetTimeline(&document) == TIMELINE_STATUS_OK;
    if (!available) { session.transport.playing = false; return false; }
    TimelinePropertyRegistryInitFoundationDefaults(&registry);
    memset(&bindings, 0, sizeof(bindings));
    TimelineEntityBindingsAdd(&bindings, "main-camera", "camera/main", TIMELINE_PROPERTY_TARGET_CAMERA, true, false);
    RuntimeSceneBridge3DLightSeedState lights;
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    for (int i=0; lights.valid && i<lights.light_count; ++i) {
        char target[TIMELINE_ID_CAPACITY];
        int n = snprintf(target, sizeof(target), "light/%s", lights.lights[i].id);
        if (n>0 && (size_t)n<sizeof(target))
            TimelineEntityBindingsAdd(&bindings, lights.lights[i].id, target, TIMELINE_PROPERTY_TARGET_LIGHT, true, false);
    }
    SceneEditorObjectTimelineBindings(&bindings);
    if (!session.transport.valid || memcmp(&session.transport.rate, &document.rate, sizeof(document.rate)) ||
        memcmp(&session.transport.range, &document.range, sizeof(document.range)))
        SceneTimelineSessionInit(&session, document.rate, document.range, revision, revision);
    else SceneTimelineSessionReconcile(&session, &bindings, revision, revision);
    selected = SIZE_MAX;
    for (size_t i=0;i<document.track_count;++i)
        if (!strcmp(selected_track,document.tracks[i].track_id)) {selected=i;break;}
    if (!selected_track[0] && document.track_count) selected=0;
    if (selected<document.track_count) {
        snprintf(selected_track,sizeof(selected_track),"%s",document.tracks[selected].track_id);
        SceneTimelineSessionSelect(&session, &bindings, &registry,
            document.tracks[selected].target_id, document.tracks[selected].property_id);
    }
    return true;
}
void SceneEditorTimelinePause(void) {if(sync_document()) SceneTimelineSessionSetPlaying(&session,false);}
bool SceneEditorTimelineSelectedTrack(TimelineTrack* track,TimelineRate* rate,TimelineRange* range,TimelineSample* sample) {
    if(!track || !rate || !range || !sample || !sync_document() || selected>=document.track_count) return false;
    *track=document.tracks[selected];*rate=document.rate;*range=document.range;*sample=session.transport.sample;return true;
}
static bool add_camera_track(const char* id, const char* property, TimelineUnit unit, double first, double last) {
    TimelineTrack track;
    int64_t end;
    if (TimelineRangeEndFrame(document.range, &end) != TIMELINE_STATUS_OK ||
        TimelineTrackInit(&track, id, "camera/main", property, TIMELINE_VALUE_SCALAR) != TIMELINE_STATUS_OK ||
        TimelineTrackSetUnit(&track, unit) != TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(&track, document.range.start_frame, TimelineValueScalar(first), TIMELINE_INTERPOLATION_LINEAR) != TIMELINE_STATUS_OK)
        return false;
    if (end != document.range.start_frame && TimelineTrackAddKey(&track, end, TimelineValueScalar(last), TIMELINE_INTERPOLATION_STEP) != TIMELINE_STATUS_OK) return false;
    return TimelineDocumentAddTrack(&document, &track) == TIMELINE_STATUS_OK;
}
const char* SceneEditorTimelineStatus(void) { return status_line; }
bool SceneEditorTimelineActivate(void) {
    if (sync_document()) return true;
    if (!SceneEditorDocumentIsOpen()) { snprintf(status_line,sizeof(status_line),"Open a saved runtime scene to author its timeline."); return false; }
    TimelineStatus existing = SceneEditorDocumentGetTimeline(&document);
    if (existing != TIMELINE_STATUS_TARGET_NOT_FOUND) {
        snprintf(status_line,sizeof(status_line),"Existing timeline cannot be loaded; activation refused.");
        return false;
    }
    static RuntimeSceneLightTimelineDocument legacy;
    bool have_light=RuntimeSceneLightTimelineGetLast(&legacy), create_light=false;
    if (have_light) document = legacy.timeline;
    else if (TimelineDocumentInit(&document, (TimelineRate){(uint32_t)(animSettings.fps>0?animSettings.fps:24),1},
        (TimelineRange){0,(uint64_t)(animSettings.framesForTravel>0?animSettings.framesForTravel:120)}) != TIMELINE_STATUS_OK) return false;
    if(!have_light && sceneSettings.bezierPath.numPoints>=2) {
        RuntimeSceneBridge3DLightSeedState lights;runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
        if(lights.valid && lights.light_count>1) {
            snprintf(status_line,sizeof(status_line),"Several lights exist. Bind the intended light path before setup.");return false;
        }
        if(lights.valid && lights.light_count==1 && lights.lights[0].id[0]) {
            memset(&legacy,0,sizeof(legacy));legacy.timeline=document;
            legacy.valid=true;legacy.spatial_path=sceneSettings.bezierPath;legacy.spatial_path_3d=sceneSettings.bezierPath3D;
            TimelineTrack progress;char target[TIMELINE_ID_CAPACITY];int64_t end;
            snprintf(target,sizeof(target),"light/%s",lights.lights[0].id);
            if(TimelineRangeEndFrame(document.range,&end)!=TIMELINE_STATUS_OK ||
               TimelineTrackInit(&progress,"light-progress",target,"light/path_progress",TIMELINE_VALUE_SCALAR)!=TIMELINE_STATUS_OK ||
               TimelineTrackSetUnit(&progress,TIMELINE_UNIT_UNITLESS)!=TIMELINE_STATUS_OK ||
               TimelineTrackAddKey(&progress,document.range.start_frame,TimelineValueScalar(0),TIMELINE_INTERPOLATION_LINEAR)!=TIMELINE_STATUS_OK ||
               TimelineTrackAddKey(&progress,end,TimelineValueScalar(1),TIMELINE_INTERPOLATION_STEP)!=TIMELINE_STATUS_OK ||
               TimelineDocumentAddTrack(&legacy.timeline,&progress)!=TIMELINE_STATUS_OK) return false;
            legacy.progress_track_index=0;document=legacy.timeline;create_light=true;
        }
    }
    if (sceneSettings.cameraPath.numPoints && !add_camera_track("camera-progress", "camera/path_progress", TIMELINE_UNIT_UNITLESS, 0, 1)) return false;
    static RayEvaluatedSceneServiceResult before;
    double fov=55;
    if(RayEvaluatedSceneCaptureForElapsed(0,&before) && before.snapshot.camera.valid) fov=before.snapshot.camera.fov_y_degrees;
    if (!add_camera_track("camera-lens", "camera/fov_y", TIMELINE_UNIT_DEGREES, fov,fov)) return false;
    if (!SceneEditorDocumentSetTimelineWithLight(&document,create_light?&legacy:NULL, SceneEditorDocumentRevision(), status_line, sizeof(status_line))) return false;
    available = false;
    return sync_document();
}
bool SceneEditorTimelineSelectTrack(size_t index) {
    if (!sync_document() || index >= document.track_count) return false;
    TimelineTrack* track=&document.tracks[index];
    if (SceneTimelineSessionSelect(&session,&bindings,&registry,track->target_id,track->property_id)!=TIMELINE_STATUS_OK) return false;
    if (!strncmp(track->target_id,"object/",7)) {
        SceneEditorObjectReadback object;
        if (!SceneEditorObjectExecute(SCENE_OBJECT_SELECT,track->target_id+7,NULL,false,
            SceneEditorDocumentRevision(),&object,status_line,sizeof(status_line))) return false;
    }
    selected=index;
    snprintf(selected_track,sizeof(selected_track),"%s",track->track_id);
    return true;
}
bool SceneEditorTimelineSeek(int64_t frame) {
    return SceneEditorTimelineSeekSample((TimelineSample){frame,0,1});
}
bool SceneEditorTimelineSeekSample(TimelineSample sample) {
    return sync_document() && SceneTimelineSessionSeek(&session,sample)==TIMELINE_STATUS_OK;
}
bool SceneEditorTimelineCurrentSample(TimelineSample* sample) {
    if(!sample || !sync_document()) return false;
    *sample=session.transport.sample;return true;
}
bool SceneEditorTimelineCopyEvaluated(RayEvaluatedSceneSnapshot* out) {
    RayEvaluatedSceneServiceResult result;
    if (!out || !sync_document() || !RayEvaluatedSceneCaptureSample(session.transport.sample,&result)) return false;
    *out=result.snapshot;
    return true;
}
/* A new/moved key shortens adjacent segments. Fit temporal handles into their
 * new interval while retaining each handle slope; never reorder key times. */
static void fit_temporal_handles(TimelineTrack* track) {
    if(track->value_type!=TIMELINE_VALUE_SCALAR) return;
    for(size_t i=0;i+1<track->key_count;++i) {
        TimelineKeyframe* left=&track->keys[i];
        TimelineKeyframe* right=&track->keys[i+1];
        double span=(double)(right->frame-left->frame);
        double extent=left->outgoing_frame_offset-right->incoming_frame_offset;
        if(span>0 && extent>span) {
            double scale=span/extent;
            left->outgoing_frame_offset*=scale;
            left->outgoing_value_offset*=scale;
            right->incoming_frame_offset*=scale;
            right->incoming_value_offset*=scale;
        }
    }
}
bool SceneEditorTimelineAddChannel(const char* property) {
    if(!property || !sync_document() || selected>=document.track_count) return false;
    const char* target=document.tracks[selected].target_id;
    bool light=!strcmp(property,"light/intensity");
    bool yaw=!strcmp(property,"camera/yaw"), pitch=!strcmp(property,"camera/pitch");
    if((!light && !yaw && !pitch) || (light?strncmp(target,"light/",6):strcmp(target,"camera/main"))) {
        snprintf(status_line,sizeof(status_line),"Select a matching camera or light track first.");return false;
    }
    for(size_t i=0;i<document.track_count;++i)
        if(!strcmp(document.tracks[i].target_id,target) && !strcmp(document.tracks[i].property_id,property))
            return SceneEditorTimelineSelectTrack(i);
    RayEvaluatedSceneSnapshot snapshot;
    if(!SceneEditorTimelineCopyEvaluated(&snapshot) ||
       (light && (!snapshot.light.valid || strcmp(snapshot.light.target_id,target)))) return false;
    double value=light?snapshot.light.intensity:yaw?snapshot.camera.yaw_radians:snapshot.camera.pitch_radians;
    const TimelinePropertyDescriptor* descriptor=NULL;
    if(TimelinePropertyRegistryFind(&registry,property,&descriptor)!=TIMELINE_STATUS_OK) return false;
    TimelineTrack track;char id[TIMELINE_ID_CAPACITY];
    unsigned candidate=0;bool collision;
    do {
        snprintf(id,sizeof(id),"scene-channel-%u",candidate++);collision=false;
        for(size_t i=0;i<document.track_count;++i) if(!strcmp(id,document.tracks[i].track_id)) collision=true;
    } while(collision);
    if(TimelineTrackInit(&track,id,target,property,TIMELINE_VALUE_SCALAR)!=TIMELINE_STATUS_OK ||
       TimelineTrackSetUnit(&track,descriptor->unit)!=TIMELINE_STATUS_OK ||
       TimelineTrackAddKey(&track,document.range.start_frame,TimelineValueScalar(value),TIMELINE_INTERPOLATION_LINEAR)!=TIMELINE_STATUS_OK ||
       SceneTimelineSessionBeginEdit(&session,SceneEditorDocumentRevision(),session.timeline_revision)!=TIMELINE_STATUS_OK) return false;
    bool ok=TimelineDocumentAddTrack(&document,&track)==TIMELINE_STATUS_OK &&
        TimelineEntityBindingsValidateDocument(&bindings,&registry,&document)==TIMELINE_STATUS_OK &&
        SceneEditorDocumentSetTimeline(&document,session.scene_revision,status_line,sizeof(status_line));
    SceneTimelineSessionEndEdit(&session);available=false;
    if(ok) snprintf(selected_track,sizeof(selected_track),"%s",id);
    sync_document();
    if(ok) {snprintf(status_line,sizeof(status_line),"Channel added from the current evaluated value.");}
    return ok;
}
bool SceneEditorTimelineSetKey(double value) {
    if (!sync_document() || selected>=document.track_count || !isfinite(value)) return false;
    TimelineTrack* track=&document.tracks[selected];
    if(!SceneEditorObjectTimelineEditable(track->target_id,status_line,sizeof(status_line))) return false;
    if (track->value_type!=TIMELINE_VALUE_SCALAR ||
        SceneTimelineSessionBeginEdit(&session,SceneEditorDocumentRevision(),session.timeline_revision)!=TIMELINE_STATUS_OK) return false;
    TimelineKeyframe key={0};
    key.frame=session.transport.sample.absolute_frame;
    key.value=TimelineValueScalar(value);
    key.interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR;
    bool replaced=false;
    for(size_t i=0;i<track->key_count;++i) if(track->keys[i].frame==key.frame) {
        track->keys[i].value=key.value; replaced=true; break;
    }
    size_t inserted_index=0;
    TimelineStatus status=replaced?TIMELINE_STATUS_OK:TimelineTrackInsertKey(track,key,&inserted_index);
    if(status==TIMELINE_STATUS_OK && !replaced) fit_temporal_handles(track);
    bool ok=status==TIMELINE_STATUS_OK && TimelineEntityBindingsValidateDocument(&bindings,&registry,&document)==TIMELINE_STATUS_OK &&
        SceneEditorDocumentSetTimeline(&document,session.scene_revision,status_line,sizeof(status_line));
    SceneTimelineSessionEndEdit(&session);
    available=false;
    sync_document();
    if(!ok) snprintf(status_line,sizeof(status_line),"Key rejected: check property bounds, target, and scene revision.");
    return ok;
}
static size_t key_at_playhead(void) {
    if(selected>=document.track_count) return SIZE_MAX;
    TimelineTrack* track=&document.tracks[selected];
    for(size_t i=0;i<track->key_count;++i)
        if(track->keys[i].frame==session.transport.sample.absolute_frame) return i;
    return SIZE_MAX;
}
/* All mutations stage in the local copy and commit exactly one retained command. */
static bool edit_existing_key(int operation, int64_t destination,
    TimelineInterpolation interpolation, const double* handles) {
    if(!sync_document() || selected>=document.track_count) return false;
    size_t index=key_at_playhead();
    if(index==SIZE_MAX) {snprintf(status_line,sizeof(status_line),"Select a key first.");return false;}
    if(SceneTimelineSessionBeginEdit(&session,SceneEditorDocumentRevision(),session.timeline_revision)!=TIMELINE_STATUS_OK) return false;
    TimelineTrack* track=&document.tracks[selected];
    TimelineStatus status=TIMELINE_STATUS_OK;
    if(operation==0) {
        if(track->key_count<=1) status=TIMELINE_STATUS_INVALID_TRACK;
        else status=TimelineTrackRemoveKey(track,index);
    } else if(operation==1) {
        status=TimelineTrackMoveScalarKey(track,index,destination,track->keys[index].value.as.scalar);
        if(status==TIMELINE_STATUS_OK) fit_temporal_handles(track);
    } else if(operation==2) {
        track->keys[index].interpolation_to_next=interpolation;
        if(interpolation==TIMELINE_INTERPOLATION_CUBIC_BEZIER && index+1<track->key_count) {
            double span=(double)(track->keys[index+1].frame-track->keys[index].frame)/3.0;
            track->keys[index].outgoing_frame_offset=span;
            track->keys[index].outgoing_value_offset=0;
            track->keys[index+1].incoming_frame_offset=-span;
            track->keys[index+1].incoming_value_offset=0;
        }
    } else if(operation==4) {
        status=TimelineTrackMoveScalarKey(track,index,destination,handles[0]);
        if(status==TIMELINE_STATUS_OK) fit_temporal_handles(track);
    } else status=TimelineTrackSetScalarTemporalHandles(track,index,handles[0],handles[1],handles[2],handles[3]);
    bool ok=status==TIMELINE_STATUS_OK && TimelineEntityBindingsValidateDocument(&bindings,&registry,&document)==TIMELINE_STATUS_OK &&
        SceneEditorDocumentSetTimeline(&document,session.scene_revision,status_line,sizeof(status_line));
    SceneTimelineSessionEndEdit(&session);
    available=false;sync_document();
    if(ok && (operation==1 || operation==4)) SceneEditorTimelineSeek(destination);
    if(!ok) snprintf(status_line,sizeof(status_line),"Edit rejected: keys must stay ordered and within valid bounds; retain at least one key.");
    return ok;
}
bool SceneEditorTimelineDeleteKey(void) {return edit_existing_key(0,0,TIMELINE_INTERPOLATION_STEP,NULL);}
bool SceneEditorTimelineMoveKey(int64_t frame) {return edit_existing_key(1,frame,TIMELINE_INTERPOLATION_STEP,NULL);}
bool SceneEditorTimelineSetInterpolation(TimelineInterpolation mode) {return edit_existing_key(2,0,mode,NULL);}
bool SceneEditorTimelineSetHandles(double fi,double vi,double fo,double vo) {
    const double handles[]={fi,vi,fo,vo};return edit_existing_key(3,0,TIMELINE_INTERPOLATION_STEP,handles);
}
bool SceneEditorTimelineAdvance(void) {
    uint64_t now=SDL_GetTicks64(), prior=last_ms;
    last_ms=now;
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_RENDER) {session.transport.playing=false;return false;}
    if(!sync_document() || !session.transport.playing) return false;
    return SceneTimelineSessionAdvance(&session,prior?(double)(now-prior)/1000.0:0)==TIMELINE_STATUS_OK;
}

void SceneEditorTimelineTogglePlaying(void) {
    if(sync_document()) {
        int64_t end;
        if(!session.transport.playing && TimelineRangeEndFrame(document.range,&end)==TIMELINE_STATUS_OK && session.transport.sample.absolute_frame>=end)
            SceneTimelineSessionSeek(&session,(TimelineSample){document.range.start_frame,0,1});
        SceneTimelineSessionSetPlaying(&session,!session.transport.playing);
        last_ms=SDL_GetTicks64();
    }
}
bool SceneEditorTimelineHandleEvent(SDL_Event* event,const SceneEditorPaneLayout* layout) {
    bool ready=sync_document();
    return SceneEditorTimelineUIEvent(event,layout,ready?&document:NULL,&session,selected);
}
void SceneEditorTimelineRender(SDL_Renderer* renderer,const SceneEditorPaneLayout* layout) {
    bool ready=sync_document();
    SceneEditorTimelineUIDraw(renderer,layout,ready?&document:NULL,&session,selected);
}
bool SceneEditorTimelineMoveKeyValue(int64_t frame,double value) {
    if(!isfinite(value)) return false;
    return edit_existing_key(4,frame,TIMELINE_INTERPOLATION_STEP,&value);
}

const TimelineDocument* SceneEditorTimelineDocumentView(size_t* index) {
    if(!sync_document()) return NULL;
    if(index) *index=selected;
    return &document;
}
bool SceneEditorTimelineCommitTrack(const TimelineTrack* track,unsigned long long revision) {
    if(!track || !sync_document() || revision!=SceneEditorDocumentRevision()) return false;
    if(!SceneEditorObjectTimelineEditable(track->target_id,status_line,sizeof(status_line))) return false;
    size_t index=SIZE_MAX;
    for(size_t i=0;i<document.track_count;++i) if(!strcmp(track->track_id,document.tracks[i].track_id)) index=i;
    if(index==SIZE_MAX || strcmp(track->target_id,document.tracks[index].target_id) ||
       strcmp(track->property_id,document.tracks[index].property_id)) return false;
    if(SceneTimelineSessionBeginEdit(&session,revision,session.timeline_revision)!=TIMELINE_STATUS_OK) return false;
    document.tracks[index]=*track;fit_temporal_handles(&document.tracks[index]);
    bool ok=TimelineEntityBindingsValidateDocument(&bindings,&registry,&document)==TIMELINE_STATUS_OK &&
        SceneEditorDocumentSetTimeline(&document,revision,status_line,sizeof(status_line));
    SceneTimelineSessionEndEdit(&session);available=false;sync_document();
    if(!ok) snprintf(status_line,sizeof(status_line),"Edit rejected: check frame collisions, range, and property bounds.");
    return ok;
}
