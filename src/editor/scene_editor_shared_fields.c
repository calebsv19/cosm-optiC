#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <stdio.h>
#include <string.h>
/* Application buffers are static panel drafts. No document pointers or values
 * are borrowed; owners still parse, validate, commit and cancel their drafts. */
static struct {SceneEditorSharedText text;char *buffer;size_t capacity;SDL_Rect clip;unsigned frame;int active;} field;
static unsigned frame;
void scene_editor_shared_fields_reset(void) {memset(&field,0,sizeof(field));frame=0;}
void scene_editor_shared_fields_cancel(void) {field.text.ready=0;field.text.dragging=0;kit_ui_text_cancel_composition(&field.text.edit);}
void scene_editor_shared_fields_frame(void) {++frame;}
void scene_editor_shared_field_begin(char *buffer,size_t capacity,int select_all) {
    memset(&field,0,sizeof(field));field.buffer=buffer;field.capacity=capacity;field.active=1;
    scene_editor_shared_text_bind(&field.text,buffer,capacity,KIT_UI_TEXT_SINGLE_LINE);
    if(select_all)(void)kit_ui_text_command(&field.text.edit,KIT_UI_TEXT_SELECT_ALL,0);
}
void scene_editor_shared_field_end(char *buffer) {if(field.buffer==buffer){field.active=0;scene_editor_shared_fields_cancel();}}
int scene_editor_shared_field_event(char *buffer,size_t capacity,const SDL_Event *event) {
    if(field.buffer!=buffer || !field.active)scene_editor_shared_field_begin(buffer,capacity,0);
    if(event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_TAB) {
            return event->key.repeat || field.text.edit.composition[0];
        }
        if(key==SDLK_ESCAPE && !field.text.edit.composition[0])return event->key.repeat;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN) {
        SDL_Point point={event->button.x,event->button.y};
        if(field.frame!=frame || !SDL_PointInRect(&point,&field.clip))return 0;
    }
    /* Presentation geometry is valid only for the last painted owner. */
    if(field.frame!=frame)field.text.ready=0;
    return scene_editor_shared_text_event(&field.text,event).consumed;
}
void scene_editor_shared_field_draw(SDL_Renderer *r,char *buffer,size_t capacity,SDL_Rect bounds,const char *label) {
    if(!field.active || field.buffer!=buffer)return;
    RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);
    SDL_Rect clip=bounds;if(clipped && !SDL_IntersectRect(&prior,&bounds,&clip))return;
    field.clip=clip;field.frame=frame;
    SceneEditorSharedUiExclude(bounds);
    SDL_SetRenderDrawColor(r,palette.background_fill.r,palette.background_fill.g,palette.background_fill.b,255);SDL_RenderFillRect(r,&bounds);
    SDL_SetRenderDrawColor(r,palette.accent_primary.r,palette.accent_primary.g,palette.accent_primary.b,255);SDL_RenderDrawRect(r,&bounds);
    SDL_Rect text=bounds;
    if(label) {
        const char *colon=strchr(label,':');
        if(colon) {
            char prefix[128];snprintf(prefix,sizeof(prefix),"%.*s",(int)(colon-label+1),label);
            int width=0;TTF_Font *font=ray_tracing_font_runtime_get_ui_regular(r,13,10);
            ray_tracing_text_measure_utf8(r,font,prefix,&width,NULL);if(width>bounds.w/2)width=bounds.w/2;
            scene_editor_shared_label(r,(SDL_Rect){bounds.x+5,bounds.y,width,bounds.h},prefix,palette.text_muted,13,0,0);
            text.x+=width+8;text.w-=width+8;
        }
    }
    (void)capacity;scene_editor_shared_text_draw(&field.text,r,text,palette.text_primary);
}
int scene_editor_shared_field_paint(SDL_Renderer *r,SDL_Rect bounds,const char *label) {
    if(!field.active || !SDL_IsTextInputActive() || !label)return 0;
    const char *colon=strchr(label,':');if(!colon)return 0;
    const char *value=colon+1;while(*value==' ')++value;
    size_t n=strlen(field.buffer);
    if(strncmp(value,field.buffer,n) || (value[n] && strcmp(value+n,"_")))return 0;
    scene_editor_shared_field_draw(r,field.buffer,field.capacity,bounds,label);return 1;
}
