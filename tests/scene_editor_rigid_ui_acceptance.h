/* Native captures use the real editor and caller-owned copied scene/runtime. */
#include "../src/editor/scene_editor_rigid_panel.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document_rigid.h"
#include "../src/editor/scene_editor_rigid_job.h"
#include "render/vk_shared_device.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
static void rigid_ui_validation_begin(void) {
    SDL_Window *seed=SDL_CreateWindow("UI validation device",0,0,64,64,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);assert(seed);
    VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_TRUE;
    assert(vk_shared_device_init(seed,&cfg));SDL_DestroyWindow(seed);
}

static void rigid_ui_settle(SceneEditor *editor) {
    for(int i=0;i<4;++i){SDL_Delay(35);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);}
}
static void rigid_ui_baseline(SceneEditor *editor) {
    capture(editor,"ordinary.ppm");
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);
    assert(SceneEditorTimelineActivate());rigid_ui_settle(editor);
    capture(editor,"rigid-normal.ppm");
    SDL_SetWindowSize(editor->window,800,700);rigid_ui_settle(editor);
    capture(editor,"rigid-narrow.ppm");
}

static SDL_Rect rigid_ui_control(const char *name) {SDL_Rect r;bool found=SceneEditorRigidPanelControl(name,&r);if(!found)fprintf(stderr,"Missing rigid control: %s workspace=%d\n",name,SceneEditorWorkspaceProfileGet());assert(found);return r;}
static void rigid_ui_pair(SceneEditor *e,SDL_Rect r) {
    Uint32 id=SDL_GetWindowID(e->window);
    shared_editor_mouse(e,SDL_MOUSEBUTTONDOWN,r,id);shared_editor_mouse(e,SDL_MOUSEBUTTONUP,r,id);
    rigid_ui_settle(e);
}
static void rigid_ui_acceptance(SceneEditor *e) {
    /* Equal raster sizes at different logical sizes must not alias ownership.
     * At2x, logical13 and a renderer-independent logical26 both load26pt. */
    TTF_Font *small=ray_tracing_font_runtime_get_ui_regular(e->renderer,13,10);
    TTF_Font *large=ray_tracing_font_runtime_get_ui_regular(NULL,26,10);
    assert(small && large && small!=large);
    int sw=0,lw=0;assert(ray_tracing_text_measure_utf8(e->renderer,small,"Camera",&sw,NULL));
    assert(ray_tracing_text_measure_utf8(e->renderer,large,"Camera",&lw,NULL));
    assert(lw>sw*1.8 && lw<sw*2.2);

    capture(e,"ordinary.ppm");
    SceneEditorWorkspaceProfileSelect(e,SCENE_WORKSPACE_RENDER);assert(SceneEditorTimelineActivate());rigid_ui_settle(e);
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(!SceneEditorRigidPanelDetailsVisible());
    capture(e,"rigid-normal.ppm");
    SDL_Rect details=rigid_ui_control("rigid_details");Uint32 id=SDL_GetWindowID(e->window);
    shared_editor_mouse(e,SDL_MOUSEBUTTONDOWN,details,id);assert(!SceneEditorRigidPanelDetailsVisible());
    shared_editor_mouse(e,SDL_MOUSEBUTTONUP,(SDL_Rect){details.x+details.w+20,details.y,1,1},id);
    assert(!SceneEditorRigidPanelDetailsVisible());rigid_ui_settle(e);
    /* Direct legacy handler calls cannot bypass the accepted shared action. */
    SDL_Event raw={0};raw.type=SDL_MOUSEBUTTONDOWN;raw.button.button=SDL_BUTTON_LEFT;raw.button.x=details.x+2;raw.button.y=details.y+2;
    (void)SceneEditorRigidPanelEvent(&raw);assert(!SceneEditorRigidPanelDetailsVisible());
    rigid_ui_pair(e,details);assert(SceneEditorRigidPanelDetailsVisible());capture(e,"rigid-details.ppm");
    rigid_ui_pair(e,rigid_ui_control("rigid_details"));assert(!SceneEditorRigidPanelDetailsVisible());
    /* Actual F6 focus traversal, followed by release-only keyboard activation. */
    key(e,SDLK_F6);
    for(int i=0;i<80 && !SceneEditorSharedUiFocusIs(212,5);i++) key(e,SDLK_F6);
    assert(SceneEditorSharedUiFocusIs(212,5));
    key(e,SDLK_RETURN);assert(!SceneEditorRigidPanelDetailsVisible());
    SDL_Event up={0};up.type=SDL_KEYUP;up.key.windowID=id;up.key.keysym.sym=SDLK_RETURN;
    SceneEditorSessionRuntimeHandleEvent(e,&up);assert(SceneEditorRigidPanelDetailsVisible());rigid_ui_settle(e);
    rigid_ui_pair(e,rigid_ui_control("rigid_details"));assert(!SceneEditorRigidPanelDetailsVisible());
    fprintf(stderr,"Rigid keyboard stage passed\n");
    /* Window and scope invalidation swallow an outstanding release. */
    details=rigid_ui_control("rigid_details");shared_editor_mouse(e,SDL_MOUSEBUTTONDOWN,details,id);
    SDL_Event lost={0};lost.type=SDL_WINDOWEVENT;lost.window.windowID=id;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    SceneEditorSessionRuntimeHandleEvent(e,&lost);shared_editor_mouse(e,SDL_MOUSEBUTTONUP,details,id);
    assert(!SceneEditorRigidPanelDetailsVisible());rigid_ui_settle(e);
    shared_editor_mouse(e,SDL_MOUSEBUTTONDOWN,rigid_ui_control("rigid_details"),id);
    SDL_SetWindowSize(e->window,800,700);rigid_ui_settle(e);
    shared_editor_mouse(e,SDL_MOUSEBUTTONUP,details,id);
    assert(!SceneEditorRigidPanelDetailsVisible());rigid_ui_settle(e);capture(e,"rigid-compact-view.ppm");
    SDL_Rect absent;assert(!SceneEditorRigidPanelControl("rigid_details",&absent));
    SDL_SetWindowSize(e->window,1024,800);rigid_ui_settle(e);
    assert(scene_editor_pane_host_set_viewport_expanded(SceneEditorGetPaneHost(),false));rigid_ui_settle(e);capture(e,"rigid-narrow.ppm");
    (void)rigid_ui_control("rigid_details");
    assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
    rigid_ui_pair(e,rigid_ui_control("rigid_validate"));assert(SceneEditorRigidPanelFeedback()[0]);
    assert(!SceneEditorDocumentIsDirty());
    /* Native picker ownership disables these actions and cancellation is local. */
    rigid_ui_pair(e,rigid_ui_control("rigid_choose"));assert(SceneEditorRigidPanelPickerActive());
    rigid_ui_pair(e,rigid_ui_control("rigid_details"));assert(!SceneEditorRigidPanelDetailsVisible());
    SceneEditorRigidPanelReset();assert(!SceneEditorRigidPanelPickerActive());rigid_ui_settle(e);
    /* An actual edit disables Run; disabled pointer and keyboard never start work. */
    int target=sceneSettings.objectCount-1;char d[512];
    SDL_Rect held=rigid_ui_control("rigid_details");shared_editor_mouse(e,SDL_MOUSEBUTTONDOWN,held,id);
    assert(SceneEditorDocumentRenameForSceneIndex(target,"UI transient label",d,sizeof(d)));
    shared_editor_mouse(e,SDL_MOUSEBUTTONUP,held,id);assert(!SceneEditorRigidPanelDetailsVisible());rigid_ui_settle(e);
    rigid_ui_pair(e,rigid_ui_control("rigid_run"));assert(!SceneEditorRigidJobBusy());
    for(int i=0;i<80;i++) {key(e,SDLK_F6);assert(!SceneEditorSharedUiFocusIs(212,4));}
    assert(SceneEditorDocumentUndo(d,sizeof(d)));rigid_ui_settle(e);
    /* Undo retains the document owner's dirty flag until explicit Save. */
    /* Save uses the existing owner on this isolated scene, then fresh reopen. */
    rigid_ui_pair(e,rigid_ui_control("rigid_save"));assert(strstr(SceneEditorRigidPanelFeedback(),"Saved scene"));
    char path[4096];snprintf(path,sizeof(path),"%s",SceneEditorDocumentPath());
    assert(SceneEditorDocumentSwitch(path,false,d,sizeof(d)));assert(SceneEditorDocumentValidateRigidResult(d,sizeof(d)));
    assert(SceneEditorTimelineActivate() && SceneEditorTimelineSeek(154));
    static RayEvaluatedSceneSnapshot before,after;assert(SceneEditorTimelineCopyEvaluated(&before));
    SceneEditorWorkspaceProfileSelect(e,SCENE_WORKSPACE_SCENE);rigid_ui_settle(e);
    SDL_Rect hidden;assert(!SceneEditorRigidPanelControl("rigid_details",&hidden));
    SceneEditorWorkspaceProfileSelect(e,SCENE_WORKSPACE_RENDER);rigid_ui_settle(e);
    assert(SceneEditorTimelineCopyEvaluated(&after));
    assert(before.object_transform_count==after.object_transform_count);
    assert(!memcmp(before.object_transforms,after.object_transforms,before.object_transform_count*sizeof(before.object_transforms[0])));
    capture(e,"rigid-reopened.ppm");
    puts("Rigid UI production-route release/outside/keyboard/window/resize/disabled/save/reopen/playback: PASS");
}
