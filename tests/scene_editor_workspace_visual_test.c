#include "editor/scene_editor_viewport_material.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_object_list.h"
#include "editor/scene_editor_rename.h"
#include "editor/material_editor_internal.h"
#include "editor/scene_editor_mesh_preview_render.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/object_editor_selection_tracker.h"
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

static void choose_menu(SceneEditor* editor,int menu,int row) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    assert(SceneEditorGetPaneLayout(&layout));
    SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    SDL_Rect anchor=menu==3 ? chrome.display_mode : menu<0 ? chrome.workspace : chrome.menus[menu];
    click(editor,anchor);
    assert(SceneEditorWorkspaceProfileMenuOpen());
    click(editor,(SDL_Rect){menu==3 ? anchor.x+anchor.w-140 : anchor.x,anchor.y+anchor.h+4+row*(anchor.h+4),menu==3 ? 140 : 230,anchor.h+4});
    assert(!SceneEditorWorkspaceProfileMenuOpen());
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

static void verify_display_modes(SceneEditor* editor) {
    unsigned long long revision=SceneEditorDocumentRevision();
    int selected=ObjectEditorGetSelectedObjectIndex();
    for(int mode=0;mode<SCENE_EDITOR_MESH_DISPLAY_COUNT;++mode) {
        choose_menu(editor,3,mode);
        assert((int)SceneEditorMeshPreviewModeGet()==mode);
        char name[80];snprintf(name,sizeof(name),"viewport_display_%d.ppm",mode);capture(editor,name);
        for(int workspace=0;workspace<SCENE_WORKSPACE_PROFILE_COUNT;++workspace) {
            choose_menu(editor,-1,workspace);
            assert((int)SceneEditorMeshPreviewModeGet()==mode);
            assert(SceneEditorDocumentRevision()==revision);
            assert(ObjectEditorGetSelectedObjectIndex()==selected);
        }
        choose_menu(editor,-1,SCENE_WORKSPACE_SCENE);
    }
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
}

static void rename_key(SceneEditor* editor, SDL_Keycode keycode, SDL_Keymod mod) {
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keycode;e.key.keysym.mod=mod;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
}
static void rename_text(SceneEditor* editor,const char* text) {
    SDL_Event e={0};e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof(e.text.text),"%s",text);
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
}
static void verify_rename(SceneEditor* editor) {
    SceneEditorDocumentObjectInfo before,after;
    assert(SceneEditorDocumentObjectById(ObjectEditorSelectionTrackerId(),&before));
    unsigned long long revision=SceneEditorDocumentRevision();
    SDL_Rect row;assert(SceneEditorObjectListRowRects(before.id,&row,NULL,NULL));
    SDL_Event dbl={0};dbl.type=SDL_MOUSEBUTTONDOWN;dbl.button.button=SDL_BUTTON_LEFT;dbl.button.clicks=2;
    dbl.button.x=row.x+10;dbl.button.y=row.y+row.h/2;
    SceneEditorSessionRuntimeHandleEvent(editor,&dbl);assert(SceneEditorRenameActive());
    capture(editor,"object_rename_selected.ppm");
    rename_key(editor,SDLK_ESCAPE,KMOD_NONE);
    rename_key(editor,SDLK_F2,KMOD_NONE);assert(SceneEditorRenameActive());
    rename_text(editor,"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    rename_text(editor,"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    rename_text(editor,"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    capture(editor,"object_rename_long.ppm");
    rename_key(editor,SDLK_HOME,KMOD_NONE);rename_key(editor,SDLK_END,KMOD_SHIFT);
    rename_text(editor,"Cancelled");rename_key(editor,SDLK_ESCAPE,KMOD_NONE);
    assert(!SceneEditorRenameActive() && SceneEditorDocumentRevision()==revision);
    rename_key(editor,SDLK_F2,KMOD_NONE);rename_text(editor,"Café 星");
    rename_key(editor,SDLK_LEFT,KMOD_NONE);rename_key(editor,SDLK_DELETE,KMOD_NONE);
    rename_key(editor,SDLK_BACKSPACE,KMOD_NONE);rename_key(editor,SDLK_BACKSPACE,KMOD_NONE);
    rename_text(editor,"é mirror");
    rename_key(editor,SDLK_HOME,KMOD_NONE);rename_key(editor,SDLK_RIGHT,KMOD_SHIFT);
    rename_text(editor,"c");
    capture(editor,"object_rename_caret.ppm");
    rename_key(editor,SDLK_RETURN,KMOD_NONE);assert(!SceneEditorRenameActive());
    assert(SceneEditorDocumentObjectById(before.id,&after) && strcmp(after.name,"café mirror")==0);
    char diagnostics[256];assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentObjectById(before.id,&after) && strcmp(after.name,before.name)==0);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentObjectById(before.id,&after) && strcmp(after.name,"café mirror")==0);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    rename_key(editor,SDLK_F2,KMOD_NONE);rename_key(editor,SDLK_BACKSPACE,KMOD_NONE);
    rename_key(editor,SDLK_RETURN,KMOD_NONE);assert(SceneEditorRenameActive());
    capture(editor,"object_rename_empty.ppm");
    rename_text(editor,"Replacement");rename_key(editor,SDLK_a,KMOD_GUI);rename_text(editor,"Final");
    rename_key(editor,SDLK_RETURN,KMOD_NONE);assert(!SceneEditorRenameActive());
    assert(SceneEditorDocumentObjectById(before.id,&after) && strcmp(after.name,"Final")==0);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    SceneEditorSessionRuntimeRender(editor);
}

static void verify_material_inspector(SceneEditor* editor) {
    unsigned long long revision=SceneEditorDocumentRevision();
    int selected=ObjectEditorGetSelectedObjectIndex();
    SceneEditorPaneLayout layout; assert(SceneEditorGetPaneLayout(&layout));
    assert(s_recipe_action_rects[0].x>=layout.right_content_rect.x);
    const MaterialEditorSubPane sections[]={MATERIAL_EDITOR_SUBPANE_RESPONSE,MATERIAL_EDITOR_SUBPANE_TEXTURES,
        MATERIAL_EDITOR_SUBPANE_STACK,MATERIAL_EDITOR_SUBPANE_FACE,MATERIAL_EDITOR_SUBPANE_GRAPH,MATERIAL_EDITOR_SUBPANE_PROOF};
    for(int i=0;i<6;++i) {
        /* Close the previous section so every navigation header remains reachable. */
        if(s_material_editor_section_open) click(editor,s_material_editor_compact_layout_rects.tab_rects[MaterialEditorGetActiveSubPane()]);
        assert(!s_material_editor_section_open);
        click(editor,s_material_editor_compact_layout_rects.tab_rects[sections[i]]);
        assert(s_material_editor_section_open && MaterialEditorGetActiveSubPane()==sections[i]);
        assert(ObjectEditorGetSelectedObjectIndex()==selected);
        assert(SceneEditorDocumentRevision()==revision);
        char name[80];snprintf(name,sizeof(name),"material_inspector_section_%d.ppm",i);capture(editor,name);
    }
    click(editor,s_material_editor_compact_layout_rects.tab_rects[MaterialEditorGetActiveSubPane()]);
    click(editor,s_material_editor_compact_layout_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_RESPONSE]);
    click(editor,s_recipe_action_rects[0]);
    assert(MaterialEditorGetRecipeMenuAxis()==MATERIAL_EDITOR_RECIPE_AXIS_FAMILY);
    capture(editor,"material_inspector_preset_menu.ppm");
    /* Dismissing the popup must not click the property beneath it. */
    click(editor,s_material_editor_compact_layout_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_RESPONSE]);
    assert(MaterialEditorGetRecipeMenuAxis()==MATERIAL_EDITOR_RECIPE_AXIS_NONE);
    assert(s_material_editor_section_open);
    click(editor,s_recipe_action_rects[0]);
    key(editor,SDLK_ESCAPE);
    assert(MaterialEditorGetRecipeMenuAxis()==MATERIAL_EDITOR_RECIPE_AXIS_NONE);
    bool found=false;
    for(int y=layout.left_content_rect.y;y<layout.left_content_rect.y+layout.left_content_rect.h;++y) {
        int x=layout.left_content_rect.x+30;
        int candidate=ObjectEditorObjectListIndexAtPoint(x,y);
        if(candidate>=0 && candidate!=selected) {
            click(editor,(SDL_Rect){x,y,1,1});
            assert(ObjectEditorGetSelectedObjectIndex()==candidate);
            assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_MATERIALS);
            found=true;break;
        }
    }
    assert(found);
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorDocumentRevision()==revision);
}

static void verify_viewport_material(SceneEditor* editor) {
    int count=sceneSettings.objectCount;
    SceneObject* saved=malloc(sizeof(SceneObject)*count);assert(saved);
    memcpy(saved,sceneSettings.sceneObjects,sizeof(SceneObject)*count);
    RuntimeMaterialTextureStack* stacks=calloc(count,sizeof(*stacks));bool* has=calloc(count,sizeof(*has));assert(stacks && has);
    SceneEditorWorkspaceProfile profile=SceneEditorWorkspaceProfileGet();
    SceneEditorMeshDisplayMode display=SceneEditorMeshPreviewModeGet();
    SceneEditorDigestOverlayNavState nav=*SceneEditorGetViewportNavState();
    for(int i=0;i<count;++i) {
        has[i]=SceneEditorMaterialStackGetObjectStack(i,&stacks[i]);
        SceneObject* o=&sceneSettings.sceneObjects[i];o->color=0xc2a780;o->hasMirrorResponseOverride=false;
        o->material_id=i%3==0 ? MATERIAL_PRESET_MIRROR : i%3==1 ? MATERIAL_PRESET_ROUGH_METAL : MATERIAL_PRESET_DEFAULT;
        o->reflectivity=i%3==0 ? 0.98 : i%3==1 ? 0.75 : 0.03;o->roughness=i%3==0 ? 0.02 : i%3==1 ? 0.55 : 0.85;
        RuntimeMaterialTextureStack stack=RuntimeMaterialTextureStackEmpty();stack.layerCount=1;
        stack.layers[0]=RuntimeMaterialTextureLayerMakeBase(i%3==1 ? RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL : RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
        assert(SceneEditorMaterialStackSetObjectStack(i,&stack));
    }
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_SOLID);
    SceneEditorSessionRuntimeRender(editor);
    Uint64 solid_start=SDL_GetPerformanceCounter();
    for(int i=0;i<12;++i) {SceneEditorDigestOverlayNavState orbit=nav;orbit.orbit_yaw_deg+=i*2;SceneEditorRestoreViewportNav(&orbit);SceneEditorSessionRuntimeRender(editor);}
    fprintf(stderr,"SOLID ORBIT average_ms=%.3f\n",(SDL_GetPerformanceCounter()-solid_start)*1000.0/SDL_GetPerformanceFrequency()/12);
    SceneEditorRestoreViewportNav(&nav);
    capture(editor,"viewport_solid_comparison.ppm");
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"viewport_material_studio.ppm");
    unsigned long long builds=SceneEditorViewportMaterialBuildCount();
    Uint64 start=SDL_GetPerformanceCounter();
    for(int i=0;i<12;++i) {SceneEditorDigestOverlayNavState orbit=nav;orbit.orbit_yaw_deg+=i*2;SceneEditorRestoreViewportNav(&orbit);SceneEditorSessionRuntimeRender(editor);}
    double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency()/12;
    fprintf(stderr,"MATERIAL ORBIT average_ms=%.3f material_bakes=%llu\n",ms,SceneEditorViewportMaterialBuildCount()-builds);
    assert(SceneEditorViewportMaterialBuildCount()==builds);
    capture(editor,"viewport_material_orbit.ppm");
    SceneEditorRestoreViewportNav(&nav);
    for(int i=0;i<5;++i) {
        SceneEditorWorkspaceProfileSelect(editor,(SceneEditorWorkspaceProfile)i);
        assert(SceneEditorMeshPreviewModeGet()==SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
        char filename[80];snprintf(filename,sizeof(filename),"viewport_material_workspace_%d.ppm",i);capture(editor,filename);
    }
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    RuntimeMaterialTextureStack pattern=RuntimeMaterialTextureStackEmpty();pattern.layerCount=1;
    pattern.layers[0]=RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    assert(SceneEditorMaterialStackSetObjectStack(0,&pattern));
    capture(editor,"viewport_material_wood.ppm");
    pattern.layers[0]=RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK);
    assert(SceneEditorMaterialStackSetObjectStack(0,&pattern));
    capture(editor,"viewport_material_brick.ppm");
    /* Editing a response with the camera stationary must invalidate the cached frame. */
    sceneSettings.sceneObjects[0].roughness=0.8;SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorViewportMaterialBuildCount()>builds);
    for(int i=0;i<count;++i) {if(has[i]) SceneEditorMaterialStackSetObjectStack(i,&stacks[i]);else SceneEditorMaterialStackClearObjectStack(i);}
    memcpy(sceneSettings.sceneObjects,saved,sizeof(SceneObject)*count);free(saved);free(stacks);free(has);
    SceneEditorRestoreViewportNav(&nav);SceneEditorWorkspaceProfileSelect(editor,profile);SceneEditorMeshPreviewModeSet(display);
    SceneEditorSessionRuntimeRender(editor);
}

static void verify_geometry_selection(SceneEditor* editor) {
    SceneEditorDigestOverlayNavState saved=*SceneEditorGetViewportNavState(), nav=saved;
    int selected=ObjectEditorGetSelectedObjectIndex();
    SceneEditorObjectTransformMode saved_tool=SceneEditorObjectTransformModeGet();
    bool saved_select_only=SceneEditorObjectSelectionOnly();
    unsigned long long revision=SceneEditorDocumentRevision();
    nav.orbit_yaw_deg=0;nav.orbit_pitch_deg=0;nav.overlay_zoom=1;
    SceneEditorRestoreViewportNav(&nav);
    SceneEditorPaneLayout layout; RuntimeSceneBridge3DDigestState digest;
    SceneEditorDigestOverlayProjector projector;
    assert(SceneEditorGetPaneLayout(&layout));
    assert(SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorViewportNavFitDigestOverlayForTarget(&nav,&layout.viewport_rect,false,EDITOR_MODE_OBJECT,-1));
    SceneEditorRestoreViewportNav(&nav);
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,&nav,&projector));
    /* Known fixture geometry, not coordinates obtained from the picker under test.
       Sphere centers overlap the floor; the frontmost surface must win. */
    const double points[][3]={{-2,.3,.75},{0,.3,.75},{2,.3,.75},{3,-2,0}};
    const int expected[]={3,4,5,0};
    for(int mode=0;mode<2;++mode) {
        ObjectEditorSetSelectedObjectIndex(-1);
        ObjectEditorSelectionTrackerReset();
        choose_menu(editor,-1,mode ? 1 : 0);
        assert(ObjectEditorGetSelectedObjectIndex()==-1);
        if(mode) assert(MaterialEditorResolveFocusedObjectIndex()==-1);
        SceneEditorRestoreViewportNav(&nav);
        if(!mode) {
            SceneEditorWorkspaceChrome pick_chrome;SceneEditorWorkspaceLayoutChrome(&layout,&pick_chrome);
            click(editor,pick_chrome.actions[0]);
        }
        for(int i=0;i<4;++i) {
            int x,y;assert(SceneEditorDigestOverlayProjectPoint(&projector,points[i][0],points[i][1],points[i][2],&x,&y));
            click(editor,(SDL_Rect){x,y,1,1});
            fprintf(stderr,"PICK mode=%d sample=%d xy=%d,%d expected=%d actual=%d query=%d\n",mode,i,x,y,expected[i],ObjectEditorGetSelectedObjectIndex(),SceneEditorViewportPickObject(&projector,x,y));
            assert(ObjectEditorGetSelectedObjectIndex()==expected[i]);
            assert(SceneEditorDocumentRevision()==revision);
        }
        capture(editor,mode ? "viewport_material_pick.ppm" : "viewport_scene_pick.ppm");
        /* An empty corner must not reuse the preceding hover/selection. */
        click(editor,(SDL_Rect){layout.viewport_rect.x+2,layout.viewport_rect.y+2,1,1});
        assert(ObjectEditorGetSelectedObjectIndex()==-1);
    }
    /* Looking from below, the floor occludes the same sphere: primitive and mesh
       hits must compete by depth instead of always preferring meshes. */
    projector.pitch_rad=3.141592653589793;
    int bx,by;assert(SceneEditorDigestOverlayProjectPoint(&projector,2,.3,.75,&bx,&by));
    assert(SceneEditorViewportPickObject(&projector,bx,by)==0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    SceneEditorObjectTransformModeSet(saved_tool);
    if(saved_select_only) SceneEditorObjectSelectTool();
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorRestoreViewportNav(&saved);
    SceneEditorSessionRuntimeRender(editor);
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
    choose_menu(editor,2,0);
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
        char picked_id[128];assert(runtime_scene_bridge_get_last_object_id_for_scene_index(i,picked_id,sizeof(picked_id)));
        assert(strcmp(ObjectEditorSelectionTrackerId(),picked_id)==0);
    }
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorSessionRuntimeRender(editor);
}

#include "scene_editor_move_acceptance.h"
#include "scene_editor_transform_acceptance.h"
#include "scene_editor_selection_acceptance.h"

#include "scene_editor_material_parity_probe.h"
#include "scene_editor_surface_mapping_m1.h"
#include "scene_editor_surface_mapping_m2.h"

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
    if(argc==4 && !strcmp(argv[3],"--mapping-panel")) {
        surface_mapping_panel_probe(&editor);DestroySceneEditor(&editor);TTF_Quit();SDL_Quit();return 0;
    }
    if(argc==4 && (!strcmp(argv[3],"--mapping-m2") || !strcmp(argv[3],"--mapping-m2-reopen"))) {
        surface_mapping_m2_probe(&editor,!strcmp(argv[3],"--mapping-m2-reopen"));
        DestroySceneEditor(&editor);TTF_Quit();SDL_Quit();return 0;
    }
    if(argc==4 && (!strcmp(argv[3],"--mapping-m1") || !strcmp(argv[3],"--mapping-m1-reopen"))) {
        surface_mapping_m1_probe(&editor,!strcmp(argv[3],"--mapping-m1-reopen"));
        DestroySceneEditor(&editor);TTF_Quit();SDL_Quit();return 0;
    }
    if(argc==4 && (strcmp(argv[3],"--material-parity")==0 || strcmp(argv[3],"--material-edit-proof")==0)) {
        material_parity_probe(&editor,strcmp(argv[3],"--material-edit-proof")==0);
        DestroySceneEditor(&editor);TTF_Quit();SDL_Quit();return 0;
    }
    if(argc==4 && strcmp(argv[3],"--u23-flags")==0) {
        SceneEditorDocumentObjectInfo info;bool found=false;
        for(int i=0;i<SceneEditorDocumentObjectCount();++i) {
            assert(SceneEditorDocumentObjectAt(i,&info));
            if(strcmp(info.name,"U2.3 review mesh")==0) {
                assert(!info.visible && info.locked && info.runtime_index==-1);
                assert(ObjectEditorSelectionTrackerSelectId(info.id));found=true;break;
            }
        }
        assert(found && !SceneEditorDocumentIsDirty());
        capture(&editor,"workspace_u23_fresh_flags.ppm");
        DestroySceneEditor(&editor);active_editor=NULL;TTF_Quit();SDL_Quit();return 0;
    }
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
        choose_menu(&editor,0,1);
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
            import_layout.right_content_rect.y + 48 + 23 + 2*29 + 8,1,1});
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
        choose_menu(&editor,0,0);
    }
    ObjectEditorSetSelectedObjectIndex(argc == 4 ? sceneSettings.objectCount - 1 : 0);
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorGetPaneLayout(&before));
    SceneEditorViewportNavFitDigestOverlayForTarget((SceneEditorDigestOverlayNavState*)SceneEditorGetViewportNavState(),
        &before.viewport_rect, true, EDITOR_MODE_OBJECT, 0);
    unsigned long long revision = SceneEditorDocumentRevision();
    int selected = ObjectEditorGetSelectedObjectIndex();
    verify_display_modes(&editor);
    verify_geometry_selection(&editor);
    verify_viewport_gestures(&editor);
    capture(&editor, "workspace_scene.ppm");
    key(&editor,SDLK_ESCAPE);
    assert(editor.running && !sceneEditorExitFlag);
    SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_ADD);
    key(&editor,SDLK_ESCAPE);
    assert(SceneEditorToolStateGetActive()==SCENE_EDITOR_TOOL_SELECT && editor.running);
    assert(SceneEditorMeshPreviewStoreHasSceneObject(selected));
    int material_before=sceneSettings.sceneObjects[selected].material_id;
    SDL_Rect material_field={before.right_content_rect.x+25,before.right_content_rect.y+48+23+5*29+66+8,1,1};
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
    verify_material_inspector(&editor);
    verify_rename(&editor);
    verify_viewport_material(&editor);
    assert(ray_tracing_shared_theme_set_preset("soft_light"));
    capture(&editor,"material_surfaces_light.ppm");
    assert(ray_tracing_shared_theme_set_preset("standard_grey"));
    capture(&editor,"material_surfaces_grey.ppm");
    assert(ray_tracing_shared_theme_set_preset("midnight_contrast"));
    capture(&editor,"material_surfaces_dark.ppm");
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
    choose_menu(&editor,-1,SCENE_WORKSPACE_SCENE);
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_SCENE);
    assert(ObjectEditorGetSelectedObjectIndex()==selected);
    assert(memcmp(&before_material,SceneEditorGetViewportNavState(),sizeof(before_material))==0);
    bool layout_dirty=SceneEditorDocumentIsDirty();
    revision=SceneEditorDocumentRevision();
    choose_menu(&editor,0,1);
    assert(SceneEditorLifecycleClosePending() && editor.running);
    capture(&editor,"workspace_close_confirmation.ppm");
    key(&editor,SDLK_RETURN);
    assert(!SceneEditorLifecycleClosePending() && editor.running);
    choose_menu(&editor,0,1);
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
    runtime_scene_bridge_get_last_object_id_for_scene_index(selected,search_input.text.text,sizeof(search_input.text.text));
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
    {
        SceneEditorPaneLayout menu_layout; SceneEditorWorkspaceChrome menu_chrome;
        assert(SceneEditorGetPaneLayout(&menu_layout));
        SceneEditorWorkspaceLayoutChrome(&menu_layout,&menu_chrome);
        unsigned long long menu_revision=SceneEditorDocumentRevision();
        int menu_selection=ObjectEditorGetSelectedObjectIndex();
        click(&editor,menu_chrome.workspace);
        capture(&editor,"workspace_selector_menu.ppm");
        key(&editor,SDLK_ESCAPE);
        assert(!SceneEditorWorkspaceProfileMenuOpen());
        click(&editor,menu_chrome.menus[2]);
        capture(&editor,"workspace_view_menu.ppm");
        click(&editor,menu_layout.viewport_rect);
        assert(!SceneEditorWorkspaceProfileMenuOpen());
        assert(ObjectEditorGetSelectedObjectIndex()==menu_selection);
        assert(SceneEditorDocumentRevision()==menu_revision);
        click(&editor,menu_chrome.workspace);
        key(&editor,SDLK_DOWN); key(&editor,SDLK_RETURN);
        assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_MATERIALS);
        SceneEditorSessionRuntimeRender(&editor);
        click(&editor,menu_chrome.context_views[1]);
        assert(MaterialEditorGetViewMode()==MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
        click(&editor,menu_chrome.context_views[0]);
        assert(MaterialEditorGetViewMode()==MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT);
        choose_menu(&editor,-1,SCENE_WORKSPACE_SCENE);
        assert(SceneEditorDocumentRevision()==menu_revision);
        assert(ObjectEditorGetSelectedObjectIndex()==menu_selection);
    }
    capture(&editor,"workspace_navigation.ppm");
    assert(!SceneEditorWorkspaceProfileMenuOpen());
    assert(SceneEditorDocumentRevision()==revision);
    for (int profile=0; profile<SCENE_WORKSPACE_PROFILE_COUNT; ++profile) {
        choose_menu(&editor,-1,profile);
        assert((int)SceneEditorWorkspaceProfileGet() == profile);
        assert(SceneEditorDocumentRevision() == revision);
        assert(ObjectEditorGetSelectedObjectIndex() == selected);
        char capture_name[80]; snprintf(capture_name,sizeof(capture_name),"workspace_profile_%d.ppm",profile);
        capture(&editor,capture_name);
    }
    choose_menu(&editor,-1,SCENE_WORKSPACE_SCENE);
    choose_menu(&editor,2,2);
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    assert(after.viewport_rect.w > before.viewport_rect.w);
    assert(SceneEditorDocumentRevision() == revision);
    assert(ObjectEditorGetSelectedObjectIndex() == selected);
    capture(&editor, "workspace_expanded.ppm");
    choose_menu(&editor,2,2);
    assert(SceneEditorGetPaneLayout(&after) && !after.viewport_expanded);
    assert(after.left_pane_rect.w == before.left_pane_rect.w);
    assert(after.right_pane_rect.w == before.right_pane_rect.w);
    choose_menu(&editor,2,3);
    assert(SceneEditorDocumentRevision() == revision);
    assert(SceneEditorDocumentIsDirty()==layout_dirty);
    SDL_SetWindowSize(editor.window, 1024, 640);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor, "workspace_compact.ppm");
    choose_menu(&editor,-1,SCENE_WORKSPACE_MATERIALS);
    MaterialEditorSetActiveSubPane(MATERIAL_EDITOR_SUBPANE_RESPONSE);
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor,"material_inspector_compact.ppm");
    choose_menu(&editor,-1,SCENE_WORKSPACE_SCENE);
    /* Exercise the actual inspector and Save action against the copied fixture. */
    assert(SceneEditorGetPaneLayout(&after));
    verify_geometry_selection(&editor);
    verify_viewport_gestures(&editor);
    SceneEditorDocumentTransform original, edited, reopened;
    char diagnostics[256];
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &original,
        diagnostics, sizeof(diagnostics)));
    SDL_Rect position_x = {after.right_content_rect.x,
        after.right_content_rect.y + 48 + 25 + 22, (after.right_content_rect.w - 8) / 3, 25};
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
    choose_menu(&editor,2,2);
    assert(!SceneEditorTransformPanelInteractionActive());
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    assert(SceneEditorDocumentRevision()==invalid_revision);
    choose_menu(&editor,2,2);
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
    choose_menu(&editor,1,0);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - original.position[0]) < 1e-6);
    choose_menu(&editor,1,1);
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
    choose_menu(&editor,0,0);
    assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(argv[2], diagnostics, sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - edited.position[0]) < 1e-6);
    capture(&editor, "workspace_saved_edit.ppm");
    verify_u23_selection(&editor,argv[2],selected);
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
    SceneEditorWorkspaceProfileSelect(&editor,SCENE_WORKSPACE_MATERIALS);
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor,"material_inspector_large_text.ppm");
    SceneEditorWorkspaceProfileSelect(&editor,SCENE_WORKSPACE_SCENE);
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
    choose_menu(&editor,0,1);
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
