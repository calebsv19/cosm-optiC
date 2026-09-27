#include "scene_editor_timeline_curve.h"
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
static bool available, focused;
static size_t selected;
static char selected_track[TIMELINE_ID_CAPACITY];
static bool dragging_key;
static int64_t drag_frame;
static unsigned long long drag_revision;
static int numeric_mode;
static char numeric_text[64];
static size_t row_offset;
static bool curve_visible=true;
static uint64_t last_ms;
static char status_line[256], heading[160];
static char row_labels[TIMELINE_DOCUMENT_TRACK_CAPACITY][160];
static char document_path[4096];

void SceneEditorTimelineReleaseFocus(void) {
    if(numeric_mode) SDL_StopTextInput();
    numeric_mode=0;focused=false;dragging_key=false;
    SceneEditorTimelineCurveCancel();
}
static bool sync_document(void) {
    unsigned long long revision = SceneEditorDocumentRevision();
    const char* path = SceneEditorDocumentPath();
    bool changed_scene = strcmp(document_path, path ? path : "") != 0;
    if (changed_scene) {
        memset(&session, 0, sizeof(session));
        selected = 0;
        selected_track[0] = 0;
        dragging_key = false;
        numeric_mode=0; row_offset=0;
        snprintf(document_path, sizeof(document_path), "%s", path ? path : "");
    }
    if (available && !changed_scene && session.scene_revision == revision) return true;
    if (dragging_key && revision != drag_revision) dragging_key = false;
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
static SDL_Rect curve_rect(SDL_Rect panel) {
    return (SDL_Rect){panel.x+244,panel.y+panel.h/2,panel.w-252,panel.h/2-28};
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
bool SceneEditorTimelineActivate(void) {
    if (sync_document()) return true;
    if (!SceneEditorDocumentIsOpen()) { snprintf(status_line,sizeof(status_line),"Open a saved runtime scene to author its timeline."); return false; }
    TimelineStatus existing = SceneEditorDocumentGetTimeline(&document);
    if (existing != TIMELINE_STATUS_TARGET_NOT_FOUND) {
        snprintf(status_line,sizeof(status_line),"Existing timeline cannot be loaded; activation refused.");
        return false;
    }
    static RuntimeSceneLightTimelineDocument legacy;
    if (RuntimeSceneLightTimelineGetLast(&legacy)) document = legacy.timeline;
    else if (TimelineDocumentInit(&document, (TimelineRate){(uint32_t)(animSettings.fps>0?animSettings.fps:24),1},
        (TimelineRange){0,(uint64_t)(animSettings.framesForTravel>0?animSettings.framesForTravel:120)}) != TIMELINE_STATUS_OK) return false;
    if (sceneSettings.cameraPath.numPoints && !add_camera_track("camera-progress", "camera/path_progress", TIMELINE_UNIT_UNITLESS, 0, 1)) return false;
    if (!add_camera_track("camera-lens", "camera/fov_y", TIMELINE_UNIT_DEGREES, 55,55)) return false;
    if (!SceneEditorDocumentSetTimeline(&document, SceneEditorDocumentRevision(), status_line, sizeof(status_line))) return false;
    available = false;
    return sync_document();
}
bool SceneEditorTimelineSelectTrack(size_t index) {
    if (!sync_document() || index >= document.track_count) return false;
    TimelineTrack* track=&document.tracks[index];
    if (SceneTimelineSessionSelect(&session,&bindings,&registry,track->target_id,track->property_id)!=TIMELINE_STATUS_OK) return false;
    selected=index;
    if(row_offset>selected) row_offset=selected;
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
    if(ok) {row_offset=selected;snprintf(status_line,sizeof(status_line),"Channel added from the current evaluated value.");}
    return ok;
}
bool SceneEditorTimelineSetKey(double value) {
    if (!sync_document() || selected>=document.track_count || !isfinite(value)) return false;
    TimelineTrack* track=&document.tracks[selected];
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
    } else status=TimelineTrackSetScalarTemporalHandles(track,index,handles[0],handles[1],handles[2],handles[3]);
    bool ok=status==TIMELINE_STATUS_OK && TimelineEntityBindingsValidateDocument(&bindings,&registry,&document)==TIMELINE_STATUS_OK &&
        SceneEditorDocumentSetTimeline(&document,session.scene_revision,status_line,sizeof(status_line));
    SceneTimelineSessionEndEdit(&session);
    available=false;sync_document();
    if(ok && operation==1) SceneEditorTimelineSeek(destination);
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
static bool inside(int x,int y,SDL_Rect r) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static double current_value(void) {
    TimelineEvaluationContext context;
    TimelineEvaluationResult result;
    if(selected<document.track_count && TimelineEvaluationContextBuild(document.rate,document.range,session.transport.sample,&context)==TIMELINE_STATUS_OK &&
        TimelineTrackEvaluate(&document.tracks[selected],&context,&result)==TIMELINE_STATUS_OK && result.value.type==TIMELINE_VALUE_SCALAR) return result.value.as.scalar;
    return 0;
}
static int64_t frame_for_x(int x,SDL_Rect r) {
    double t=r.w>260?fmax(0,fmin(1,(double)(x-r.x-240)/(r.w-250))):0;
    return document.range.start_frame+(int64_t)llround(t*(document.range.frame_count-1));
}
bool SceneEditorTimelineHandleEvent(SDL_Event* event,const SceneEditorPaneLayout* layout) {
    if(!event || !layout || !layout->timeline_visible) return false;
    sync_document();
    SDL_Rect r=layout->timeline_rect;
    if(curve_visible && available && r.h>=220 && SceneEditorTimelineCurveEvent(event,curve_rect(r))) {
        focused=true;return true;
    }
    if(event->type==SDL_TEXTINPUT && numeric_mode) {
        size_t used=strlen(numeric_text), added=strlen(event->text.text);
        if(used+added<sizeof(numeric_text)) memcpy(numeric_text+used,event->text.text,added+1);
        return true;
    }
    if(event->type==SDL_KEYDOWN && numeric_mode) {
        SDL_Keycode key=event->key.keysym.sym;
        if(key==SDLK_ESCAPE) {numeric_mode=0;SDL_StopTextInput();return true;}
        if(key==SDLK_BACKSPACE) {size_t n=strlen(numeric_text);if(n) numeric_text[n-1]=0;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            char* end=NULL;bool ok=false;errno=0;
            if(numeric_mode==1) {
                long long frame=strtoll(numeric_text,&end,10);
                if(!errno && end!=numeric_text && !*end) ok=SceneEditorTimelineSeek((int64_t)frame);
            } else {
                double value=strtod(numeric_text,&end);
                if(!errno && end!=numeric_text && !*end && isfinite(value)) ok=SceneEditorTimelineSetKey(value);
            }
            if(ok) {numeric_mode=0;SDL_StopTextInput();}
            else snprintf(status_line,sizeof(status_line),"Enter a valid %s within the timeline/property bounds.",numeric_mode==1?"frame":"value");
        }
        return true;
    }
    if(event->type==SDL_MOUSEWHEEL) {
        int mx,my;SDL_GetMouseState(&mx,&my);
        if(inside(mx,my,r)) {
            int next=(int)row_offset-event->wheel.y;
            if(next<0) next=0;
            if((size_t)next>=document.track_count) next=document.track_count?(int)document.track_count-1:0;
            row_offset=(size_t)next;return true;
        }
    }
    if(event->type==SDL_MOUSEMOTION && dragging_key) {
        drag_frame=frame_for_x(event->motion.x,r);return true;
    }
    if(event->type==SDL_MOUSEBUTTONUP && dragging_key) {
        dragging_key=false;
        if(SceneEditorDocumentRevision()==drag_revision) SceneEditorTimelineMoveKey(drag_frame);
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN) {
        focused=inside(event->button.x,event->button.y,r);
        if(numeric_mode) {numeric_mode=0;SDL_StopTextInput();}
        if(!focused) return false;
        int x=event->button.x-r.x,y=event->button.y-r.y;
        if(!available) {SceneEditorTimelineActivate();return true;}
        if(y<30) {
            if(x<80) {SceneTimelineSessionSetPlaying(&session,!session.transport.playing);last_ms=SDL_GetTicks64();}
            else if(x<160 && selected<document.track_count) SceneEditorTimelineSetKey(current_value());
            else if(x<200 && selected<document.track_count) SceneEditorTimelineSetKey(current_value()-(document.tracks[selected].unit==TIMELINE_UNIT_DEGREES?1:.05));
            else if(x<240 && selected<document.track_count) SceneEditorTimelineSetKey(current_value()+(document.tracks[selected].unit==TIMELINE_UNIT_DEGREES?1:.05));
            else if(x>=240) {
                numeric_mode=x<390?1:2;
                numeric_text[0]=0;
                session.transport.playing=false;
                SDL_StartTextInput();
            }
        } else if(y>=30 && y<54 && x>=350 && x<620) {
            SceneEditorTimelineAddChannel(x<440?"light/intensity":x<530?"camera/yaw":"camera/pitch");
        } else if(y>=30 && y<54 && x>=240 && x<340) {
            curve_visible=!curve_visible;SceneEditorTimelineCurveCancel();
        } else if(y>=30 && y<54 && x<240) {
            if(x<60) SceneEditorTimelineDeleteKey();
            else if(x<120) SceneEditorTimelineSetInterpolation(TIMELINE_INTERPOLATION_STEP);
            else if(x<180) SceneEditorTimelineSetInterpolation(TIMELINE_INTERPOLATION_LINEAR);
            else SceneEditorTimelineSetInterpolation(TIMELINE_INTERPOLATION_CUBIC_BEZIER);
        } else if(x>=240 && r.w>260) {
            SceneTimelineSessionSetPlaying(&session,false);
            size_t row=y>=56?row_offset+(size_t)((y-56)/24):SIZE_MAX;
            bool hit=false;
            if(row<document.track_count && SceneEditorTimelineSelectTrack(row)) {
                TimelineTrack* track=&document.tracks[row];
                for(size_t k=0;k<track->key_count;++k) {
                    double t=document.range.frame_count>1?(double)(track->keys[k].frame-document.range.start_frame)/(document.range.frame_count-1):0;
                    if(fabs((double)x-(240+t*(r.w-250)))<=7) {
                        SceneEditorTimelineSeek(track->keys[k].frame);
                        dragging_key=true;drag_frame=track->keys[k].frame;drag_revision=SceneEditorDocumentRevision();hit=true;break;
                    }
                }
            }
            if(!hit) SceneEditorTimelineSeek(frame_for_x(event->button.x,r));
        } else if(y>=56) SceneEditorTimelineSelectTrack(row_offset+(size_t)((y-56)/24));
        return true;
    }
    if(event->type==SDL_KEYDOWN && focused && available) {
        if((event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI)) && event->key.keysym.sym==SDLK_z) {
            if(event->key.keysym.mod&KMOD_SHIFT) SceneEditorDocumentRedo(status_line,sizeof(status_line));
            else SceneEditorDocumentUndo(status_line,sizeof(status_line));
            available=false;sync_document();return true;
        }
        if(event->key.keysym.sym==SDLK_ESCAPE) {dragging_key=false;return true;}
        if(event->key.keysym.sym==SDLK_DELETE || event->key.keysym.sym==SDLK_BACKSPACE) {SceneEditorTimelineDeleteKey();return true;}
        if(event->key.keysym.sym==SDLK_LEFT || event->key.keysym.sym==SDLK_RIGHT) {
            int step=(event->key.keysym.mod&KMOD_SHIFT)?10:1;
            if(event->key.keysym.sym==SDLK_LEFT) step=-step;
            SceneEditorTimelineMoveKey(session.transport.sample.absolute_frame+step);return true;
        }
        if(event->key.keysym.sym==SDLK_SPACE) {SceneTimelineSessionSetPlaying(&session,!session.transport.playing);last_ms=SDL_GetTicks64();return true;}
        return !((event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI)) && event->key.keysym.sym==SDLK_s);
    }
    return false;
}
void SceneEditorTimelineRender(SDL_Renderer* renderer,const SceneEditorPaneLayout* layout) {
    if(!renderer || !layout || !layout->timeline_visible) return;
    sync_document();
    SDL_Rect r=layout->timeline_rect;
    SDL_SetRenderDrawColor(renderer,30,34,43,255);SDL_RenderFillRect(renderer,&r);
    TTF_Font* font=ray_tracing_font_runtime_get_ui_regular(renderer,11,8);
    SDL_Color text={225,230,240,255};
    if(!available) {
        ray_tracing_text_draw_utf8_at(renderer,font,"Enable scene timeline (click) — camera and light animation",r.x+12,r.y+12,text);
        return;
    }
    const char* labels[]={session.transport.playing?"Pause":"Play", "Key", "-", "+"};
    const int offsets[]={0,80,160,200}, widths[]={80,80,40,40};
    for(int i=0;i<4;++i) {
        SDL_Rect button={r.x+offsets[i]+2,r.y+2,widths[i]-4,26};
        SDL_SetRenderDrawColor(renderer,49,68,88,255);SDL_RenderFillRect(renderer,&button);
        ray_tracing_text_draw_utf8_at(renderer,font,labels[i],button.x+8,button.y+5,text);
    }
    if(numeric_mode) snprintf(heading,sizeof(heading),"%s: %s_  (Enter / Esc)",numeric_mode==1?"Frame":"Value",numeric_text);
    else snprintf(heading,sizeof(heading),"Frame %lld",(long long)session.transport.sample.absolute_frame);
    ray_tracing_text_draw_utf8_at(renderer,font,heading,r.x+250,r.y+8,text);
    static char value_text[64];
    if(!numeric_mode) {
        snprintf(value_text,sizeof(value_text),"Value %.4g",current_value());
        ray_tracing_text_draw_utf8_at(renderer,font,value_text,r.x+400,r.y+8,text);
    }
    const char* modes[]={"Delete","Hold","Linear","Ease"};
    for(int i=0;i<4;++i) {
        SDL_Rect button={r.x+i*60+2,r.y+31,56,22};
        SDL_SetRenderDrawColor(renderer,44,49,60,255);SDL_RenderFillRect(renderer,&button);
        ray_tracing_text_draw_utf8_at(renderer,font,modes[i],button.x+4,button.y+3,text);
    }
    ray_tracing_text_draw_utf8_at(renderer,font,curve_visible?"Curve: on":"Curve: off",r.x+250,r.y+34,text);
    ray_tracing_text_draw_utf8_at(renderer,font,"+Intensity",r.x+350,r.y+34,text);
    ray_tracing_text_draw_utf8_at(renderer,font,"+Yaw",r.x+440,r.y+34,text);
    ray_tracing_text_draw_utf8_at(renderer,font,"+Pitch",r.x+530,r.y+34,text);
    int rows_bottom=curve_visible && r.h>=220?r.h/2-8:r.h-25;
    int width=r.w-250;
    for(size_t i=row_offset;i<document.track_count && 56+(int)(i-row_offset)*24<rows_bottom;++i) {
        int y=r.y+56+(int)(i-row_offset)*24;
        if(i==selected) {SDL_Rect row={r.x,y,r.w,22};SDL_SetRenderDrawColor(renderer,49,68,88,255);SDL_RenderFillRect(renderer,&row);}
        snprintf(row_labels[i],sizeof(row_labels[i]),"%s  %s",document.tracks[i].target_id,document.tracks[i].property_id);
        ray_tracing_text_draw_utf8_at(renderer,font,row_labels[i],r.x+8,y+3,text);
        if(width>0) for(size_t k=0;k<document.tracks[i].key_count;++k) {
            double t=document.range.frame_count>1?(double)(document.tracks[i].keys[k].frame-document.range.start_frame)/(document.range.frame_count-1):0;
            SDL_Rect key={r.x+240+(int)(t*width)-3,y+7,6,8};
            SDL_SetRenderDrawColor(renderer,105,196,239,255);SDL_RenderFillRect(renderer,&key);
        }
    }
    if(width>0) {
        double t=document.range.frame_count>1?(double)(session.transport.sample.absolute_frame-document.range.start_frame)/(document.range.frame_count-1):0;
        int x=r.x+240+(int)(t*width);
        SDL_SetRenderDrawColor(renderer,245,180,80,255);SDL_RenderDrawLine(renderer,x,r.y+32,x,r.y+r.h-22);
    }
    if(curve_visible && r.h>=220) SceneEditorTimelineCurveRender(renderer,curve_rect(r));
    if(dragging_key && width>0) {
        double t=document.range.frame_count>1?(double)(drag_frame-document.range.start_frame)/(document.range.frame_count-1):0;
        int x=r.x+240+(int)(t*width);
        SDL_SetRenderDrawColor(renderer,240,240,240,255);SDL_RenderDrawLine(renderer,x,r.y+54,x,r.y+r.h-22);
    }
    ray_tracing_text_draw_utf8_at(renderer,font,status_line,r.x+8,r.y+r.h-19,text);
}
