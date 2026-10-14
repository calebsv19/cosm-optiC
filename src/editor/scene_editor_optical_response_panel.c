#include "editor/scene_editor_optical_response_panel.h"
#include "editor/scene_editor_optical_response.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_surface_material_panel.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <stdio.h>
#include <string.h>
static const char *keys[]={"transmission","ior","absorption_distance","thin_walled","base_roughness","interface_tint","absorption_color"};
static const char *labels[]={"Transmission","IOR","Absorption distance","Thin wall","Base roughness","Interface tint RGB24","Absorption color RGB24"};
static SDL_Rect controls[7][2],glass_button;
static double values[7];static int owner=-1;static unsigned long long revision;static char status[512];
static json_object *get(json_object *o,const char *key){json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;}
static bool inside(SDL_Rect r,int x,int y){return r.w>0 && x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h;}
void SceneEditorOpticalResponsePanelInvalidate(void){memset(controls,0,sizeof(controls));glass_button=(SDL_Rect){0};owner=-1;}
int SceneEditorOpticalResponsePanelRender(SDL_Renderer *r,SDL_Rect b,int index) {
    SceneEditorOpticalResponsePanelInvalidate();owner=index;revision=SceneEditorDocumentRevision();
    char message[512];json_object *read=SceneEditorOpticalResponseRead(index,message,sizeof(message));if(!read)return b.y;
    int y=b.y;bool glass=!strcmp(json_object_get_string(get(read,"family")),"glass");
    if(!glass){glass_button=(SDL_Rect){b.x,y,b.w,28};SceneEditorLabelLeft(r,glass_button,"Use glass optical response",(SDL_Color){222,226,230,255});json_object_put(read);return y+32;}
    SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"Glass optical response",(SDL_Color){222,226,230,255});y+=28;
    json_object *effective=get(read,"effective");
    for(int i=0;i<7 && y+28<=b.y+b.h;i++) {
        values[i]=json_object_get_double(get(effective,keys[i]));char line[100];
        if(i>=5)snprintf(line,sizeof(line),"%s: #%06X",labels[i],(unsigned)values[i]);
        else if(i==3)snprintf(line,sizeof(line),"%s: %s",labels[i],values[i]>0?"on":"off");
        else snprintf(line,sizeof(line),"%s: %.3g",labels[i],values[i]);
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w-52,26},line,(SDL_Color){222,226,230,255});
        for(int j=0;j<2;j++){controls[i][j]=(SDL_Rect){b.x+b.w-50+j*25,y,23,26};SceneEditorLabelLeft(r,controls[i][j],j?"+":"-",(SDL_Color){222,226,230,255});}y+=30;
    }
    const char *source=json_object_get_string(get(read,"roughness_source"));if(y+48<=b.y+b.h)y+=SceneEditorLabelWrapped(r,(SDL_Rect){b.x,y,b.w,48},source,(SDL_Color){180,194,210,255});
    if(status[0] && y+40<=b.y+b.h)y+=SceneEditorLabelWrapped(r,(SDL_Rect){b.x,y,b.w,40},status,(SDL_Color){220,190,160,255});
    json_object_put(read);return y;
}
bool SceneEditorOpticalResponsePanelEvent(const SDL_Event *e,int index) {
    if(!e || owner!=index || e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT || RayTracingDeepRenderDesktopHost_HasActiveWork())return false;
    json_object *settings=NULL;
    if(inside(glass_button,e->button.x,e->button.y)){settings=json_object_new_object();json_object_object_add(settings,"base_preset",json_object_new_string("glass"));}
    for(int i=0;i<7 && !settings;i++)for(int j=0;j<2 && !settings;j++)if(inside(controls[i][j],e->button.x,e->button.y)) {
        double step=i==1?.05:i==2?.25:i>=5?0x111111:.05;double min=i==1?1:i==2?.25:0,max=i==1?2.5:i==2?8:i>=5?0xFFFFFF:1;
        double v=values[i]+(j?step:-step);if(v<min)v=min;if(v>max)v=max;
        settings=json_object_new_object();json_object_object_add(settings,keys[i],i==3?json_object_new_boolean(j!=0):i>=5?json_object_new_int((int)v):json_object_new_double(v));
    }
    if(!settings)return false;
    bool ok=SceneEditorOpticalResponseSet(index,settings,revision,status,sizeof(status));json_object_put(settings);if(ok)snprintf(status,sizeof(status),"Optical response applied");return true;
}
bool SceneEditorOpticalResponsePanelControl(const char *name,SDL_Rect *out) {
    if(!name||!out)return false;
    if(!strcmp(name,"optical_glass")){*out=glass_button;return out->w>0;}
    for(int i=0;i<7;i++)for(int j=0;j<2;j++){char key[80];snprintf(key,sizeof(key),"optical_%s_%s",keys[i],j?"plus":"minus");if(!strcmp(name,key)){*out=controls[i][j];return out->w>0;}}
    return false;
}
