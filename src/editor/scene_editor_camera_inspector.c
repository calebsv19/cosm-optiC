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
    if(field>=0) SDL_StopTextInput();
    field=-1;feedback[0]=0;
}
static int selected_point(void) {
    int selected=CameraEditorGetSelectedPointIndex();
    return selected>=0 && selected<sceneSettings.cameraPath.numPoints?selected:0;
}
static double value_at(int index,int which) {
    bool path=sceneSettings.cameraPath.numPoints>0;
    switch(which) {
        case 0:return path?sceneSettings.cameraPath.points[index].x:sceneSettings.camera.x;
        case 1:return path?sceneSettings.cameraPath.points[index].y:sceneSettings.camera.y;
        case 2:return path?sceneSettings.cameraPath3D.point_z[index]:sceneSettings.cameraZ;
        case 3:return (path?sceneSettings.cameraPath.rotations[index]:sceneSettings.camera.rotation)*degrees;
        default:return path?sceneSettings.cameraPath3D.point_pitch[index]*degrees:0;
    }
}
static bool commit(double value) {
    Path path=sceneSettings.cameraPath;
    CameraPath3D depth=sceneSettings.cameraPath3D;
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
    if(!SceneEditorDocumentSetCameraPath(&path,&depth,revision,feedback,sizeof(feedback))) return false;
    CameraEditorSetSelectedPointIndex(point);
    return true;
}
static bool inside(SDL_Rect r,int x,int y) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
bool SceneEditorCameraInspectorEvent(SDL_Event* event,const SceneEditorPaneLayout* layout) {
    if(!event || !layout) return false;
    SDL_Rect r=layout->right_content_rect;
    if(event->type==SDL_MOUSEBUTTONDOWN) {
        if(!inside(r,event->button.x,event->button.y)) {SceneEditorCameraInspectorReset();return false;}
        SceneEditorTimelineReleaseFocus();
        int row=(event->button.y-r.y-46)/32;
        if(event->button.y>=r.y+46 && row>=0 && row<5 && SceneEditorDocumentIsOpen()) {
            field=row;point=selected_point();revision=SceneEditorDocumentRevision();
            input[0]=0;feedback[0]=0;SDL_StartTextInput();
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
            if(!errno && end!=input && !*end && commit(value)) {field=-1;SDL_StopTextInput();}
            else snprintf(feedback,sizeof(feedback),"Invalid value or scene changed. Esc cancels.");
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
    snprintf(title,sizeof(title),sceneSettings.cameraPath.numPoints?"Camera path point %d":"Camera pose",index+1);
    ray_tracing_text_draw_utf8_at(renderer,font,title,r.x+10,r.y+10,color);
    for(int i=0;i<5;++i) {
        SDL_Rect box={r.x+8,r.y+46+i*32,r.w-16,28};
        SDL_SetRenderDrawColor(renderer,field==i?53:42,field==i?72:47,field==i?94:60,255);SDL_RenderFillRect(renderer,&box);
        if(field==i) snprintf(rows[i],sizeof(rows[i]),"%s: %s_",labels[i],input);
        else snprintf(rows[i],sizeof(rows[i]),"%s: %.6g",labels[i],value_at(index,i));
        ray_tracing_text_draw_utf8_at(renderer,font,rows[i],box.x+6,box.y+5,color);
    }
    ray_tracing_text_draw_utf8_at(renderer,font,"Select a path point to edit its pose.",r.x+10,r.y+220,color);
    ray_tracing_text_draw_utf8_at(renderer,font,"Enter commits. Escape cancels.",r.x+10,r.y+244,color);
    ray_tracing_text_draw_utf8_at(renderer,font,feedback,r.x+10,r.y+276,color);
    SDL_RenderSetClipRect(renderer,clipped?&prior:NULL);
}
