#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_atmosphere_inspector.h"
#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_atmosphere_viewport.h"
#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/scene_editor_transform_ergonomics.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_timeline.h"
#include "render/runtime_volume_3d_transform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {MOVE,ROTATE,SCALE,POSITION,ROTATION,SIZING,DIMENSIONS,FRAME,FIT,PROXY,SNAP,COUNT};
static const char* names[]={"move","rotate","scale","position","rotation","sizing","dimensions","frame","fit","proxy","snap"};
static struct {
    SDL_Rect rects[COUNT];char labels[COUNT][256],draft[128],status[512];
    int editing;unsigned long long revision;bool visible;
    SceneAtmosphereAttachment original;RuntimeVolumeAttachment3D geometry;Vec3 centre;
} panel={.editing=-1};
void SceneEditorAtmosphereInspectorReset(void) {
    if(panel.editing>=0) do{scene_editor_shared_field_end(panel.draft);SDL_StopTextInput();}while(0);panel.editing=-1;panel.visible=false;memset(panel.rects,0,sizeof(panel.rects));
}
bool SceneEditorAtmosphereInspectorActive(void) {return panel.editing>=0;}
void SceneEditorAtmosphereInspectorBeginFrame(void) {
    if(!panel.visible) SceneEditorAtmosphereInspectorReset();
    panel.visible=false;memset(panel.rects,0,sizeof(panel.rects));
}
static void button(SDL_Renderer* r,SDL_Rect bounds,int id,int* y,const char* text,bool enabled) {
    snprintf(panel.labels[id],sizeof(panel.labels[id]),"%s",text);
    SDL_Rect rect={bounds.x+8,*y,bounds.w-16,28};
    SceneEditorSharedUiPanelButton(r,69,128+id,rect,panel.labels[id],false,enabled);
    if(enabled && rect.y>=bounds.y && rect.y+rect.h<=bounds.y+bounds.h) panel.rects[id]=rect;
    *y+=32;
}
static void field(SDL_Renderer* r,SDL_Rect bounds,int id,int* y,const char* name,const double v[3]) {
    char text[256];
    if(panel.editing==id) snprintf(text,sizeof(text),"%s: %s_",name,panel.draft);
    else snprintf(text,sizeof(text),"%s: %.5g, %.5g, %.5g",name,v[0],v[1],v[2]);
    button(r,bounds,id,y,text,true);
}
int SceneEditorAtmosphereInspectorDraw(SDL_Renderer* r,SDL_Rect bounds) {
    memset(panel.rects,0,sizeof(panel.rects));panel.visible=false;int y=bounds.y+6;
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT || !SceneEditorAtmosphereSelected()) {
        SceneEditorAtmosphereInspectorReset();
        SceneEditorLabelLeft(r,(SDL_Rect){bounds.x+8,y,bounds.w-16,28},"Select Atmosphere to edit its volume",(SDL_Color){185,195,205,255});return y+32;
    }
    if(panel.revision!=SceneEditorDocumentRevision()) SceneEditorAtmosphereInspectorReset();
    if(panel.editing<0) {
        if(!SceneEditorAtmosphereRead(&panel.original) || !SceneEditorAtmosphereGeometry(&panel.geometry,panel.status,sizeof(panel.status))) {
            SceneEditorLabelWrapped(r,bounds,panel.status,(SDL_Color){245,180,110,255});return y+60;
        }
        if(SceneEditorAtmospherePlacementPreview(&panel.original))
            SceneEditorAtmospherePlaceGeometry(&panel.geometry,&panel.original,SceneEditorDocumentWorldScale());
        panel.centre=SceneEditorAtmosphereCentre(&panel.geometry);panel.revision=SceneEditorDocumentRevision();
    }
    panel.visible=true;
    SceneEditorLabelLeft(r,(SDL_Rect){bounds.x+8,y,bounds.w-16,28},SceneEditorAtmosphereGestureActive()?"Atmosphere | drag preview":"Atmosphere volume",(SDL_Color){255,210,90,255});y+=34;
    const char* tools[]={"Move (W)","Rotate (R)","Scale (E)"};
    for(int i=0;i<3;++i) {
        snprintf(panel.labels[i],sizeof(panel.labels[i]),"%s%s",tools[i],(int)SceneEditorObjectTransformModeGet()==i?"  *":"");
        SDL_Rect rect={bounds.x+8+i*(bounds.w-16)/3,y,(bounds.w-16)/3-4,28};
        SceneEditorSharedUiPanelButton(r,69,256+i,rect,panel.labels[i],(int)SceneEditorObjectTransformModeGet()==i,true);
        if(rect.y+rect.h<=bounds.y+bounds.h) panel.rects[i]=rect;
    }y+=40;
    double w=SceneEditorDocumentWorldScale(),centre[]={panel.centre.x/w,panel.centre.y/w,panel.centre.z/w};
    field(r,bounds,POSITION,&y,"Centre XYZ",centre);
    field(r,bounds,ROTATION,&y,"Rotation XYZ (degrees)",panel.original.rotation_degrees);
    field(r,bounds,SIZING,&y,"Scale XYZ",panel.original.scale);
    Vec3 size=vec3_sub(panel.geometry.grid.boundsMax,panel.geometry.grid.boundsMin);
    double dims[]={size.x*panel.original.scale[0]/w,size.y*panel.original.scale[1]/w,size.z*panel.original.scale[2]/w};
    field(r,bounds,DIMENSIONS,&y,"Size XYZ (scene units)",dims);
    button(r,bounds,FRAME,&y,"Frame atmosphere (F)",true);
    button(r,bounds,FIT,&y,"Fit volume to scene",true);
    button(r,bounds,PROXY,&y,SceneEditorAtmosphereDensityProxy()?"Density guide: On":"Density guide: Off",true);
    button(r,bounds,SNAP,&y,SceneEditorTransformSnapEnabled()?"Snap: On":"Snap: Off",true);
    SDL_Rect hint={bounds.x+8,y,bounds.w-16,100};
    y+=SceneEditorLabelWrapped(r,hint,"XYZ fields use comma-separated values. Gizmos rotate and scale around the volume centre. Density guide shows cache structure; use rendering to judge lighting.",(SDL_Color){170,190,205,255})+10;
    if(!strcmp(panel.original.source_kind,"scene_bundle")) {
        y+=SceneEditorLabelWrapped(r,(SDL_Rect){bounds.x+8,y,bounds.w-16,70},"Volume transforms do not move bundled water geometry.",(SDL_Color){235,190,120,255})+8;
    }
    if(panel.status[0]) y+=SceneEditorLabelWrapped(r,(SDL_Rect){bounds.x+8,y,bounds.w-16,100},panel.status,(SDL_Color){230,200,130,255});
    return y;
}
static bool apply(void) {
    double values[3];const char* p=panel.draft;char* end;
    for(int i=0;i<3;++i) {
        values[i]=strtod(p,&end);if(end==p || !isfinite(values[i])) return false;
        p=end;while(*p==' ')++p;
        if(i<2) {if(*p!=',') return false;++p;}
    }
    if(*p) return false;
    SceneAtmosphereAttachment v=panel.original;double w=SceneEditorDocumentWorldScale();
    Vec3 centre=panel.centre;
    if(panel.editing==POSITION) centre=vec3(values[0]*w,values[1]*w,values[2]*w);
    else if(panel.editing==ROTATION) memcpy(v.rotation_degrees,values,sizeof(values));
    else if(panel.editing==SIZING) memcpy(v.scale,values,sizeof(values));
    else if(panel.editing==DIMENSIONS) {
        Vec3 size=vec3_sub(panel.geometry.grid.boundsMax,panel.geometry.grid.boundsMin);
        v.scale[0]=values[0]*w/size.x;v.scale[1]=values[1]*w/size.y;v.scale[2]=values[2]*w/size.z;
    }
    if(!SceneEditorAtmosphereSetCentre(&v,&panel.geometry,centre,w)) {
        snprintf(panel.status,sizeof(panel.status),"Enter positive size/scale and finite XYZ values.");return false;
    }
    return SceneEditorAtmosphereSet(&v,panel.revision,panel.status,sizeof(panel.status));
}
bool SceneEditorAtmosphereInspectorEvent(SDL_Event* e) {
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT || !SceneEditorAtmosphereSelected()) {SceneEditorAtmosphereInspectorReset();return false;}
    if(e->type==SDL_WINDOWEVENT && (e->window.event==SDL_WINDOWEVENT_FOCUS_LOST || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) {SceneEditorAtmosphereInspectorReset();return false;}
    if(panel.editing>=0 && panel.revision!=SceneEditorDocumentRevision()) {SceneEditorAtmosphereInspectorReset();return false;}
    if(panel.editing>=0) {
        if((panel.editing>=0) && scene_editor_shared_field_event(panel.draft,sizeof(panel.draft),e))return true;
    if(e->type==SDL_TEXTINPUT) {if(strlen(panel.draft)+strlen(e->text.text)<sizeof(panel.draft)) strcat(panel.draft,e->text.text);return true;}
        if(e->type==SDL_KEYDOWN) {
            if(e->key.keysym.sym==SDLK_ESCAPE) SceneEditorAtmosphereInspectorReset();
            else if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(panel.draft);if(n)panel.draft[n-1]=0;}
            else if(e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER) {
                panel.status[0]=0;bool ok=apply();
                if(ok) snprintf(panel.status,sizeof(panel.status),"Atmosphere placement updated; Undo available.");
                else if(!panel.status[0]) snprintf(panel.status,sizeof(panel.status),"Enter three comma-separated XYZ values.");
                SceneEditorAtmosphereInspectorReset();SceneEditorChromeShellSetActionFeedback(panel.status,4000);
            }return true;
        }
        return e->type==SDL_KEYUP || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEWHEEL;
    }
    if(!panel.visible || e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    SDL_Point point={e->button.x,e->button.y};
    for(int i=0;i<COUNT;++i) if(panel.rects[i].w && SDL_PointInRect(&point,&panel.rects[i])) {
        if(i<=SCALE) {SceneEditorAtmospherePreviewSetEnabled(false);SceneEditorObjectTransformModeSet(i);}
        else if(i==FRAME) SceneEditorAtmosphereFrame();
        else if(i==FIT) {
            SceneAtmosphereAttachment v=panel.original;
            bool ok=SceneEditorAtmosphereFit(&v,&panel.geometry,NULL) && SceneEditorAtmosphereSet(&v,panel.revision,panel.status,sizeof(panel.status));
            if(ok) {SceneEditorAtmosphereFrame();snprintf(panel.status,sizeof(panel.status),"Volume fitted to scene; Undo available.");}
        }
        else if(i==PROXY) SceneEditorAtmosphereDensityProxyToggle();
        else if(i==SNAP) SceneEditorTransformSnapToggle();
        else {SceneEditorTimelinePause();panel.editing=i;panel.draft[0]=0;do{scene_editor_shared_field_begin(panel.draft,sizeof(panel.draft),0);SDL_StartTextInput();}while(0);}
        return true;
    }return false;
}
bool SceneEditorAtmosphereInspectorControl(const char* name,SDL_Rect* out) {
    for(int i=0;i<COUNT;++i) if(!strcmp(name,names[i]) && panel.rects[i].w) {*out=panel.rects[i];return true;}return false;
}
