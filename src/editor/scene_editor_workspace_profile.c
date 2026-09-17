#include "editor/scene_editor_tool_state.h"
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_transform_panel.h"
static SceneEditorWorkspaceProfile active;
static bool menu_open, add_menu;
static int menu_focus;
static bool scene_nav_saved;
static SceneEditorDigestOverlayNavState scene_nav;
bool SceneEditorWorkspaceProfileMenuOpen(void) { return menu_open; }
SceneEditorWorkspaceProfile SceneEditorWorkspaceProfileGet(void) { return active; }
void SceneEditorWorkspaceProfileReset(void) { active = SCENE_WORKSPACE_SCENE; scene_nav_saved=false; menu_open=false; add_menu=false; SceneEditorLifecycleReset(); }
const char* SceneEditorWorkspaceProfileLabel(int profile) {
    static const char* labels[] = {"Scene", "Materials", "Surface", "Atmos / Water", "Render"};
    return profile >= 0 && profile < SCENE_WORKSPACE_PROFILE_COUNT ? labels[profile] : "Scene";
}
void SceneEditorWorkspaceProfileSelect(SceneEditor* editor, SceneEditorWorkspaceProfile profile) {
    if (!editor || profile < 0 || profile >= SCENE_WORKSPACE_PROFILE_COUNT) return;
    int selected = ObjectEditorGetSelectedObjectIndex();
    bool entering_material=profile==SCENE_WORKSPACE_MATERIALS && active!=SCENE_WORKSPACE_MATERIALS;
    bool leaving_material=profile!=SCENE_WORKSPACE_MATERIALS && active==SCENE_WORKSPACE_MATERIALS;
    if (entering_material) { scene_nav=*SceneEditorGetViewportNavState(); scene_nav_saved=true; }
    menu_open=false;
    active = profile;
    SetSceneMode(editor, profile == SCENE_WORKSPACE_MATERIALS ? EDITOR_MODE_MATERIAL :
        profile == SCENE_WORKSPACE_RENDER ? EDITOR_MODE_CAMERA : EDITOR_MODE_OBJECT);
    ObjectEditorSetSelectedObjectIndex(selected);
    if (entering_material && selected>=0) SceneEditorFrameViewport(true);
    if (leaving_material && scene_nav_saved) { SceneEditorRestoreViewportNav(&scene_nav); scene_nav_saved=false; }
    SceneEditorSidebarReset();
    if (profile==SCENE_WORKSPACE_SCENE) SceneEditorSidebarShowLibrary(false);
}
static SDL_Rect menu_row(const SceneEditorWorkspaceChrome* chrome,int i) {
    if (!add_menu) return chrome->modes[i];
    SDL_Rect anchor=chrome->actions[1];
    return (SDL_Rect){anchor.x,anchor.y+anchor.h+4+i*(anchor.h+4),210,anchor.h+4};
}
static void select_add(SceneEditor* editor,int i) {
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    if (i==0) {
        SceneEditorSidebarShowLibrary(true);
        SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_ADD);
        SceneEditorChromeShellSetActionFeedback("Choose a library object, then click the viewport to place it. Escape cancels.",5000);
    } else {
        SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
        SceneEditorTransformPanelOpenImport();
        SceneEditorChromeShellSetActionFeedback("Choose STL source units in the inspector, then Import STL",5000);
    }
}
bool SceneEditorWorkspaceProfileHandleEvent(SceneEditor* editor, const SDL_Event* event) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    if (!event || !SceneEditorGetPaneLayout(&layout)) return false;
    if (event->type==SDL_WINDOWEVENT &&
        (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST ||
         event->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) menu_open=false;
    int count=add_menu ? 2 : SCENE_WORKSPACE_PROFILE_COUNT;
    if (menu_open && event->type==SDL_KEYDOWN) {
        if (event->key.keysym.sym==SDLK_ESCAPE) menu_open=false;
        else if (event->key.keysym.sym==SDLK_DOWN) menu_focus=(menu_focus+1)%count;
        else if (event->key.keysym.sym==SDLK_UP) menu_focus=(menu_focus+count-1)%count;
        else if (event->key.keysym.sym==SDLK_RETURN) {
            if (add_menu) select_add(editor,menu_focus);
            else SceneEditorWorkspaceProfileSelect(editor,menu_focus);
        }
        return true;
    }
    SceneEditorWorkspaceLayoutChrome(&layout, &chrome);
    if (event->type!=SDL_MOUSEBUTTONDOWN) return menu_open &&
        (event->type==SDL_MOUSEBUTTONUP || event->type==SDL_MOUSEMOTION ||
         event->type==SDL_MOUSEWHEEL || event->type==SDL_TEXTINPUT || event->type==SDL_KEYUP);
    if (event->button.button!=SDL_BUTTON_LEFT) { bool consumed=menu_open; menu_open=false; return consumed; }
    SDL_Point point={event->button.x,event->button.y};
    if (SDL_PointInRect(&point,&chrome.workspace)) {
        if (SceneEditorTransformPanelInteractionActive()) return true;
        menu_open=!menu_open; add_menu=false; menu_focus=active; return true;
    }
    if (!menu_open && SDL_PointInRect(&point,&chrome.actions[1]) && editor->currentMode==EDITOR_MODE_OBJECT) {
        if (SceneEditorTransformPanelInteractionActive()) return true;
        menu_open=true; add_menu=true; menu_focus=0; return true;
    }
    if (!menu_open && (active==SCENE_WORKSPACE_MATERIALS || active==SCENE_WORKSPACE_SURFACE) &&
        SDL_PointInRect(&point,&chrome.actions[4])) {
        SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE); return true;
    }
    if (!menu_open) {
        if (SDL_PointInRect(&point,&chrome.frame_all)) { SceneEditorFrameViewport(false); return true; }
        if (SDL_PointInRect(&point,&chrome.frame_selected)) { SceneEditorFrameViewport(true); return true; }
        if (SDL_PointInRect(&point,&chrome.undo)) { SceneEditorTransformPanelHistory(false); return true; }
        if (SDL_PointInRect(&point,&chrome.redo)) { SceneEditorTransformPanelHistory(true); return true; }
    }
    if (!menu_open) return false;
    menu_open=false;
    for (int i=0;i<count;++i) {
        SDL_Rect r = menu_row(&chrome,i);
        if (event->button.x >= r.x && event->button.x < r.x+r.w &&
            event->button.y >= r.y && event->button.y < r.y+r.h) {
            if (add_menu) select_add(editor,i); else SceneEditorWorkspaceProfileSelect(editor, i); return true;
        }
    }
    return true; /* Dismissal does not click through to the underlying scene. */
}

void SceneEditorWorkspaceProfileRenderOverlay(SDL_Renderer* renderer) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    if (!menu_open || !SceneEditorGetPaneLayout(&layout)) return;
    SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    SDL_Rect prior; SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&prior); SDL_RenderSetClipRect(renderer,NULL);
    for (int i=0;i<(add_menu ? 2 : SCENE_WORKSPACE_PROFILE_COUNT);++i) {
        SDL_Rect row=menu_row(&chrome,i);
        SDL_Color fill=i==menu_focus ? palette.button_active_fill : palette.panel_fill;
        SDL_SetRenderDrawColor(renderer,fill.r,fill.g,fill.b,255); SDL_RenderFillRect(renderer,&row);
        SDL_SetRenderDrawColor(renderer,palette.panel_border.r,palette.panel_border.g,palette.panel_border.b,255);
        SDL_RenderDrawRect(renderer,&row);
        row.x+=8; row.w-=16;
        SceneEditorLabelLeft(renderer,row,add_menu ? (i==0 ? "Place from library" : "Import STL...") : SceneEditorWorkspaceProfileLabel(i),
            ray_tracing_theme_choose_button_text(fill,palette));
    }
    SDL_RenderSetClipRect(renderer,clipped ? &prior : NULL);
}

void SceneEditorWorkspaceProfileSyncMode(int mode) {
    if (mode == EDITOR_MODE_MATERIAL) active = SCENE_WORKSPACE_MATERIALS;
    else if (mode == EDITOR_MODE_CAMERA) active = SCENE_WORKSPACE_RENDER;
    else if (mode == EDITOR_MODE_PATH) active = SCENE_WORKSPACE_SCENE;
    else if (active == SCENE_WORKSPACE_MATERIALS || active == SCENE_WORKSPACE_RENDER) active = SCENE_WORKSPACE_SCENE;
}
