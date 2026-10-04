#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "editor/scene_editor_typography.h"
#ifdef __APPLE__
#include "../../src/platform/ray_tracing_macos_file_panel.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {PREVIEW,IMPORT,PRESET,GENERATE,ENABLED,VISIBLE,LIGHTING,APPEARANCE,PLACEMENT,PLAYBACK,
    DENSITY,GAMMA,ABSORPTION,SCATTER,STEP,TINT,TRANSLATION,ROTATION,SCALE,
    ANIMATED,RATE,START,SOURCE_START,MISSING,REMOVE,COUNT};
static const char* names[COUNT]={"preview","import","preset","generate","enabled","visible","lighting",
    "appearance","placement","playback","density","gamma","absorption","scatter","step","tint",
    "translation","rotation","scale","animated","rate","start","source_start","missing","remove"};
static const char* presets[]={"room_haze_v1","low_floor_fog_v1","cloud_bank_v1","mist_patch_v1","plume_column_v1"};
static const char* preset_names[]={"Room haze","Floor fog","Cloud bank","Mist patch","Plume"};
static struct {
    SDL_Rect rects[COUNT];char labels[COUNT][256],draft[128],status[512],source[1200],cache[128];
    int editing,preset;bool visible,appearance,placement,playback;
    unsigned long long revision;
    SceneAtmosphereAttachment value;
} panel={.editing=-1};
static void cancel(void) {if(panel.editing>=0) SDL_StopTextInput();panel.editing=-1;}
bool SceneEditorAtmospherePanelInteractionActive(void) {return panel.editing>=0;}
void SceneEditorAtmospherePanelBeginFrame(void) {
    memset(panel.rects,0,sizeof(panel.rects));panel.visible=false;
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) cancel();
}
static void button(SDL_Renderer* r,SDL_Rect bounds,int id,int* y,const char* label,bool enabled) {
    snprintf(panel.labels[id],sizeof(panel.labels[id]),"%s",label);
    SDL_Rect rect={bounds.x+8,*y,bounds.w-16,26};
    SceneEditorRenderButton(r,rect,panel.labels[id],false,enabled);
    if(enabled && rect.y>=bounds.y && rect.y+rect.h<=bounds.y+bounds.h) panel.rects[id]=rect;
    *y+=30;
}
static void field(SDL_Renderer* r,SDL_Rect bounds,int id,int* y,const char* label,double* values,int count) {
    char text[256];
    if(panel.editing==id) snprintf(text,sizeof(text),"%s: %s_",label,panel.draft);
    else if(count==3) snprintf(text,sizeof(text),"%s: %.4g, %.4g, %.4g",label,values[0],values[1],values[2]);
    else snprintf(text,sizeof(text),"%s: %.5g",label,*values);
    button(r,bounds,id,y,text,true);
}
int SceneEditorAtmospherePanelDraw(SDL_Renderer* r,SDL_Rect bounds) {
    SceneAtmosphereAttachment v;int y=bounds.y+4;
    if(!SceneEditorAtmosphereRead(&v)) return y;
    if(panel.revision!=SceneEditorDocumentRevision()) cancel();
    panel.value=v;panel.revision=SceneEditorDocumentRevision();panel.visible=true;
    button(r,bounds,PREVIEW,&y,SceneEditorAtmospherePreviewEnabled()?"Native preview: On":"Native preview: Off",true);
    button(r,bounds,IMPORT,&y,v.present?"Replace atmosphere...":"Attach atmosphere...",true);
    char label[256];snprintf(label,sizeof(label),"Preset: %s",preset_names[panel.preset]);
    button(r,bounds,PRESET,&y,label,true);
    button(r,bounds,GENERATE,&y,"Create static atmosphere",true);
    if(v.present) {
        const char* name=strrchr(v.source_path,'/');name=name?name+1:v.source_path;
        snprintf(panel.source,sizeof(panel.source),"%s | %s",name,v.source_kind);
        ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,11,8),panel.source,
            bounds.x+8,y,(SDL_Color){180,195,210,255});y+=24;
        RayEvaluatedSceneSnapshot snapshot;
        if(SceneEditorTimelineCopyEvaluated(&snapshot) && snapshot.simulation.valid)
            snprintf(panel.cache,sizeof(panel.cache),"Selected cache frame: %lld%s",(long long)snapshot.simulation.source_frame_index,v.animated?"":" (static)");
        else snprintf(panel.cache,sizeof(panel.cache),"%s",v.enabled?"Cache unavailable; check source and timing.":"Atmosphere disabled");
        ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,11,8),panel.cache,
            bounds.x+8,y,(SDL_Color){180,195,210,255});y+=24;
        button(r,bounds,ENABLED,&y,v.enabled?"Atmosphere: On":"Atmosphere: Off",true);
        button(r,bounds,VISIBLE,&y,v.visible?"Visible volume: On":"Visible volume: Off",true);
        button(r,bounds,LIGHTING,&y,v.affects_lighting?"Affects lighting: On":"Affects lighting: Off",true);
        button(r,bounds,APPEARANCE,&y,panel.appearance?"Appearance  -":"Appearance  +",true);
        if(panel.appearance) {
            field(r,bounds,DENSITY,&y,"Density",&v.density_scale,1);
            field(r,bounds,GAMMA,&y,"Density gamma",&v.density_gamma,1);
            field(r,bounds,ABSORPTION,&y,"Absorption",&v.absorption_gain,1);
            field(r,bounds,SCATTER,&y,"Scattering",&v.scatter_gain,1);
            field(r,bounds,STEP,&y,"Sampling step",&v.step_scale,1);
            field(r,bounds,TINT,&y,"Tint RGB",v.tint,3);
        }
        button(r,bounds,PLACEMENT,&y,panel.placement?"Volume placement  -":"Volume placement  +",true);
        if(panel.placement) {
            field(r,bounds,TRANSLATION,&y,"Position XYZ",v.translation,3);
            field(r,bounds,ROTATION,&y,"Rotation XYZ (deg)",v.rotation_degrees,3);
            field(r,bounds,SCALE,&y,"Scale XYZ",v.scale,3);
        }
        button(r,bounds,PLAYBACK,&y,panel.playback?"Cache timing  -":"Cache timing  +",true);
        if(panel.playback) {
            bool sequence=!strcmp(v.source_kind,"manifest") || !strcmp(v.source_kind,"scene_bundle");
            button(r,bounds,ANIMATED,&y,v.animated?"Playback: Animated":"Playback: Static",sequence);
            if(v.animated) {
                if(panel.editing==RATE) snprintf(label,sizeof(label),"Source FPS (N/D): %s_",panel.draft);
                else snprintf(label,sizeof(label),"Source FPS: %u/%u",v.source_rate.frames_per_second_numerator,v.source_rate.frames_per_second_denominator);
                button(r,bounds,RATE,&y,label,true);
                double start=v.timeline_start;field(r,bounds,START,&y,"Timeline start frame",&start,1);
            }
            if(sequence) {double start=v.source_start;field(r,bounds,SOURCE_START,&y,"Source start frame",&start,1);}
            button(r,bounds,MISSING,&y,v.missing_is_error?"Missing frame: Error":"Missing frame: Hold",sequence);
        }
        button(r,bounds,REMOVE,&y,"Remove atmosphere",true);
    }
    if(panel.status[0]) {
        SDL_Rect message={bounds.x+8,y,bounds.w-16,100};
        y+=SceneEditorLabelWrapped(r,message,panel.status,(SDL_Color){235,200,135,255})+8;
    }
    return y;
}
static bool numeric(int id,const char* text) {
    if(id==RATE) {
        char* end;unsigned long numerator=strtoul(text,&end,10);
        if(end==text || numerator<1 || numerator>1000000) return false;
        while(*end==' ') ++end;
        unsigned long denominator=1;
        if(*end=='/') {
            const char* start=end+1;denominator=strtoul(start,&end,10);
            if(end==start || denominator<1 || denominator>1000000) return false;
            while(*end==' ') ++end;
        }
        if(*end) return false;
        panel.value.source_rate=(TimelineRate){(uint32_t)numerator,(uint32_t)denominator};
        return true;
    }
    double values[3]={0};int count=(id==TINT || id==TRANSLATION || id==ROTATION || id==SCALE)?3:1;
    char* end;const char* p=text;
    for(int i=0;i<count;++i) {
        values[i]=strtod(p,&end);if(end==p || !isfinite(values[i])) return false;
        p=end;while(*p==' ') ++p;
        if(i+1<count) {if(*p!=',') return false;++p;}
    }
    if(*p) return false;
    SceneAtmosphereAttachment* v=&panel.value;double* target=NULL;
    switch(id) {
      case DENSITY:target=&v->density_scale;break;case GAMMA:target=&v->density_gamma;break;
      case ABSORPTION:target=&v->absorption_gain;break;case SCATTER:target=&v->scatter_gain;break;
      case STEP:target=&v->step_scale;break;case TINT:target=v->tint;break;
      case TRANSLATION:target=v->translation;break;case ROTATION:target=v->rotation_degrees;break;
      case SCALE:target=v->scale;break;
      case START:case SOURCE_START:
        if(fabs(values[0])>1e9 || values[0]!=floor(values[0])) return false;
        if(id==START) v->timeline_start=(int64_t)values[0];else v->source_start=(int64_t)values[0];break;
      default:return false;
    }
    if(target) memcpy(target,values,(size_t)count*sizeof(double));
    return true;
}
static void report(bool ok) {
    if(ok) snprintf(panel.status,sizeof(panel.status),"Atmosphere updated. Undo available; File Save to retain.");
    SceneEditorChromeShellSetActionFeedback(panel.status,6000);
}
bool SceneEditorAtmospherePanelEvent(SDL_Event* e) {
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) {cancel();return false;}
    if(!panel.visible) return false;
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {cancel();return false;}
    if(panel.editing>=0) {
        if(e->type==SDL_TEXTINPUT) {if(strlen(panel.draft)+strlen(e->text.text)<sizeof(panel.draft)) strcat(panel.draft,e->text.text);return true;}
        if(e->type==SDL_KEYDOWN) {
            if(e->key.keysym.sym==SDLK_ESCAPE) cancel();
            else if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(panel.draft);if(n) panel.draft[n-1]=0;}
            else if(e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER) {
                bool ok=numeric(panel.editing,panel.draft);
                if(ok) ok=SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
                else snprintf(panel.status,sizeof(panel.status),"Enter a number, three comma-separated XYZ/RGB values, or source FPS as N/D (for example 30000/1001).");
                cancel();report(ok);
            }
            return true;
        }
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    SDL_Point point={e->button.x,e->button.y};
    for(int id=0;id<COUNT;++id) if(panel.rects[id].w && SDL_PointInRect(&point,&panel.rects[id])) {
        cancel();bool ok=false;
        switch(id) {
          case IMPORT: {
#ifdef __APPLE__
            char path[4096];
            if(RayTracing_MacOSFilePanelSelect("Attach atmosphere cache",NULL,path,sizeof(path))!=RAY_TRACING_FOLDER_PICKER_SELECTED) return true;
            ok=SceneEditorAtmosphereImport(path,false,panel.revision,panel.status,sizeof(panel.status));
#else
            snprintf(panel.status,sizeof(panel.status),"Use atmosphere.import through the local agent connection.");
#endif
            break;
          }
          case PRESET:panel.preset=(panel.preset+1)%5;return true;
          case PREVIEW:SceneEditorAtmospherePreviewSetEnabled(!SceneEditorAtmospherePreviewEnabled());return true;
          case GENERATE:ok=SceneEditorAtmosphereImport(presets[panel.preset],true,panel.revision,panel.status,sizeof(panel.status));break;
          case APPEARANCE:panel.appearance=!panel.appearance;return true;
          case PLACEMENT:panel.placement=!panel.placement;return true;
          case PLAYBACK:panel.playback=!panel.playback;return true;
          case ENABLED:panel.value.enabled=!panel.value.enabled;break;
          case VISIBLE:panel.value.visible=!panel.value.visible;break;
          case LIGHTING:panel.value.affects_lighting=!panel.value.affects_lighting;break;
          case ANIMATED:panel.value.animated=!panel.value.animated;break;
          case MISSING:panel.value.missing_is_error=!panel.value.missing_is_error;break;
          case REMOVE:ok=SceneEditorAtmosphereRemove(panel.revision,panel.status,sizeof(panel.status));report(ok);return true;
          default:panel.editing=id;panel.draft[0]=0;SDL_StartTextInput();return true;
        }
        if(id==ENABLED || id==VISIBLE || id==LIGHTING || id==ANIMATED || id==MISSING)
            ok=SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
        report(ok);return true;
    }
    cancel();return false;
}
bool SceneEditorAtmospherePanelControl(const char* name,SDL_Rect* out) {
    for(int i=0;i<COUNT;++i) if(!strcmp(name,names[i])) {*out=panel.rects[i];return out->w>0;}
    return false;
}
