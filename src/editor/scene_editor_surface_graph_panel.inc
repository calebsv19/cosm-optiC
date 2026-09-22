/* Compact typed-node inspector. Connections cycle through type-compatible nodes;
 * the document compiler rejects cycles and validates the whole transaction. */
static int graph_node_index;
static char graph_node_id[64], graph_key[6][24];
static int graph_component[6];
static SDL_Rect graph_edges[3];
static json_object *graph_node(json_object *row) {
    json_object *nodes = get(get(row, "surface_graph"), "nodes");
    int count = (int)json_object_array_length(nodes);
    if (!count)
        return NULL;
    graph_node_index %= count;
    return json_object_array_get_idx(nodes, graph_node_index);
}
static int graph_panel_render(SDL_Renderer *r, SDL_Rect b, int index, json_object *row) {
    (void)index;
    json_object *node = graph_node(row);
    int y = b.y;
    char text[180];
    const char *id = json_object_get_string(get(node, "id")),
               *kind = json_object_get_string(get(node, "kind"));
    snprintf(graph_node_id, sizeof(graph_node_id), "%s", id ? id : "");
    snprintf(text, sizeof(text), "Node: %s (%s)  >", id ? id : "", kind ? kind : "");
    layer_rect = (SDL_Rect){b.x, y, b.w, 27};
    draw(r, layer_rect, text, false);
    y += 31;
    memset(graph_key, 0, sizeof(graph_key));
    memset(graph_edges, 0, sizeof(graph_edges));
    SceneEditorLabelLeft(r, (SDL_Rect){b.x, y, b.w, 24}, "Typed graph / linear color",
                         (SDL_Color){180, 198, 212, 255});
    y += 28;
    int count = 0;
    if (!strcmp(kind, "scalar")) {
        strcpy(graph_key[0], "value");
        graph_component[0] = -1;
        count = 1;
    }
    if (!strcmp(kind, "color"))
        for (int a = 0; a < 3; ++a) {
            strcpy(graph_key[a], "value");
            graph_component[a] = a;
            count = 3;
        }
    if (!strcmp(kind, "coordinate")) {
        strcpy(graph_key[0], "scale_m");
        graph_component[0] = -1;
        count = 4;
        for (int a = 1; a < 4; ++a) {
            strcpy(graph_key[a], "offset");
            graph_component[a] = a - 1;
        }
        scope_rect = (SDL_Rect){b.x, y, b.w, 27};
        snprintf(text, sizeof(text), "Space: %s  >", json_object_get_string(get(node, "space")));
        draw(r, scope_rect, text, false);
        y += 31;
    }
    if (!strcmp(kind, "noise3d")) {
        strcpy(graph_key[0], "seed");
        graph_component[0] = -1;
        count = 1;
    }
    if (!strcmp(kind, "triplanar_checker")) {
        strcpy(graph_key[0], "sharpness");
        graph_component[0] = -1;
        count = 1;
    }
    for (int a = 0; a < count && y + 27 < b.y + b.h; ++a) {
        json_object *value = get(node, graph_key[a]);
        if (graph_component[a] >= 0)
            value = json_object_array_get_idx(value, graph_component[a]);
        fields[a] = (SDL_Rect){b.x, y, b.w, 25};
        if (editing == a)
            snprintf(text, sizeof(text), "%s: %s", graph_key[a], draft);
        else
            snprintf(text, sizeof(text), "%s%s: %.6g", graph_key[a],
                     graph_component[a] >= 0 ? (!strcmp(graph_key[a], "value") ? (a == 0   ? " R"
                                                                                  : a == 1 ? " G"
                                                                                           : " B")
                                                                               : (a == 1   ? " X"
                                                                                  : a == 2 ? " Y"
                                                                                           : " Z"))
                                             : "",
                     json_object_get_double(value));
        draw(r, fields[a], text, editing == a);
        y += 29;
    }
    json_object *inputs = get(node, "inputs");
    for (size_t a = 0;
         inputs && a < json_object_array_length(inputs) && a < 3 && y + 27 < b.y + b.h; ++a) {
        graph_edges[a] = (SDL_Rect){b.x, y, b.w, 25};
        snprintf(text, sizeof(text), "Input %zu: %s  >", a,
                 json_object_get_string(json_object_array_get_idx(inputs, a)));
        draw(r, graph_edges[a], text, false);
        y += 29;
    }
    if (status[0]) {
        SceneEditorLabelLeft(r, (SDL_Rect){b.x, y, b.w, 24}, status,
                             (SDL_Color){225, 196, 153, 255});
        y += 28;
    }
    return y;
}
static bool graph_panel_commit(int index, json_object *row, unsigned long long rev) {
    bool ok = SceneEditorDocumentSetSurfaceGraph(
        index, json_object_to_json_string(get(row, "surface_graph")), rev, status, sizeof(status));
    if (ok)
        snprintf(status, sizeof(status), "Graph updated; Undo available");
    return ok;
}
static bool graph_panel_apply(int index, double value) {
    json_object *row = current(index), *node = graph_node(row);
    bool ok = false;
    if (node && !strcmp(graph_node_id, json_object_get_string(get(node, "id")))) {
        if (graph_component[editing] >= 0)
            json_object_array_put_idx(get(node, graph_key[editing]), graph_component[editing],
                                      json_object_new_double(value));
        else
            json_object_object_add(node, graph_key[editing], json_object_new_double(value));
        ok = graph_panel_commit(index, row, revision);
    }
    if (row)
        json_object_put(row);
    return ok;
}
static int graph_panel_type(json_object *node) {
    const char *kind = json_object_get_string(get(node, "kind"));
    return !strcmp(kind, "coordinate") ? 2
                                       : (!strcmp(kind, "color") || !strcmp(kind, "mix") ? 1 : 0);
}
static bool graph_panel_click(int index, int x, int y) {
    json_object *row = current(index), *node = graph_node(row);
    bool handled = false;
    if (inside(scope_rect, x, y)) {
        const char *space = json_object_get_string(get(node, "space"));
        json_object_object_add(
            node, "space",
            json_object_new_string(!strcmp(space, "world") ? "object_rest" : "world"));
        graph_panel_commit(index, row, SceneEditorDocumentRevision());
        handled = true;
    }
    for (int a = 0; a < 3; ++a)
        if (inside(graph_edges[a], x, y)) {
            json_object *nodes = get(get(row, "surface_graph"), "nodes"),
                        *inputs = get(node, "inputs");
            const char *old = json_object_get_string(json_object_array_get_idx(inputs, a)),
                       *kind = json_object_get_string(get(node, "kind"));
            int count = (int)json_object_array_length(nodes), start = 0,
                expected = !strcmp(kind, "mix")        ? (a < 2 ? 1 : 0)
                           : !strcmp(kind, "multiply") ? 0
                                                       : 2;
            for (int j = 0; j < count; ++j)
                if (!strcmp(old,
                            json_object_get_string(get(json_object_array_get_idx(nodes, j), "id"))))
                    start = j;
            for (int j = 1; j < count; ++j) {
                json_object *candidate = json_object_array_get_idx(nodes, (start + j) % count);
                if (candidate == node || graph_panel_type(candidate) != expected)
                    continue;
                json_object_array_put_idx(
                    inputs, a,
                    json_object_new_string(json_object_get_string(get(candidate, "id"))));
                if (graph_panel_commit(index, row, SceneEditorDocumentRevision()))
                    break;
            }
            handled = true;
        }
    if (row)
        json_object_put(row);
    return handled;
}
