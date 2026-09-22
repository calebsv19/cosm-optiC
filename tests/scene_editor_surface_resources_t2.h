#include "render/runtime_surface_sampling.h"
/* Import uses deterministic document commands; cards and numeric edits use SDL events. */
static json_object *resources_t2_sampling(int index) {
    size_t size = SceneEditorDocumentSurfaceSamplingJSONSize(index);
    assert(size);
    char *text = malloc(size);
    assert(text);
    assert(SceneEditorDocumentGetSurfaceSamplingJSON(index, text, size));
    json_object *result = json_tokener_parse(text);
    free(text);
    assert(result);
    return result;
}
static json_object *resources_t2_get(json_object *value, const char *key) {
    json_object *out = NULL;
    assert(json_object_object_get_ex(value, key, &out));
    return out;
}
static void resources_t2_assert_same(json_object *expected) {
    json_object *actual = resources_t2_sampling(0);
    assert(json_object_equal(actual, expected));
    json_object_put(actual);
}
static SDL_Rect resources_t2_control(SceneEditor *editor, const char *name) {
    SDL_Rect control;
    SceneEditorSessionRuntimeRender(editor);
    if (SceneEditorSurfaceMaterialPanelControl(name, &control))
        return control;
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    SDL_WarpMouseInWindow(editor->window, layout.right_content_rect.x+layout.right_content_rect.w/2,
                         layout.right_content_rect.y+layout.right_content_rect.h-20);
    SDL_PumpEvents();
    SDL_Event wheel = {0};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.y = 40;
    SceneEditorSessionRuntimeHandleEvent(editor, &wheel);
    SceneEditorSessionRuntimeRender(editor);
    for (int i = 0; i < 64; ++i) {
        if (SceneEditorSurfaceMaterialPanelControl(name, &control))
            return control;
        wheel.wheel.y = -1;
        SceneEditorSessionRuntimeHandleEvent(editor, &wheel);
        SceneEditorSessionRuntimeRender(editor);
    }
    fprintf(stderr, "Resource control unavailable: %s\n", name);
    assert(false);
    return (SDL_Rect){0};
}
static void resources_t2_click(SceneEditor *editor, const char *name) {
    SDL_Rect control = resources_t2_control(editor, name);
    fprintf(stderr, "T2 click %s rect=%d,%d,%d,%d rev=%llu\n", name, control.x, control.y,
            control.w, control.h, SceneEditorDocumentRevision());
    click(editor, control);
    fprintf(stderr, "T2 after %s active=%d rev=%llu\n", name,
            SceneEditorSurfaceMaterialPanelActive(), SceneEditorDocumentRevision());
}
static void resources_t2_text(SceneEditor *editor, const char *value) {
    SDL_Event text = {0};
    text.type = SDL_TEXTINPUT;
    snprintf(text.text.text, sizeof(text.text.text), "%s", value);
    SceneEditorSessionRuntimeHandleEvent(editor, &text);
}
static void resources_t2_select_all(SceneEditor *editor) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_a;
    event.key.keysym.mod = KMOD_CTRL;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
}
static void resources_t2_import(const char *channel, const char *variable, const char *encoding) {
    char diagnostic[512];
    const char *path = getenv(variable);
    assert(path && path[0]);
    assert(SceneEditorDocumentSurfaceSamplingSetChannel(
        0, channel, path, encoding, SceneEditorDocumentRevision(), diagnostic, sizeof(diagnostic)));
    assert(
        SceneEditorDocumentSurfaceSamplingCheckChannel(0, channel, diagnostic, sizeof(diagnostic)));
}
static void resources_t2_relative(json_object *sampling) {
    json_object *channels = resources_t2_get(sampling, "channels");
    json_object_object_foreach(channels, key, value) {
        (void)key;
        const char *path = json_object_get_string(resources_t2_get(value, "path"));
        const char *digest = json_object_get_string(resources_t2_get(value, "sha256"));
        assert(path && !strncmp(path, "assets/materials/", strlen("assets/materials/")) &&
               path[0] != '/');
        assert(digest && strlen(digest) == 64);
    }
}
/* Relinking repairs the pinned cache identity without following an unexpected symlink. */
static void resources_t2_repair(void) {
    char diagnostic[512], destination[4096], relative[128], digest[65];
    json_object *sampling = resources_t2_sampling(0);
    json_object *entry = resources_t2_get(resources_t2_get(sampling, "channels"), "base_color");
    snprintf(relative, sizeof(relative), "%s",
             json_object_get_string(resources_t2_get(entry, "path")));
    snprintf(digest, sizeof(digest), "%s",
             json_object_get_string(resources_t2_get(entry, "sha256")));
    snprintf(destination, sizeof(destination), "%s", SceneEditorDocumentPath());
    char *slash = strrchr(destination, '/');
    assert(slash);
    slash[1] = 0;
    assert(strlen(destination) + strlen(relative) < sizeof(destination));
    strcat(destination, relative);
    json_object_put(sampling);
    FILE *corrupt = fopen(destination, "wb");
    assert(corrupt);
    assert(fputs("corrupt imported fixture", corrupt) >= 0);
    assert(fclose(corrupt) == 0);
    assert(!SceneEditorDocumentSurfaceSamplingCheckChannel(0, "base_color", diagnostic,
                                                           sizeof(diagnostic)));
    resources_t2_import("base_color", "OPTIC_T2_BASE_COLOR", "srgb");
    sampling = resources_t2_sampling(0);
    entry = resources_t2_get(resources_t2_get(sampling, "channels"), "base_color");
    assert(!strcmp(relative, json_object_get_string(resources_t2_get(entry, "path"))));
    assert(!strcmp(digest, json_object_get_string(resources_t2_get(entry, "sha256"))));
    json_object_put(sampling);
    assert(unlink(destination) == 0);
    assert(!SceneEditorDocumentSurfaceSamplingCheckChannel(0, "base_color", diagnostic,
                                                           sizeof(diagnostic)));
    resources_t2_import("base_color", "OPTIC_T2_BASE_COLOR", "srgb");
    assert(unlink(destination) == 0);
    assert(getenv("OPTIC_T2_OUTSIDE_IMAGE"));
    assert(symlink(getenv("OPTIC_T2_OUTSIDE_IMAGE"), destination) == 0);
    assert(!SceneEditorDocumentSurfaceSamplingCheckChannel(0,"base_color",diagnostic,sizeof(diagnostic)));
    unsigned long long revision = SceneEditorDocumentRevision();
    sampling = resources_t2_sampling(0);
    assert(!SceneEditorDocumentSurfaceSamplingSetChannel(0, "base_color",
                                                         getenv("OPTIC_T2_BASE_COLOR"), "srgb",
                                                         revision, diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == revision);
    resources_t2_assert_same(sampling);
    json_object_put(sampling);
    assert(unlink(destination) == 0);
    resources_t2_import("base_color", "OPTIC_T2_BASE_COLOR", "srgb");
    sampling = resources_t2_sampling(0);
    entry = resources_t2_get(resources_t2_get(sampling, "channels"), "base_color");
    assert(!strcmp(relative, json_object_get_string(resources_t2_get(entry, "path"))));
    assert(!strcmp(digest, json_object_get_string(resources_t2_get(entry, "sha256"))));
    json_object_put(sampling);
    assert(RuntimeSurfaceSamplingActive(0));
}
static void surface_resources_t2_probe(SceneEditor *editor, bool reopen) {
    char diagnostic[512];
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_MATERIALS);
    SceneEditorSessionRuntimeRender(editor);
    SDL_Rect control;
    assert(SceneEditorSurfaceMaterialPanelControl("section:Sources", &control));
    click(editor, control);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorSurfaceMaterialPanelControl("resource:base_color:card", &control));
    if (reopen) {
        json_object *sampling = resources_t2_sampling(0),
                    *expected = json_object_from_file("resources_t2_expected.json");
        assert(expected && json_object_equal(sampling, expected));
        resources_t2_relative(sampling);
        assert(RuntimeSurfaceSamplingActive(0));
        capture(editor, "resources-t2-reopen.ppm");
        FILE *reopened = fopen("resources_t2_reopen.json", "w");
        assert(reopened);
        fputs("{\"fresh_process_sampling_match\":true,\"project_relative_resources\":true}\n",
              reopened);
        fclose(reopened);
        json_object_put(expected);
        json_object_put(sampling);
        return;
    }
    assert(SceneEditorDocumentSurfaceSamplingSupported(0, diagnostic, sizeof(diagnostic)));
    resources_t2_import("base_color", "OPTIC_T2_BASE_COLOR", "srgb");
    resources_t2_import("roughness", "OPTIC_T2_ROUGHNESS", "data");
    resources_t2_import("normal", "OPTIC_T2_NORMAL", "data");
    resources_t2_repair();
    assert(RuntimeSurfaceSamplingActive(0));
    resources_t2_click(editor, "resource:base_color:card");
    resources_t2_click(editor, "resource:base_color:check");
    json_object *before = resources_t2_sampling(0);
    resources_t2_relative(before);
    unsigned long long revision = SceneEditorDocumentRevision();
    resources_t2_click(editor, "resource:base_color:encoding");
    assert(SceneEditorDocumentRevision() > revision);
    json_object *sampling = resources_t2_sampling(0);
    assert(!strcmp(
        json_object_get_string(resources_t2_get(
            resources_t2_get(resources_t2_get(sampling, "channels"), "base_color"), "color_space")),
        "linear"));
    json_object_put(sampling);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    resources_t2_assert_same(before);
    resources_t2_click(editor, "resource:normal:card");
    resources_t2_click(editor, "resource:normal_strength");
    revision = SceneEditorDocumentRevision();
    resources_t2_text(editor, "5");
    key(editor, SDLK_RETURN);
    assert(SceneEditorSurfaceMaterialPanelActive());
    assert(SceneEditorDocumentRevision() == revision);
    resources_t2_assert_same(before);
    resources_t2_select_all(editor);
    resources_t2_text(editor, "0.45");
    key(editor, SDLK_RETURN);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    sampling = resources_t2_sampling(0);
    assert(fabs(json_object_get_double(resources_t2_get(sampling, "normal_strength")) - .45) <
           1e-12);
    json_object_put(sampling);
    resources_t2_click(editor, "resource:period_u");
    resources_t2_text(editor, "7");
    key(editor, SDLK_RETURN);
    sampling = resources_t2_sampling(0);
    assert(json_object_get_int(
               json_object_array_get_idx(resources_t2_get(sampling, "period_tiles"), 0)) == 7);
    json_object_put(sampling);
    resources_t2_click(editor, "resource:period_v");
    if (!SceneEditorSurfaceMaterialPanelActive()) {
        capture(editor, "resources-t2-period-v-focus-failure.ppm");
        assert(SceneEditorSurfaceMaterialPanelActive());
    }
    resources_t2_text(editor, "3.5");
    revision = SceneEditorDocumentRevision();
    key(editor, SDLK_RETURN);
    if (!SceneEditorSurfaceMaterialPanelActive() || SceneEditorDocumentRevision() != revision) {
        json_object *failure = resources_t2_sampling(0);
        fprintf(stderr, "T2 invalid period_v active=%d before=%llu after=%llu sampling=%s\n",
                SceneEditorSurfaceMaterialPanelActive(), revision, SceneEditorDocumentRevision(),
                json_object_to_json_string(failure));
        json_object_put(failure);
        capture(editor, "resources-t2-period-v-rejection-failure.ppm");
    }
    assert(SceneEditorSurfaceMaterialPanelActive() && SceneEditorDocumentRevision() == revision);
    key(editor, SDLK_ESCAPE);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    resources_t2_click(editor, "resource:roughness:card");
    sampling = resources_t2_sampling(0);
    resources_t2_click(editor, "resource:roughness:remove");
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    resources_t2_assert_same(sampling);
    json_object_put(sampling);
    revision = SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentSurfaceSamplingSetValue(0, "period_u", 4, revision - 1, diagnostic,
                                                       sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == revision);
    SceneEditorDocumentObjectInfo info;
    assert(SceneEditorDocumentObjectAt(0, &info));
    assert(SceneEditorDocumentSetFlag(info.id, "locked", true, SceneEditorDocumentRevision(),
                                      diagnostic, sizeof(diagnostic)));
    revision = SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentSurfaceSamplingSetValue(0, "period_u", 4, revision, diagnostic,
                                                       sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == revision);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    /* Relink explicitly repins changed bytes and preserves the prior resource metadata. */
    resources_t2_import("base_color", "OPTIC_T2_RELINK", "srgb");
    resources_t2_click(editor, "resource:base_color:card");
    resources_t2_click(editor, "resource:base_color:check");
    assert(SceneEditorDocumentSurfaceSamplingSetChannel(
        0, "normal", NULL, NULL, SceneEditorDocumentRevision(), diagnostic, sizeof(diagnostic)));
    resources_t2_import("height", "OPTIC_T2_HEIGHT", "data");
    resources_t2_click(editor, "resource:height:card");
    resources_t2_click(editor, "resource:height_m");
    resources_t2_text(editor, "0.012");
    key(editor, SDLK_RETURN);
    sampling = resources_t2_sampling(0);
    assert(fabs(json_object_get_double(resources_t2_get(sampling, "height_m")) - .012) < 1e-12);
    resources_t2_relative(sampling);
    int copy = -1;
    assert(SceneEditorDocumentDuplicateForSceneIndex(0, &copy, diagnostic, sizeof(diagnostic)));
    json_object *copied = resources_t2_sampling(copy);
    assert(json_object_equal(sampling, copied));
    json_object_put(copied);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    resources_t2_assert_same(sampling);
    assert(SceneEditorDocumentIsDirty());revision=SceneEditorDocumentRevision();
    resources_t2_click(editor,"resource:adopt_candidate");
    assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty());
    assert(!SceneEditorSurfaceMaterialPanelActive());
    bool saved = SceneEditorRuntimeScenePersistAuthoring(diagnostic, sizeof(diagnostic));
    if (!saved)
        fprintf(stderr, "T2 save failed: %s\n", diagnostic);
    assert(saved);
    assert(json_object_to_file_ext("resources_t2_expected.json", sampling,
                                   JSON_C_TO_STRING_PRETTY) == 0);
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor, "resources-t2-material.ppm");
    SDL_SetWindowSize(editor->window,1024,620);
    SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
    resources_t2_click(editor,"resource:period_v");
    key(editor,SDLK_ESCAPE);
    resources_t2_assert_same(sampling);
    capture(editor,"resources-t2-narrow.ppm");
    FILE *report = fopen("resources_t2.json", "w");
    assert(report);
    fputs("{\"cards_numeric_encoding_remove\":true,\"rejected_draft_preserved\":true,\"undo_"
          "duplicate_relative_resources\":true,\"stale_locked_guards\":true,\"import_relink_via_"
          "document_api\":true,\"corrupt_missing_pin_repair\":true,\"symlink_refused_unchanged_"
          "revision\":true,\"native_file_chooser_completion_tested\":false}\n",
          report);
    fclose(report);
    json_object_put(sampling);
    json_object_put(before);
}
