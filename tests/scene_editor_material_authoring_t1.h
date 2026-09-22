/* T1 authoring acceptance starts with legacy objects, uses real inspector events,
 * and verifies retained source/history independently of preview captures. */
static json_object *authoring_t1_row(int index) {
    size_t size = SceneEditorDocumentSurfaceMaterialJSONSize(index);
    assert(size > 0);
    char *bytes = malloc(size);
    assert(bytes);
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(index, bytes, size));
    assert(strlen(bytes) + 1 == size);
    json_object *row = json_tokener_parse(bytes);
    free(bytes);
    assert(row);
    return row;
}
static json_object *authoring_t1_member(json_object *value, const char *key) {
    json_object *out = NULL;
    assert(json_object_object_get_ex(value, key, &out));
    return out;
}
static void authoring_t1_same(int index, json_object *expected) {
    json_object *actual = authoring_t1_row(index);
    assert(json_object_equal(actual, expected));
    json_object_put(actual);
}
static void authoring_t1_click(SceneEditor *editor, const char *name) {
    SDL_Rect rect;
    fprintf(stderr, "T1 control: %s\n", name);
    SceneEditorSessionRuntimeRender(editor);
    bool found = SceneEditorSurfaceMaterialPanelControl(name, &rect);
    if (!found && (!strcmp(name, "new") || !strcmp(name, "assign") || !strcmp(name, "duplicate") ||
                   !strcmp(name, "replace") || !strcmp(name, "source_reset") ||
                   !strcmp(name, "mapping_reset") || !strcmp(name, "add_node") ||
                   !strcmp(name, "delete_node") || !strncmp(name, "output_", 7) ||
                   !strncmp(name, "parameter", 9))) {
        SDL_Rect assignment;
        if (SceneEditorSurfaceMaterialPanelControl("assignment", &assignment)) {
            click(editor, assignment); /* Intended expand/collapse affordance. */
            found = SceneEditorSurfaceMaterialPanelControl(name, &rect);
        }
    }
    if (!found)
        capture(editor, "t1-missing-control.ppm");
    assert(found);
    assert(rect.w > 0 && rect.h > 0);
    click(editor, rect);
}
static void authoring_t1_select(SceneEditor *editor, const char *id) {
    char control[100];
    authoring_t1_click(editor, "node_list");
    authoring_t1_click(editor, "node_search");
    SDL_Event search = {0};
    search.type = SDL_TEXTINPUT;
    snprintf(search.text.text, sizeof(search.text.text), "%s", id);
    SceneEditorSessionRuntimeHandleEvent(editor, &search);
    snprintf(control, sizeof(control), "node:%s", id);
    authoring_t1_click(editor, control);
}
static void authoring_t1_text(SceneEditor *editor, const char *text) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_a;
    event.key.keysym.mod = KMOD_GUI;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    event = (SDL_Event){0};
    event.type = SDL_TEXTINPUT;
    snprintf(event.text.text, sizeof(event.text.text), "%s", text);
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
}
static void authoring_t1_mode(SceneEditor *editor, SceneEditorMeshDisplayMode mode) {
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorMeshPreviewModeGet() == mode);
}
static void authoring_t1_preset(SceneEditor *editor, const char *action, const char *preset,
                                bool accept) {
    char control[100];
    authoring_t1_click(editor, action);
    snprintf(control, sizeof(control), "preset:%s", preset);
    authoring_t1_click(editor, control);
    authoring_t1_click(editor, accept ? "confirm" : "cancel");
}
static void authoring_t1_parameter_undo(SceneEditor *editor, const char *node,
                                        const char *parameter, const char *value) {
    char diagnostic[512];
    json_object *before = authoring_t1_row(0);
    unsigned long long revision = SceneEditorDocumentRevision();
    authoring_t1_select(editor, node);
    authoring_t1_click(editor, parameter);
    authoring_t1_text(editor, value);
    key(editor, SDLK_RETURN);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    assert(SceneEditorDocumentRevision() > revision);
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, before);
    json_object_put(before);
}
static json_object *authoring_t1_node(json_object *graph, const char *id) {
    json_object *nodes = authoring_t1_member(graph, "nodes");
    for (size_t i = 0; i < json_object_array_length(nodes); ++i) {
        json_object *node = json_object_array_get_idx(nodes, i);
        if (!strcmp(json_object_get_string(authoring_t1_member(node, "id")), id))
            return node;
    }
    assert(false);
    return NULL;
}
static void authoring_t1_hidden_controls(SceneEditor *editor) {
    authoring_t1_select(editor, "position");
    SDL_Rect previous, unused;
    assert(SceneEditorSurfaceMaterialPanelControl("parameter0", &previous));
    json_object *before = authoring_t1_row(0);
    unsigned long long revision = SceneEditorDocumentRevision();
    const char *sections[] = {"section:Appearance", "section:Preview"};
    const char *hidden[] = {"node_list", "input0", "parameter0", "add_node"};
    for (size_t section = 0; section < 2; ++section) {
        authoring_t1_click(editor, sections[section]);
        for (size_t i = 0; i < 4; ++i)
            assert(!SceneEditorSurfaceMaterialPanelControl(hidden[i], &unused));
        click(editor, previous);
        key(editor, SDLK_ESCAPE);
        assert(SceneEditorDocumentRevision() == revision);
        authoring_t1_same(0, before);
    }
    authoring_t1_click(editor, "section:Sources");
    json_object_put(before);
}
static void authoring_t1_resets(SceneEditor *editor) {
    char diagnostic[1024];
    json_object *original = authoring_t1_row(0);
    json_object *graph = json_tokener_parse(
        json_object_to_json_string(authoring_t1_member(original, "surface_graph")));
    assert(graph);
    json_object *coordinate = authoring_t1_node(graph, "position");
    json_object_object_add(coordinate, "space", json_object_new_string("world"));
    json_object_object_add(coordinate, "scale_m", json_object_new_double(.73));
    json_object_object_add(coordinate, "offset", json_tokener_parse("[0.1,0.2,0.3]"));
    json_object_object_add(authoring_t1_node(graph, "dark"), "value",
                           json_tokener_parse("[0.3,0.4,0.5]"));
    json_object_object_add(authoring_t1_node(graph, "rough"), "value", json_object_new_double(.23));
    json_object_object_add(authoring_t1_member(graph, "producer"), "t1_reset_metadata",
                           json_tokener_parse("{\"keep\":[1,2,3]}"));
    assert(SceneEditorDocumentSetSurfaceGraph(0, json_object_to_json_string(graph),
                                              SceneEditorDocumentRevision(), diagnostic,
                                              sizeof(diagnostic)));
    json_object *changed = authoring_t1_row(0);
    unsigned long long revision = SceneEditorDocumentRevision();
    assert(
        !SceneEditorDocumentMaterialResetSource(0, revision + 1, diagnostic, sizeof(diagnostic)));
    assert(
        !SceneEditorDocumentMaterialResetMapping(0, revision + 1, diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, changed);
    const char *controls[] = {"source_reset", "mapping_reset"};
    for (int mode = 0; mode < 2; ++mode) {
        revision = SceneEditorDocumentRevision();
        authoring_t1_click(editor, controls[mode]);
        authoring_t1_click(editor, "cancel");
        assert(SceneEditorDocumentRevision() == revision);
        authoring_t1_same(0, changed);
        authoring_t1_click(editor, controls[mode]);
        authoring_t1_click(editor, "confirm");
        json_object *reset = authoring_t1_row(0);
        json_object *reset_graph = authoring_t1_member(reset, "surface_graph");
        assert(json_object_equal(authoring_t1_member(reset_graph, "producer"),
                                 authoring_t1_member(graph, "producer")));
        assert(json_object_equal(authoring_t1_member(reset_graph, "outputs"),
                                 authoring_t1_member(graph, "outputs")));
        json_object *nodes = authoring_t1_member(graph, "nodes");
        for (size_t i = 0; i < json_object_array_length(nodes); ++i) {
            json_object *node = json_object_array_get_idx(nodes, i);
            const char *id = json_object_get_string(authoring_t1_member(node, "id"));
            bool is_coordinate =
                !strcmp(json_object_get_string(authoring_t1_member(node, "kind")), "coordinate");
            if ((!mode && is_coordinate) || (mode && !is_coordinate))
                assert(json_object_equal(node, authoring_t1_node(reset_graph, id)));
        }
        if (!mode) {
            assert(json_object_get_double(authoring_t1_member(
                       authoring_t1_node(reset_graph, "rough"), "value")) == .65);
            assert(!json_object_equal(authoring_t1_node(reset_graph, "dark"),
                                      authoring_t1_node(graph, "dark")));
        } else {
            json_object *reset_coordinate = authoring_t1_node(reset_graph, "position");
            assert(!strcmp(json_object_get_string(authoring_t1_member(reset_coordinate, "space")),
                           "object_rest"));
            assert(json_object_get_double(authoring_t1_member(reset_coordinate, "scale_m")) == .2);
            json_object *offset = authoring_t1_member(reset_coordinate, "offset");
            for (int i = 0; i < 3; ++i)
                assert(json_object_get_double(json_object_array_get_idx(offset, i)) == 0);
        }
        json_object_put(reset);
        assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
        authoring_t1_same(0, changed);
        authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    }
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);
    json_object_put(changed);
    json_object_put(graph);
    json_object_put(original);
}
/* Collect source identities across active rows, archives and provenance rather
 * than restricting the regression to the currently assigned sources. */
static void authoring_t1_collect_ids(json_object *value, json_object *ids) {
    if (json_object_is_type(value, json_type_array)) {
        for (size_t i = 0; i < json_object_array_length(value); ++i)
            authoring_t1_collect_ids(json_object_array_get_idx(value, i), ids);
    } else if (json_object_is_type(value, json_type_object)) {
        json_object_object_foreach(value, key, item) {
            if ((!strcmp(key, "source_id") || !strcmp(key, "origin_source_id")) &&
                json_object_is_type(item, json_type_string))
                json_object_array_add(ids, json_object_get(item));
            authoring_t1_collect_ids(item, ids);
        }
    }
}
static void authoring_t1_reopened_identity(SceneEditor *editor) {
    char diagnostic[1024], folder[PATH_MAX], path[PATH_MAX];
    json_object *candidate = json_object_from_file(SceneEditorDocumentPath());
    assert(candidate);
    json_object *rows = authoring_t1_member(
        authoring_t1_member(
            authoring_t1_member(authoring_t1_member(candidate, "extensions"), "ray_tracing"),
            "authoring"),
        "object_materials");
    json_object *row = json_object_array_get_idx(rows, 0);
    const size_t producer_size = 70001;
    char *producer = malloc(producer_size);
    assert(producer);
    for (size_t i = 0; i < producer_size - 1; ++i)
        producer[i] = (char)('a' + i % 26);
    producer[producer_size - 1] = '\0';
    json_object_object_add(row, "t1_large_producer_metadata", json_object_new_string(producer));
    free(producer);
    json_object *meta = authoring_t1_member(row, "material_authoring");
    json_object *history = NULL;
    if (!json_object_object_get_ex(meta, "replaced_sources", &history)) {
        history = json_object_new_array();
        json_object_object_add(meta, "replaced_sources", history);
    }
    assert(json_object_is_type(history, json_type_array));
    /* Document Open starts revision1, so the former allocator first tried this
     * exact ID regardless of how many commands produced the saved test fixture. */
    json_object_array_add(
        history,
        json_tokener_parse("{\"source_id\":\"material_2_1\",\"material_authoring\":{\"source_id\":"
                           "\"material_2_1\"},"
                           "\"producer\":{\"origin_source_id\":\"material_2_2\",\"keep\":true}}"));
    assert(getcwd(folder, sizeof(folder)));
    assert(snprintf(path, sizeof(path), "%s/t1-identity-candidate.json", folder) <
           (int)sizeof(path));
    assert(json_object_to_file(path, candidate) == 0);
    RuntimeSceneBridgePreflight summary = {0};
    assert(runtime_scene_bridge_apply_file(path, &summary));
    assert(SceneEditorDocumentOpen(path, diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == 1);
    assert(SceneEditorDocumentSurfaceMaterialJSONSize(0) > 65536);
    ObjectEditorSetSelectedObjectIndex(0);
    authoring_t1_click(editor, "section:Sources");
    authoring_t1_select(editor, "pattern");
    SDL_Rect visible_parameter;
    assert(SceneEditorSurfaceMaterialPanelControl("node_list", &visible_parameter));
    assert(SceneEditorSurfaceMaterialPanelControl("parameter0", &visible_parameter));
    json_object *reserved = json_object_new_array();
    authoring_t1_collect_ids(candidate, reserved);
    assert(json_object_array_length(reserved) >= 2);
    json_object *before = authoring_t1_row(0);
    assert(SceneEditorDocumentMaterialDuplicate(0, SceneEditorDocumentRevision(), diagnostic,
                                                sizeof(diagnostic)));
    json_object *after = authoring_t1_row(0);
    assert(json_object_equal(authoring_t1_member(before, "t1_large_producer_metadata"),
                             authoring_t1_member(after, "t1_large_producer_metadata")));
    assert(strlen(json_object_get_string(
               authoring_t1_member(after, "t1_large_producer_metadata"))) == producer_size - 1);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorSurfaceMaterialPanelControl("node_list", &visible_parameter));
    assert(SceneEditorSurfaceMaterialPanelControl("parameter0", &visible_parameter));
    const char *identity = json_object_get_string(
        authoring_t1_member(authoring_t1_member(after, "material_authoring"), "source_id"));
    for (size_t i = 0; i < json_object_array_length(reserved); ++i)
        assert(strcmp(identity, json_object_get_string(json_object_array_get_idx(reserved, i))));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, before);
    json_object_put(before);
    json_object_put(after);
    json_object_put(reserved);
    json_object_put(candidate);
}
static void surface_material_authoring_t1_probe(SceneEditor *editor, bool reopen) {
    char diagnostic[1024];
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_MATERIALS);
    assert(SceneEditorFrameViewport(true));
    SceneEditorSessionRuntimeRender(editor);
    if (reopen) {
        json_object *expected = json_object_from_file("t1-retained-expected.json");
        assert(expected && json_object_array_length(expected) == 2);
        for (int i = 0; i < 2; ++i) {
            assert(RuntimeSurfaceGraphActive(i));
            authoring_t1_same(i, json_object_array_get_idx(expected, i));
        }
        json_object_put(expected);
        authoring_t1_reopened_identity(editor);
        ObjectEditorSetSelectedObjectIndex(0);
        /* Preview mode is a session choice; source reopening is the persistence gate. */
        authoring_t1_click(editor, "preview");
        authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
        FILE *receipt = fopen("material_authoring_t1_reopen.json", "w");
        assert(receipt);
        fputs("{\"source_ids_graphs_metadata_preserved\":true,\"material_preview_available\":true,"
              "\"historical_source_id_noncollision\":true,\"identity_duplicate_undo\":true,\"large_"
              "retained_row_ui\":true,\"large_metadata_duplicate_undo\":true}\n",
              receipt);
        assert(fclose(receipt) == 0);
        return;
    }
    assert(!RuntimeSurfaceGraphActive(0));
    char rowless[65536];
    assert(!SceneEditorDocumentGetSurfaceMaterialJSON(2, rowless, sizeof(rowless)));
    assert(SceneEditorDocumentMaterialPreset(2, "noise3d", false, SceneEditorDocumentRevision(),
                                             diagnostic, sizeof(diagnostic)));
    assert(RuntimeSurfaceGraphActive(2));
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(2, rowless, sizeof(rowless)));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    assert(!RuntimeSurfaceGraphActive(2));
    assert(!SceneEditorDocumentGetSurfaceMaterialJSON(2, rowless, sizeof(rowless)));
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_SOLID);
    authoring_t1_preset(editor, "new", "noise3d", true);
    assert(RuntimeSurfaceGraphActive(0));
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_SOLID);
    authoring_t1_click(editor, "preview");
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    authoring_t1_click(editor, "section:Sources");
    json_object *original = authoring_t1_row(0);
    unsigned long long revision = SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentMaterialPreset(0, "triplanar_checker", false, revision, diagnostic,
                                              sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == revision);
    authoring_t1_same(0, original);
    authoring_t1_preset(editor, "replace", "triplanar_checker", false);
    authoring_t1_same(0, original);
    assert(SceneEditorDocumentRevision() == revision);

    authoring_t1_click(editor, "duplicate");
    json_object *duplicated = authoring_t1_row(0);
    const char *old_id = json_object_get_string(
        authoring_t1_member(authoring_t1_member(original, "material_authoring"), "source_id"));
    const char *new_id = json_object_get_string(
        authoring_t1_member(authoring_t1_member(duplicated, "material_authoring"), "source_id"));
    assert(strcmp(old_id, new_id));
    assert(json_object_equal(authoring_t1_member(original, "surface_graph"),
                             authoring_t1_member(duplicated, "surface_graph")));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    json_object_put(duplicated);

    /* Parameter draft cancellation and invalid submission preserve source/revision. */
    authoring_t1_select(editor, "position");
    authoring_t1_click(editor, "parameter0");
    authoring_t1_text(editor, "0.37");
    key(editor, SDLK_ESCAPE);
    authoring_t1_same(0, original);
    revision = SceneEditorDocumentRevision();
    authoring_t1_click(editor, "parameter0");
    authoring_t1_text(editor, "0");
    key(editor, SDLK_RETURN);
    assert(SceneEditorSurfaceMaterialPanelActive());
    assert(SceneEditorDocumentRevision() == revision);
    authoring_t1_same(0, original);
    key(editor, SDLK_ESCAPE);
    authoring_t1_click(editor, "parameter0");
    authoring_t1_text(editor, "0.37");
    key(editor, SDLK_RETURN);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);
    assert(SceneEditorDocumentRedo(diagnostic, sizeof(diagnostic)));
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);

    authoring_t1_parameter_undo(editor, "rough", "parameter0", "0.43");
    authoring_t1_parameter_undo(editor, "dark", "parameter0", "0.17");
    authoring_t1_parameter_undo(editor, "dark", "parameter1", "0.27");
    authoring_t1_parameter_undo(editor, "dark", "parameter2", "0.37");
    authoring_t1_parameter_undo(editor, "pattern", "parameter0", "31");
    authoring_t1_parameter_undo(editor, "position", "parameter1", "0.13");
    authoring_t1_parameter_undo(editor, "position", "parameter2", "0.23");
    authoring_t1_parameter_undo(editor, "position", "parameter3", "0.33");

    authoring_t1_hidden_controls(editor);
    authoring_t1_resets(editor);

    /* Real node list, creation/deletion and named typed connection pickers. */
    authoring_t1_click(editor, "add_node");
    authoring_t1_click(editor, "kind:scalar");
    json_object *with_node = authoring_t1_row(0);
    json_object *nodes =
        authoring_t1_member(authoring_t1_member(with_node, "surface_graph"), "nodes");
    size_t count = json_object_array_length(nodes);
    assert(count == 7);
    char added_id[64];
    snprintf(added_id, sizeof(added_id), "%s",
             json_object_get_string(
                 authoring_t1_member(json_object_array_get_idx(nodes, count - 1), "id")));
    authoring_t1_select(editor, added_id);
    authoring_t1_click(editor, "delete_node");
    authoring_t1_same(0, original);
    json_object_put(with_node);
    authoring_t1_select(editor, "finish");
    authoring_t1_click(editor, "input0");
    authoring_t1_click(editor, "candidate:light");
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);
    authoring_t1_click(editor, "output_color");
    authoring_t1_click(editor, "candidate:dark");
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);

    /* Every supported node kind participates in retained structural commands. */
    const char *additions[] = {
        "{\"id\":\"extra_coordinate\",\"kind\":\"coordinate\",\"space\":\"world\",\"scale_m\":0.2,"
        "\"offset\":[0,0,0]}",
        "{\"id\":\"extra_scalar\",\"kind\":\"scalar\",\"value\":0.3}",
        "{\"id\":\"extra_color\",\"kind\":\"color\",\"value\":[0.2,0.4,0.6]}",
        "{\"id\":\"extra_noise\",\"kind\":\"noise3d\",\"seed\":31,\"inputs\":[\"extra_coordinate\"]"
        "}",
        "{\"id\":\"extra_checker\",\"kind\":\"triplanar_checker\",\"sharpness\":3,\"inputs\":["
        "\"extra_coordinate\"]}",
        "{\"id\":\"extra_multiply\",\"kind\":\"multiply\",\"inputs\":[\"extra_noise\",\"extra_"
        "scalar\"]}",
        "{\"id\":\"extra_mix\",\"kind\":\"mix\",\"inputs\":[\"dark\",\"extra_color\",\"extra_"
        "multiply\"]}"};
    const char *ids[] = {"extra_coordinate", "extra_scalar",   "extra_color", "extra_noise",
                         "extra_checker",    "extra_multiply", "extra_mix"};
    for (size_t i = 0; i < 7; ++i)
        assert(SceneEditorDocumentSurfaceGraphAddNode(
            0, additions[i], SceneEditorDocumentRevision(), diagnostic, sizeof(diagnostic)));
    revision = SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentSurfaceGraphDeleteNode(0, "extra_coordinate", revision, diagnostic,
                                                      sizeof(diagnostic)));
    assert(!SceneEditorDocumentSurfaceGraphConnect(0, "finish", "a", "finish", revision, diagnostic,
                                                   sizeof(diagnostic)));
    assert(!SceneEditorDocumentSurfaceGraphSetOutput(0, "base_color", "extra_scalar", revision,
                                                     diagnostic, sizeof(diagnostic)));
    assert(!SceneEditorDocumentSurfaceGraphAddNode(0, additions[0], revision + 1, diagnostic,
                                                   sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == revision);
    assert(SceneEditorDocumentSurfaceGraphSetOutput(0, "base_color", "extra_mix", revision,
                                                    diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentSurfaceGraphSetOutput(0, "roughness", "extra_noise",
                                                    SceneEditorDocumentRevision(), diagnostic,
                                                    sizeof(diagnostic)));
    assert(SceneEditorDocumentSurfaceGraphConnect(0, "finish", "factor", "extra_checker",
                                                  SceneEditorDocumentRevision(), diagnostic,
                                                  sizeof(diagnostic)));
    assert(SceneEditorDocumentSurfaceGraphConnect(0, "finish", "factor", "pattern",
                                                  SceneEditorDocumentRevision(), diagnostic,
                                                  sizeof(diagnostic)));
    assert(SceneEditorDocumentSurfaceGraphSetOutput(
        0, "base_color", "finish", SceneEditorDocumentRevision(), diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentSurfaceGraphSetOutput(
        0, "roughness", "rough", SceneEditorDocumentRevision(), diagnostic, sizeof(diagnostic)));
    for (int i = 6; i >= 0; --i)
        assert(SceneEditorDocumentSurfaceGraphDeleteNode(0, ids[i], SceneEditorDocumentRevision(),
                                                         diagnostic, sizeof(diagnostic)));
    authoring_t1_same(0, original);
    json_object_put(original);
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);

    /* Replace archives the previous source; assignment creates independent identity. */
    authoring_t1_preset(editor, "replace", "triplanar_checker", true);
    authoring_t1_parameter_undo(editor, "pattern", "parameter0", "3");
    json_object *checker = authoring_t1_row(0);
    ObjectEditorSetSelectedObjectIndex(1);
    SceneEditorSessionRuntimeRender(editor);
    authoring_t1_click(editor, "assign");
    authoring_t1_click(editor, "assign:surface");
    authoring_t1_click(editor, "confirm");
    json_object *assigned = authoring_t1_row(1);
    assert(RuntimeSurfaceGraphActive(1));
    assert(json_object_get_double(authoring_t1_member(assigned, "glass_ior")) == 1.8);
    assert(!strcmp(json_object_get_string(authoring_t1_member(
                       authoring_t1_member(assigned, "t1_unknown_producer_metadata"), "preserve")),
                   "recipient"));
    assert(json_object_equal(authoring_t1_member(checker, "surface_graph"),
                             authoring_t1_member(assigned, "surface_graph")));
    assert(strcmp(json_object_get_string(authoring_t1_member(
                      authoring_t1_member(checker, "material_authoring"), "source_id")),
                  json_object_get_string(authoring_t1_member(
                      authoring_t1_member(assigned, "material_authoring"), "source_id"))));
    json_object_put(checker);
    json_object_put(assigned);
    ObjectEditorSetSelectedObjectIndex(0);
    authoring_t1_click(editor, "section:Sources");
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorDocumentSave(diagnostic, sizeof(diagnostic)));
    authoring_t1_mode(editor, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    json_object *expected = json_object_new_array();
    for (int i = 0; i < 2; ++i)
        json_object_array_add(expected, authoring_t1_row(i));
    assert(json_object_to_file("t1-retained-expected.json", expected) == 0);
    json_object_put(expected);
    capture(editor, "t1-material-normal.ppm");
    SDL_SetWindowSize(editor->window, 800, 600);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout viewport_only;
    assert(SceneEditorGetPaneLayout(&viewport_only) && viewport_only.viewport_expanded);
    const char *hidden_at_small_size[] = {
        "assignment",   "new",           "assign",       "duplicate",       "replace",
        "source_reset", "mapping_reset", "preview",      "section:Sources", "node_list",
        "parameter0",   "input0",        "output_color", "add_node",        "delete_node"};
    SDL_Rect hidden_control;
    for (size_t i = 0; i < sizeof(hidden_at_small_size) / sizeof(hidden_at_small_size[0]); ++i)
        assert(!SceneEditorSurfaceMaterialPanelControl(hidden_at_small_size[i], &hidden_control));
    SDL_SetWindowSize(editor->window, 1024, 620);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(editor);
    assert(scene_editor_pane_host_restore_workspace(SceneEditorGetPaneHost()));
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout recovered;
    assert(SceneEditorGetPaneLayout(&recovered) && !recovered.viewport_expanded &&
           recovered.right_content_rect.w > 0);
    json_object *narrow_before = authoring_t1_row(0);
    authoring_t1_select(editor, "pattern");
    authoring_t1_click(editor, "parameter0");
    authoring_t1_text(editor, "4");
    key(editor, SDLK_ESCAPE);
    authoring_t1_same(0, narrow_before);
    json_object_put(narrow_before);
    capture(editor, "t1-material-narrow.ppm");
    FILE *receipt = fopen("material_authoring_t1.json", "w");
    assert(receipt);
    fputs("{\"ui_new_replace_assign_duplicate\":true,\"typed_structure_all_seven_kinds\":true,"
          "\"invalid_cancel_preserved\":true,\"undo_redo\":true,\"material_mode_preserved\":true,"
          "\"save_source_identity\":true,\"normal_narrow_captures\":true,\"rowless_new_undo\":true,"
          "\"reset_independence\":true,\"hidden_controls_inert\":true,\"viewport_only_controls_"
          "inert\":true,\"pane_recovery\":true}\n",
          receipt);
    assert(fclose(receipt) == 0);
}
