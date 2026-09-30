#include "scene_editor_object_timeline_panel.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_motion_trail.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_pointer_event.h"
#include "editor/editor_mode_router.h"
/* Render's subject and task selection are independent of timeline availability. */
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_object_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "scene_editor_timeline_key_inspector.h"
#include "scene_editor_timeline_view.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_camera_inspector.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_tool_state.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "kit_ui_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool timing;
static int left_offset,left_max;
static int editing=-1;
static char draft[64],feedback[256],light_label[128],readouts[8][160];
static unsigned long long edit_revision;
static SDL_Rect controls[17],fields[2];
static bool controls_enabled[17];
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && r.h>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static void label(SDL_Renderer* r,const char* text,int x,int y) {
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,12,9),text,x,y,SceneEditorChromeShellResolvePalette().text_primary);
}
void SceneEditorRenderButton(SDL_Renderer* r,SDL_Rect rect,const char* text,bool selected,bool enabled) {
    KitUiHudStyle style;kit_ui_hud_style_dark_floating(&style);style.button_corner_radius=0;
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    style.button_fill=(KitRenderColor){p.button_fill.r,p.button_fill.g,p.button_fill.b,p.button_fill.a};
    style.button_active_fill=(KitRenderColor){p.accent_primary.r,p.accent_primary.g,p.accent_primary.b,p.accent_primary.a};
    int x,y;Uint32 mouse=SDL_GetMouseState(&x,&y);
    bool hovered=enabled && hit(rect,x,y);
    bool pressed=hovered && (mouse & SDL_BUTTON_LMASK);
    SDL_Color fill=selected?p.accent_primary:p.button_fill;
    SDL_Color ink=enabled?ray_tracing_theme_choose_button_text(fill,p):p.text_muted;
    if(hovered) {
        /* The shared SDL HUD fill does not interpret hover. Apply app theme
         * feedback to both ordinary and selected buttons before drawing it. */
        SDL_Color toward=ink;
        int amount=pressed?28:16;
        fill.r=(Uint8)((fill.r*(100-amount)+toward.r*amount)/100);
        fill.g=(Uint8)((fill.g*(100-amount)+toward.g*amount)/100);
        fill.b=(Uint8)((fill.b*(100-amount)+toward.b*amount)/100);
    }
    style.button_fill=style.button_active_fill=(KitRenderColor){fill.r,fill.g,fill.b,fill.a};
    KitUiButtonState state={.selected=selected,.disabled=!enabled,.hovered=hovered,.pressed=pressed};
    kit_ui_sdl_draw_button(r,&rect,"",&state,&style,NULL);
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);SDL_Rect clip=rect;if(clipped) SDL_IntersectRect(&prior,&rect,&clip);SDL_RenderSetClipRect(r,&clip);
    if(hovered) {
        SDL_SetRenderDrawColor(r,ink.r,ink.g,ink.b,255);
        SDL_Rect edge={rect.x+1,rect.y+1,rect.w-2,rect.h-2};SDL_RenderDrawRect(r,&edge);
    }
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,12,9),text,rect.x+8,rect.y+8,ink);
    SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}
bool SceneEditorRenderAuthoringTiming(void) {return timing;}
void SceneEditorRenderAuthoringSetTiming(bool enabled) {if(enabled) SceneEditorMotionPathPanelSelect(false);SceneEditorTimelineKeyInspectorReset();timing=enabled;editing=-1;SDL_StopTextInput();SceneEditorCameraInspectorReset();}
void SceneEditorRenderAuthoringReset(void) {SceneEditorMotionTrailReset();SceneEditorMotionPathPanelReset();SceneEditorObjectTimelinePanelReset();SceneEditorTimelineKeyInspectorReset();timing=false;left_offset=left_max=0;editing=-1;feedback[0]=0;memset(controls,0,sizeof(controls));}
void SceneEditorRenderAuthoringSelect(SceneEditor* editor,bool camera) {
    SceneEditorMotionPathPanelSelect(false);
    SceneEditorCameraGestureCancel();SceneEditorLightGestureCancel();SceneEditorCameraInspectorReset();
    SceneEditorTimelineReleaseFocus();SceneEditorTimelinePause();editing=-1;SDL_StopTextInput();
    left_offset=0;SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
    editor->currentMode=camera?EDITOR_MODE_CAMERA:EDITOR_MODE_PATH;
    animSettings.editorMode=editor->currentMode;
    SceneEditorTimelineClearSelection();
    static TimelineDocument doc;
    if(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK) {
        for(size_t i=0;i<doc.track_count;++i) if(!strncmp(doc.tracks[i].target_id,camera?"camera/":"light/",camera?7:6)) {
            SceneEditorTimelineSelectTrack(i);break;
        }
    }
}
static void path_action(SceneEditor* editor,int action) {
    bool camera=editor->currentMode==EDITOR_MODE_CAMERA;
    Path path=camera?sceneSettings.cameraPath:sceneSettings.bezierPath;
    CameraPath3D depth=camera?sceneSettings.cameraPath3D:sceneSettings.bezierPath3D;
    int point=camera?CameraEditorGetSelectedPointIndex():BezierEditorGetSelectedPointIndex();
    if(action==6 || action==7) {
        if(path.numPoints) {point=(point+(action==6?-1:1)+path.numPoints)%path.numPoints;
            if(camera) CameraEditorSetSelectedPointIndex(point);else BezierEditorSetSelectedPointIndex(point);}
        return;
    }
    if(action==8 || action==9 || action==10) {
        SceneEditorToolStateSetActive(action==8?SCENE_EDITOR_TOOL_SELECT:action==9?SCENE_EDITOR_TOOL_ADD:SCENE_EDITOR_TOOL_DELETE);return;
    }
    if(action==11) path.mode=path.mode==BEZIER_CUBIC?BEZIER_QUADRATIC:BEZIER_CUBIC;
    if(action==12) {
        if(point<0 || point>=path.numPoints) {snprintf(feedback,sizeof(feedback),"Select a path point first.");return;}
        path.handleLink[point]=!path.handleLink[point];
    }
    bool ok=camera?SceneEditorDocumentSetCameraPath(&path,&depth,SceneEditorDocumentRevision(),feedback,sizeof(feedback)):
        SceneEditorDocumentSetLightPath(&path,&depth,SceneEditorDocumentRevision(),feedback,sizeof(feedback));
    if(ok) snprintf(feedback,sizeof(feedback),"Path updated. Undo is available.");
}
bool SceneEditorRenderAuthoringEvent(SceneEditor* editor,SDL_Event* e) {
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_RENDER) return false;
    SceneEditorPaneLayout layout;if(!SceneEditorGetPaneLayout(&layout) || layout.viewport_expanded) return false;
    if(SceneEditorMotionPathPanelEvent(editor,e,&layout)) return true;
    if(SceneEditorMotionPathPanelActive()) {if(e->type==SDL_MOUSEBUTTONDOWN && hit(controls[0],e->button.x,e->button.y)) {SceneEditorRenderAuthoringSelect(editor,true);return true;}if(e->type==SDL_MOUSEBUTTONDOWN && hit(controls[1],e->button.x,e->button.y)) {SceneEditorRenderAuthoringSelect(editor,false);return true;}return false;}
    if(SceneEditorMotionTrailEvent(editor,e,&layout)) return true;
    if(SceneEditorObjectTimelinePanelEvent(e)) {editing=-1;return true;}
    if(SceneEditorObjectTimelineEvent(editor,e)) return true;
    if(timing && SceneEditorTimelineKeyInspectorEvent(e)) {editing=-1;return true;}
    if(editing>=0 && (e->type==SDL_TEXTINPUT || e->type==SDL_KEYDOWN)) {
        if(e->type==SDL_TEXTINPUT) {if(strlen(draft)+strlen(e->text.text)<sizeof(draft)) strcat(draft,e->text.text);return true;}
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE) {editing=-1;SDL_StopTextInput();return true;}
        if(key==SDLK_BACKSPACE) {size_t n=strlen(draft);if(n) draft[n-1]=0;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            char* end;double value=strtod(draft,&end);bool ok=false;
            if(end!=draft && !*end && isfinite(value) && edit_revision==SceneEditorDocumentRevision()) {
                if(editing==0 && value>=-9007199254740991.0 && value<=9007199254740991.0 && floor(value)==value) ok=SceneEditorTimelineSeek((int64_t)value);
                else if(editing==1) {ok=SceneEditorTimelineSetKey(value);if(ok) {TimelineSample at;if(SceneEditorTimelineCurrentSample(&at)) SceneEditorTimelineSelectKey(at.absolute_frame,false);}}
            }
            snprintf(feedback,sizeof(feedback),"%s",ok?"Applied. Undo is available for keys.":"Invalid value/range. Esc cancels.");
            if(ok) {editing=-1;SDL_StopTextInput();}
        }
        return true;
    }
    if(e->type==SDL_MOUSEWHEEL) {
        int x,y;SceneEditorWheelPosition(e,&x,&y);
        if(hit(layout.left_content_rect,x,y)) {left_offset-=e->wheel.y*32;if(left_offset<0) left_offset=0;if(left_offset>left_max) left_offset=left_max;return true;}
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    int x=e->button.x,y=e->button.y;
    if(editing>=0) {editing=-1;SDL_StopTextInput();}
    for(int i=0;i<17;++i) if(hit(layout.left_content_rect,x,y) && hit(controls[i],x,y)) {
        if(!controls_enabled[i]) return true;
        if(i==16) {left_offset=0;SceneEditorMotionPathPanelSelect(true);return true;}
        if(i<2) SceneEditorRenderAuthoringSelect(editor,i==0);
        else if(i<4) {SceneEditorTimelineKeyInspectorReset();timing=i==3;left_offset=0;SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);SceneEditorCameraInspectorReset();SceneEditorTimelineReleaseFocus();SceneEditorTimelinePause();}
        else if(i==4) {
            TimelineSample sample;
            if(SceneEditorTimelineCurrentSample(&sample)) {
                bool ok=SceneEditorChromeActionsSaveAuthoring();
                snprintf(feedback,sizeof(feedback),"%s",ok?"Saved scene and animation.":SceneEditorObjectTimelinePanelPending()?"Set position key or Escape before saving.":"Save failed. Edits remain open; see File / Save.");
            } else {
                bool ok=SceneEditorTimelineActivate();
                snprintf(feedback,sizeof(feedback),"%s",ok?"Animation created. Save scene to keep it.":SceneEditorTimelineStatus());
                SceneEditorRenderAuthoringSelect(editor,editor->currentMode==EDITOR_MODE_CAMERA);
            }
        }
        else if(i==5) SceneEditorFrameViewport(false);
        else if(i<13) path_action(editor,i);
        else SceneEditorTimelineAddChannel(i==13?"camera/yaw":i==14?"camera/pitch":"light/intensity");
        return true;
    }
    if(timing) for(int i=0;i<2;++i) if(hit(fields[i],x,y)) {
        SceneEditorTimelineReleaseFocus();SceneEditorTimelinePause();editing=i;draft[0]=0;
        edit_revision=SceneEditorDocumentRevision();SDL_StartTextInput();return true;
    }
    if(timing && hit(layout.viewport_rect,x,y)) {
        snprintf(feedback,sizeof(feedback),"Use Path shape to edit the route; Timing edits keys.");return true;
    }
    /* These panes own their input; legacy controls underneath must not receive it. */
    return hit(layout.left_content_rect,x,y) || (timing && hit(layout.right_content_rect,x,y));
}
void SceneEditorRenderAuthoringDraw(SceneEditor* editor,const SceneEditorPaneLayout* layout) {
    memset(controls,0,sizeof(controls));memset(fields,0,sizeof(fields));
    for(int i=0;i<17;++i) controls_enabled[i]=true;
    if(layout->viewport_expanded) return;
    SceneEditorMotionPathOverlayDraw(editor,layout);
    SceneEditorMotionTrailDraw(editor,layout);
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    bool ready=SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample);
    bool object=ready && !strncmp(track.target_id,"object/",7);
    if(object) timing=true;
    controls_enabled[2]=!object;
    controls_enabled[13]=controls_enabled[14]=controls_enabled[15]=ready;
    bool path_follower=object && !strcmp(track.property_id,MOTION_PROGRESS_PROPERTY);
    bool camera=editor->currentMode==EDITOR_MODE_CAMERA;
    SDL_Renderer* r=editor->renderer;SDL_Rect pane=layout->left_content_rect,prior;
    SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);SDL_RenderSetClipRect(r,&pane);
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    SDL_SetRenderDrawColor(r,p.panel_fill.r,p.panel_fill.g,p.panel_fill.b,p.panel_fill.a);SDL_RenderFillRect(r,&pane);
    int x=pane.x+10,y=pane.y+10-left_offset,w=pane.w-20;

    controls[0]=(SDL_Rect){x,y,(w-12)/3,30};SceneEditorRenderButton(r,controls[0],"Camera",camera && !object && !SceneEditorMotionPathPanelActive(),true);
    static RuntimeSceneLightTimelineDocument light;
    bool has_light=RuntimeSceneLightTimelineGetLast(&light);
    snprintf(light_label,sizeof(light_label),"Light: %s",has_light?light.timeline.tracks[light.progress_track_index].target_id+6:"path not bound");
    controls[1]=(SDL_Rect){x+(w+6)/3,y,(w-12)/3,30};SceneEditorRenderButton(r,controls[1],"Light",!camera && !object && !SceneEditorMotionPathPanelActive(),true);
    controls[16]=(SDL_Rect){x+2*(w+6)/3,y,(w-12)/3,30};SceneEditorRenderButton(r,controls[16],"Paths",SceneEditorMotionPathPanelActive(),true);y+=38;
    if(SceneEditorMotionPathPanelActive()) {SDL_RenderSetClipRect(r,clipped?&prior:NULL);SceneEditorMotionPathPanelDraw(editor,layout);return;}
    label(r,object?"Subject: selected object":camera?"Subject: Camera":light_label,x,y);y+=26;
    controls[2]=(SDL_Rect){x,y,(w-6)/2,34};controls[3]=(SDL_Rect){x+(w-6)/2+6,y,(w-6)/2,34};
    SceneEditorRenderButton(r,controls[2],"Path shape",!timing,!object);SceneEditorRenderButton(r,controls[3],"Timing",timing,true);y+=40;
    label(r,path_follower?"Object follows a reusable path":object?"Object uses position keys":timing?"When it moves: edit progress keys":"Where it moves: edit points and handles",x,y);y+=26;
    TimelineSample retained_sample;bool retained=SceneEditorTimelineCurrentSample(&retained_sample);
    controls[4]=(SDL_Rect){x,y,(w-6)/2,30};
    SceneEditorRenderButton(r,controls[4],retained?"Save animation":"Set up animation",false,true);
    controls[5]=(SDL_Rect){x+(w+6)/2,y,(w-6)/2,30};
    SceneEditorRenderButton(r,controls[5],"Frame paths",false,true);y+=38;
    if(timing) SceneEditorObjectTimelineDraw(r,pane,&y);
    if(!timing) {
        const Path* path=camera?&sceneSettings.cameraPath:&sceneSettings.bezierPath;
        int point=camera?CameraEditorGetSelectedPointIndex():BezierEditorGetSelectedPointIndex();
        snprintf(readouts[0],sizeof(readouts[0]),"%s path | %d points",camera?"Camera":"Light",path->numPoints);label(r,readouts[0],x,y);y+=27;
        controls_enabled[6]=controls_enabled[7]=path->numPoints>0;
        controls_enabled[12]=point>=0 && point<path->numPoints;
        for(int i=6;i<=7;++i) {controls[i]=(SDL_Rect){x+(i-6)*(w+6)/2,y,(w-6)/2,34};SceneEditorRenderButton(r,controls[i],i==6?"Previous point":"Next point",false,path->numPoints>0);}y+=42;
        const char* tools[]={"Move","Add","Delete"};
        for(int i=8;i<=10;++i) {controls[i]=(SDL_Rect){x+(i-8)*(w+6)/3,y,(w-12)/3,30};SceneEditorRenderButton(r,controls[i],tools[i-8],SceneEditorToolStateGetActive()==(SceneEditorTool)(i-8),true);}y+=38;
        controls[11]=(SDL_Rect){x,y,w,34};SceneEditorRenderButton(r,controls[11],path->mode==BEZIER_CUBIC?"Path: Cubic Bezier":"Path: Quadratic Bezier",false,true);y+=40;
        controls[12]=(SDL_Rect){x,y,w,34};SceneEditorRenderButton(r,controls[12],point>=0 && point<path->numPoints && path->handleLink[point]?"Handles: Linked":"Handles: Independent",false,controls_enabled[12]);y+=40;
        snprintf(readouts[5],sizeof(readouts[5]),point>=0?"Selected point: %d of %d":"No point selected (%d points)",point>=0?point+1:path->numPoints,path->numPoints);
        label(r,readouts[5],x,y);y+=25;
        SceneEditorTool tool=SceneEditorToolStateGetActive();
        label(r,tool==SCENE_EDITOR_TOOL_ADD?"Click in the view to add a point.":tool==SCENE_EDITOR_TOOL_DELETE?"Click a point to delete it. Undo restores it.":"Select a point or handle; edit XYZ at right.",x,y);
    } else {
        label(r,"Select a channel in the timeline.",x,y);y+=28;
        if(object) {label(r,path_follower?"Paths tab edits shape and attachment.":"Position X / Y / Z in scene units.",x,y);y+=28;}
        else if(camera) for(int i=13;i<=14;++i) {controls[i]=(SDL_Rect){x,y,w,34};SceneEditorRenderButton(r,controls[i],i==13?"Add / select Yaw":"Add / select Pitch",false,ready);y+=40;}
        else {controls[15]=(SDL_Rect){x,y,w,34};SceneEditorRenderButton(r,controls[15],"Add / select Intensity",false,ready);y+=40;}
        label(r,"Scrub to a frame, then set a key.",x,y);
    }
    y+=26;
    bool pending=SceneEditorObjectTimelinePanelPending();
    if(pending) {label(r,"Position draft: Apply key before saving",x,y);y+=24;}
    if(feedback[0] && !pending) {label(r,feedback,x,y);y+=24;}
    left_max=y+30+left_offset-(pane.y+pane.h);if(left_max<0) left_max=0;
    if(left_max>0) {
        KitUiSdlScrollbarLayout scroll;kit_ui_sdl_scrollbar_layout(&pane,pane.h+left_max,left_offset,&scroll);
        kit_ui_sdl_draw_scrollbar(r,&scroll,(KitRenderColor){40,44,50,255},(KitRenderColor){130,140,155,255});
    }
    SDL_RenderSetClipRect(r,clipped?&prior:NULL);
    if(!timing) return;
    pane=layout->right_content_rect;SDL_RenderSetClipRect(r,&pane);
    SDL_SetRenderDrawColor(r,p.panel_fill.r,p.panel_fill.g,p.panel_fill.b,p.panel_fill.a);SDL_RenderFillRect(r,&pane);
    x=pane.x+10;y=pane.y+10;w=pane.w-20;
    static char subject[180];SceneEditorDocumentObjectInfo object_info;
    if(object && SceneEditorDocumentObjectById(track.target_id+7,&object_info)) snprintf(subject,sizeof(subject),"Object: %s",object_info.name);
    else snprintf(subject,sizeof(subject),"%s",camera?"Camera animation":"Light animation");
    label(r,subject,x,y);y+=26;
    if(ready) {
        snprintf(readouts[1],sizeof(readouts[1]),"Channel: %s",TimelineChannelLabel(track.property_id));label(r,readouts[1],x,y);y+=26;
        TimelineEvaluationContext context;TimelineEvaluationResult result;
        double value=0;if(TimelineEvaluationContextBuild(rate,range,sample,&context)==TIMELINE_STATUS_OK && TimelineTrackEvaluate(&track,&context,&result)==TIMELINE_STATUS_OK) value=result.value.as.scalar;
        snprintf(readouts[2],sizeof(readouts[2]),"Playhead: %lld",(long long)sample.absolute_frame);
        snprintf(readouts[3],sizeof(readouts[3]),"At playhead (%s): %.6g",track.unit==TIMELINE_UNIT_WORLD_DISTANCE?SceneEditorDocumentUnitLabel():TimelineUnitLabel(track.unit),value);
        for(int i=0;i<(object && !path_follower?1:2);++i) {fields[i]=(SDL_Rect){x,y,w,28};if(editing==i) snprintf(readouts[i+2],sizeof(readouts[i+2]),"%s: %s_",i?"Value":"Frame",draft);SceneEditorRenderButton(r,fields[i],readouts[i+2],editing==i,true);y+=32;}
        if(object && !path_follower) y=SceneEditorObjectTimelinePanelDraw(r,(SDL_Rect){x,y,w,pane.y+pane.h-y});
        snprintf(readouts[4],sizeof(readouts[4]),"%llu frames | %.3g fps",(unsigned long long)range.frame_count,(double)rate.frames_per_second_numerator/rate.frames_per_second_denominator);
        if(!object || path_follower) {label(r,editing>=0 && feedback[0]?feedback:readouts[4],x,y);y+=26;}
        SceneEditorTimelineKeyInspectorDraw(r,(SDL_Rect){x,y,w,pane.y+pane.h-y});
        y=pane.y+pane.h;
    } else {label(r,"Set up scene animation to add keys.",x,y);y+=32;}
    label(r,feedback,x,y);SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}

bool SceneEditorRenderAuthoringControl(const char* name,SDL_Rect* out) {
    const char* names[]={"camera","light","path","animation","setup","frame_paths","previous","next","select","add","delete","interpolation","handles","yaw","pitch","intensity","paths"};
    if(!name || !out) return false;
    if(SceneEditorMotionTrailControl(name,out)) return true;
    if(SceneEditorMotionPathPanelControl(name,out)) return true;
    if(SceneEditorObjectTimelinePanelControl(name,out)) return true;
    if(!strcmp(name,"shape")) name="path";
    if(!strcmp(name,"timing")) name="animation";
    if(!strcmp(name,"save_animation")) {*out=controls[4];return out->w>0;}
    if(SceneEditorObjectTimelineControl(name,out)) return true;
    if(SceneEditorTimelineKeyInspectorControl(name,out)) return true;
    for(int i=0;i<17;++i) if(!strcmp(name,names[i])) {*out=controls[i];return out->w>0;}
    if(!strcmp(name,"frame") || !strcmp(name,"value")) {*out=fields[!strcmp(name,"value")];return out->w>0;}
    return false;
}
