#include "editor/scene_editor_document.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_pointer_event.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_workspace_profile.h"
#include "render/render_helper.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/scene_editor.h"
#include "editor/scene_editor_surface_render.h"
#include "editor/scene_editor_object_list.h"
#include "kit_ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

typedef struct Sidebar { SDL_Rect viewport; float offset; int content; bool dragging; } Sidebar;
static Sidebar panes[2];
static int last_mode = -1;
static bool library_active, search_active;
static SDL_Rect tabs[2], search_box;
static char search_text[96];
static bool diagnostics_visible;
static SDL_Rect diagnostics_button;
bool SceneEditorSidebarDiagnosticsVisible(void) { return diagnostics_visible; }
bool SceneEditorSidebarLibraryActive(void) { return library_active; }
void SceneEditorSidebarShowLibrary(bool active) { SceneEditorSidebarReset(); library_active=active; }
bool SceneEditorSidebarTextActive(void) { return search_active; }

static bool contains(SDL_Rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y && x < r.x+r.w && y < r.y+r.h;
}
void SceneEditorSidebarReset(void) { memset(panes, 0, sizeof(panes)); last_mode = -1; search_active = false; }
void SceneEditorSidebarRestoreDefaults(void) {
    SceneEditorSidebarReset(); library_active=false; search_text[0]=0;
    diagnostics_visible=false;
    SceneEditorObjectListSetFilter("");
}
static void clamp(Sidebar* pane) {
    float max = pane->content - pane->viewport.h;
    if (max < 0) max = 0;
    if (pane->offset > max) pane->offset = max;
    if (pane->offset < 0) pane->offset = 0;
}
static SDL_Rect track(Sidebar* pane) {
    SDL_Rect r = pane->viewport; r.x += r.w - 10; r.w = 10; return r;
}
static void seek(Sidebar* pane, int y) {
    float fraction = (float)(y - pane->viewport.y) / pane->viewport.h;
    pane->offset = fraction * pane->content - pane->viewport.h * 0.5f;
    clamp(pane);
}
bool SceneEditorSidebarInspectorEventVisible(const SDL_Event* event) {
    SceneEditorPaneLayout layout;
    if (!SceneEditorGetPaneLayout(&layout) || layout.viewport_expanded) return false;
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
        return contains(layout.right_content_rect, event->button.x, event->button.y);
    if (event->type == SDL_MOUSEMOTION)
        return contains(layout.right_content_rect, event->motion.x, event->motion.y);
    return true;
}
bool SceneEditorSidebarHandleEvent(const SDL_Event* event) {
    SceneEditorPaneLayout layout;
    if (!event) return false;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
        SceneEditorGetPaneLayout(&layout) && !layout.viewport_expanded &&
        contains(diagnostics_button,event->button.x,event->button.y)) {
        diagnostics_visible=!diagnostics_visible;
        panes[1].offset=0;
        return true;
    }
    if (search_active && event->type == SDL_TEXTINPUT) {
        size_t used = strlen(search_text), added = strlen(event->text.text);
        if (used+added < sizeof(search_text)) memcpy(search_text+used,event->text.text,added+1);
        SceneEditorObjectListSetFilter(search_text); return true;
    }
    if (search_active && event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_RETURN) search_active=false;
        else if (event->key.keysym.sym == SDLK_BACKSPACE) {
            size_t length=strlen(search_text);
            if (length) { --length; while (length && ((unsigned char)search_text[length] & 0xc0) == 0x80) --length; search_text[length]=0; }
        } else if ((event->key.keysym.mod & (KMOD_CTRL|KMOD_GUI)) && event->key.keysym.sym == SDLK_a) search_text[0]=0;
        SceneEditorObjectListSetFilter(search_text); return true;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        search_active = false;
        if (SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_SCENE &&
            !SceneEditorGetPaneHost()->viewport_expanded) {
            for (int i=0;i<2;++i) if (contains(tabs[i],event->button.x,event->button.y)) {
                library_active=i != 0; panes[0].offset=0; return true;
            }
            if (!library_active && contains(search_box,event->button.x,event->button.y)) {
                search_active=true; SDL_StartTextInput(); return true;
            }
        }
    }

    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        panes[0].dragging = panes[1].dragging = false;
        search_active = false;
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        bool active = panes[0].dragging || panes[1].dragging;
        panes[0].dragging = panes[1].dragging = false;
        if (active) return true;
    }
    if (!SceneEditorGetPaneLayout(&layout) || layout.viewport_expanded) return false;
    for (int i=0; i<2; ++i) {
        Sidebar* pane = &panes[i];
        if (event->type == SDL_MOUSEMOTION && pane->dragging) { seek(pane, event->motion.y); return true; }
        if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
            pane->content > pane->viewport.h && contains(track(pane), event->button.x, event->button.y)) {
            pane->dragging = true; seek(pane, event->button.y); return true;
        }
        if (event->type == SDL_MOUSEWHEEL) {
            int x, y; SceneEditorWheelPosition(event, &x, &y);
            if (!contains(pane->viewport, x, y)) continue;
            float delta = event->wheel.preciseY;
            if (delta == 0) delta = (float)event->wheel.y;
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) delta = -delta;
            /* The outliner owns its rows; other sidebar space scrolls the pane. */
            if (i == 0 && animSettings.editorMode == EDITOR_MODE_OBJECT &&
                SceneEditorWorkspaceProfileGet() != SCENE_WORKSPACE_ENVIRONMENT &&
                (!library_active || SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_SURFACE) &&
                SceneEditorObjectListContainsPoint(x,y) &&
                SceneEditorObjectListHandleWheel(x,y,delta)) return true;
            KitUiScrollResult result = kit_ui_eval_scroll(
                (KitRenderRect){pane->viewport.x,pane->viewport.y,pane->viewport.w,pane->viewport.h},
                pane->offset, pane->content, delta * 3);
            pane->offset = result.offset_y;
            return true;
        }
    }
    return false;
}
void SceneEditorSidebarRender(SDL_Renderer* renderer, const SceneEditorPaneLayout* layout,
    const SceneEditorControlSurfaceContract* contract, SDL_Color title, SDL_Color body) {
    if(library_active || contract->activeMode!=EDITOR_MODE_OBJECT || SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_SCENE)
        SceneEditorObjectListClearHits();
    diagnostics_button=(SDL_Rect){0};
    if (!layout->viewport_expanded) {
        diagnostics_button=(SDL_Rect){layout->right_pane_rect.x+layout->right_pane_rect.w-88,
            layout->right_pane_rect.y+4,78,22};
        SceneEditorButtonText(renderer,diagnostics_button,
            diagnostics_visible ? "Details −" : "Details +",body);
    }
    if (last_mode != contract->activeMode) {
        SceneEditorSidebarReset(); last_mode = contract->activeMode;
    }
    for (int i=0; i<2; ++i) {
        Sidebar* pane=&panes[i];
        pane->viewport = i ? layout->right_content_rect : layout->left_content_rect;
        if (pane->viewport.w <= 0 || pane->viewport.h <= 0) continue;
        if (i == 0 && contract->activeMode == EDITOR_MODE_OBJECT &&
            SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_SCENE) {
            int row_height=animation_config_scale_text_point_size(&animSettings,28,28);
            for (int tab=0;tab<2;++tab) {
                tabs[tab]=(SDL_Rect){pane->viewport.x+tab*(pane->viewport.w/2),pane->viewport.y,
                    pane->viewport.w/2-4,row_height};
                SDL_SetRenderDrawColor(renderer,body.r,body.g,body.b,library_active==tab ? 180 : 70);
                SDL_RenderFillRect(renderer,&tabs[tab]);
                SceneEditorButtonText(renderer,tabs[tab],tab ? "Assets" : "Scene",title);
            }
            pane->viewport.y+=row_height+6; pane->viewport.h-=row_height+6;
            search_box=(SDL_Rect){0};
            if (!library_active) {
                search_box=(SDL_Rect){pane->viewport.x,pane->viewport.y,pane->viewport.w-14,row_height};
                SDL_SetRenderDrawColor(renderer,body.r,body.g,body.b,90);
                SDL_RenderDrawRect(renderer,&search_box);
                char label[128]; snprintf(label,sizeof(label),"%s%s",search_active ? "> " : "",
                    search_text[0] ? search_text : "Search name, ID or type");
                SceneEditorLabelLeft(renderer,search_box,label,title);
                pane->viewport.y+=row_height+6; pane->viewport.h-=row_height+6;
            }
        }

        if(i==1) {
            static SceneEditorDocumentObjectInfo info;
            SDL_Rect identity=pane->viewport;
            identity.h=22;
            const char* id=ObjectEditorSelectionTrackerId();
            if(SceneEditorDocumentObjectById(id,&info)) {
                SceneEditorLabelLeft(renderer,identity,info.name,title);identity.y+=24;
                static char label[180];snprintf(label,sizeof(label),"%s%s%s",SceneEditorDocumentTypeLabel(info.type),info.locked ? " | Locked" : "",info.visible ? "" : " | Hidden");
                SceneEditorLabelLeft(renderer,identity,label,body);identity.y+=24;
                if(info.locked) { SceneEditorLabelLeft(renderer,identity,"Unlock in Scene to edit",body);identity.y+=24; }
                if(diagnostics_visible) { SceneEditorLabelLeft(renderer,identity,info.id,body);identity.y+=24; }
            } else {
                SceneEditorLabelLeft(renderer,identity,"No object selected",title);identity.y+=24;
                SceneEditorLabelLeft(renderer,identity,"Select one object in Scene or viewport",body);identity.y+=24;
            }
            if(diagnostics_visible) {SceneEditorLabelLeft(renderer,identity,"Single selection; multi-edit unavailable",body);identity.y+=24;}
            int header_height=identity.y-pane->viewport.y;
            pane->viewport.y+=header_height;pane->viewport.h-=header_height;
        }
        clamp(pane);
        SceneEditorPaneLayout virtual_layout = *layout;
        SDL_Rect content = pane->viewport;
        content.w -= 14;
        content.y -= (int)lroundf(pane->offset);
        /* Finite measure space; actual content end sets the scroll range. */
        content.h = animation_config_scale_text_point_size(&animSettings, 1400, 1400);
        if (i==0 && contract->activeMode==EDITOR_MODE_OBJECT && !library_active &&
            SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_SCENE) {
            pane->offset=0;
            content.y=pane->viewport.y;
            content.h=pane->viewport.h;
        }
        SDL_Rect old_clip;
        SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
        SDL_RenderGetClipRect(renderer, &old_clip);
        SDL_RenderSetClipRect(renderer, &pane->viewport);
        int end;
        if (i == 0) {
            virtual_layout.left_content_rect = content;
            end = SceneEditorSurfaceRenderLeftPaneContent(renderer,&virtual_layout,contract,title,body);
        } else {
            virtual_layout.right_content_rect = content;
            end = SceneEditorSurfaceRenderRightPaneStatus(renderer,&virtual_layout,contract,
                content.y+content.h,title,body);
        }
        pane->content = end - content.y + 8;
        if (i==0 && contract->activeMode==EDITOR_MODE_OBJECT && !library_active &&
            SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_SCENE)
            pane->content=pane->viewport.h;
        SDL_RenderSetClipRect(renderer, clipped ? &old_clip : NULL);
        if (pane->content > pane->viewport.h) {
            SDL_Rect bar = track(pane);
            SDL_SetRenderDrawColor(renderer,body.r,body.g,body.b,70);
            SDL_RenderFillRect(renderer,&bar);
            int height = pane->viewport.h * pane->viewport.h / pane->content;
            if (height < 20) height = 20;
            bar.y += (int)(pane->offset * (pane->viewport.h-height) / (pane->content-pane->viewport.h));
            bar.h = height;
            SDL_SetRenderDrawColor(renderer,body.r,body.g,body.b,220);
            SDL_RenderFillRect(renderer,&bar);
        }
    }
}
