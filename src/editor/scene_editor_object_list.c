#include "editor/scene_editor_surfaces.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_document.h"
#include <ctype.h>
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include "editor/scene_editor_object_list.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/object_editor.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_mesh_asset_loader.h"
#include "kit_ui.h"
#include "render/render_helper.h"

#define OBJECT_LIST_ROW_HEIGHT 24
#define OBJECT_LIST_ROW_GAP 4

static SDL_Rect g_viewport = {0, 0, 0, 0};
static float g_scroll_offset = 0.0f;
static float g_content_height = 0.0f;
static int g_last_selected = -1;

static char filter_text[96];
void SceneEditorObjectListSetFilter(const char* text) {
    snprintf(filter_text,sizeof(filter_text),"%s",text ? text : "");
    for (char* p=filter_text; *p; ++p) *p=(char)tolower((unsigned char)*p);
    g_scroll_offset=0; g_last_selected=-1;
}
typedef struct ObjectRowHit { SDL_Rect select, visibility, lock; char id[128], label[180]; } ObjectRowHit;
static ObjectRowHit row_hits[MAX_OBJECTS];
static int hit_count;
void SceneEditorObjectListClearHits(void) { hit_count=0;g_viewport=(SDL_Rect){0};ObjectEditorClearObjectListRows(); }
bool SceneEditorObjectListRowRects(const char* id,SDL_Rect* select,SDL_Rect* visibility,SDL_Rect* lock) {
    for(int i=0;id && i<hit_count;++i) if(strcmp(id,row_hits[i].id)==0) {
        if(select) *select=row_hits[i].select;
        if(visibility) *visibility=row_hits[i].visibility;
        if(lock) *lock=row_hits[i].lock;
        return true;
    }
    return false;
}
static bool filter_matches(const SceneEditorDocumentObjectInfo* info) {
    char haystack[384];
    snprintf(haystack,sizeof(haystack),"%s %s %s",info->name,info->id,info->type);
    for(char* p=haystack;*p;++p) *p=(char)tolower((unsigned char)*p);
    return !filter_text[0] || strstr(haystack,filter_text)!=NULL;
}

static void draw_scrollbar(SDL_Renderer* renderer) {
    RayTracingThemePalette surfaces=SceneEditorChromeShellResolvePalette();
    SDL_Rect track = {0, 0, 0, 0};
    SDL_Rect thumb = {0, 0, 0, 0};
    int content_height = (int)ceilf(g_content_height);
    int max_offset = content_height - g_viewport.h;
    int thumb_height = 0;
    int travel = 0;
    if (!renderer || g_viewport.w <= 0 || g_viewport.h <= 0 || max_offset <= 0) return;
    track = (SDL_Rect){g_viewport.x + g_viewport.w - 8, g_viewport.y, 6, g_viewport.h};
    thumb_height = (g_viewport.h * g_viewport.h) / content_height;
    if (thumb_height < g_viewport.h / 10) thumb_height = g_viewport.h / 10;
    if (thumb_height < 1) thumb_height = 1;
    travel = track.h - thumb_height;
    thumb = (SDL_Rect){track.x,
                       track.y + ((int)lroundf(g_scroll_offset) * travel) / max_offset,
                       track.w,
                       thumb_height};
    SDL_SetRenderDrawColor(renderer, surfaces.panel_fill.r, surfaces.panel_fill.g, surfaces.panel_fill.b, 255);
    SDL_RenderFillRect(renderer, &track);
    SDL_SetRenderDrawColor(renderer, surfaces.text_muted.r, surfaces.text_muted.g, surfaces.text_muted.b, 255);
    SDL_RenderFillRect(renderer, &thumb);
}

static bool point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int render_line(SDL_Renderer* renderer,
                       SDL_Rect bounds,
                       int cursor_y,
                       int bottom_y,
                       const char* text,
                       SDL_Color color) {
    SDL_Rect line_rect = {bounds.x, cursor_y, bounds.w, bottom_y - cursor_y};
    int used_height = 0;
    if (!renderer || !text || !text[0] || line_rect.w <= 0 || line_rect.h <= 0) {
        return cursor_y;
    }
    used_height = SceneEditorLabelLeft(renderer, line_rect, text, color);
    if (used_height < 1) used_height = 18;
    return cursor_y + used_height + 6;
}

static float clamp_offset(float offset) {
    float max_offset = g_content_height - (float)g_viewport.h;
    if (max_offset < 0.0f) max_offset = 0.0f;
    if (offset < 0.0f) return 0.0f;
    if (offset > max_offset) return max_offset;
    return offset;
}

bool SceneEditorObjectListContainsPoint(int x, int y) {
    return point_in_rect(x, y, &g_viewport);
}

bool SceneEditorObjectListHandleWheel(int x, int y, float wheel_delta_y) {
    KitUiScrollResult result = {0};
    KitRenderRect viewport = {0};
    if (!SceneEditorObjectListContainsPoint(x, y) || wheel_delta_y == 0.0f) return false;
    viewport = (KitRenderRect){(float)g_viewport.x,
                               (float)g_viewport.y,
                               (float)g_viewport.w,
                               (float)g_viewport.h};
    result = kit_ui_eval_scroll(viewport,
                                g_scroll_offset,
                                g_content_height,
                                wheel_delta_y);
    g_scroll_offset = result.offset_y;
    return result.changed != 0;
}

float SceneEditorObjectListScrollOffset(void) {
    return g_scroll_offset;
}

void SceneEditorObjectListReset(void) {
    g_viewport = (SDL_Rect){0, 0, 0, 0};
    g_scroll_offset = 0.0f;
    g_content_height = 0.0f;
    g_last_selected = -1;
}

bool SceneEditorObjectListHandleClick(int x,int y) {
    if((animSettings.editorMode!=EDITOR_MODE_OBJECT && animSettings.editorMode!=EDITOR_MODE_MATERIAL) || (SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_SCENE && SceneEditorSidebarLibraryActive())) return false;
    for(int i=0;i<hit_count;++i) {
        ObjectRowHit* hit=&row_hits[i];
        bool visibility=point_in_rect(x,y,&hit->visibility), lock=point_in_rect(x,y,&hit->lock);
        if (!visibility && !lock && !point_in_rect(x,y,&hit->select)) continue;
        SceneEditorDocumentObjectInfo info;
        if(!SceneEditorDocumentObjectById(hit->id,&info)) return true;
        SceneEditorObjectMoveGizmoReset();
        if (visibility || lock) {
            char message[256]={0};
            if(RayTracingDeepRenderDesktopHost_HasActiveWork()) {
                SceneEditorChromeShellSetActionFeedback("Object edits wait for the active render",4000);return true;
            }
            bool ok=SceneEditorObjectExecute(visibility ? SCENE_OBJECT_VISIBILITY : SCENE_OBJECT_LOCK,info.id,NULL,
                visibility ? !info.visible : !info.locked,SceneEditorDocumentRevision(),NULL,message,sizeof(message));
            SceneEditorChromeShellSetActionFeedback(ok ? (visibility ? "Visibility updated; Undo available" : "Lock updated; Undo available") : message,4000);
        } else {
            SceneEditorObjectExecute(SCENE_OBJECT_SELECT,info.id,NULL,false,SceneEditorDocumentRevision(),NULL,NULL,0);
        }
        return true;
    }
    return false;
}
int SceneEditorObjectListRender(SDL_Renderer* renderer,SDL_Rect bounds,int cursor_y,int bottom_y,
    int selected_index,SDL_Color title_color,SDL_Color body_color) {
    (void)selected_index;
    RayTracingThemePalette surfaces=SceneEditorChromeShellResolvePalette();
    const int pitch=animation_config_scale_text_point_size(&animSettings,24,24);
    int matches[MAX_OBJECTS],count=0,selected_row=-1;
    static char line[180];
    hit_count=0; ObjectEditorClearObjectListRows();
    if(!renderer || bounds.w<=0) return cursor_y;
    const char* selected_id=ObjectEditorSelectionTrackerId();
    for(int i=0;i<SceneEditorDocumentObjectCount() && count<MAX_OBJECTS;++i) {
        SceneEditorDocumentObjectInfo info;
        if(SceneEditorDocumentObjectAt(i,&info) && filter_matches(&info)) {
            if(strcmp(info.id,selected_id)==0) selected_row=count;
            matches[count++]=i;
        }
    }
    snprintf(line,sizeof(line),"Objects %d / %d",count,SceneEditorDocumentObjectCount());
    int title_y=cursor_y;
    cursor_y=render_line(renderer,bounds,cursor_y,bottom_y,line,title_color);
    SceneEditorButtonText(renderer,(SDL_Rect){bounds.x+bounds.w-88,title_y,36,22},"View",body_color);
    SceneEditorButtonText(renderer,(SDL_Rect){bounds.x+bounds.w-48,title_y,36,22},"Lock",body_color);
    g_viewport=(SDL_Rect){bounds.x,cursor_y,bounds.w,bottom_y-cursor_y};
    if(g_viewport.h<pitch) {g_viewport.h=0;return cursor_y;}
    g_content_height=count*pitch;
    g_scroll_offset=clamp_offset(g_scroll_offset);
    if(selected_row>=0 && selected_row!=g_last_selected) {
        float top=selected_row*pitch;
        if(top<g_scroll_offset) g_scroll_offset=top;
        if(top+pitch>g_scroll_offset+g_viewport.h) g_scroll_offset=top+pitch-g_viewport.h;
        g_scroll_offset=clamp_offset(g_scroll_offset);
    }
    g_last_selected=selected_row;
    SDL_Rect old_clip,clip=g_viewport; SDL_bool had_clip=SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&old_clip);
    if(had_clip) SDL_IntersectRect(&clip,&old_clip,&clip);
    SDL_RenderSetClipRect(renderer,&clip);
    for(int row=(int)(g_scroll_offset/pitch);row<count;++row) {
        SceneEditorDocumentObjectInfo info;
        SceneEditorDocumentObjectAt(matches[row],&info);
        SDL_Rect rect={bounds.x,cursor_y+row*pitch-(int)g_scroll_offset,bounds.w-12,pitch};
        if(rect.y>=cursor_y+g_viewport.h) break;
        bool selected=strcmp(info.id,selected_id)==0;
        SDL_Color row_fill=selected ? SceneEditorSurfaceBlend(surfaces.button_fill,surfaces.accent_primary,22) : SceneEditorSurfaceGroup(surfaces);
        SDL_SetRenderDrawColor(renderer,row_fill.r,row_fill.g,row_fill.b,255);
        SDL_RenderFillRect(renderer,&rect);
        if(selected) {SDL_SetRenderDrawColor(renderer,surfaces.accent_primary.r,surfaces.accent_primary.g,surfaces.accent_primary.b,255);SDL_RenderDrawLine(renderer,rect.x,rect.y,rect.x,rect.y+rect.h-1);}
        ObjectRowHit* hit=&row_hits[hit_count++];
        snprintf(hit->id,sizeof(hit->id),"%s",info.id);
        hit->visibility=(SDL_Rect){rect.x+rect.w-76,rect.y,36,pitch};
        hit->lock=(SDL_Rect){rect.x+rect.w-36,rect.y,36,pitch};
        hit->select=(SDL_Rect){rect.x,rect.y,rect.w-80,pitch};
        snprintf(hit->label,sizeof(hit->label),"[%c] %s",SceneEditorDocumentTypeLabel(info.type)[0],info.name);
        SDL_Rect label=hit->select;label.x+=6;label.w-=12;
        RenderFixedSizedText(renderer,label,hit->label,selected ? title_color : body_color,13,false,false);
        SDL_SetRenderDrawColor(renderer,surfaces.panel_border.r,surfaces.panel_border.g,surfaces.panel_border.b,255);
        SDL_RenderDrawLine(renderer,hit->visibility.x-2,rect.y,hit->visibility.x-2,rect.y+rect.h-1);
        SDL_RenderDrawLine(renderer,hit->lock.x-2,rect.y,hit->lock.x-2,rect.y+rect.h-1);
        SceneEditorButtonText(renderer,hit->visibility,info.visible ? "On" : "Off",body_color);
        SceneEditorButtonText(renderer,hit->lock,info.locked ? "Yes" : "No",body_color);
        SDL_IntersectRect(&hit->select,&clip,&hit->select);
        if(info.runtime_index>=0) ObjectEditorRegisterObjectListRow(info.runtime_index,hit->select);
        SDL_IntersectRect(&hit->visibility,&clip,&hit->visibility);
        SDL_IntersectRect(&hit->lock,&clip,&hit->lock);
    }
    SDL_RenderSetClipRect(renderer,had_clip ? &old_clip : NULL);
    draw_scrollbar(renderer);
    return bottom_y;
}
