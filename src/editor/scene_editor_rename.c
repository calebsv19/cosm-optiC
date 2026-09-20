#include "editor/scene_editor_rename.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_object_list.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/scene_editor_surfaces.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "render/render_helper.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static struct {
    bool active, dragging, owns_input;
    char id[128], text[128], error[256];
    size_t cursor, anchor;
    unsigned long long revision;
    SDL_Rect field, apply, cancel;
    SDL_Renderer* renderer;
    int origin, text_y;
} edit;
/* Positions are UTF-8 boundaries, never offsets inside a multibyte character. */
static size_t previous(size_t p) { if(p) --p; while(p && ((unsigned char)edit.text[p]&0xc0)==0x80) --p; return p; }
static size_t next(size_t p) { if(edit.text[p]) ++p; while(edit.text[p] && ((unsigned char)edit.text[p]&0xc0)==0x80) ++p; return p; }
static bool space_at(size_t p) { return (unsigned char)edit.text[p]<128 && isspace((unsigned char)edit.text[p]); }
static size_t word_left(size_t p) { while(p && space_at(previous(p))) p=previous(p); while(p && !space_at(previous(p))) p=previous(p); return p; }
static size_t word_right(size_t p) { while(edit.text[p] && !space_at(p)) p=next(p); while(edit.text[p] && space_at(p)) p=next(p); return p; }
bool SceneEditorRenameActive(void) { return edit.active; }
void SceneEditorRenameCancel(void) { if(edit.active && edit.owns_input) SDL_StopTextInput(); memset(&edit,0,sizeof(edit)); }
bool SceneEditorRenameBegin(void) {
    SceneEditorDocumentObjectInfo info;
    if(edit.active) return true;
    if(SceneEditorTransformPanelInteractionActive() || SceneEditorSidebarTextActive()) return false;
    if(!SceneEditorDocumentObjectById(ObjectEditorSelectionTrackerId(),&info)) return false;
    if(info.locked || RayTracingDeepRenderDesktopHost_HasActiveWork()) {
        SceneEditorChromeShellSetActionFeedback(info.locked ? "Unlock this object to rename" : "Object edits wait for the active render",4000);return true;
    }
    memset(&edit,0,sizeof(edit));edit.active=true;
    snprintf(edit.id,sizeof(edit.id),"%s",info.id);snprintf(edit.text,sizeof(edit.text),"%s",info.name);
    edit.cursor=strlen(edit.text);edit.revision=SceneEditorDocumentRevision();
    edit.owns_input=!SDL_IsTextInputActive();SDL_StartTextInput();return true;
}
static void replace_selection(const char* text) {
    size_t lo=edit.cursor<edit.anchor?edit.cursor:edit.anchor, hi=edit.cursor>edit.anchor?edit.cursor:edit.anchor;
    size_t n=strlen(text), old=strlen(edit.text);
    if(old-(hi-lo)+n>96) {snprintf(edit.error,sizeof(edit.error),"Name must fit within 96 UTF-8 bytes");return;}
    for(size_t i=0;i<n;++i) if((unsigned char)text[i]<32 || text[i]==127) {snprintf(edit.error,sizeof(edit.error),"Use a single-line name");return;}
    memmove(edit.text+lo+n,edit.text+hi,old-hi+1);memcpy(edit.text+lo,text,n);
    edit.cursor=edit.anchor=lo+n;edit.error[0]=0;
}
static TTF_Font* font(void) { return ray_tracing_font_runtime_get_ui_regular(edit.renderer,13,13); }
static int width(size_t n) {char prefix[128];int w=0;snprintf(prefix,sizeof(prefix),"%.*s",(int)n,edit.text);ray_tracing_text_measure_utf8(edit.renderer,font(),prefix,&w,NULL);return w;}
static size_t position(int x) {size_t p=0;while(edit.text[p]) {size_t q=next(p);if(x-edit.origin<(width(p)+width(q))/2) break;p=q;}return p;}
static void apply(void) {
    bool nonspace=false;for(size_t p=0;edit.text[p];p=next(p)) if(!space_at(p)) nonspace=true;
    if(!nonspace) {snprintf(edit.error,sizeof(edit.error),"Enter a name before applying");return;}
    if(SceneEditorObjectExecute(SCENE_OBJECT_RENAME,edit.id,edit.text,false,edit.revision,NULL,edit.error,sizeof(edit.error))) {
        SceneEditorRenameCancel();SceneEditorChromeShellSetActionFeedback("Object renamed; Undo available",3000);
    }
}
bool SceneEditorRenameHandleEvent(SDL_Event* e) {
    if(!edit.active) {
        if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_F2 && !e->key.repeat) return SceneEditorRenameBegin();
        if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT && e->button.clicks==2) {
            for(int i=0;i<SceneEditorDocumentObjectCount();++i) {
                SceneEditorDocumentObjectInfo info;SDL_Rect row;
                if(SceneEditorDocumentObjectAt(i,&info) && SceneEditorObjectListRowRects(info.id,&row,NULL,NULL) && SDL_PointInRect(&(SDL_Point){e->button.x,e->button.y},&row)) {
                    if(SceneEditorObjectExecute(SCENE_OBJECT_SELECT,info.id,NULL,false,SceneEditorDocumentRevision(),NULL,NULL,0)) return SceneEditorRenameBegin();
                }
            }
        }
        return false;
    }
    if(e->type==SDL_QUIT || (e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_CLOSE)) {SceneEditorRenameCancel();return false;}
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {edit.dragging=false;return false;}
    if(e->type==SDL_TEXTINPUT) {replace_selection(e->text.text);return true;}
    if(e->type==SDL_MOUSEBUTTONDOWN) {
        SDL_Point p={e->button.x,e->button.y};
        if(e->button.button==SDL_BUTTON_LEFT) {
            if(SDL_PointInRect(&p,&edit.field) && edit.renderer) {edit.cursor=position(p.x);if(!(SDL_GetModState()&KMOD_SHIFT)) edit.anchor=edit.cursor;edit.dragging=true;}
            else if(SDL_PointInRect(&p,&edit.apply)) apply();
            else if(SDL_PointInRect(&p,&edit.cancel)) SceneEditorRenameCancel();
        }return true;
    }
    if(e->type==SDL_MOUSEMOTION) {if(edit.dragging) edit.cursor=position(e->motion.x);return true;}
    if(e->type==SDL_MOUSEBUTTONUP) {edit.dragging=false;return true;}
    if(e->type!=SDL_KEYDOWN) return e->type==SDL_KEYUP || e->type==SDL_MOUSEWHEEL || e->type==SDL_TEXTEDITING;
    SDL_Keycode k=e->key.keysym.sym;SDL_Keymod mod=e->key.keysym.mod;
    bool command=(mod&(KMOD_CTRL|KMOD_GUI))!=0, shift=(mod&KMOD_SHIFT)!=0, word=(mod&(KMOD_CTRL|KMOD_ALT))!=0;
    if(k==SDLK_ESCAPE) {SceneEditorRenameCancel();return true;}
    if(k==SDLK_RETURN || k==SDLK_KP_ENTER) {apply();return true;}
    if(command && k==SDLK_a) {edit.anchor=0;edit.cursor=strlen(edit.text);return true;}
    if(command && (k==SDLK_c || k==SDLK_x)) {
        size_t lo=edit.cursor<edit.anchor?edit.cursor:edit.anchor,hi=edit.cursor>edit.anchor?edit.cursor:edit.anchor;
        if(hi>lo) {char copy[128];snprintf(copy,sizeof(copy),"%.*s",(int)(hi-lo),edit.text+lo);if(SDL_SetClipboardText(copy)==0 && k==SDLK_x) replace_selection("");}return true;
    }
    if(command && k==SDLK_v) {char* clip=SDL_GetClipboardText();if(clip) {replace_selection(clip);SDL_free(clip);}return true;}
    if(k==SDLK_LEFT || k==SDLK_RIGHT || k==SDLK_HOME || k==SDLK_END) {
        bool left=k==SDLK_LEFT;
        if(k==SDLK_HOME || (left && (mod&KMOD_GUI))) edit.cursor=0;
        else if(k==SDLK_END || (k==SDLK_RIGHT && (mod&KMOD_GUI))) edit.cursor=strlen(edit.text);
        else if(!shift && edit.cursor!=edit.anchor && !word) edit.cursor=left ? (edit.cursor<edit.anchor?edit.cursor:edit.anchor) : (edit.cursor>edit.anchor?edit.cursor:edit.anchor);
        else edit.cursor=left ? (word?word_left(edit.cursor):previous(edit.cursor)) : (word?word_right(edit.cursor):next(edit.cursor));
        if(!shift) edit.anchor=edit.cursor;return true;
    }
    if(k==SDLK_BACKSPACE || k==SDLK_DELETE) {
        if(edit.cursor==edit.anchor) edit.anchor=k==SDLK_BACKSPACE ? ((mod&KMOD_GUI)?0:(word?word_left(edit.cursor):previous(edit.cursor))) : (word?word_right(edit.cursor):next(edit.cursor));
        replace_selection("");return true;
    }
    return true;
}
void SceneEditorRenameRender(SDL_Renderer* r) {
    if(!edit.active) return;
    SceneEditorPaneLayout layout;if(!SceneEditorGetPaneLayout(&layout)) return;
    edit.renderer=r;RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    int w=layout.mode_router_rect.w-32;if(w>480) w=480;
    SDL_Rect box={layout.mode_router_rect.x+(layout.mode_router_rect.w-w)/2,layout.viewport_rect.y+40,w,172};
    SceneEditorSurfaceFill(r,box,palette.panel_fill);SDL_SetRenderDrawColor(r,palette.panel_border.r,palette.panel_border.g,palette.panel_border.b,255);SDL_RenderDrawRect(r,&box);
    RenderFixedSizedText(r,(SDL_Rect){box.x+12,box.y+10,w-24,24},"Rename object",palette.text_primary,13,false,false);
    edit.field=(SDL_Rect){box.x+12,box.y+42,w-24,34};SDL_SetTextInputRect(&edit.field);
    SceneEditorSurfaceFill(r,edit.field,palette.background_fill);SDL_SetRenderDrawColor(r,palette.accent_primary.r,palette.accent_primary.g,palette.accent_primary.b,255);SDL_RenderDrawRect(r,&edit.field);
    int overflow=width(edit.cursor)-(edit.field.w-16);edit.origin=edit.field.x+6-(overflow>0?overflow:0);
    int height=16;ray_tracing_text_line_height(r,font(),&height);edit.text_y=edit.field.y+(edit.field.h-height)/2;
    SDL_Rect old,clip={edit.field.x+2,edit.field.y+1,edit.field.w-4,edit.field.h-2};bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&old);SDL_RenderSetClipRect(r,&clip);
    size_t lo=edit.cursor<edit.anchor?edit.cursor:edit.anchor,hi=edit.cursor>edit.anchor?edit.cursor:edit.anchor;
    SceneEditorSurfaceFill(r,(SDL_Rect){edit.origin+width(lo),edit.field.y+3,width(hi)-width(lo),edit.field.h-6},SceneEditorSurfaceBlend(palette.button_fill,palette.accent_primary,28));
    ray_tracing_text_draw_utf8_at(r,font(),edit.text,edit.origin,edit.text_y,palette.text_primary);
    SDL_SetRenderDrawColor(r,palette.text_primary.r,palette.text_primary.g,palette.text_primary.b,255);int x=edit.origin+width(edit.cursor);SDL_RenderDrawLine(r,x,edit.field.y+5,x,edit.field.y+edit.field.h-6);
    SDL_RenderSetClipRect(r,clipped?&old:NULL);
    RenderFixedSizedText(r,(SDL_Rect){box.x+12,box.y+84,w-24,32},edit.error[0]?edit.error:"Enter to apply · Escape to cancel",palette.text_muted,12,true,false);
    edit.cancel=(SDL_Rect){box.x+w-180,box.y+130,76,28};edit.apply=(SDL_Rect){box.x+w-96,box.y+130,84,28};
    SceneEditorSurfaceFill(r,edit.cancel,palette.button_fill);SceneEditorSurfaceFill(r,edit.apply,palette.button_fill);
    RenderFixedSizedText(r,edit.cancel,"Cancel",palette.text_primary,13,false,true);RenderFixedSizedText(r,edit.apply,"Rename",palette.text_primary,13,false,true);
}
