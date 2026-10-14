/* Explicit native qualification on a task-owned copied scene, never an attached
 * user document. The production command and canvas owners remain under test. */
#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_scene_commands.h"
static void shared_editor_mouse(SceneEditor *editor,Uint32 type,SDL_Rect area,Uint32 id) {
    SDL_Event e={0};e.type=type;e.button.windowID=id;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=area.x+area.w/2;e.button.y=area.y+area.h/2;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
}
static void shared_editor_acceptance(SceneEditor *editor,const char *scene) {
    for(int i=0;i<3;i++){SDL_Delay(30);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);}
    Uint32 id=SDL_GetWindowID(editor->window);SceneEditorPaneLayout layout;SceneEditorWorkspaceChrome chrome;
    assert(SceneEditorGetPaneLayout(&layout));SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    shared_editor_mouse(editor,SDL_MOUSEBUTTONDOWN,chrome.menus[0],id);
    assert(!SceneEditorWorkspaceProfileMenuOpen()); /* Commands wait for release. */
    shared_editor_mouse(editor,SDL_MOUSEBUTTONUP,layout.viewport_rect,id);
    assert(!SceneEditorWorkspaceProfileMenuOpen());
    shared_editor_mouse(editor,SDL_MOUSEBUTTONDOWN,chrome.menus[0],id+100);
    shared_editor_mouse(editor,SDL_MOUSEBUTTONUP,chrome.menus[0],id+100);
    assert(!SceneEditorWorkspaceProfileMenuOpen());
    shared_editor_mouse(editor,SDL_MOUSEBUTTONDOWN,chrome.menus[0],id);
    SDL_Event invalid={0};invalid.type=SDL_WINDOWEVENT;invalid.window.windowID=id;invalid.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    SceneEditorSessionRuntimeHandleEvent(editor,&invalid);
    shared_editor_mouse(editor,SDL_MOUSEBUTTONUP,chrome.menus[0],id);
    assert(!SceneEditorWorkspaceProfileMenuOpen());SceneEditorSessionRuntimeRender(editor);
    key(editor,SDLK_F6);key(editor,SDLK_RETURN);assert(!SceneEditorWorkspaceProfileMenuOpen());
    SDL_Event release={0};release.type=SDL_KEYUP;release.key.keysym.sym=SDLK_RETURN;
    SceneEditorSessionRuntimeHandleEvent(editor,&release);assert(SceneEditorWorkspaceProfileMenuOpen());
    capture(editor,"shared_editor_keyboard_menu.ppm");key(editor,SDLK_ESCAPE);SceneEditorSessionRuntimeRender(editor);
    /* A semantic draft has composition, atomic bounded edits, scalar boundaries,
     * and clipped native presentation, without committing a document revision. */
    unsigned long long revision=SceneEditorDocumentRevision();
    char buffer[12]="Café";SceneEditorSharedText draft;
    scene_editor_shared_text_bind(&draft,buffer,sizeof(buffer),KIT_UI_TEXT_SINGLE_LINE);
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_END;
    assert(scene_editor_shared_text_event(&draft,&e).consumed);
    e=(SDL_Event){0};e.type=SDL_TEXTEDITING;snprintf(e.edit.text,sizeof(e.edit.text),"星");
    assert(scene_editor_shared_text_event(&draft,&e).consumed && !strcmp(buffer,"Café"));
    VkCommandBuffer cmd;VkFramebuffer framebuffer;VkExtent2D extent;
    assert(vk_renderer_begin_frame((VkRenderer*)editor->renderer,&cmd,&framebuffer,&extent)==VK_SUCCESS);
    scene_editor_shared_text_draw(&draft,editor->renderer,(SDL_Rect){layout.viewport_rect.x+20,layout.viewport_rect.y+20,90,32},(SDL_Color){255,255,255,255});
    assert(draft.ready && draft.view.composition_bytes==3);
    assert(vk_renderer_end_frame((VkRenderer*)editor->renderer,cmd)==VK_SUCCESS);
    e=(SDL_Event){0};e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof(e.text.text)," 星");
    assert(scene_editor_shared_text_event(&draft,&e).changed && !strcmp(buffer,"Café 星"));
    snprintf(e.text.text,sizeof(e.text.text),"overflow");assert(scene_editor_shared_text_event(&draft,&e).status.code!=CORE_OK && !strcmp(buffer,"Café 星"));
    e=(SDL_Event){0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_BACKSPACE;
    assert(scene_editor_shared_text_event(&draft,&e).changed && !strcmp(buffer,"Café "));
    assert(SceneEditorDocumentRevision()==revision);
    ObjectEditorSetSelectedObjectIndex(0);SceneEditorSessionRuntimeRender(editor);
    verify_viewport_gestures_for_fixture(editor,false);verify_rename(editor);
    assert(SceneEditorDocumentRevision()>revision);
    SceneEditorSceneDialogOpen("open");rename_key(editor,SDLK_a,KMOD_GUI);rename_text(editor,"/nonexistent/shared-ui-proof.json");rename_key(editor,SDLK_RETURN,KMOD_NONE);
    assert(SceneEditorSceneDialogActive() && !strcmp(SceneEditorDocumentPath(),scene));
    capture(editor,"shared_editor_path_failure.ppm");rename_key(editor,SDLK_ESCAPE,KMOD_NONE);
    assert(!SceneEditorSceneDialogActive());
    /* Save uses the original document command; fresh process readback is tested
     * by the separate lifecycle acceptance. */
    assert(SceneEditorChromeActionsSaveAuthoring());assert(!SceneEditorDocumentIsDirty());
    assert(ray_tracing_shared_theme_set_preset("soft_light"));capture(editor,"shared_editor_light.ppm");
    assert(ray_tracing_shared_theme_set_preset("midnight_contrast"));
    SDL_SetWindowSize(editor->window,900,600);
    for(int i=0;i<3;i++){SDL_Delay(40);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);}
    capture(editor,"shared_editor_compact.ppm");
    SceneEditorSharedUiReset(); /* Session reuse cannot inherit an armed control. */
    puts("shared editor release/focus/window scope, F6 traversal, UTF-8/IME/overflow, rename undo/redo, path failure and save: PASS");
}
