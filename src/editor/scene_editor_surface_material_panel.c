#include "render/runtime_surface_mapping.h"
#include "editor/scene_editor_surface_material_panel.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_typography.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Values are edited in the retained document; runtime stacks are readback only. */
static const char* roles[]={"Object","front","back","left","right","top","bottom"};
static const char* keys[]={"opacity","roughness_influence","offset_u","offset_v","strength","grain"};
static const char* groups[]={"","","placement","placement","placement","parameters"};
static const char* labels[]={"Opacity","Roughness","Layer U","Layer V","Strength","Grain"};
static SDL_Rect scope_rect,layer_rect,enable_rect,reset_rect,fields[6];
static int selected=-1,scope=0,layer_index=0,editing=-1;
static unsigned long long revision;
static char draft[64],status[256],layer_id[32];
static json_object* get(json_object* o,const char* k) {json_object* v=NULL;if(o) json_object_object_get_ex(o,k,&v);return v;}
static json_object* clone(json_object* o) {return o?json_tokener_parse(json_object_to_json_string(o)):NULL;}
static json_object* current(int index) {
    char text[65536];return SceneEditorDocumentGetSurfaceMaterialJSON(index,text,sizeof(text))?json_tokener_parse(text):NULL;
}
static bool inside(SDL_Rect r,int x,int y) {return r.w>0 && x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h;}
static void cancel(void) {editing=-1;SDL_StopTextInput();}
static void draw(SDL_Renderer* r,SDL_Rect b,const char* text,bool active) {
    SDL_SetRenderDrawColor(r,active?65:38,active?76:42,active?88:46,255);SDL_RenderFillRect(r,&b);
    SceneEditorLabelLeft(r,(SDL_Rect){b.x+5,b.y,b.w-10,b.h},text,(SDL_Color){222,226,230,255});
}
static json_object* region(json_object* binding) {
    json_object* regions=get(binding,"regions");
    for(size_t i=0;regions && i<json_object_array_length(regions);++i) {
        json_object* r=json_object_array_get_idx(regions,i);const char* role=json_object_get_string(get(r,"face_role"));
        if(role && !strcmp(role,roles[scope])) return r;
    }
    return NULL;
}
static json_object* layers(json_object* source,bool* graph) {
    json_object* g=get(source,"material_graph");if(!g) g=get(source,"materialGraph");
    *graph=g!=NULL;return g?get(g,"nodes"):get(get(source,"material_texture_stack"),"layers");
}
static json_object* active_layer(json_object* source) {
    bool graph;json_object* list=layers(source,&graph);int n=list?(int)json_object_array_length(list):0;
    if(!n) return NULL;layer_index%=n;
    json_object* layer=json_object_array_get_idx(list,layer_index);return graph?get(layer,"layer"):layer;
}
int SceneEditorSurfaceMaterialPanelRender(SDL_Renderer* r,SDL_Rect b,int index) {
    if(selected!=index) {cancel();selected=index;scope=layer_index=0;status[0]=0;}
    memset(fields,0,sizeof(fields));scope_rect=layer_rect=enable_rect=reset_rect=(SDL_Rect){0};
    json_object* row=current(index);if(!row) return b.y;
    int y=b.y;json_object* binding=get(row,"surface_material_binding");
    if(!binding) {
        enable_rect=(SDL_Rect){b.x,y,b.w,27};draw(r,enable_rect,"Enable retained source editing",false);
        json_object_put(row);return y+31;
    }
    if(!RuntimeSurfaceMappingPreviewSupported(index)) {
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},"Asset graph: renderer only; preview unavailable",(SDL_Color){230,188,145,255});y+=28;
    }
    scope_rect=(SDL_Rect){b.x,y,b.w,27};char text[180];snprintf(text,sizeof(text),"Scope: %s  >",roles[scope]);draw(r,scope_rect,text,false);y+=31;
    json_object* reg=scope?region(binding):NULL,*source=reg?get(reg,"source"):NULL;
    bool overridden=source!=NULL;if(!source) source=row;
    json_object* layer=active_layer(source);const char* id=json_object_get_string(get(layer,"id"));
    snprintf(layer_id,sizeof(layer_id),"%s",id?id:"");
    snprintf(text,sizeof(text),"Layer: %s  >",id?id:"unsupported");layer_rect=(SDL_Rect){b.x,y,b.w,27};draw(r,layer_rect,text,false);y+=31;
    bool graph=false;(void)layers(source,&graph);
    snprintf(text,sizeof(text),"%s | %s",graph?"Graph retained":"Stack retained",scope?(overridden?"Region override":"Inherited; edit creates override"):"Object default");
    SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},text,(SDL_Color){180,198,212,255});y+=28;
    RuntimeMaterialTextureStack compiled;bool compiled_ok=RuntimeSurfaceMaterialCompileSource(source,&compiled) && layer_index<compiled.layerCount;
    for(int i=0;i<6 && layer;++i) {
        if(y+27>b.y+b.h) break;
        json_object* owner=groups[i][0]?get(layer,groups[i]):layer,*v=get(owner,keys[i]);
        double value=v?json_object_get_double(v):(i==0 || i==4?1:(i==5?.5:0));
        if(compiled_ok) {
            const RuntimeMaterialTextureLayer* actual=&compiled.layers[layer_index];
            const double values[]={actual->opacity,actual->roughnessInfluence,actual->placement.offsetU,actual->placement.offsetV,actual->placement.strength,actual->params.grain};
            value=values[i];
        }
        fields[i]=(SDL_Rect){b.x+(i%2)*(b.w/2),y,b.w/2-3,25};
        if(editing==i) snprintf(text,sizeof(text),"%s: %s",labels[i],draft);
        else snprintf(text,sizeof(text),"%s: %.6g",labels[i],value);
        draw(r,fields[i],text,editing==i);if(i%2) y+=29;
    }
    if(scope && y+28<b.y+b.h) {reset_rect=(SDL_Rect){b.x,y,b.w,25};draw(r,reset_rect,"Reset region to object defaults",false);y+=29;}
    if(status[0]) {SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,24},status,(SDL_Color){225,196,153,255});y+=28;}
    json_object_put(row);return y;
}
static bool apply_value(int index,double value) {
    if(scope==0) return SceneEditorDocumentSetSurfaceLayerValue(index,layer_id,groups[editing],keys[editing],value,revision,status,sizeof(status));
    json_object* row=current(index);if(!row) return false;
    json_object* binding=get(row,"surface_material_binding"),*reg=region(binding);
    if(!reg) {
        json_object* list=get(binding,"regions");if(!list) {list=json_object_new_array();json_object_object_add(binding,"regions",list);}
        reg=json_object_new_object();json_object_object_add(reg,"id",json_object_new_string(roles[scope]));json_object_object_add(reg,"face_role",json_object_new_string(roles[scope]));json_object_array_add(list,reg);
    }
    json_object* source=get(reg,"source");
    if(!source) {
        source=json_object_new_object();json_object_object_add(reg,"source",source);
        const char* names[]={"material_graph","materialGraph","material_texture_stack"};
        for(int i=0;i<3;++i) if(get(row,names[i])) json_object_object_add(source,names[i],clone(get(row,names[i])));
    }
    json_object* layer=active_layer(source);if(!layer) {json_object_put(row);return false;}
    json_object* owner=layer;
    if(groups[editing][0]) {owner=get(layer,groups[editing]);if(!owner) {owner=json_object_new_object();json_object_object_add(layer,groups[editing],owner);}}
    json_object_object_add(owner,keys[editing],json_object_new_double(value));
    bool ok=SceneEditorDocumentSetSurfaceBinding(index,json_object_to_json_string(binding),revision,status,sizeof(status));json_object_put(row);return ok;
}
bool SceneEditorSurfaceMaterialPanelEvent(const SDL_Event* e,int index) {
    if(!e) return false;
    if(index!=selected) {cancel();return false;}
    if(editing>=0 && ((e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) ||
       (e->type==SDL_MOUSEBUTTONDOWN && !inside(fields[editing],e->button.x,e->button.y)))) cancel();
    if(editing>=0) {
        if(revision!=SceneEditorDocumentRevision()) {cancel();return true;}
        if(e->type==SDL_TEXTINPUT) {if(strlen(draft)+strlen(e->text.text)<sizeof(draft)) strcat(draft,e->text.text);return true;}
        if(e->type==SDL_KEYDOWN) {
            if(e->key.keysym.sym==SDLK_ESCAPE) {cancel();return true;}
            if(e->key.keysym.sym==SDLK_a && (e->key.keysym.mod&(KMOD_GUI|KMOD_CTRL))) {draft[0]=0;return true;}
            if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(draft);if(n) draft[n-1]=0;return true;}
            if(e->key.keysym.sym==SDLK_RETURN) {
                char* end;double value=strtod(draft,&end);
                if(end==draft || *end || !isfinite(value)) {snprintf(status,sizeof(status),"Enter a finite numeric value");return true;}
                if(RayTracingDeepRenderDesktopHost_HasActiveWork()) {cancel();return true;}
                if(apply_value(index,value)) snprintf(status,sizeof(status),"Source updated; Undo available");cancel();return true;
            }
            return true;
        }
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    int x=e->button.x,y=e->button.y;
    if(inside(scope_rect,x,y)) {
        SceneEditorDocumentObjectInfo info;int count=1;
        if(SceneEditorDocumentObjectAt(0,&info)) {
            for(int i=0;i<SceneEditorDocumentObjectCount();++i) if(SceneEditorDocumentObjectAt(i,&info) && info.runtime_index==index) {
                count=!strcmp(info.type,"rect_prism_primitive")?7:!strcmp(info.type,"plane_primitive")?2:1;break;
            }
        }
        scope=(scope+1)%count;layer_index=0;cancel();return true;
    }
    if(inside(layer_rect,x,y)) {++layer_index;cancel();return true;}
    if(RayTracingDeepRenderDesktopHost_HasActiveWork()) return false;
    if(inside(enable_rect,x,y)) {
        SceneEditorDocumentSetSurfaceBinding(index,"{\"version\":1,\"required_capability\":\"optic.surface_material_v3\"}",SceneEditorDocumentRevision(),status,sizeof(status));return true;
    }
    if(inside(reset_rect,x,y)) {
        json_object* row=current(index),*binding=get(row,"surface_material_binding"),*list=get(binding,"regions");
        for(size_t i=0;list && i<json_object_array_length(list);++i) {
            json_object* item=json_object_array_get_idx(list,i);const char* role=json_object_get_string(get(item,"face_role"));
            if(role && !strcmp(role,roles[scope])) {json_object_array_del_idx(list,i,1);break;}
        }
        SceneEditorDocumentSetSurfaceBinding(index,json_object_to_json_string(binding),SceneEditorDocumentRevision(),status,sizeof(status));json_object_put(row);return true;
    }
    for(int i=0;i<6;++i) if(inside(fields[i],x,y)) {
        editing=i;revision=SceneEditorDocumentRevision();draft[0]=0;SDL_StartTextInput();return true;
    }
    return false;
}
bool SceneEditorSurfaceMaterialPanelActive(void) {return editing>=0;}
bool SceneEditorSurfaceMaterialPanelControl(const char* name,SDL_Rect* out) {
    if(!name || !out) return false;
    if(!strcmp(name,"scope")) *out=scope_rect;
    else if(!strcmp(name,"layer")) *out=layer_rect;
    else if(!strcmp(name,"enable")) *out=enable_rect;
    else if(!strcmp(name,"reset")) *out=reset_rect;
    else {for(int i=0;i<6;++i) if(!strcmp(name,keys[i])) {*out=fields[i];return out->w>0;}return false;}
    return out->w>0;
}
