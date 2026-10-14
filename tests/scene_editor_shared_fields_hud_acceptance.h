#include "render/font_runtime.h"
#include "editor/scene_editor_shared_fields.h"
#include "render/runtime_native_3d_progress_hud_paint.h"
static void shared_fields_hud_acceptance(SceneEditor *editor) {
    unsigned long long revision=SceneEditorDocumentRevision();
    char value[16]="123";scene_editor_shared_field_begin(value,sizeof(value),1);
    SDL_Event event={0};event.type=SDL_TEXTINPUT;snprintf(event.text.text,sizeof(event.text.text),"12.5");
    assert(scene_editor_shared_field_event(value,sizeof(value),&event) && !strcmp(value,"12.5"));
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_HOME;
    assert(scene_editor_shared_field_event(value,sizeof(value),&event));
    event.type=SDL_TEXTINPUT;snprintf(event.text.text,sizeof(event.text.text),"-");
    assert(scene_editor_shared_field_event(value,sizeof(value),&event) && !strcmp(value,"-12.5"));
    event=(SDL_Event){0};event.type=SDL_TEXTEDITING;snprintf(event.edit.text,sizeof(event.edit.text),"星");
    assert(scene_editor_shared_field_event(value,sizeof(value),&event));
    event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
    assert(scene_editor_shared_field_event(value,sizeof(value),&event)); /* Preedit never submits. */
    event.key.keysym.sym=SDLK_ESCAPE;assert(scene_editor_shared_field_event(value,sizeof(value),&event));
    event.key.keysym.sym=SDLK_RETURN;assert(!scene_editor_shared_field_event(value,sizeof(value),&event));
    event.key.repeat=1;assert(scene_editor_shared_field_event(value,sizeof(value),&event));
    assert(SceneEditorDocumentRevision()==revision);scene_editor_shared_field_end(value);
    /* Exercise actual scene-search filtering through its owner. */
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    click(editor,(SDL_Rect){layout.left_content_rect.x+12,layout.left_content_rect.y+42,1,1});
    rename_text(editor,"no-such-object");rename_key(editor,SDLK_a,KMOD_GUI);rename_text(editor,"Object");
    capture(editor,"shared_search_selection.ppm");rename_key(editor,SDLK_ESCAPE,KMOD_NONE);
    /* Shared HUD painting is clipped at normal and narrow logical extents;
     * caller clip survives and no document/solver state is written. */
    VkRenderer *v=(VkRenderer*)editor->renderer;
    VkCommandBuffer command;VkFramebuffer framebuffer;VkExtent2D extent;
    assert(vk_renderer_request_capture(v,"shared_progress_hud.ppm")==VK_SUCCESS);
    assert(vk_renderer_begin_frame(v,&command,&framebuffer,&extent)==VK_SUCCESS);
    TTF_Font *font=ray_tracing_font_runtime_get_ui_regular(editor->renderer,13,10);
    RuntimeNative3DProgressHUDPaint(editor->renderer,font,1280,800,"Disney v2 | progress","Frame 12 of 240","Pass 45 of 100","Tiles 789 of 1024","Elapsed: 12m 40s","Remaining: 25m 12s",.456);
    RuntimeNative3DProgressHUDPaint(editor->renderer,font,320,260,"Disney v2 | progress","Frame 12 of 240","Pass 45 of 100","Tiles 789 of 1024","Elapsed: 12m 40s","Remaining: 25m 12s",.456);
    assert(!SDL_RenderIsClipEnabled(editor->renderer));
    assert(vk_renderer_end_frame(v,command)==VK_SUCCESS);
    assert(SceneEditorDocumentRevision()==revision);
    puts("shared panel draft editing/selection/IME/repeat, search owner and clipped normal/narrow HUD: PASS");
}
