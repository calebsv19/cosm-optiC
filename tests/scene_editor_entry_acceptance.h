/* Normal embedded entry and compatibility-route acceptance. No workspace
 * selection is allowed between session initialization and first-frame proof. */
static void entry_scene_visible(SceneEditor* editor) {
    assert(SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_SCENE);
    assert(editor->currentMode == EDITOR_MODE_OBJECT);
    assert(animSettings.editorMode == EDITOR_MODE_OBJECT);
    assert(SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_SELECT);
    assert(SceneEditorObjectSelectionOnly());
    assert(!SceneEditorGetPaneHost()->timeline_visible);
    assert(SceneEditorGetViewportNavState()->target_valid);
    assert(SceneEditorMeshPreviewStoreInstanceCount() > 0);
    SceneEditorMaterialPerfEnable(true, true);
    SceneEditorMaterialPerfBeginSample();
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorMaterialPerfSample visible = SceneEditorMaterialPerfRead();
    assert(visible.instances > 0 && visible.rendered_triangles > 0);
    SceneEditorMaterialPerfEnable(false, false);
    assert(!SceneEditorDocumentIsDirty());
}

static void entry_acceptance(SceneEditor* editor) {
    entry_scene_visible(editor);
    capture(editor, "entry_scene.ppm");
    /* Menu sessions reuse the host window/device; exercise that exact lifecycle
     * with persisted light, camera, material and invalid old mode values. */
    const int saved_modes[] = {EDITOR_MODE_PATH, EDITOR_MODE_CAMERA, EDITOR_MODE_MATERIAL, 999};
    SDL_Window* window = editor->window;
    SDL_Renderer* renderer = editor->renderer;
    bool owns_device = editor->owns_shared_device;
    for (size_t i = 0; i < sizeof(saved_modes)/sizeof(saved_modes[0]); ++i) {
        animSettings.editorMode = saved_modes[i];
        assert(SaveAnimationConfigChecked());
        SceneEditorSessionEnd(editor);
        assert(SceneEditorSessionBegin(editor, renderer, window));
        /* Retain test-owned host cleanup, not application session ownership. */
        editor->owns_window = true;
        editor->owns_renderer = true;
        editor->owns_shared_device = owns_device;
        entry_scene_visible(editor);
    }
    unsigned long long revision = SceneEditorDocumentRevision();
    choose_menu(editor, 2, 6); /* View > Light path (Render). */
    assert(SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER);
    assert(editor->currentMode == EDITOR_MODE_PATH);
    assert(!SceneEditorRenderAuthoringTiming());
    /* Preview reinitializes the selected backend. It must preserve Render. */
    SetSceneMode(editor, EDITOR_MODE_PATH);
    assert(SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER);
    assert(SceneEditorGetPaneHost()->timeline_visible);
    SceneEditorMaterialPerfEnable(true, true);
    SceneEditorMaterialPerfBeginSample();
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorMaterialPerfSample visible = SceneEditorMaterialPerfRead();
    assert(visible.instances > 0 && visible.rendered_triangles > 0);
    SceneEditorMaterialPerfEnable(false, false);
    capture(editor, "light_path_context.ppm");
    choose_menu(editor, 2, 7); /* No light-only legacy dock. */
    assert(SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER);
    assert(SceneEditorRenderAuthoringTiming());
    choose_menu(editor, -1, SCENE_WORKSPACE_SCENE);
    entry_scene_visible(editor);
    choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
    key(editor, SDLK_ESCAPE);
    assert(editor->currentMode == EDITOR_MODE_CAMERA); /* No stale light-track override. */
    choose_menu(editor, -1, SCENE_WORKSPACE_SCENE);
    /* Tab follows visible workspaces, never the hidden legacy mode ring. */
    for (int i = 1; i <= SCENE_WORKSPACE_PROFILE_COUNT; ++i) {
        key(editor, SDLK_TAB);
        assert(SceneEditorWorkspaceProfileGet() == (SceneEditorWorkspaceProfile)(i % SCENE_WORKSPACE_PROFILE_COUNT));
    }
    SDL_Event reverse = {.type = SDL_KEYDOWN};
    reverse.key.keysym.sym = SDLK_TAB;
    reverse.key.keysym.mod = KMOD_SHIFT;
    SceneEditorSessionRuntimeHandleEvent(editor, &reverse);
    assert(SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER);
    SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_DELETE);
    choose_menu(editor, -1, SCENE_WORKSPACE_SCENE);
    entry_scene_visible(editor);
    assert(SceneEditorDocumentRevision() == revision);
    capture(editor, "scene_after_switching.ppm");
    /* Failed preview construction must still draw known object bounds. */
    const RayTracingRuntimeMeshAssetInstance* instance = SceneEditorMeshPreviewStoreGetInstance(0);
    assert(instance);
    const CoreMeshAssetRuntimeContract* contract = SceneEditorMeshPreviewStoreGetContract(instance->asset_index);
    assert(contract);
    RayTracingRuntimeMeshAssetSet* unavailable = calloc(1, sizeof(*unavailable));
    assert(unavailable);
    unavailable->asset_count = unavailable->instance_count = 1;
    unavailable->assets[0].document.contract = *contract;
    unavailable->instances[0] = *instance;
    unavailable->instances[0].asset_index = 0;
    int selected = instance->scene_object_index;
    SceneEditorMeshPreviewRenderReset(editor->renderer);
    SceneEditorMeshPreviewStorePrepare(unavailable);
    assert(SceneEditorMeshPreviewStoreInstanceCount() == 1);
    assert(!SceneEditorMeshPreviewStoreGet(0));
    ObjectEditorSetSelectedObjectIndex(selected);
    assert(SceneEditorFrameViewport(true));
    capture(editor, "unavailable_mesh_bounds.ppm");
    free(unavailable);
    SceneEditorMeshPreviewRenderReset(editor->renderer);
    SceneEditorMeshPreviewStorePrepare(ray_tracing_runtime_mesh_assets_last());
    SceneEditorFrameViewport(false);
    fprintf(stderr, "Scene entry and workspace routing acceptance passed.\n");
}
