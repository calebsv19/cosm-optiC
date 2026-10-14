#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_lighting.h"
#include "editor/bezier_editor.h"
#include "editor/scene_editor_camera_inspector.h"
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_document.h"
#include "editor/camera_editor.h"
#include "editor/scene_editor_timeline.h"
#include "config/config_manager.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int field=-1, point;
static unsigned long long revision;
static char input[64], title[96], rows[5][96], feedback[160];
static const char* labels[]={"X","Y","Z","Yaw (degrees)","Pitch (degrees)"};
static const double degrees=57.29577951308232;
void SceneEditorCameraInspectorReset(void) {
    if(field>=0) do{scene_editor_shared_field_end(input);SDL_StopTextInput();}while(0);
    field=-1;feedback[0]=0;
}
bool SceneEditorCameraInspectorActive(void) {return field>=0;}
static bool is_camera(void) {return animSettings.editorMode==EDITOR_MODE_CAMERA;}
static Path* current_path(void) {return is_camera()?&sceneSettings.cameraPath:&sceneSettings.bezierPath;}
static CameraPath3D* current_depth(void) {return is_camera()?&sceneSettings.cameraPath3D:&sceneSettings.bezierPath3D;}
static bool selected_handle(void) {
    return is_camera()?(CameraEditorGetSelectionKind()==CAMERA_EDITOR_SELECTION_BEZIER_HANDLE || CameraEditorGetSelectionKind()==CAMERA_EDITOR_SELECTION_ROTATION_HANDLE):BezierEditorGetSelectionKind()==BEZIER_EDITOR_SELECTION_HANDLE;
}
static int field_count(void) {return is_camera() && !selected_handle()?5:3;}
static int selected_point(void) {
    int selected=is_camera()?CameraEditorGetSelectedPointIndex():BezierEditorGetSelectedPointIndex();
    return selected>=0 && selected<current_path()->numPoints?selected:-1;
}
static double value_at(int index,int which) {
    if(selected_handle()) {
        double xyz[3]={0};
        if(is_camera()) CameraEditorGetSelectedGizmoWorldPosition(&xyz[0],&xyz[1],&xyz[2]);
        else BezierEditorGetSelectionWorldPosition3D(&xyz[0],&xyz[1],&xyz[2]);
        return xyz[which<3?which:0];
    }
    bool path=index>=0 && current_path()->numPoints>0;
    switch(which) {
        case 0:return path?current_path()->points[index].x:sceneSettings.camera.x;
        case 1:return path?current_path()->points[index].y:sceneSettings.camera.y;
        case 2:return path?current_depth()->point_z[index]:sceneSettings.cameraZ;
        case 3:return (path?current_path()->rotations[index]:sceneSettings.camera.rotation)*degrees;
        default:return path?current_depth()->point_pitch[index]*degrees:0;
    }
}
static bool commit(double value) {
    if(selected_handle()) {
        if(!isfinite(value) || revision!=SceneEditorDocumentRevision()) return false;
        double xyz[3]={value_at(0,0),value_at(0,1),value_at(0,2)};xyz[field]=value;
        bool ok=is_camera()?SceneEditorCameraGestureBegin():SceneEditorLightGestureBegin();
        if(!ok) return false;
        ok=is_camera()?CameraEditorMoveSelectedGizmoTo(xyz[0],xyz[1],xyz[2]):BezierEditorMoveSelectionTo3D(xyz[0],xyz[1],xyz[2]);
        if(!ok) {if(is_camera()) SceneEditorCameraGestureCancel();else SceneEditorLightGestureCancel();return false;}
        return is_camera()?SceneEditorCameraGestureCommit():SceneEditorLightGestureCommit();
    }
    Path path=*current_path();
    CameraPath3D depth=*current_depth();
    if(!isfinite(value) || SceneEditorDocumentRevision()!=revision) return false;
    if(path.numPoints==0) {
        memset(&path,0,sizeof(path));memset(&depth,0,sizeof(depth));
        path.mode=BEZIER_CUBIC;path.numPoints=1;
        path.points[0]=(Point){sceneSettings.camera.x,sceneSettings.camera.y};
        path.rotations[0]=sceneSettings.camera.rotation;path.rotationSet[0]=true;
        depth.point_z[0]=sceneSettings.cameraZ;
        point=0;
    }
    if(point<0 || point>=path.numPoints) return false;
    switch(field) {
        case 0:path.points[point].x=value;break;
        case 1:path.points[point].y=value;break;
        case 2:depth.point_z[point]=value;break;
        case 3:path.rotations[point]=value/degrees;path.rotationSet[point]=true;break;
        case 4:depth.point_pitch[point]=value/degrees;break;
        default:return false;
    }
    if(is_camera() && path.numPoints==1 && SceneEditorDocumentStaticCameraEditable(NULL,0)) {
        double xyz[3]={path.points[0].x,path.points[0].y,depth.point_z[0]};
        bool ok=SceneEditorDocumentSetStaticCameraPose(xyz,path.rotations[0]*degrees,depth.point_pitch[0]*degrees,revision,feedback,sizeof(feedback));
        if(ok) CameraEditorSetSelectedPointIndex(0);
        return ok;
    }
    if(!is_camera() && path.numPoints==1 && SceneEditorLightingPrimaryEditable(NULL,0)) {
        double xyz[3]={path.points[0].x,path.points[0].y,depth.point_z[0]};
        json_object* read=SceneEditorLightingRead(),*light=NULL,*v=NULL;double intensity=0,radius=0;
        json_object_object_get_ex(read,"primary",&light);json_object_object_get_ex(light,"intensity",&v);intensity=json_object_get_double(v);
        json_object_object_get_ex(light,"radius_m",&v);radius=json_object_get_double(v);
        bool ok=SceneEditorLightingSetPrimary(xyz,intensity,radius,revision,feedback,sizeof(feedback));json_object_put(read);
        if(ok) BezierEditorSetSelectedPointIndex(0);return ok;
    }
    bool ok=is_camera()?SceneEditorDocumentSetCameraPath(&path,&depth,revision,feedback,sizeof(feedback)):
        SceneEditorDocumentSetLightPath(&path,&depth,revision,feedback,sizeof(feedback));
    if(!ok) return false;
    if(is_camera()) CameraEditorSetSelectedPointIndex(point);else BezierEditorSetSelectedPointIndex(point);
    return true;
}
static bool inside(SDL_Rect r,int x,int y) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
bool SceneEditorCameraInspectorEvent(SDL_Event* event,const SceneEditorPaneLayout* layout) {
    if(!event || !layout) return false;
    if((field>=0) && scene_editor_shared_field_event(input,sizeof(input),event))return true;
    SDL_Rect r=layout->right_content_rect;
    if(event->type==SDL_MOUSEBUTTONDOWN) {
        if(!inside(r,event->button.x,event->button.y)) {SceneEditorCameraInspectorReset();return false;}
        SceneEditorTimelineReleaseFocus();
        int row=(event->button.y-r.y-46)/32;
        if(event->button.y>=r.y+46 && row>=0 && row<field_count() && (selected_point()>=0 || selected_handle()) && SceneEditorDocumentIsOpen()) {
            field=row;point=selected_point();revision=SceneEditorDocumentRevision();
            input[0]=0;feedback[0]=0;do{scene_editor_shared_field_begin(input,sizeof(input),0);SDL_StartTextInput();}while(0);
        }
        return true;
    }
    if(field<0) return false;

    if(event->type==SDL_TEXTINPUT) {
        size_t n=strlen(input),added=strlen(event->text.text);
        if(n+added<sizeof(input)) memcpy(input+n,event->text.text,added+1);
        return true;
    }
    if(event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if(key==SDLK_ESCAPE) {SceneEditorCameraInspectorReset();return true;}
        if(key==SDLK_BACKSPACE) {size_t n=strlen(input);if(n) input[n-1]=0;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            char* end=NULL;errno=0;double value=strtod(input,&end);
            if(!errno && end!=input && !*end && commit(value)) {field=-1;do{scene_editor_shared_field_end(input);SDL_StopTextInput();}while(0);}
            else if(!feedback[0]) snprintf(feedback,sizeof(feedback),"Invalid value or scene changed. Esc cancels.");
        }
        return true;
    }
    return false;
}
void SceneEditorCameraInspectorRender(SDL_Renderer* renderer,const SceneEditorPaneLayout* layout) {
    if(!renderer || !layout) return;
    SDL_Rect r=layout->right_content_rect,prior;
    SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&prior);
    SDL_RenderSetClipRect(renderer,&r);
    SDL_SetRenderDrawColor(renderer,30,34,43,255);SDL_RenderFillRect(renderer,&r);
    TTF_Font* font=ray_tracing_font_runtime_get_ui_regular(renderer,12,9);
    SDL_Color color={225,230,240,255};
    int index=selected_point();
    if(selected_handle()) snprintf(title,sizeof(title),"%s path | Selected handle",is_camera()?"Camera":"Light");
    else if(index>=0) snprintf(title,sizeof(title),"%s path | Point %d",is_camera()?"Camera":"Light",index+1);
    else snprintf(title,sizeof(title),"%s path | Select a point",is_camera()?"Camera":"Light");
    scene_editor_shared_label_at(renderer,title,r.x+10,r.y+10,color,12);
    for(int i=0;i<field_count() && (index>=0 || selected_handle());++i) {
        SDL_Rect box={r.x+8,r.y+46+i*32,r.w-16,28};
        SDL_SetRenderDrawColor(renderer,field==i?53:42,field==i?72:47,field==i?94:60,255);SDL_RenderFillRect(renderer,&box);
        if(field==i) snprintf(rows[i],sizeof(rows[i]),"%s: %s_",labels[i],input);
        else snprintf(rows[i],sizeof(rows[i]),"%s: %.6g",labels[i],value_at(index,i));
        SceneEditorSharedUiPanelButton(renderer,74,128+i,box,rows[i],field==i,true);
    }
    scene_editor_shared_label_at(renderer,"Path geometry (not a keyframe).",r.x+10,r.y+220,color,12);
    scene_editor_shared_label_at(renderer,"Enter commits. Escape cancels.",r.x+10,r.y+244,color,12);
    scene_editor_shared_label_at(renderer,feedback,r.x+10,r.y+276,color,12);
    SDL_RenderSetClipRect(renderer,clipped?&prior:NULL);
}
