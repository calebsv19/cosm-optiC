#include "editor/scene_editor_mesh_preview_store.h"
#include <limits.h>
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_tool_state.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/material_editor.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/object_editor_panels.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL_ttf.h>
#include "editor/scene_editor.h"
#include "editor/scene_editor_internal.h"
#include "editor/scene_editor_session_runtime.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/object_editor_selection_tracker.h"
#include "vk_renderer.h"
#include "editor/scene_editor_viewport_nav.h"
#include "editor/scene_editor_digest_overlay_internal.h"

/* Acceptance failures are ordinary test exits, not OS crash reports. */
static SceneEditor* active_editor;
static void fail_check(const char* expression, int line) {
    fprintf(stderr, "UI acceptance failed at line %d: %s\n", line, expression);
    if (active_editor) DestroySceneEditor(active_editor);
    TTF_Quit(); SDL_Quit(); exit(EXIT_FAILURE);
}
#undef assert
#define assert(expression) do { if (!(expression)) fail_check(#expression, __LINE__); } while (0)

static void click(SceneEditor* editor, SDL_Rect rect) {
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = SDL_GetWindowID(editor->window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = rect.x + rect.w / 2;
    event.button.y = rect.y + rect.h / 2;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    event.type = SDL_MOUSEBUTTONUP;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    SceneEditorSessionRuntimeRender(editor);
}

static void key(SceneEditor* editor, SDL_Keycode code) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
}

static void capture(SceneEditor* editor, const char* path) {
    VkRenderer* renderer = (VkRenderer*)editor->renderer;
    assert(vk_renderer_request_capture(renderer, path) == VK_SUCCESS);
    for (int i = 0; i < 8 && !renderer->debug_capture.dumped; ++i) {
        SDL_PumpEvents();
        SceneEditorSessionRuntimeRender(editor);
    }
    assert(renderer->debug_capture.dumped);
}

static void verify_viewport_gestures(SceneEditor* editor) {
    SceneEditorPaneLayout layout;
    assert(SceneEditorGetPaneLayout(&layout));
    int x=layout.viewport_rect.x+layout.viewport_rect.w/2;
    int y=layout.viewport_rect.y+layout.viewport_rect.h/2;
    unsigned long long revision=SceneEditorDocumentRevision();
    int selected=ObjectEditorGetSelectedObjectIndex();
    SDL_WarpMouseInWindow(editor->window,x,y);
    SDL_PumpEvents();
    SceneEditorDigestOverlayNavState before=*SceneEditorGetViewportNavState();
    SDL_Keymod previous_mods=SDL_GetModState();
    SDL_SetModState(KMOD_ALT);
    SDL_Event event={0}; event.type=SDL_MOUSEMOTION;
    event.motion.windowID=SDL_GetWindowID(editor->window);
    event.motion.x=x; event.motion.y=y;
    event.motion.xrel=32; event.motion.yrel=12;
    event.motion.state=SDL_BUTTON_LMASK;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorGetViewportNavState()->orbit_yaw_deg!=before.orbit_yaw_deg);
    SDL_SetModState(previous_mods);
    event=(SDL_Event){0}; event.type=SDL_MOUSEBUTTONUP;
    event.button.button=SDL_BUTTON_LEFT; event.button.x=x; event.button.y=y;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    before=*SceneEditorGetViewportNavState();
    event.type=SDL_MOUSEBUTTONDOWN; event.button.button=SDL_BUTTON_MIDDLE;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    event=(SDL_Event){0}; event.type=SDL_MOUSEMOTION;
    event.motion.x=x+20; event.motion.y=y+10;
    event.motion.xrel=20; event.motion.yrel=10; event.motion.state=SDL_BUTTON_MMASK;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    const SceneEditorDigestOverlayNavState* current=SceneEditorGetViewportNavState();
    assert(current->target_x!=before.target_x || current->target_y!=before.target_y ||
           current->target_z!=before.target_z);
    event=(SDL_Event){0}; event.type=SDL_MOUSEBUTTONUP;
    event.button.button=SDL_BUTTON_MIDDLE; event.button.x=x; event.button.y=y;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    before=*SceneEditorGetViewportNavState();
    event=(SDL_Event){0}; event.type=SDL_MOUSEWHEEL;
    /* Framing a tiny imported mesh may already be at maximum zoom. */
    event.wheel.y = before.overlay_zoom >= before.zoom_max ? -1 : 1;
    event.wheel.preciseY = (float)event.wheel.y;
#if SDL_VERSION_ATLEAST(2,26,0)
    event.wheel.mouseX=x; event.wheel.mouseY=y;
#endif
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorGetViewportNavState()->overlay_zoom!=before.overlay_zoom);
    key(editor,SDLK_f);
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorWorkspaceChrome chrome;
    SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    click(editor,chrome.frame_all);
    assert(SceneEditorDocumentRevision()==revision);
    assert(ObjectEditorGetSelectedObjectIndex()==selected);
    RuntimeSceneBridge3DDigestState digest={0};
    SceneEditorDigestOverlayProjector projector;
    assert(SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,
        SceneEditorGetViewportNavState(),&projector));
    /* The fixture contains three visible mesh spheres above a primitive floor.
       Pick their projected centers through ordinary session input. */
    for (int i=3;i<=5;++i) {
        SceneEditorDocumentTransform transform; char diagnostics[256];
        assert(SceneEditorDocumentGetTransformForSceneIndex(i,&transform,diagnostics,sizeof(diagnostics)));
        int px,py;
        assert(SceneEditorDigestOverlayProjectPoint(&projector,transform.position[0],
            transform.position[1],transform.position[2],&px,&py));
        click(editor,(SDL_Rect){px,py,1,1});
        assert(ObjectEditorGetSelectedObjectIndex()==i);
    }
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorSessionRuntimeRender(editor);
}

#include "scene_editor_move_acceptance.h"
#include "scene_editor_transform_acceptance.h"

int main(int argc, char** argv) {
    SceneEditor editor;
    SceneEditorPaneLayout before, after;
    assert(argc == 3 || argc == 4);
    bool reopen_only = argc == 4 && strcmp(argv[3], "--reopen") == 0;
    bool review_only = argc == 4 && strcmp(argv[3], "--review") == 0; /* Task-owned working directory and copied runtime scene. */
    assert(chdir(argv[1]) == 0);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.spaceMode = SPACE_MODE_3D;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", argv[2]);
    assert(InitializeSceneEditor(&editor));
    active_editor = &editor;
    SDL_SetWindowTitle(editor.window, "optiC E0/E1 isolated source proof");
    SDL_SetWindowSize(editor.window, 1280, 800);
    SceneEditorWorkspaceProfileSelect(&editor, SCENE_WORKSPACE_SCENE);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorDocumentIsOpen());
    if (review_only) {
        SDL_SetWindowTitle(editor.window, "optiC E0/E1 review — isolated scene copy");
        ObjectEditorSetSelectedObjectIndex(-1);
        SceneEditorPaneLayout review_layout;
        if (SceneEditorGetPaneLayout(&review_layout))
            SceneEditorViewportNavFitDigestOverlayForTarget((SceneEditorDigestOverlayNavState*)SceneEditorGetViewportNavState(),
                &review_layout.viewport_rect,true,EDITOR_MODE_OBJECT,-1);
        SceneEditorLoop(&editor);
        active_editor = NULL; TTF_Quit(); SDL_Quit();
        fprintf(stderr,"[editor lifecycle] review process exited normally\n");
        return 0;
    }
    if (argc==4 && strcmp(argv[3],"--world-scale")==0) {
        int selected=sceneSettings.objectCount-1;char diagnostics[256],label[256];
        SceneEditorDocumentTransform original,preview,baseline,current;
        assert(SceneEditorDocumentWorldScale()==2.0 && strcmp(SceneEditorDocumentUnitLabel(),"meters")==0);
        ObjectEditorSetSelectedObjectIndex(selected);assert(SceneEditorFrameViewport(true));
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&original,diagnostics,sizeof(diagnostics)));
        SceneEditorObjectTransformHandle h;int ex,ey;transform_handle(&editor,selected,1,&h,&ex,&ey);
        move_pointer(&editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(&editor,SDL_MOUSEMOTION,ex,ey);
        assert(SceneEditorObjectTransformPreview(selected,&baseline,&preview));
        double pixels=(ex-h.x)*h.ux+(ey-h.y)*h.uy;
        assert(fabs(preview.position[0]-original.position[0]-pixels/(h.pixels_per_unit*2.0))<1e-9);
        assert(SceneEditorTransformOperationLabel(selected,label,sizeof(label)) && strstr(label,"meters"));
        capture(&editor,"workspace_world_scale_move.ppm");
        move_pointer(&editor,SDL_MOUSEBUTTONUP,ex,ey);
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        transform_compare(&current,&preview);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        active_editor=NULL;DestroySceneEditor(&editor);
        assert(strcmp(SceneEditorDocumentUnitLabel(),"scene units")==0);
        TTF_Quit();SDL_Quit();return 0;
    }
    if (argc==4 && (strcmp(argv[3],"--rotate")==0 || strcmp(argv[3],"--scale")==0 ||
                    strcmp(argv[3],"--rotate-preview")==0 || strcmp(argv[3],"--scale-preview")==0)) {
        int selected=sceneSettings.objectCount-1;char diagnostics[256],name[128];SceneEditorDocumentTransform actual,expected;
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&actual,diagnostics,sizeof(diagnostics)));
        snprintf(name,sizeof(name),"%s_expected.txt",argv[3]+2);FILE* f=fopen(name,"r");assert(f);
        for (int i=0;i<3;++i) assert(fscanf(f,"%lf %lf %lf",&expected.position[i],&expected.rotation_degrees[i],&expected.scale[i])==3);
        fclose(f);transform_compare(&actual,&expected);assert(!SceneEditorDocumentIsDirty());
        ObjectEditorSetSelectedObjectIndex(selected);assert(SceneEditorFrameViewport(false));
        snprintf(name,sizeof(name),"workspace_%s_reopen.ppm",argv[3]+2);capture(&editor,name);
        active_editor=NULL;DestroySceneEditor(&editor);TTF_Quit();SDL_Quit();return 0;
    }
    if (argc==4 && strcmp(argv[3],"--move-reopen")==0) {
        int selected=sceneSettings.objectCount-1; char diagnostics[256];
        SceneEditorDocumentTransform transform;
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&transform,diagnostics,sizeof(diagnostics)));
        FILE* expected=fopen("move_expected.txt","r"); assert(expected);
        for (int i=0;i<3;++i) { double value; assert(fscanf(expected,"%lf",&value)==1); assert(fabs(value-transform.position[i])<1e-9); }
        fclose(expected);
        assert(!SceneEditorDocumentIsDirty());
        ObjectEditorSetSelectedObjectIndex(selected); assert(SceneEditorFrameViewport(true));
        capture(&editor,"workspace_committed_move_reopen.ppm");
        active_editor=NULL; DestroySceneEditor(&editor); TTF_Quit(); SDL_Quit(); return 0;
    }
    if (reopen_only) {
        int selected = sceneSettings.objectCount-1;
        SceneEditorDocumentTransform loaded; char diagnostics[256];
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&loaded,diagnostics,sizeof(diagnostics)));
        assert(fabs(loaded.position[0] - 0.25) < 1e-6);
        assert(sceneSettings.sceneObjects[selected].material_id == 1);
        ObjectEditorSetSelectedObjectIndex(selected);
        assert(SceneEditorFrameViewport(false));
        capture(&editor,"workspace_fresh_reopen.ppm");
        loaded.position[0]+=9.0;
        assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&loaded,diagnostics,sizeof(diagnostics)));
        click(&editor,backToMenuButton);
        assert(SceneEditorLifecycleClosePending() && editor.running && SceneEditorDocumentIsDirty());
        SceneEditorPaneLayout close_layout;
        assert(SceneEditorGetPaneLayout(&close_layout));
        int dialog_width=close_layout.workspace_header_rect.w-32;
        if (dialog_width>580) dialog_width=580;
        int dialog_x=close_layout.workspace_header_rect.x+(close_layout.workspace_header_rect.w-dialog_width)/2;
        click(&editor,(SDL_Rect){dialog_x+12+(dialog_width-40)/3+8+10,248,1,1});
        assert(!editor.running && sceneEditorExitFlag);

        active_editor = NULL; DestroySceneEditor(&editor); TTF_Quit(); SDL_Quit();
        puts("Fresh process reopened imported object with saved transform and Mirror material");
        return 0;
    }
    if (argc == 4) {
        int original_count = sceneSettings.objectCount;
        ObjectEditorSetSelectedObjectIndex(-1);
        SceneEditorSessionRuntimeRender(&editor);
        SceneEditorPaneLayout import_layout;
        assert(SceneEditorGetPaneLayout(&import_layout));
        /* Add is the single entry to import setup; keyboard choice is supported. */
        click(&editor,addButton);
        assert(SceneEditorWorkspaceProfileMenuOpen());
        capture(&editor,"workspace_add_menu.ppm");
        key(&editor,SDLK_DOWN); key(&editor,SDLK_RETURN);
        SceneEditorSessionRuntimeRender(&editor);
        assert(!SceneEditorWorkspaceProfileMenuOpen());
        /* The fixture is 1000 mm wide. Exercise the actual source-unit control. */
        click(&editor,(SDL_Rect){import_layout.right_content_rect.x + import_layout.right_content_rect.w*3/4,
            import_layout.right_content_rect.y + 23 + 2*29 + 8,1,1});
        capture(&editor, "workspace_import_units.ppm");
        SDL_Event drop = {0}; drop.type = SDL_DROPFILE;
        drop.drop.file = SDL_strdup(argv[3]);
        SceneEditorSessionRuntimeHandleEvent(&editor, &drop);
        capture(&editor, "workspace_import_started.ppm");
        assert(SceneEditorTransformPanelInteractionActive());
        Uint32 deadline = SDL_GetTicks() + 30000;
        while (SceneEditorTransformPanelInteractionActive() && SDL_GetTicks() < deadline) {
            SceneEditorTransformPanelPoll();
            SDL_PumpEvents();
            SceneEditorSessionRuntimeRender(&editor);
            SDL_Delay(10);
        }
        assert(!SceneEditorTransformPanelInteractionActive());
        assert(sceneSettings.objectCount == original_count + 1);
        /* Managed candidate adoption publishes atomically and retains undo. */
        assert(SceneEditorDocumentCanUndo());
        click(&editor, saveButton);
    }
    ObjectEditorSetSelectedObjectIndex(argc == 4 ? sceneSettings.objectCount - 1 : 0);
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorGetPaneLayout(&before));
    SceneEditorViewportNavFitDigestOverlayForTarget((SceneEditorDigestOverlayNavState*)SceneEditorGetViewportNavState(),
        &before.viewport_rect, true, EDITOR_MODE_OBJECT, 0);
    unsigned long long revision = SceneEditorDocumentRevision();
    int selected = ObjectEditorGetSelectedObjectIndex();
    verify_viewport_gestures(&editor);
    capture(&editor, "workspace_scene.ppm");
    key(&editor,SDLK_ESCAPE);
    assert(editor.running && !sceneEditorExitFlag);
    SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_ADD);
    key(&editor,SDLK_ESCAPE);
    assert(SceneEditorToolStateGetActive()==SCENE_EDITOR_TOOL_SELECT && editor.running);
    assert(SceneEditorMeshPreviewStoreHasSceneObject(selected));
    int material_before=sceneSettings.sceneObjects[selected].material_id;
    SDL_Rect material_field={before.right_content_rect.x+25,before.right_content_rect.y+23+5*29+66+8,1,1};
    click(&editor,material_field);
    capture(&editor,"workspace_inspector_materials.ppm");
    /* Mirror is the second preset; assignment stays in the selected-object inspector. */
    click(&editor,(SDL_Rect){material_field.x,material_field.y+2*29,1,1});
    assert(sceneSettings.sceneObjects[selected].material_id==1);
    if (material_before!=1) assert(SceneEditorTransformPanelHistory(false));
    assert(sceneSettings.sceneObjects[selected].material_id==material_before);
    SceneEditorSessionRuntimeRender(&editor);
    SceneEditorDigestOverlayNavState before_material=*SceneEditorGetViewportNavState();
    click(&editor,(SDL_Rect){material_field.x,material_field.y+29,1,1});
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_MATERIALS);
    assert(ObjectEditorGetSelectedObjectIndex()==selected);
    assert(MaterialEditorGetViewMode()==MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT);
    assert(memcmp(&before_material,SceneEditorGetViewportNavState(),sizeof(before_material))==0);
    capture(&editor,"workspace_material_scene_context.ppm");
    /* Focusing the object is now explicit; entering Materials keeps the scene. */
    assert(SceneEditorFrameViewport(true));
    RuntimeSceneBridge3DDigestState material_digest={0};
    assert(SceneEditorDigestOverlayResolve(&material_digest));
    SceneEditorDigestOverlayProjector material_projector;
    assert(SceneEditorDigestOverlayBuildObjectProjector(&material_digest,&before.viewport_rect,
        SceneEditorGetViewportNavState(),selected,false,&material_projector));
    double lo[3],hi[3];
    assert(SceneEditorDigestOverlayResolveObjectExtents(&material_digest,selected,
        &lo[0],&lo[1],&lo[2],&hi[0],&hi[1],&hi[2],NULL));
    int minx=INT_MAX,miny=INT_MAX,maxx=INT_MIN,maxy=INT_MIN;
    for (int corner=0;corner<8;++corner) {
        int x,y;
        assert(SceneEditorDigestOverlayProjectPoint(&material_projector,
            corner&1 ? hi[0] : lo[0],corner&2 ? hi[1] : lo[1],corner&4 ? hi[2] : lo[2],&x,&y));
        if (x<minx) minx=x; if (x>maxx) maxx=x;
        if (y<miny) miny=y; if (y>maxy) maxy=y;
    }
    fprintf(stderr,"MATERIAL FIT span=%d,%d viewport=%d,%d zoom=%g scale=%g bounds=%g,%g,%g to %g,%g,%g\n",maxx-minx,maxy-miny,before.viewport_rect.w,before.viewport_rect.h,SceneEditorGetViewportNavState()->overlay_zoom,material_projector.scale,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
    capture(&editor,"workspace_material_fit_debug.ppm");
    assert(maxx-minx<=before.viewport_rect.w && maxy-miny<=before.viewport_rect.h);
    assert(maxx-minx>before.viewport_rect.w/4 || maxy-miny>before.viewport_rect.h/4);
    capture(&editor,"workspace_selected_material_tools.ppm");
    SceneEditorWorkspaceChrome return_chrome;
    SceneEditorWorkspaceLayoutChrome(&before,&return_chrome);
    click(&editor,return_chrome.actions[4]);
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_SCENE);
    assert(ObjectEditorGetSelectedObjectIndex()==selected);
    assert(memcmp(&before_material,SceneEditorGetViewportNavState(),sizeof(before_material))==0);
    bool layout_dirty=SceneEditorDocumentIsDirty();
    revision=SceneEditorDocumentRevision();
    click(&editor,backToMenuButton);
    assert(SceneEditorLifecycleClosePending() && editor.running);
    capture(&editor,"workspace_close_confirmation.ppm");
    key(&editor,SDLK_RETURN);
    assert(!SceneEditorLifecycleClosePending() && editor.running);
    click(&editor,backToMenuButton);
    key(&editor,SDLK_ESCAPE);
    assert(!SceneEditorLifecycleClosePending() && editor.running);
    SDL_Event close_event={0}; close_event.type=SDL_WINDOWEVENT;
    close_event.window.windowID=SDL_GetWindowID(editor.window);
    close_event.window.event=SDL_WINDOWEVENT_CLOSE;
    SceneEditorSessionRuntimeHandleEvent(&editor,&close_event);
    assert(SceneEditorLifecycleClosePending() && editor.running);
    key(&editor,SDLK_ESCAPE);
    assert(!SceneEditorLifecycleClosePending() && editor.running);
    assert(SceneEditorDocumentRevision()==revision);

    /* Search captures typing/escape and does not alter selection or history. */
    SDL_Rect search_field={before.left_content_rect.x+12,before.left_content_rect.y+42,1,1};
    click(&editor,search_field);
    SDL_Event search_input={0}; search_input.type=SDL_TEXTINPUT;
    snprintf(search_input.text.text,sizeof(search_input.text.text),"no-such-object");
    SceneEditorSessionRuntimeHandleEvent(&editor,&search_input);
    SceneEditorSessionRuntimeRender(&editor);
    for (int y=before.left_content_rect.y+70;y<before.left_content_rect.y+before.left_content_rect.h;++y)
        assert(ObjectEditorObjectListIndexAtPoint(before.left_content_rect.x+30,y) < 0);
    SDL_Event select_all={0}; select_all.type=SDL_KEYDOWN;
    select_all.key.keysym.sym=SDLK_a; select_all.key.keysym.mod=KMOD_GUI;
    SceneEditorSessionRuntimeHandleEvent(&editor,&select_all);
    snprintf(search_input.text.text,sizeof(search_input.text.text),"#%d",selected);
    SceneEditorSessionRuntimeHandleEvent(&editor,&search_input);
    key(&editor,SDLK_ESCAPE);
    SceneEditorSessionRuntimeRender(&editor);
    ObjectEditorSetSelectedObjectIndex(0);
    bool picked=false;
    for (int y=before.left_content_rect.y+70;y<before.left_content_rect.y+before.left_content_rect.h;++y) {
        int x=before.left_content_rect.x+30;
        if (ObjectEditorObjectListIndexAtPoint(x,y)==selected) {
            click(&editor,(SDL_Rect){x,y,1,1}); picked=true; break;
        }
    }
    assert(picked && ObjectEditorGetSelectedObjectIndex()==selected);
    assert(SceneEditorDocumentRevision()==revision && editor.running);
    click(&editor,search_field);
    SceneEditorSessionRuntimeHandleEvent(&editor,&select_all);
    key(&editor,SDLK_ESCAPE);
    SceneEditorSessionRuntimeRender(&editor);

    SceneEditorWorkspaceChrome chrome;
    SceneEditorWorkspaceLayoutChrome(&before, &chrome);
    capture(&editor,"workspace_navigation.ppm");
    assert(!SceneEditorWorkspaceProfileMenuOpen());
    assert(SceneEditorDocumentRevision()==revision);
    for (int profile=0; profile<SCENE_WORKSPACE_PROFILE_COUNT; ++profile) {
        click(&editor, chrome.modes[profile]);
        assert((int)SceneEditorWorkspaceProfileGet() == profile);
        assert(SceneEditorDocumentRevision() == revision);
        assert(ObjectEditorGetSelectedObjectIndex() == selected);
        char capture_name[80]; snprintf(capture_name,sizeof(capture_name),"workspace_profile_%d.ppm",profile);
        capture(&editor,capture_name);
    }
    click(&editor, chrome.modes[SCENE_WORKSPACE_SCENE]);
    click(&editor, expandViewportButton);
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    assert(after.viewport_rect.w > before.viewport_rect.w);
    assert(SceneEditorDocumentRevision() == revision);
    assert(ObjectEditorGetSelectedObjectIndex() == selected);
    capture(&editor, "workspace_expanded.ppm");
    click(&editor, expandViewportButton);
    assert(SceneEditorGetPaneLayout(&after) && !after.viewport_expanded);
    assert(after.left_pane_rect.w == before.left_pane_rect.w);
    assert(after.right_pane_rect.w == before.right_pane_rect.w);
    click(&editor, restoreWorkspaceButton);
    assert(SceneEditorDocumentRevision() == revision);
    assert(SceneEditorDocumentIsDirty()==layout_dirty);
    SDL_SetWindowSize(editor.window, 1024, 640);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor, "workspace_compact.ppm");
    /* Exercise the actual inspector and Save action against the copied fixture. */
    assert(SceneEditorGetPaneLayout(&after));
    verify_viewport_gestures(&editor);
    SceneEditorDocumentTransform original, edited, reopened;
    char diagnostics[256];
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &original,
        diagnostics, sizeof(diagnostics)));
    SDL_Rect position_x = {after.right_content_rect.x,
        after.right_content_rect.y + 25 + 22, (after.right_content_rect.w - 8) / 3, 25};
    click(&editor, position_x);
    for (int i=0; i<32; ++i) key(&editor,SDLK_BACKSPACE);
    SDL_Event invalid_input={0}; invalid_input.type=SDL_TEXTINPUT;
    snprintf(invalid_input.text.text,sizeof(invalid_input.text.text),"--");
    unsigned long long invalid_revision=SceneEditorDocumentRevision();
    SceneEditorSessionRuntimeHandleEvent(&editor,&invalid_input);
    key(&editor,SDLK_RETURN);
    assert(SceneEditorDocumentRevision()==invalid_revision);
    assert(SceneEditorTransformPanelInteractionActive());
    key(&editor,SDLK_ESCAPE);
    assert(!SceneEditorTransformPanelInteractionActive() && editor.running);
    /* An invalid draft must not trap clicks in the inspector. Use a real
       workspace action to prove the outside click continues through routing. */
    click(&editor, position_x);
    for (int i=0; i<32; ++i) key(&editor,SDLK_BACKSPACE);
    SceneEditorSessionRuntimeHandleEvent(&editor,&invalid_input);
    assert(SceneEditorTransformPanelInteractionActive());
    click(&editor, expandViewportButton);
    assert(!SceneEditorTransformPanelInteractionActive());
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    assert(SceneEditorDocumentRevision()==invalid_revision);
    click(&editor, expandViewportButton);
    assert(SceneEditorGetPaneLayout(&after) && !after.viewport_expanded);
    click(&editor, position_x);
    SDL_Event focus_lost={0}; focus_lost.type=SDL_WINDOWEVENT;
    focus_lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    SceneEditorSessionRuntimeHandleEvent(&editor,&focus_lost);
    assert(!SceneEditorTransformPanelInteractionActive());
    click(&editor, position_x);
    for (int i = 0; i < 32; ++i) key(&editor, SDLK_BACKSPACE);
    SDL_Event text = {0};
    text.type = SDL_TEXTINPUT;
    snprintf(text.text.text, sizeof(text.text.text), "%.6g", original.position[0] + 0.25);
    SceneEditorSessionRuntimeHandleEvent(&editor, &text);
    key(&editor, SDLK_RETURN);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &edited,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(edited.position[0] - original.position[0] - 0.25) < 1e-6);
    assert(SceneEditorDocumentIsDirty());
    SceneEditorSessionRuntimeRender(&editor);
    SceneEditorWorkspaceLayoutChrome(&after,&chrome);
    SDL_Rect undo=chrome.undo;
    click(&editor, undo);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - original.position[0]) < 1e-6);
    SDL_Rect redo = chrome.redo;
    click(&editor, redo);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - edited.position[0]) < 1e-6);
    SceneEditorPaneLayout material_layout;
    assert(SceneEditorGetPaneLayout(&material_layout));
    click(&editor,(SDL_Rect){material_layout.left_content_rect.x+material_layout.left_content_rect.w*3/4,
        material_layout.left_content_rect.y+8,1,1});
    SDL_Rect scrollbar_bottom = {material_layout.left_content_rect.x + material_layout.left_content_rect.w - 8,
        material_layout.left_content_rect.y + material_layout.left_content_rect.h - 4, 2, 2};
    SceneEditorChromeShellSetActionFeedback(NULL,0);
    capture(&editor,"workspace_library_top.ppm");
    click(&editor, scrollbar_bottom);
    capture(&editor,"workspace_library_scrolled.ppm");
    int old_material = sceneSettings.sceneObjects[selected].material_id;
    bool material_clicked = false;
    for (int y=material_layout.left_content_rect.y; y<material_layout.left_content_rect.y+material_layout.left_content_rect.h; ++y) {
        int x=material_layout.left_content_rect.x+30;
        int candidate=ObjectEditorPanels_MaterialIndexAtPoint(x,y);
        if (candidate >= 0 && candidate != old_material) {
            click(&editor,(SDL_Rect){x,y,1,1});
            material_clicked = sceneSettings.sceneObjects[selected].material_id != old_material;
            if (material_clicked) break;
        }
    }
    assert(material_clicked);
    capture(&editor,"workspace_material_assignment.ppm");
    click(&editor, saveButton);
    assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(argv[2], diagnostics, sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - edited.position[0]) < 1e-6);
    capture(&editor, "workspace_saved_edit.ppm");
    verify_move_acceptance(&editor,argv[2],selected);
    verify_primitive_move_preview(&editor,selected);
    verify_transform_acceptance(&editor,argv[2],selected);
    verify_primitive_transform_parity(&editor,selected);
    verify_mesh_transform_parity(&editor,selected);
    revision = SceneEditorDocumentRevision();
    animSettings.textZoomStep = 2;
    SceneEditorWorkspaceProfileSelect(&editor, SCENE_WORKSPACE_SCENE);
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor,"workspace_large_text.ppm");
    assert(SceneEditorDocumentRevision() == revision);
    animSettings.textZoomStep = 0;
    SDL_SetWindowSize(editor.window,1440,900);
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor,"workspace_1440.ppm");
    SDL_SetWindowSize(editor.window,800,600);
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    capture(&editor,"workspace_narrow.ppm");

    assert(SceneEditorDocumentRevision()==revision);


    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&reopened,diagnostics,sizeof(diagnostics)));
    reopened.position[0]+=1.0;
    assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&reopened,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentIsDirty());
    SDL_SetWindowSize(editor.window,1280,800);
    SceneEditorSessionRuntimeRender(&editor);
    click(&editor,backToMenuButton);
    assert(SceneEditorLifecycleClosePending() && editor.running);
    SceneEditorPaneLayout close_layout;
    assert(SceneEditorGetPaneLayout(&close_layout));
    int dialog_width=close_layout.workspace_header_rect.w-32;
    if (dialog_width>580) dialog_width=580;
    int dialog_x=close_layout.workspace_header_rect.x+(close_layout.workspace_header_rect.w-dialog_width)/2;
    char unavailable_scene[4096];
    snprintf(unavailable_scene,sizeof(unavailable_scene),"%s.close-test-backup",argv[2]);
    assert(rename(argv[2],unavailable_scene)==0);
    click(&editor,(SDL_Rect){dialog_x+22,248,1,1});
    bool stayed_open=editor.running && !sceneEditorExitFlag && SceneEditorLifecycleClosePending();
    assert(rename(unavailable_scene,argv[2])==0);
    assert(stayed_open);
    capture(&editor,"workspace_close_save_failure.ppm");
    click(&editor,(SDL_Rect){dialog_x+22,248,1,1});
    assert(!editor.running && sceneEditorExitFlag && !SceneEditorDocumentIsDirty());
    active_editor = NULL;
    DestroySceneEditor(&editor);
    TTF_Quit();
    SDL_Quit();
    puts("Workspace source UI: expand/restore/resize preserves document revision and selection; inspector edit/undo/redo/save/reopen passed");
    return 0;
}
