#include "editor/scene_editor_mesh_preview_render.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_internal.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/scene_editor_tool_state.h"
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_transform_ergonomics.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/material_editor.h"
static SceneEditorWorkspaceProfile active;
static bool menu_open, add_menu;
static int document_menu=-1;
static int menu_focus;
static bool scene_nav_saved;
static SceneEditorDigestOverlayNavState scene_nav;
bool SceneEditorWorkspaceProfileMenuOpen(void) { return menu_open; }
SceneEditorWorkspaceProfile SceneEditorWorkspaceProfileGet(void) { return active; }
void SceneEditorWorkspaceProfileReset(void) { SceneEditorObjectTransformModeSet(SCENE_EDITOR_OBJECT_TRANSFORM_MOVE); SceneEditorTransformErgonomicsReset(); active = SCENE_WORKSPACE_SCENE; scene_nav_saved=false; menu_open=false; add_menu=false; document_menu=-1; SceneEditorObjectMoveGizmoReset(); SceneEditorLifecycleReset(); }
const char* SceneEditorWorkspaceProfileLabel(int profile) {
    static const char* labels[] = {"Scene", "Material", "Surface", "Environment", "Render"};
    return profile >= 0 && profile < SCENE_WORKSPACE_PROFILE_COUNT ? labels[profile] : "Scene";
}
void SceneEditorWorkspaceProfileSelect(SceneEditor* editor, SceneEditorWorkspaceProfile profile) {
    if (!editor || profile < 0 || profile >= SCENE_WORKSPACE_PROFILE_COUNT) return;
    SceneEditorObjectMoveGizmoReset();
    SceneEditorChromeShellSetActionFeedback("",0);
    int selected = ObjectEditorGetSelectedObjectIndex();
    char selected_id[128];snprintf(selected_id,sizeof(selected_id),"%s",ObjectEditorSelectionTrackerId());
    bool entering_material=profile==SCENE_WORKSPACE_MATERIALS && active!=SCENE_WORKSPACE_MATERIALS;
    bool leaving_material=profile!=SCENE_WORKSPACE_MATERIALS && active==SCENE_WORKSPACE_MATERIALS;
    if (entering_material) { scene_nav=*SceneEditorGetViewportNavState(); scene_nav_saved=true; }
    menu_open=false;
    active = profile;
    SetSceneMode(editor, profile == SCENE_WORKSPACE_MATERIALS ? EDITOR_MODE_MATERIAL :
        profile == SCENE_WORKSPACE_RENDER ? EDITOR_MODE_CAMERA : EDITOR_MODE_OBJECT);
    if(selected_id[0]) ObjectEditorSelectionTrackerSelectId(selected_id);
    else ObjectEditorSetSelectedObjectIndex(selected);
    if (entering_material) {
        MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT);
    }
    if (leaving_material && scene_nav_saved) { SceneEditorRestoreViewportNav(&scene_nav); scene_nav_saved=false; }
    SceneEditorSidebarReset();
    if (profile==SCENE_WORKSPACE_SCENE) SceneEditorSidebarShowLibrary(false);
}
static int menu_count(void) {
    if (document_menu==3) return SCENE_EDITOR_MESH_DISPLAY_COUNT;
    if (document_menu==0) return 2;
    if (document_menu==1) return 2;
    if (document_menu==2) return 8;
    return add_menu ? 2 : SCENE_WORKSPACE_PROFILE_COUNT;
}
static SDL_Rect menu_row(const SceneEditorWorkspaceChrome* chrome,int i) {
    SDL_Rect anchor=document_menu==3 ? chrome->display_mode : document_menu>=0 ? chrome->menus[document_menu] :
        add_menu ? chrome->actions[1] : chrome->workspace;
    return (SDL_Rect){document_menu==3 ? anchor.x+anchor.w-140 : anchor.x,anchor.y+anchor.h+4+i*(anchor.h+4),document_menu==3 ? 140 : 230,anchor.h+4};
}
static const char* menu_label(int i) {
    static const char* file[]={"Save", "Leave editor..."};
    static const char* edit[]={"Undo", "Redo"};
    static const char* view[]={"Frame all", "Frame selected", "Expand / restore viewport", "Reset layout",
        "World / Local label", "Toggle snapping", "Paths", "Light keyframes"};
    if(document_menu==3) return SceneEditorMeshDisplayModeName(i);
    if(document_menu==0) return file[i];
    if(document_menu==1) return edit[i];
    if(document_menu==2) return view[i];
    return add_menu ? (i==0 ? "Place from library" : "Import STL...") : SceneEditorWorkspaceProfileLabel(i);
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
static bool menu_enabled(int i) {
    if(document_menu==1) return i==0 ? SceneEditorDocumentCanUndo() : SceneEditorDocumentCanRedo();
    if(document_menu==2 && i==1) return ObjectEditorGetSelectedObjectIndex()>=0;
    if(document_menu==2 && (i==4 || i==5)) return active==SCENE_WORKSPACE_SCENE;
    return true;
}
static void select_menu(SceneEditor* editor,int i) {
    if(!menu_enabled(i)) return;
    menu_open=false;
    if (document_menu<0) {
        if(add_menu) select_add(editor,i); else SceneEditorWorkspaceProfileSelect(editor,i);
        return;
    }
    if(document_menu==3) { SceneEditorMeshPreviewModeSet(i);return; }
    SceneEditorChromeAction action={0};
    if(document_menu==0) action.kind=i==0 ? SCENE_EDITOR_CHROME_ACTION_SAVE : SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU;
    else if(document_menu==1) { SceneEditorTransformPanelHistory(i==1); return; }
    else if(i<2) { SceneEditorFrameViewport(i==1); return; }
    else if(i==2) action.kind=SCENE_EDITOR_CHROME_ACTION_EXPAND_VIEWPORT;
    else if(i==3) action.kind=SCENE_EDITOR_CHROME_ACTION_RESTORE_WORKSPACE;
    else if(i==4) { SceneEditorTransformSpaceToggle(); return; }
    else if(i==5) { SceneEditorTransformSnapToggle(); return; }
    else if(i==6) { SetSceneMode(editor,EDITOR_MODE_PATH); return; }
    else action.kind=SCENE_EDITOR_CHROME_ACTION_TOGGLE_LIGHT_TIMELINE;
    SceneEditorInputRouterCallbacks callbacks=SceneEditorBuildInputRouterCallbacks(editor);
    callbacks.apply_chrome_action(callbacks.context,&action);
}
bool SceneEditorWorkspaceProfileHandleEvent(SceneEditor* editor, const SDL_Event* event) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    if (!event || !SceneEditorGetPaneLayout(&layout)) return false;
    if (event->type==SDL_WINDOWEVENT &&
        (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST ||
         event->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) menu_open=false;
    int count=menu_count();
    if (menu_open && event->type==SDL_KEYDOWN) {
        if (event->key.keysym.sym==SDLK_ESCAPE) menu_open=false;
        else if (event->key.keysym.sym==SDLK_DOWN) menu_focus=(menu_focus+1)%count;
        else if (event->key.keysym.sym==SDLK_UP) menu_focus=(menu_focus+count-1)%count;
        else if (event->key.keysym.sym==SDLK_RETURN) {
            select_menu(editor,menu_focus);
        }
        return true;
    }
    SceneEditorWorkspaceLayoutChrome(&layout, &chrome);
    if(menu_open && event->type==SDL_MOUSEMOTION) {
        SDL_Point hover={event->motion.x,event->motion.y};
        for(int i=0;i<count;++i) { SDL_Rect row=menu_row(&chrome,i);
            if(SDL_PointInRect(&hover,&row)) menu_focus=i; }
    }
    if (event->type!=SDL_MOUSEBUTTONDOWN) return menu_open &&
        (event->type==SDL_MOUSEBUTTONUP || event->type==SDL_MOUSEMOTION ||
         event->type==SDL_MOUSEWHEEL || event->type==SDL_TEXTINPUT || event->type==SDL_KEYUP);
    if (event->button.button!=SDL_BUTTON_LEFT) { bool consumed=menu_open; menu_open=false; return consumed; }
    SDL_Point point={event->button.x,event->button.y};
    if(SDL_PointInRect(&point,&chrome.display_mode)) {
        if(SceneEditorTransformPanelInteractionActive()) return true;
        bool close=menu_open && document_menu==3;
        document_menu=3;add_menu=false;menu_open=!close;menu_focus=SceneEditorMeshPreviewModeGet();return true;
    }
    for(int i=0;i<3;++i) if(SDL_PointInRect(&point,&chrome.menus[i])) {
        if(SceneEditorTransformPanelInteractionActive()) return true;
        bool close=menu_open && document_menu==i;
        document_menu=i; add_menu=false; menu_open=!close; menu_focus=0; return true;
    }
    if(SDL_PointInRect(&point,&chrome.workspace)) {
        if(SceneEditorTransformPanelInteractionActive()) return true;
        bool close=menu_open && document_menu<0 && !add_menu;
        document_menu=-1; add_menu=false; menu_open=!close; menu_focus=active; return true;
    }
    if(!menu_open && active==SCENE_WORKSPACE_SCENE && SDL_PointInRect(&point,&chrome.actions[0])) {
        SceneEditorObjectSelectTool();return true;
    }
    if(!menu_open && active==SCENE_WORKSPACE_MATERIALS) {
        for(int i=0;i<2;++i) if(SDL_PointInRect(&point,&chrome.context_views[i])) {
            MaterialEditorSetViewMode(i==0 ? MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT : MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
            return true;
        }
    }
    if (!menu_open && active==SCENE_WORKSPACE_SCENE && editor->currentMode==EDITOR_MODE_OBJECT &&
        SDL_PointInRect(&point,&chrome.transform_space)) {
        if (SceneEditorTransformPanelInteractionActive()) return true;
        SceneEditorTransformSpaceToggle();
        SceneEditorChromeShellSetActionFeedback(SceneEditorTransformSpaceGet()==SCENE_EDITOR_TRANSFORM_SPACE_LOCAL ?
            "Local transform orientation" : "World transform orientation",1600);
        return true;
    }
    if (!menu_open && active==SCENE_WORKSPACE_SCENE && editor->currentMode==EDITOR_MODE_OBJECT &&
        SDL_PointInRect(&point,&chrome.transform_snap)) {
        if (SceneEditorTransformPanelInteractionActive()) return true;
        SceneEditorTransformSnapToggle();
        SceneEditorChromeShellSetActionFeedback(SceneEditorTransformSnapEnabled() ?
            "Snapping on: move 0.1, rotate 15°, scale 0.1" : "Transform snapping off",1800);
        return true;
    }
    if (!menu_open && SDL_PointInRect(&point,&chrome.actions[1]) && active==SCENE_WORKSPACE_SCENE && editor->currentMode==EDITOR_MODE_OBJECT) {
        if (SceneEditorTransformPanelInteractionActive()) return true;
        menu_open=true; add_menu=true; document_menu=-1; menu_focus=0; return true;
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
    for (int i=0;i<count;++i) {
        SDL_Rect r = menu_row(&chrome,i);
        if (event->button.x >= r.x && event->button.x < r.x+r.w &&
            event->button.y >= r.y && event->button.y < r.y+r.h) {
            select_menu(editor,i); return true;
        }
    }
    menu_open=false;
    return true; /* Dismissal does not click through to the underlying scene. */
}

void SceneEditorWorkspaceProfileRenderOverlay(SDL_Renderer* renderer) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    if (!menu_open || !SceneEditorGetPaneLayout(&layout)) return;
    SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    SDL_Rect prior; SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&prior); SDL_RenderSetClipRect(renderer,NULL);
    for (int i=0;i<menu_count();++i) {
        SDL_Rect row=menu_row(&chrome,i);
        SDL_Color fill=i==menu_focus ? palette.button_active_fill : palette.panel_fill;
        SDL_SetRenderDrawColor(renderer,fill.r,fill.g,fill.b,255); SDL_RenderFillRect(renderer,&row);
        SDL_SetRenderDrawColor(renderer,palette.panel_border.r,palette.panel_border.g,palette.panel_border.b,255);
        SDL_RenderDrawRect(renderer,&row);
        row.x+=8; row.w-=16;
        SceneEditorLabelLeft(renderer,row,menu_label(i),
            menu_enabled(i) ? ray_tracing_theme_choose_button_text(fill,palette) : palette.text_muted);
    }
    SDL_RenderSetClipRect(renderer,clipped ? &prior : NULL);
}

void SceneEditorWorkspaceProfileSyncMode(int mode) {
    if (mode == EDITOR_MODE_MATERIAL) active = SCENE_WORKSPACE_MATERIALS;
    else if (mode == EDITOR_MODE_CAMERA) active = SCENE_WORKSPACE_RENDER;
    else if (mode == EDITOR_MODE_PATH) active = SCENE_WORKSPACE_SCENE;
    else if (active == SCENE_WORKSPACE_MATERIALS || active == SCENE_WORKSPACE_RENDER) active = SCENE_WORKSPACE_SCENE;
}
