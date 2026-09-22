/* Typed graph controls edit the retained document through validated commands. */
static int graph_node_index, graph_scroll, graph_picker, graph_page, graph_input;
static bool graph_search_active;
static char graph_node_id[64], graph_key[6][24], graph_search[64];
static int graph_component[6];
static SDL_Rect graph_edges[3], graph_outputs[2], graph_list, graph_add, graph_delete;
static SDL_Rect graph_search_rect, graph_cancel_rect, graph_prev, graph_next, graph_bounds;
static SDL_Rect graph_choices[32];
static char graph_choice_ids[32][64];
static bool graph_choice_valid[32];
static int graph_choice_count, graph_page_size, graph_total_rows;
static unsigned long long graph_picker_revision;
static const char *graph_kinds[] = {
    "scalar", "color", "coordinate", "noise3d", "triplanar_checker", "multiply", "mix"};

static int graph_panel_type(json_object *node) {
    const char *kind = json_object_get_string(get(node, "kind"));
    if (!kind)
        return -1;
    return !strcmp(kind, "coordinate") ? 2
                                       : (!strcmp(kind, "color") || !strcmp(kind, "mix") ? 1 : 0);
}
static json_object *graph_node(json_object *row) {
    json_object *nodes = get(get(row, "surface_graph"), "nodes");
    int count = (int)json_object_array_length(nodes);
    if (!count)
        return NULL;
    if (graph_node_index < 0)
        graph_node_index = 0;
    graph_node_index %= count;
    return json_object_array_get_idx(nodes, graph_node_index);
}
static bool graph_panel_select_coordinates(int index) {
    json_object *row=current(index), *nodes=get(get(row,"surface_graph"),"nodes");
    int count=nodes?(int)json_object_array_length(nodes):0;
    bool found=false;
    for(int n=0;n<count;++n) {
        int i=(graph_node_index+n)%count;
        const char *kind=json_object_get_string(get(json_object_array_get_idx(nodes,i),"kind"));
        if(kind && !strcmp(kind,"coordinate")){graph_node_index=i;found=true;break;}
    }
    if(row)json_object_put(row);
    return found;
}
static void graph_panel_invalidate(void) {
    memset(graph_edges,0,sizeof(graph_edges));memset(graph_outputs,0,sizeof(graph_outputs));
    memset(graph_choices,0,sizeof(graph_choices));
    graph_list=graph_add=graph_delete=graph_search_rect=graph_cancel_rect=graph_prev=graph_next=graph_bounds=(SDL_Rect){0};
}
static void graph_panel_reset(void) {
    graph_panel_invalidate();
    graph_node_index = graph_scroll = graph_picker = graph_page = 0;
    graph_search_active = false;
    graph_search[0] = 0;
    graph_bounds = (SDL_Rect){0};
    memset(graph_choices, 0, sizeof(graph_choices));
}
static bool graph_panel_active(void) { return graph_picker != 0; }
static const char *graph_port(json_object *node, int input) {
    const char *kind = json_object_get_string(get(node, "kind"));
    if (!strcmp(kind, "mix"))
        return (const char *[]){"a", "b", "factor"}[input];
    if (!strcmp(kind, "multiply"))
        return input ? "b" : "a";
    return "coordinates";
}
static const char *graph_port_label(json_object *node, int input) {
    const char *kind = json_object_get_string(get(node, "kind"));
    if (!strcmp(kind, "mix"))
        return (const char *[]){"Color A", "Color B", "Mask"}[input];
    if (!strcmp(kind, "multiply"))
        return input ? "Scalar B" : "Scalar A";
    return "Coordinates";
}
static void graph_picker_close(void) {
    graph_picker = 0;
    graph_search_active = false;
    SDL_StopTextInput();
}
static void graph_picker_open(int mode) {
    cancel();
    graph_picker = mode;
    graph_page = 0;
    graph_search[0] = 0;
    graph_picker_revision = SceneEditorDocumentRevision();
    graph_search_active = true;
    SDL_StartTextInput();
}
/* A single logical row helper omits offscreen hit targets as well as drawing. */
static void graph_row(SDL_Renderer *r, SDL_Rect b, int row, SDL_Rect *control, const char *text,
                      bool active) {
    int y = b.y + (row - graph_scroll) * 29;
    *control = (SDL_Rect){0};
    if (row >= graph_scroll && y + 25 <= b.y + b.h) {
        *control = (SDL_Rect){b.x, y, b.w, 25};
        draw(r, *control, text, active);
    }
}
static bool graph_candidate_valid(json_object *row, const char *id) {
    json_object *candidate = clone(get(row, "surface_graph"));
    if (!candidate)
        return false;
    if (graph_picker == 3) {
        json_object *node = json_object_array_get_idx(get(candidate, "nodes"), graph_node_index);
        json_object_array_put_idx(get(node, "inputs"), graph_input, json_object_new_string(id));
    } else {
        json_object *outputs = get(candidate, "outputs");
        const char *key = graph_picker == 4 ? "base_color" : "roughness";
        if (id[0])
            json_object_object_add(outputs, key, json_object_new_string(id));
        else
            json_object_object_del(outputs, key);
    }
    CoreSurfaceGraph compiled;
    char diagnostic[256];
    bool valid = RuntimeSurfaceGraphParse(candidate, &compiled, diagnostic, sizeof(diagnostic));
    json_object_put(candidate);
    return valid;
}
static int graph_picker_render(SDL_Renderer *r, SDL_Rect b, json_object *row) {
    char text[180];
    int y = b.y;
    graph_choice_count = 0;
    memset(graph_choices, 0, sizeof(graph_choices));
    graph_prev = graph_next = graph_search_rect = graph_cancel_rect = (SDL_Rect){0};
    if (b.h < 60)
        return b.y;
    graph_search_rect = (SDL_Rect){b.x, y, b.w - 65, 25};
    graph_cancel_rect = (SDL_Rect){b.x + b.w - 61, y, 61, 25};
    snprintf(text, sizeof(text), "Search: %s", graph_search);
    draw(r, graph_search_rect, text, graph_search_active);
    draw(r, graph_cancel_rect, "Cancel", false);
    y += 29;
    json_object *nodes = get(get(row, "surface_graph"), "nodes"), *node = graph_node(row);
    const char *ids[33];
    bool valid[33];
    int count = 0;
    int expected =
        graph_picker == 4 ? 1
        : graph_picker == 5
            ? 0
            : (!strcmp(json_object_get_string(get(node, "kind")), "mix") ? (graph_input < 2 ? 1 : 0)
               : !strcmp(json_object_get_string(get(node, "kind")), "multiply") ? 0
                                                                                : 2);
    if (graph_picker == 5 && !graph_search[0]) {
        ids[count] = "";
        valid[count++] = true;
    }
    int total = graph_picker == 2 ? 7 : (int)json_object_array_length(nodes);
    for (int i = 0; i < total; ++i) {
        json_object *item = graph_picker == 2 ? NULL : json_object_array_get_idx(nodes, i);
        const char *id =
            graph_picker == 2 ? graph_kinds[i] : json_object_get_string(get(item, "id"));
        const char *kind = item ? json_object_get_string(get(item, "kind")) : id;
        if (graph_picker == 1 && assignment_section == 2 && strcmp(kind,"coordinate"))
            continue;
        if (graph_search[0] && !strstr(id, graph_search) && !strstr(kind, graph_search))
            continue;
        if (graph_picker >= 3 && graph_panel_type(item) != expected)
            continue;
        ids[count] = id;
        valid[count] = graph_picker < 3 || graph_candidate_valid(row, id);
        ++count;
    }
    graph_page_size = (b.h - (status[0] ? 91 : 62)) / 29;
    if (graph_page_size < 1)
        graph_page_size = 1;
    if (graph_page_size > 32)
        graph_page_size = 32;
    int pages = (count + graph_page_size - 1) / graph_page_size;
    if (!pages)
        pages = 1;
    if (graph_page >= pages)
        graph_page = pages - 1;
    if (graph_page < 0)
        graph_page = 0;
    for (int i = graph_page * graph_page_size; i < count && graph_choice_count < graph_page_size;
         ++i) {
        int slot = graph_choice_count++;
        snprintf(graph_choice_ids[slot], sizeof(graph_choice_ids[slot]), "%s", ids[i]);
        graph_choice_valid[slot] = valid[i];
        graph_choices[slot] = (SDL_Rect){b.x, y, b.w, 25};
        snprintf(text, sizeof(text), "%s%s", ids[i][0] ? ids[i] : "Use base roughness",
                 valid[i] ? "" : " (cycle: unavailable)");
        draw(r, graph_choices[slot], text, false);
        y += 29;
    }
    if (!count) {
        SceneEditorLabelLeft(r, (SDL_Rect){b.x, y, b.w, 24}, "No matching nodes",
                             (SDL_Color){210, 190, 160, 255});
        y += 29;
    }
    if (status[0] && y + 25 <= b.y + b.h) {
        SceneEditorLabelLeft(r, (SDL_Rect){b.x, y, b.w, 24}, status,
                             (SDL_Color){230, 190, 145, 255});
        y += 29;
    }
    if (y + 25 <= b.y + b.h) {
        graph_prev = (SDL_Rect){b.x, y, b.w / 2 - 2, 25};
        graph_next = (SDL_Rect){b.x + b.w / 2, y, b.w / 2, 25};
        snprintf(text, sizeof(text), "Previous (%d/%d)", graph_page + 1, pages);
        draw(r, graph_prev, text, false);
        draw(r, graph_next, "Next", false);
        y += 29;
    }
    return y;
}
static int graph_panel_render(SDL_Renderer *r, SDL_Rect b, int index, json_object *row) {
    (void)index;
    graph_bounds = b;
    memset(graph_edges, 0, sizeof(graph_edges));
    memset(graph_outputs, 0, sizeof(graph_outputs));
    graph_list = graph_add = graph_delete = scope_rect = layer_rect = (SDL_Rect){0};
    memset(fields, 0, sizeof(fields));
    memset(graph_key, 0, sizeof(graph_key));
    json_object *node = graph_node(row);
    if (!node)
        return b.y;
    const char *id = json_object_get_string(get(node, "id")),
               *kind = json_object_get_string(get(node, "kind"));
    snprintf(graph_node_id, sizeof(graph_node_id), "%s", id);
    if (graph_picker)
        return graph_picker_render(r, b, row);
    memset(graph_choices, 0, sizeof(graph_choices));
    graph_choice_count = 0;
    graph_search_rect = graph_cancel_rect = graph_prev = graph_next = (SDL_Rect){0};
    int parameter_count = !strcmp(kind, "coordinate") ? 5
                          : !strcmp(kind, "color")
                              ? 3
                              : (!strcmp(kind, "scalar") || !strcmp(kind, "noise3d") ||
                                         !strcmp(kind, "triplanar_checker")
                                     ? 1
                                     : 0);
    json_object *declared_inputs = get(node, "inputs");
    int input_count = declared_inputs ? (int)json_object_array_length(declared_inputs) : 0;
    int maximum = 1 + parameter_count + input_count + 4 + (status[0] ? 1 : 0) - b.h / 29;
    if (maximum < 0)
        maximum = 0;
    if (graph_scroll > maximum)
        graph_scroll = maximum;
    char text[180];
    int row_number = 0;
    snprintf(text, sizeof(text), "Node: %s (%s)", id, kind);
    graph_row(r, b, row_number++, &graph_list, text, false);
    if (graph_list.w > 40) {
        graph_list.w -= 34;
        layer_rect = (SDL_Rect){graph_list.x + graph_list.w + 4, graph_list.y, 30, 25};
        draw(r, layer_rect, ">", false);
    }
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
        snprintf(text, sizeof(text), "Space: %s", json_object_get_string(get(node, "space")));
        graph_row(r, b, row_number++, &scope_rect, text, false);
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
    for (int a = 0; a < count; ++a) {
        json_object *value = get(node, graph_key[a]);
        if (graph_component[a] >= 0)
            value = json_object_array_get_idx(value, graph_component[a]);
        field_values[a] = json_object_get_double(value);
        const char *label = graph_key[a];
        char component[40];
        if (graph_component[a] >= 0) {
            snprintf(component, sizeof(component), "%s %s", label,
                     (!strcmp(label, "value")
                          ? (const char *[]){"R", "G", "B"}
                          : (const char *[]){"X", "Y", "Z"})[graph_component[a]]);
            label = component;
        }
        if (editing == a)
            snprintf(text, sizeof(text), "%s: %s", label, draft);
        else
            snprintf(text, sizeof(text), "%s: %.9g", label, field_values[a]);
        graph_row(r, b, row_number++, &fields[a], text, editing == a);
    }
    json_object *inputs = get(node, "inputs");
    for (size_t a = 0; inputs && a < json_object_array_length(inputs) && a < 3; ++a) {
        snprintf(text, sizeof(text), "%s: %s", graph_port_label(node, (int)a),
                 json_object_get_string(json_object_array_get_idx(inputs, a)));
        graph_row(r, b, row_number++, &graph_edges[a], text, false);
    }
    json_object *outputs = get(get(row, "surface_graph"), "outputs");
    for (int a = 0; a < 2; ++a) {
        const char *output = json_object_get_string(get(outputs, a ? "roughness" : "base_color"));
        snprintf(text, sizeof(text), "%s output: %s", a ? "Roughness" : "Color",
                 output ? output : "base response");
        graph_row(r, b, row_number++, &graph_outputs[a], text, false);
    }
    graph_row(r, b, row_number++, &graph_add, "Add node...", false);
    graph_row(r, b, row_number++, &graph_delete, "Delete selected node", false);
    if (status[0]) {
        SDL_Rect message;
        graph_row(r, b, row_number++, &message, status, false);
    }
    graph_total_rows = row_number;
    return b.y + (row_number - graph_scroll) * 29;
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
    if (node && editing >= 0 && editing < 6 &&
        !strcmp(graph_node_id, json_object_get_string(get(node, "id")))) {
        json_object *v = !strcmp(graph_key[editing], "seed") && value >= 0 && value <= UINT32_MAX &&
                                 floor(value) == value
                             ? json_object_new_int64((int64_t)value)
                             : json_object_new_double(value);
        if (graph_component[editing] >= 0)
            json_object_array_put_idx(get(node, graph_key[editing]), graph_component[editing], v);
        else
            json_object_object_add(node, graph_key[editing], v);
        ok = graph_panel_commit(index, row, revision);
    }
    if (row)
        json_object_put(row);
    return ok;
}
static const char *graph_first_type(json_object *nodes, int type) {
    for (size_t i = 0; i < json_object_array_length(nodes); ++i) {
        json_object *node = json_object_array_get_idx(nodes, i);
        if (graph_panel_type(node) == type)
            return json_object_get_string(get(node, "id"));
    }
    return NULL;
}
static bool graph_add_kind(int index, json_object *row, const char *kind) {
    json_object *nodes = get(get(row, "surface_graph"), "nodes"), *node = json_object_new_object();
    char id[64];
    int serial = 1;
    bool used;
    do {
        snprintf(id, sizeof(id), "%s_%d", kind, serial++);
        used = false;
        for (size_t i = 0; i < json_object_array_length(nodes); ++i)
            if (!strcmp(id, json_object_get_string(get(json_object_array_get_idx(nodes, i), "id"))))
                used = true;
    } while (used);
    json_object_object_add(node, "id", json_object_new_string(id));
    json_object_object_add(node, "kind", json_object_new_string(kind));
    if (!strcmp(kind, "scalar"))
        json_object_object_add(node, "value", json_object_new_double(.5));
    else if (!strcmp(kind, "color"))
        json_object_object_add(node, "value", json_tokener_parse("[0.5,0.5,0.5]"));
    else if (!strcmp(kind, "coordinate")) {
        json_object_object_add(node, "space", json_object_new_string("object_rest"));
        json_object_object_add(node, "scale_m", json_object_new_double(.2));
        json_object_object_add(node, "offset", json_tokener_parse("[0,0,0]"));
    } else {
        json_object *inputs = json_object_new_array();
        json_object_object_add(node, "inputs", inputs);
        int count = !strcmp(kind, "mix") ? 3 : !strcmp(kind, "multiply") ? 2 : 1;
        for (int i = 0; i < count; ++i) {
            int type = !strcmp(kind, "mix") ? (i < 2 ? 1 : 0) : !strcmp(kind, "multiply") ? 0 : 2;
            const char *source = graph_first_type(nodes, type);
            if (!source) {
                snprintf(status, sizeof(status), "Add a %s node first",
                         type == 2   ? "coordinate"
                         : type == 1 ? "color"
                                     : "scalar");
                json_object_put(node);
                return false;
            }
            json_object_array_add(inputs, json_object_new_string(source));
        }
        if (!strcmp(kind, "noise3d"))
            json_object_object_add(node, "seed", json_object_new_int(17));
        if (!strcmp(kind, "triplanar_checker"))
            json_object_object_add(node, "sharpness", json_object_new_double(2));
    }
    int next = (int)json_object_array_length(nodes);
    bool ok = SceneEditorDocumentSurfaceGraphAddNode(index, json_object_to_json_string(node),
                                                     graph_picker_revision, status, sizeof(status));
    if (ok) {
        graph_node_index = next;
        graph_scroll = 0;
    }
    json_object_put(node);
    return ok;
}
static bool graph_panel_click(int index, int x, int y) {
    if (graph_picker)
        return false;
    if (inside(graph_list, x, y)) {
        graph_picker_open(1);
        return true;
    }
    if (inside(graph_add, x, y)) {
        graph_picker_open(2);
        return true;
    }
    json_object *row = current(index), *node = graph_node(row);
    if (!node) {
        if (row)
            json_object_put(row);
        return false;
    }
    bool handled = false;
    if (inside(graph_delete, x, y)) {
        if (SceneEditorDocumentSurfaceGraphDeleteNode(
                index, graph_node_id, SceneEditorDocumentRevision(), status, sizeof(status))) {
            graph_node_index = 0;
            graph_scroll = 0;
            snprintf(status, sizeof(status), "Node deleted; Undo available");
        }
        handled = true;
    } else if (inside(scope_rect, x, y)) {
        const char *space = json_object_get_string(get(node, "space"));
        json_object_object_add(
            node, "space",
            json_object_new_string(!strcmp(space, "world") ? "object_rest" : "world"));
        graph_panel_commit(index, row, SceneEditorDocumentRevision());
        handled = true;
    }
    for (int a = 0; a < 3; ++a)
        if (inside(graph_edges[a], x, y)) {
            graph_input = a;
            graph_picker_open(3);
            handled = true;
        }
    for (int a = 0; a < 2; ++a)
        if (inside(graph_outputs[a], x, y)) {
            graph_picker_open(4 + a);
            handled = true;
        }
    json_object_put(row);
    return handled;
}
static bool graph_panel_event(const SDL_Event *e, int index) {
    if (!RuntimeSurfaceGraphActive(index))
        return false;
    if (e->type == SDL_MOUSEWHEEL) {
        int x, y;
        SDL_GetMouseState(&x, &y);
        if (!inside(graph_bounds, x, y))
            return false;
        if (graph_picker) {
            graph_page -= e->wheel.y;
            if (graph_page < 0)
                graph_page = 0;
        } else {
            cancel();
            graph_scroll -= e->wheel.y;
            int visible = graph_bounds.h / 29;
            int maximum = graph_total_rows - visible;
            if (maximum < 0)
                maximum = 0;
            if (graph_scroll > maximum)
                graph_scroll = maximum;
            if (graph_scroll < 0)
                graph_scroll = 0;
        }
        return true;
    }
    if (!graph_picker)
        return false;
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        graph_picker_close();
        return true;
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            graph_picker_close();
            return true;
        }
        if (e->key.keysym.sym == SDLK_BACKSPACE) {
            size_t n = strlen(graph_search);
            if (n)
                graph_search[n - 1] = 0;
            graph_page = 0;
            return true;
        }
        if (e->key.keysym.sym == SDLK_a && (e->key.keysym.mod & (KMOD_GUI | KMOD_CTRL))) {
            graph_search[0] = 0;
            graph_page = 0;
            return true;
        }
        return true;
    }
    if (e->type == SDL_TEXTINPUT && graph_search_active) {
        if (strlen(graph_search) + strlen(e->text.text) < sizeof(graph_search))
            strcat(graph_search, e->text.text);
        graph_page = 0;
        return true;
    }
    if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT)
        return false;
    int x = e->button.x, y = e->button.y;
    if (inside(graph_cancel_rect, x, y)) {
        graph_picker_close();
        return true;
    }
    if (inside(graph_search_rect, x, y)) {
        graph_search_active = true;
        SDL_StartTextInput();
        return true;
    }
    if (inside(graph_prev, x, y)) {
        if (graph_page)
            --graph_page;
        return true;
    }
    if (inside(graph_next, x, y)) {
        ++graph_page;
        return true;
    }
    for (int a = 0; a < graph_choice_count; ++a)
        if (inside(graph_choices[a], x, y)) {
            if (!graph_choice_valid[a]) {
                snprintf(status, sizeof(status),
                         "Connection would create a cycle; choose another node");
                return true;
            }
            if (RayTracingDeepRenderDesktopHost_HasActiveWork()) {
                snprintf(status, sizeof(status), "Wait for active rendering before editing");
                return true;
            }
            json_object *row = current(index), *node = graph_node(row),
                        *nodes = get(get(row, "surface_graph"), "nodes");
            bool ok = false;
            if (graph_picker == 1) {
                for (size_t j = 0; j < json_object_array_length(nodes); ++j)
                    if (!strcmp(graph_choice_ids[a],
                                json_object_get_string(
                                    get(json_object_array_get_idx(nodes, j), "id")))) {
                        graph_node_index = (int)j;
                        graph_scroll = 0;
                        ok = true;
                        break;
                    }
            } else if (graph_picker == 2)
                ok = graph_add_kind(index, row, graph_choice_ids[a]);
            else if (graph_picker == 3)
                ok = SceneEditorDocumentSurfaceGraphConnect(
                    index, json_object_get_string(get(node, "id")), graph_port(node, graph_input),
                    graph_choice_ids[a], graph_picker_revision, status, sizeof(status));
            else
                ok = SceneEditorDocumentSurfaceGraphSetOutput(
                    index, graph_picker == 4 ? "base_color" : "roughness",
                    graph_choice_ids[a][0] ? graph_choice_ids[a] : NULL, graph_picker_revision,
                    status, sizeof(status));
            if (row)
                json_object_put(row);
            if (ok)
                graph_picker_close();
            return true;
        }
    return inside(graph_bounds, x, y);
}
static bool graph_panel_control(const char *name, SDL_Rect *out) {
    if (!strcmp(name, "node_list"))
        *out = graph_list;
    else if (!strcmp(name, "node_search"))
        *out = graph_search_rect;
    else if (!strcmp(name, "add_node"))
        *out = graph_add;
    else if (!strcmp(name, "delete_node"))
        *out = graph_delete;
    else if (!strcmp(name, "output_color"))
        *out = graph_outputs[0];
    else if (!strcmp(name, "output_roughness"))
        *out = graph_outputs[1];
    else if (!strcmp(name, "picker_cancel"))
        *out = graph_cancel_rect;
    else if (!strcmp(name, "picker_prev"))
        *out = graph_prev;
    else if (!strcmp(name, "picker_next"))
        *out = graph_next;
    else {
        const char *id = strchr(name, ':');
        if (!id || (strncmp(name, "node:", 5) && strncmp(name, "kind:", 5) &&
                    strncmp(name, "candidate:", 10)))
            return false;
        ++id;
        for (int i = 0; i < graph_choice_count; ++i)
            if (!strcmp(id, graph_choice_ids[i])) {
                *out = graph_choices[i];
                return out->w > 0;
            }
        return false;
    }
    return out->w > 0;
}
