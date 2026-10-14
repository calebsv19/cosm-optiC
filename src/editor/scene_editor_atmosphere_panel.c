#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_atmosphere_viewport.h"
#include "editor/scene_editor_atmosphere_inspector.h"
#include "app/scene_atmosphere_appearance.h"
#include "render/runtime_volume_3d_transform.h"
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
    ANIMATED,RATE,START,SOURCE_START,MISSING,REMOVE,SELECT,FRAME,FIT,
    LOOK_CLOUD,LOOK_MIST,LOOK_SMOKE,TRANSPARENCY,HUE,SATURATION,ADVANCED,PROFILE,CONCENTRATION,EXTINCTION,ALBEDO,ANISOTROPY,SKY,LAYOUT,SKY_BUDGET,COUNT};
static const char* names[COUNT]={"preview","import","preset","generate","enabled","visible","lighting",
    "appearance","placement","playback","density","gamma","absorption","scatter","step","tint",
    "translation","rotation","scale","animated","rate","start","source_start","missing","remove","select","frame","fit",
    "look_cloud","look_mist","look_smoke","transparency","hue","saturation","advanced","optical_profile","concentration","mass_extinction","scattering_albedo","anisotropy","sky_illumination","sampling_layout","sky_error_budget"};
static const char* presets[]={"room_haze_v1","low_floor_fog_v1","cloud_bank_v1","mist_patch_v1","plume_column_v1"};
static const char* preset_names[]={"Room haze","Floor fog","Cloud bank","Mist patch","Plume"};
static struct {
    SDL_Rect rects[COUNT];char labels[COUNT][256],draft[128],status[512],source[1200],cache[512];
    int editing,preset;bool visible,appearance,placement,playback,advanced;
    unsigned long long revision;
    SceneAtmosphereAttachment value;
} panel={.editing=-1};
static void cancel(void) {if(panel.editing>=0) do{scene_editor_shared_field_end(panel.draft);SDL_StopTextInput();}while(0);panel.editing=-1;}
bool SceneEditorAtmospherePanelInteractionActive(void) {return panel.editing>=0 || SceneEditorAtmosphereInspectorActive();}
void SceneEditorAtmospherePanelBeginFrame(void) {
    SceneEditorAtmosphereInspectorBeginFrame();
    memset(panel.rects,0,sizeof(panel.rects));panel.visible=false;
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) cancel();
}
static void button(SDL_Renderer* r,SDL_Rect bounds,int id,int* y,const char* label,bool enabled) {
    snprintf(panel.labels[id],sizeof(panel.labels[id]),"%s",label);
    SDL_Rect rect={bounds.x+8,*y,bounds.w-16,26};
    SceneEditorSharedUiPanelButton(r,70,128+id,rect,panel.labels[id],false,enabled);
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
    button(r,bounds,IMPORT,&y,v.present?"Replace atmosphere...":"Attach atmosphere...",true);
    char label[256];snprintf(label,sizeof(label),"Field shape: %s",preset_names[panel.preset]);
    button(r,bounds,PRESET,&y,label,true);
    button(r,bounds,GENERATE,&y,v.present?"Replace field shape":"Create static atmosphere",true);
    if(v.present) {
        button(r,bounds,SELECT,&y,SceneEditorAtmosphereSelected()?"Atmosphere selected":"Select atmosphere",true);
        button(r,bounds,FRAME,&y,"Frame atmosphere",true);
        const char* name=strrchr(v.source_path,'/');name=name?name+1:v.source_path;
        snprintf(panel.source,sizeof(panel.source),"%s | %s",name,v.source_kind);
        scene_editor_shared_label_at(r,panel.source,bounds.x+8,y,(SDL_Color){180,195,210,255},11);y+=24;
        RuntimeVolumeAttachment3D volume;char diagnostic[256]={0};
        if(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic))) {
            Vec3 size=vec3_sub(volume.grid.boundsMax,volume.grid.boundsMin);double w=SceneEditorDocumentWorldScale();
            snprintf(panel.cache,sizeof(panel.cache),"Cache loaded | Frame %llu%s | %.4g x %.4g x %.4g %s",
                (unsigned long long)volume.grid.frameIndex,v.animated?"":" (static)",
                size.x*v.scale[0]/w,size.y*v.scale[1]/w,size.z*v.scale[2]/w,SceneEditorDocumentUnitLabel());
        } else snprintf(panel.cache,sizeof(panel.cache),"Cache unavailable: %.240s",diagnostic);
        y+=SceneEditorLabelWrapped(r,(SDL_Rect){bounds.x+8,y,bounds.w-16,80},panel.cache,(SDL_Color){180,195,210,255})+8;
        button(r,bounds,ENABLED,&y,v.enabled?"Atmosphere: On":"Atmosphere: Off",true);
        button(r,bounds,VISIBLE,&y,v.visible?"Visible volume: On":"Visible volume: Off",true);
        button(r,bounds,LIGHTING,&y,v.affects_lighting?"Affects lighting: On":"Affects lighting: Off",!(v.optics.present && v.optics.physical));
        bool physical=v.optics.present && v.optics.physical;
        button(r,bounds,PROFILE,&y,physical?"Optics: Physical smoke (Direct/Disney V2)":"Optics: Legacy artistic",true);
        for(int i=0;i<SCENE_ATMOSPHERE_LOOK_COUNT;++i) {
            int id=LOOK_CLOUD+i;
            snprintf(panel.labels[id],sizeof(panel.labels[id]),"%s",SceneAtmosphereLookName(i));
            SDL_Rect rect={bounds.x+8+i*(bounds.w-16)/3,y,(bounds.w-16)/3-3,26};
            SceneEditorSharedUiPanelButton(r,70,256+id,rect,panel.labels[id],!physical && SceneAtmosphereCurrentLook(&v)==i,!physical);
            if(!physical && rect.y>=bounds.y && rect.y+rect.h<=bounds.y+bounds.h) panel.rects[id]=rect;
        }y+=30;
        snprintf(label,sizeof(label),"Appearance: %s  %s",physical?"Physical smoke":SceneAtmosphereLookName(SceneAtmosphereCurrentLook(&v)),panel.appearance?"-":"+");
        button(r,bounds,APPEARANCE,&y,label,true);
        if(panel.appearance && physical) {
            field(r,bounds,CONCENTRATION,&y,"Reference concentration (kg/m3)",&v.optics.reference_concentration_kg_m3,1);
            field(r,bounds,EXTINCTION,&y,"Mass extinction (m2/kg)",&v.optics.mass_extinction_m2_kg,1);
            field(r,bounds,ALBEDO,&y,"Scattering albedo (0..1)",&v.optics.scattering_albedo,1);
            field(r,bounds,ANISOTROPY,&y,"Anisotropy (-0.95..0.95)",&v.optics.anisotropy,1);
            button(r,bounds,SKY,&y,v.optics.sky_illumination?"Sky illumination: On":"Sky illumination: Off",true);
            button(r,bounds,LAYOUT,&y,v.optics.cell_centered?"Sampling: Cell centered":"Sampling: Legacy nodes",true);
            field(r,bounds,STEP,&y,"Integration step scale",&v.step_scale,1);
            field(r,bounds,SKY_BUDGET,&y,"Sky omission budget (0..0.0001)",&v.optics.sky_error_budget,1);
        }
        if(panel.appearance && !physical) {
            double transparency=100*(1-v.opacity),hsv[3];SceneAtmosphereTintHSV(&v,hsv);
            double saturation=100*hsv[1];
            field(r,bounds,TRANSPARENCY,&y,"Transparency (%)",&transparency,1);
            field(r,bounds,HUE,&y,"Hue (0-360 degrees)",hsv,1);
            field(r,bounds,SATURATION,&y,"Saturation (%)",&saturation,1);
            field(r,bounds,DENSITY,&y,"Density",&v.density_scale,1);
            field(r,bounds,ABSORPTION,&y,"Light absorption (darkening)",&v.absorption_gain,1);
            field(r,bounds,SCATTER,&y,"Light scattering (brightness)",&v.scatter_gain,1);
            button(r,bounds,ADVANCED,&y,panel.advanced?"Advanced appearance  -":"Advanced appearance  +",true);
            if(panel.advanced) {
                field(r,bounds,GAMMA,&y,"Density gamma",&v.density_gamma,1);
                field(r,bounds,STEP,&y,"Sampling step",&v.step_scale,1);
                field(r,bounds,TINT,&y,"Tint RGB",v.tint,3);
            }
        }
        button(r,bounds,PLACEMENT,&y,"Edit volume placement >",true);
        button(r,bounds,FIT,&y,"Fit volume to scene",true);
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
    button(r,bounds,PREVIEW,&y,SceneEditorAtmospherePreviewEnabled()?"Direct light preview: On (manual)":"Direct light preview: Off (manual)",true);
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
      case TRANSPARENCY:
        if(values[0]<0 || values[0]>100) return false;
        v->opacity=1-values[0]/100;break;
      case HUE:case SATURATION: {
        double hsv[3];SceneAtmosphereTintHSV(v,hsv);
        if(id==HUE) {hsv[0]=values[0];if(hsv[1]<1e-6)hsv[1]=0.35;if(hsv[2]<1e-6)hsv[2]=1;}
        else hsv[1]=values[0]/100;
        if(!SceneAtmosphereSetTintHSV(v,hsv)) return false;break;
      }
      case DENSITY:target=&v->density_scale;break;case GAMMA:target=&v->density_gamma;break;
      case ABSORPTION:target=&v->absorption_gain;break;case SCATTER:target=&v->scatter_gain;break;
      case CONCENTRATION:target=&v->optics.reference_concentration_kg_m3;break;
      case EXTINCTION:target=&v->optics.mass_extinction_m2_kg;break;
      case ALBEDO:target=&v->optics.scattering_albedo;break;
      case ANISOTROPY:target=&v->optics.anisotropy;break;
      case SKY_BUDGET:target=&v->optics.sky_error_budget;break;
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
    if(ok && !panel.status[0]) snprintf(panel.status,sizeof(panel.status),"Atmosphere updated. Undo available; File Save to retain.");
    SceneEditorChromeShellSetActionFeedback(panel.status,6000);
}
bool SceneEditorAtmospherePanelEvent(SDL_Event* e) {
    if(SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) {cancel();return false;}
    if(!panel.visible) return false;
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {cancel();return false;}
    if(panel.editing>=0) {
        if((panel.editing>=0) && scene_editor_shared_field_event(panel.draft,sizeof(panel.draft),e))return true;
    if(e->type==SDL_TEXTINPUT) {if(strlen(panel.draft)+strlen(e->text.text)<sizeof(panel.draft)) strcat(panel.draft,e->text.text);return true;}
        if(e->type==SDL_KEYDOWN) {
            if(e->key.keysym.sym==SDLK_ESCAPE) cancel();
            else if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(panel.draft);if(n) panel.draft[n-1]=0;}
            else if(e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER) {
                bool ok=numeric(panel.editing,panel.draft);
                if(ok) ok=SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
                else if(panel.editing==TRANSPARENCY) snprintf(panel.status,sizeof(panel.status),"Transparency must be 0 to 100 percent; 100 is fully transparent.");
                else if(panel.editing==HUE) snprintf(panel.status,sizeof(panel.status),"Hue must be 0 to 360 degrees.");
                else if(panel.editing==SATURATION) snprintf(panel.status,sizeof(panel.status),"Saturation must be 0 to 100 percent.");
                else snprintf(panel.status,sizeof(panel.status),"Enter a number, three comma-separated XYZ/RGB values, or source FPS as N/D (for example 30000/1001).");
                cancel();report(ok);
            }
            return true;
        }
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    SDL_Point point={e->button.x,e->button.y};
    for(int id=0;id<COUNT;++id) if(panel.rects[id].w && SDL_PointInRect(&point,&panel.rects[id])) {
        cancel();bool ok=false;panel.status[0]=0;
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
          case ADVANCED:panel.advanced=!panel.advanced;return true;
          case LOOK_CLOUD:case LOOK_MIST:case LOOK_SMOKE:
            ok=SceneAtmosphereApplyLook(&panel.value,id-LOOK_CLOUD) &&
                SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
            if(ok) snprintf(panel.status,sizeof(panel.status),"%s look applied. Source and placement preserved; Undo available.",SceneAtmosphereLookName(id-LOOK_CLOUD));
            break;
          case SELECT:case PLACEMENT:SceneEditorAtmosphereSelect(true);SceneEditorAtmospherePreviewSetEnabled(false);return true;
          case FRAME:SceneEditorAtmosphereSelect(true);SceneEditorAtmosphereFrame();return true;
          case FIT: {
            RuntimeVolumeAttachment3D volume;
            ok=SceneEditorAtmosphereGeometry(&volume,panel.status,sizeof(panel.status)) &&
                SceneEditorAtmosphereFit(&panel.value,&volume,NULL) &&
                SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
            if(ok) {SceneEditorAtmosphereSelect(true);SceneEditorAtmosphereFrame();}
            report(ok);return true;
          }
          case PLAYBACK:panel.playback=!panel.playback;return true;
          case PROFILE:
            panel.value.optics.present=true;panel.value.optics.physical=!panel.value.optics.physical;
            if(panel.value.optics.physical) panel.value.affects_lighting=true;
            break;
          case SKY:panel.value.optics.sky_illumination=!panel.value.optics.sky_illumination;break;
          case LAYOUT:panel.value.optics.cell_centered=!panel.value.optics.cell_centered;break;
          case ENABLED:panel.value.enabled=!panel.value.enabled;break;
          case VISIBLE:panel.value.visible=!panel.value.visible;break;
          case LIGHTING:panel.value.affects_lighting=!panel.value.affects_lighting;break;
          case ANIMATED:panel.value.animated=!panel.value.animated;break;
          case MISSING:panel.value.missing_is_error=!panel.value.missing_is_error;break;
          case REMOVE:ok=SceneEditorAtmosphereRemove(panel.revision,panel.status,sizeof(panel.status));report(ok);return true;
          default:panel.editing=id;panel.draft[0]=0;do{scene_editor_shared_field_begin(panel.draft,sizeof(panel.draft),0);SDL_StartTextInput();}while(0);return true;
        }
        if(id==PROFILE || id==SKY || id==LAYOUT || id==ENABLED || id==VISIBLE || id==LIGHTING || id==ANIMATED || id==MISSING)
            ok=SceneEditorAtmosphereSet(&panel.value,panel.revision,panel.status,sizeof(panel.status));
        report(ok);return true;
    }
    cancel();return false;
}
bool SceneEditorAtmospherePanelControl(const char* name,SDL_Rect* out) {
    for(int i=0;i<COUNT;++i) if(!strcmp(name,names[i])) {*out=panel.rects[i];return out->w>0;}
    return false;
}
