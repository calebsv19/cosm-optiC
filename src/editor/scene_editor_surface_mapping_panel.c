#include "editor/scene_editor_surface_mapping_panel.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/runtime_surface_mapping.h"
#include "render/runtime_surface_sampling.h"
#include "render/runtime_surface_graph.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <json-c/json.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* keys[]={"tile_m","tile_m","offset_m","offset_m","reference_radius_m","seam_rad","pole_radius_m","rotation_rad","origin_m","origin_m","origin_m","seed","height_range_m","height_range_m"};
static const char* names[]={"tile_width","tile_height","offset_u","offset_v","radius","seam","pole","rotation","origin_x","origin_y","origin_z","seed","height_min","height_max"};
static const char* labels[]={"Width m","Height m","Offset U m","Offset V m","Radius m","Seam rad","Pole fade m","Rotation rad","Origin X m","Origin Y m","Origin Z m","Seed","Detail min m","Detail max m"};
static const int components[]={0,1,0,1,-1,-1,-1,-1,0,1,2,-1,0,1};
#define FIELDS 14
static SDL_Rect expand,methods[3],space,axis,fields[FIELDS];
static bool opened,enabled;static int selected=-1,edit=-1;static unsigned long long edit_revision;
static char draft[80],status[256];
static bool inside(SDL_Rect r,int x,int y) {return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static json_object* get(json_object* o,const char* key) {json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;}
static json_object* current(int index) {
    char json[8192];if(!SceneEditorDocumentGetSurfaceMappingJSON(index,json,sizeof(json))) return NULL;
    return json_tokener_parse(json);
}
static const char* field_key(json_object* m,int i) {
    if(json_object_get_int(get(m,"version"))==3 && i<4) return i<2?"uv_scale":"uv_offset";
    return keys[i];
}
static void cancel(void) {edit=-1;SDL_StopTextInput();}
static void button(SDL_Renderer* r,SDL_Rect rect,const char* text,bool active) {
    SDL_SetRenderDrawColor(r,active?66:38,active?76:42,active?85:46,255);SDL_RenderFillRect(r,&rect);
    SceneEditorLabelLeft(r,(SDL_Rect){rect.x+5,rect.y,rect.w-10,rect.h},text,(SDL_Color){220,223,226,255});
}
static bool apply(json_object* map,int index) {
    bool ok=SceneEditorDocumentSetSurfaceMappingForSceneIndex(index,map?json_object_to_json_string(map):NULL,status,sizeof(status));
    if(ok) snprintf(status,sizeof(status),"Mapping updated; Undo available");
    if(map) json_object_put(map);return ok;
}
static json_object* defaults(bool axial) {
    json_object* m=json_tokener_parse("{\"version\":1,\"required_capability\":\"optic.planar_surface_v1\",\"method\":\"planar\",\"space\":\"object_rest\",\"source_domain\":\"brick_cells_v1\",\"scale_policy\":\"stretch_with_object\",\"origin_m\":[0,0,0],\"axis_u\":[1,0,0],\"axis_v\":[0,1,0],\"tile_m\":[0.5,0.25],\"offset_m\":[0,0],\"pivot_m\":[0,0],\"rotation_rad\":0,\"seed\":1729}");
    if(axial) {
        json_object_object_add(m,"version",json_object_new_int(2));
        json_object_object_add(m,"required_capability",json_object_new_string("optic.axial_surface_v2"));
        json_object_object_add(m,"method",json_object_new_string("axial_height"));
        json_object_object_add(m,"axis_v",json_tokener_parse("[0,0,1]"));
        json_object_object_add(m,"reference_radius_m",json_object_new_double(1));
        json_object_object_add(m,"seam_rad",json_object_new_double(0));
        json_object_object_add(m,"pole_radius_m",json_object_new_double(.12));
        json_object_object_add(m,"height_range_m",json_tokener_parse("[-1,1]"));
        json_object_object_add(m,"repeat_policy",json_object_new_string("integer_circumference"));
        json_object_object_add(m,"pole_policy",json_object_new_string("fade_to_base"));
    }
    return m;
}
int SceneEditorSurfaceMappingPanelRender(SDL_Renderer* r,SDL_Rect b,int y,int index,bool editable) {
    if(index!=selected) {cancel();selected=index;status[0]=0;}
    enabled=editable;memset(methods,0,sizeof(methods));memset(fields,0,sizeof(fields));space=axis=(SDL_Rect){0};
    expand=(SDL_Rect){b.x,y,b.w,25};button(r,expand,opened?"Surface mapping  -":"Surface mapping  +",opened);y+=29;
    if(RuntimeSurfaceGraphActive(index)) {
        /* Graph coordinates belong to retained nodes, not a second mapping declaration. */
        cancel();enabled=false;
        if(opened) {
            SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"Coordinates: material graph nodes",(SDL_Color){185,200,210,255});y+=26;
            SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"Edit in the Material inspector.",(SDL_Color){185,200,210,255});y+=26;
        }
        return y;
    }
    if(!opened || index<0) return y;
    json_object* m=current(index);int version=m?json_object_get_int(get(m,"version")):0;
    if(RuntimeSurfaceSamplingActive(index)){
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"Sampling: filtered / linear color",(SDL_Color){185,200,210,255});y+=26;
    }
    if(version==3) {
        char text[150];snprintf(text,sizeof(text),"Authored UV set: %s",json_object_get_string(get(m,"uv_set_id")));
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},text,(SDL_Color){185,200,210,255});y+=26;
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"LOD: source detail preserves UV seams",(SDL_Color){185,200,210,255});y+=26;
        const int indices[]={0,1,2,3,7,11};const char* uv_labels[]={"U scale","V scale","U offset","V offset","Rotation rad","Seed"};
        for(int k=0;k<6;++k) {
            int i=indices[k];json_object *v=get(m,field_key(m,i));if(components[i]>=0) v=json_object_array_get_idx(v,components[i]);
            fields[i]=(SDL_Rect){b.x+(k%2)*(b.w/2),y,b.w/2-3,24};
            if(edit==i) snprintf(text,sizeof(text),"%s: %s",uv_labels[k],draft);
            else snprintf(text,sizeof(text),"%s: %.7g",uv_labels[k],json_object_get_double(v));
            button(r,fields[i],text,edit==i);if(k%2) y+=27;
        }
        if(status[0]) {SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},status,(SDL_Color){225,195,150,255});y+=27;}
        json_object_put(m);return y;
    }
    for(int i=0;i<3;++i) {methods[i]=(SDL_Rect){b.x+i*b.w/3,y,b.w/3-3,25};button(r,methods[i],(const char*[]){"Legacy","Planar","Axial brick"}[i],version==i);}y+=29;
    if(m) {
        const char* sp=json_object_get_string(get(m,"space"));
        space=(SDL_Rect){b.x,y,b.w/2-2,25};axis=(SDL_Rect){b.x+b.w/2,y,b.w/2,25};
        button(r,space,sp && !strcmp(sp,"world")?"Space: World":"Space: Object",false);
        json_object* av=get(m,"axis_v");int direction=0;
        for(int i=1;i<3;++i) if(fabs(json_object_get_double(json_object_array_get_idx(av,i)))>.9) direction=i;
        char text[100];snprintf(text,sizeof(text),"%s axis: %c",version==2?"Height":"V",'X'+direction);button(r,axis,text,false);y+=29;
        int column=0;
        for(int i=0;i<FIELDS;++i) {
            if((version==1 && (i==4 || i==5 || i==6 || i>=12)) || (version==2 && i==7)) continue;
            fields[i]=(SDL_Rect){b.x+column*(b.w/2),y,b.w/2-3,24};json_object* v=get(m,field_key(m,i));if(components[i]>=0) v=json_object_array_get_idx(v,components[i]);
            if(edit==i) snprintf(text,sizeof(text),"%s: %s",labels[i],draft);
            else snprintf(text,sizeof(text),"%s: %.7g",labels[i],json_object_get_double(v));
            button(r,fields[i],text,edit==i);column=1-column;if(column==0) y+=27;
        }
        if(column) y+=27;
        if(version==2) {
            CoreAuthoredSurfaceMapping def;
            if(RuntimeSurfaceMappingDefinition(index,&def)) {
                double n=round(6.283185307179586*def.reference_radius_m/def.tile_m[0]);
                snprintf(text,sizeof(text),"%.0f bricks around; width %.4g m",n,6.283185307179586*def.reference_radius_m/n);
                SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,22},text,(SDL_Color){185,200,210,255});y+=22;
            }
            SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,22},"Poles: fade to base; tiles pinch",(SDL_Color){185,200,210,255});y+=22;
        }
        json_object_put(m);
    }
    if(status[0]) {SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},status,(SDL_Color){225,195,150,255});y+=27;}
    return y;
}
bool SceneEditorSurfaceMappingPanelEvent(const SDL_Event* e,int index) {
    if(!e || index!=selected) return false;
    /* Recheck the active source even before the next layout clears old hit targets. */
    if(RuntimeSurfaceGraphActive(index)) {
        cancel();
        if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT &&
           inside(expand,e->button.x,e->button.y)) {opened=!opened;return true;}
        return false;
    }
    if(edit>=0) {
        if(SceneEditorDocumentRevision()!=edit_revision) {cancel();return false;}
        if(e->type==SDL_TEXTINPUT) {if(strlen(draft)+strlen(e->text.text)<sizeof(draft)) strcat(draft,e->text.text);return true;}
        if(e->type==SDL_KEYDOWN) {
            if(e->key.keysym.sym==SDLK_ESCAPE) {cancel();return true;}
            if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(draft);if(n) draft[n-1]=0;return true;}
            if(e->key.keysym.sym==SDLK_a && (e->key.keysym.mod & (KMOD_CTRL|KMOD_GUI))) {draft[0]=0;return true;}
            if(e->key.keysym.sym==SDLK_RETURN) {
                char* end;double value=strtod(draft,&end);
                if(end==draft || *end || !isfinite(value) || (edit==11 && (value<0 || value>UINT32_MAX || floor(value)!=value))) {snprintf(status,sizeof(status),"Enter a valid finite value");return true;}
                if(!enabled || RayTracingDeepRenderDesktopHost_HasActiveWork()) {cancel();return true;}
                json_object* m=current(index);if(!m) {cancel();return true;}
                json_object* v=edit==11?json_object_new_int64((int64_t)value):json_object_new_double(value);
                if(components[edit]>=0) json_object_array_put_idx(get(m,field_key(m,edit)),components[edit],v);
                else json_object_object_add(m,field_key(m,edit),v);
                apply(m,index);cancel();return true;
            }
        }
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    int x=e->button.x,y=e->button.y;
    if(inside(expand,x,y)) {opened=!opened;cancel();return true;}
    if(!enabled || RayTracingDeepRenderDesktopHost_HasActiveWork()) return false;
    for(int i=0;i<3;++i) if(inside(methods[i],x,y)) {
        json_object* old=current(index);json_object* m=i?defaults(i==2):NULL;
        /* Method changes retain explicit seed and producer fields. */
        if(m && old) {
            json_object_object_foreach(old,key,value) {
                if(!get(m,key) || !strcmp(key,"seed")) json_object_object_add(m,key,json_object_get(value));
            }
        }
        if(old) json_object_put(old);apply(m,index);cancel();return true;
    }
    if(inside(space,x,y) || inside(axis,x,y)) {
        json_object* m=current(index);if(!m) return true;
        if(inside(space,x,y)) {
            const char* value=json_object_get_string(get(m,"space"));
            json_object_object_add(m,"space",json_object_new_string(value && !strcmp(value,"world")?"object_rest":"world"));
        } else {
            json_object* av=get(m,"axis_v");int a=0;for(int i=1;i<3;++i) if(fabs(json_object_get_double(json_object_array_get_idx(av,i)))>.9) a=i;
            a=(a+1)%3;json_object* v=json_object_new_array();json_object* u=json_object_new_array();
            for(int i=0;i<3;++i) {json_object_array_add(v,json_object_new_int(i==a));json_object_array_add(u,json_object_new_int(i==(a+1)%3));}
            json_object_object_add(m,"axis_u",u);json_object_object_add(m,"axis_v",v);
        }
        apply(m,index);return true;
    }
    for(int i=0;i<FIELDS;++i) if(inside(fields[i],x,y)) {
        json_object* m=current(index);if(!m) return true;
        json_object* v=get(m,field_key(m,i));if(components[i]>=0) v=json_object_array_get_idx(v,components[i]);
        snprintf(draft,sizeof(draft),"%.12g",json_object_get_double(v));json_object_put(m);
        edit=i;edit_revision=SceneEditorDocumentRevision();SDL_StartTextInput();return true;
    }
    return false;
}
void SceneEditorSurfaceMappingPanelRelease(const SDL_Event* e) {
    if(edit<0 || !e) return;
    if((e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) ||
       (e->type==SDL_MOUSEBUTTONDOWN && !inside(fields[edit],e->button.x,e->button.y))) cancel();
}
void SceneEditorSurfaceMappingPanelReset(void) {cancel();opened=false;selected=-1;enabled=false;expand=(SDL_Rect){0};}
bool SceneEditorSurfaceMappingPanelActive(void) {return edit>=0;}
bool SceneEditorSurfaceMappingPanelControl(const char* name,SDL_Rect* out) {
    if(!name || !out) return false;
    if(!strcmp(name,"expand")) *out=expand;
    else if(!strcmp(name,"legacy")) *out=methods[0];
    else if(!strcmp(name,"planar")) *out=methods[1];
    else if(!strcmp(name,"axial")) *out=methods[2];
    else if(!strcmp(name,"space")) *out=space;
    else if(!strcmp(name,"axis")) *out=axis;
    else {for(int i=0;i<FIELDS;++i) if(!strcmp(name,names[i])) {*out=fields[i];return out->w>0;}return false;}
    return out->w>0;
}
