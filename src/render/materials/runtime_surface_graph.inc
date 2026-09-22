/* Host boundary: retained JSON -> validated shared program -> immutable
 * per-object frame. */
#include "render/runtime_surface_graph.h"
typedef struct {
    bool active, composition;
    bool region_active[6];
    CoreSurfaceGraph regions[6];
    CoreSurfaceGraph program;
    RuntimeMaterialPayload3D base;
    Vec3 gradient[3], axes[3];
    double offset[3], lengths[3], world_scale;
} SurfaceGraph;
static SurfaceGraph surface_graphs[MAX_OBJECTS];
static const char *sg_kinds[] = {
    "scalar",   "color", "coordinate",  "noise3d",      "triplanar_checker",
    "multiply", "mix",   "image_color", "image_scalar", "roughness_mix"};
const char *RuntimeSurfaceGraphCapabilities(void) {
    return "{\"version\":1,\"capability\":\"optic.surface_graph_v1\",\"max_"
           "nodes\":32,"
           "\"nodes\":[\"scalar\",\"color\",\"coordinate\",\"noise3d\","
           "\"triplanar_checker\","
           "\"multiply\",\"mix\"],"
           "\"spaces\":[\"object_rest\",\"world\"],\"outputs\":[\"base_color\","
           "\"roughness\"],"
           "\"color_space\":\"linear\",\"combinations\":\"exclusive_source\","
           "\"filter\":\"box_checker_noise_fade\",\"secondary_footprint\":"
           "\"source_mean\",\"composition\":{\"version\":2,\"capability\":\"optic.surface_"
           "composition_v1\","
           "\"nodes\":[\"scalar\",\"color\",\"coordinate\",\"noise3d\",\"triplanar_checker\","
           "\"multiply\",\"mix\",\"image_color\",\"image_scalar\",\"roughness_mix\"],"
           "\"image_resources\":[\"base_color\",\"roughness\",\"height\",\"base_color_alpha\"],"
           "\"mapping\":\"single_object_surface_mapping\",\"color_alpha\":\"straight_linear_"
           "explicit_alpha\","
           "\"roughness_mix\":\"rms_material_mixture\",\"regions\":[\"front\",\"back\",\"left\","
           "\"right\",\"top\",\"bottom\"]}}";
}
static int graph_reference(json_object *nodes, json_object *reference) {
    if (!json_object_is_type(reference, json_type_string))
        return -1;
    for (size_t i = 0; i < json_object_array_length(nodes); ++i)
        if (token(json_object_array_get_idx(nodes, i), "id", json_object_get_string(reference)))
            return (int)i;
    return -1;
}
/* Diagnostics stay host-owned: the shared compiler has no JSON or retained IDs.
 */
static bool graph_has_field(json_object *object, const char *key) {
    json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value);
}
static bool graph_error(RuntimeSurfaceGraphDiagnostic *d, const char *node, const char *property,
                        const char *code, const char *message) {
    if (d) {
        snprintf(d->node_id, sizeof(d->node_id), "%s", node ? node : "");
        snprintf(d->property, sizeof(d->property), "%s", property ? property : "");
        snprintf(d->code, sizeof(d->code), "%s", code);
        snprintf(d->message, sizeof(d->message), "%s", message);
    }
    return false;
}
static void graph_format_error(const RuntimeSurfaceGraphDiagnostic *d, char *text, size_t size) {
    if (!text || !size)
        return;
    if (!d->code[0]) {
        text[0] = 0;
        return;
    }
    snprintf(text, size, "surface_graph: code=%s object=%s node=%s property=%s: %s", d->code,
             d->object_id[0] ? d->object_id : "-", d->node_id[0] ? d->node_id : "-", d->property,
             d->message);
}
static const char *graph_node_id(json_object *nodes, int index) {
    json_object *id = field(json_object_array_get_idx(nodes, (size_t)index), "id");
    return json_object_is_type(id, json_type_string) ? json_object_get_string(id) : NULL;
}
static int graph_node_type(CoreSurfaceGraphKind kind) {
    return kind == CORE_SG_COORDINATE
               ? 2
               : (kind == CORE_SG_COLOR || kind == CORE_SG_MIX || kind == CORE_SG_IMAGE_COLOR ? 1
                                                                                              : 0);
}
static int graph_node_arity(CoreSurfaceGraphKind kind) {
    return (kind == CORE_SG_MIX || kind == CORE_SG_ROUGHNESS_MIX) ? 3
           : kind == CORE_SG_MULTIPLY
               ? 2
               : (kind == CORE_SG_NOISE3D || kind == CORE_SG_TRIPLANAR ? 1 : 0);
}
/* Identify the retained edge closing a cycle. Shared prepare still owns
 * executable order. */
static bool graph_check_cycle(const CoreSurfaceGraph *g, json_object *nodes, int index,
                              unsigned char *state, RuntimeSurfaceGraphDiagnostic *d) {
    if (state[index] == 2)
        return true;
    state[index] = 1;
    for (int port = 0; port < graph_node_arity(g->nodes[index].kind); ++port) {
        int target = g->nodes[index].inputs[port];
        char property[32];
        snprintf(property, sizeof(property), "inputs[%d]", port);
        if (state[target] == 1)
            return graph_error(d, graph_node_id(nodes, index), property, "cycle",
                               "Connect this input to a node that does not depend on this node.");
        if (!graph_check_cycle(g, nodes, target, state, d))
            return false;
    }
    state[index] = 2;
    return true;
}
static bool graph_regions(json_object *source, json_object *object, const char *id,
                          const CoreSurfaceGraph *base, CoreSurfaceGraph programs[6],
                          bool active[6], RuntimeSurfaceGraphDiagnostic *d);
bool RuntimeSurfaceGraphParseDetailed(json_object *source, const char *object_id,
                                      CoreSurfaceGraph *out,
                                      RuntimeSurfaceGraphDiagnostic *diagnostic) {
    RuntimeSurfaceGraphDiagnostic local = {0};
    RuntimeSurfaceGraphDiagnostic *d = diagnostic ? diagnostic : &local;
    memset(d, 0, sizeof(*d));
    snprintf(d->object_id, sizeof(d->object_id), "%s", object_id ? object_id : "");
    if (out)
        memset(out, 0, sizeof(*out));
    CoreSurfaceGraph g = {0};
    g.roughness_output = -1;
    if (!json_object_is_type(source, json_type_object))
        return graph_error(d, NULL, "surface_graph", "invalid_type", "Provide a graph object.");
    bool composition = token(source, "required_capability", "optic.surface_composition_v1");
    if (!composition && !token(source, "required_capability", "optic.surface_graph_v1"))
        return graph_error(d, NULL, "required_capability", "unsupported_capability",
                           "Use optic.surface_graph_v1 or optic.surface_composition_v1.");
    if (!json_object_is_type(field(source, "version"), json_type_int) ||
        json_object_get_int(field(source, "version")) != (composition ? 2 : 1))
        return graph_error(d, NULL, "version", "unsupported_version",
                           "Use graph version 1, or version 2 for surface composition.");
    if (!composition && graph_has_field(source, "regions"))
        return graph_error(d, NULL, "regions", "unsupported_feature",
                           "Regions require surface composition version 2.");
    if (!token(source, "color_space", "linear"))
        return graph_error(d, NULL, "color_space", "unsupported_encoding",
                           "Declare linear graph colors.");
    if (graph_has_field(source, "required_features"))
        return graph_error(d, NULL, "required_features", "unsupported_feature",
                           "This renderer cannot execute additional required graph features.");
    json_object *nodes = field(source, "nodes"), *outputs = field(source, "outputs");
    if (!json_object_is_type(nodes, json_type_array))
        return graph_error(d, NULL, "nodes", "invalid_type", "Provide an array of graph nodes.");
    g.count = json_object_array_length(nodes);
    if (!g.count || g.count > CORE_SURFACE_GRAPH_MAX_NODES)
        return graph_error(d, NULL, "nodes", "node_limit", "Provide between 1 and 32 nodes.");
    if (!json_object_is_type(outputs, json_type_object))
        return graph_error(d, NULL, "outputs", "invalid_type", "Provide named graph outputs.");
    for (size_t i = 0; i < g.count; ++i) {
        json_object *node = json_object_array_get_idx(nodes, i), *id = field(node, "id"),
                    *inputs = field(node, "inputs");
        const char *name = graph_node_id(nodes, (int)i);
        char property[48];
        snprintf(property, sizeof(property), "nodes[%zu].id", i);
        if (!json_object_is_type(id, json_type_string) || !name[0] || strlen(name) >= 64)
            return graph_error(d, NULL, property, "invalid_node_id",
                               "Use a unique node ID of 1 to 63 bytes.");
        for (size_t j = 0; j < i; ++j)
            if (token(json_object_array_get_idx(nodes, j), "id", name))
                return graph_error(d, name, "id", "duplicate_node_id",
                                   "Give this node a unique ID and update its references.");
        CoreSurfaceGraphNode *n = &g.nodes[i];
        n->kind = (CoreSurfaceGraphKind)-1;
        for (int k = 0; k < (composition ? 10 : 7); ++k)
            if (token(node, "kind", sg_kinds[k]))
                n->kind = (CoreSurfaceGraphKind)k;
        if ((int)n->kind < 0)
            return graph_error(d, name, "kind", "unsupported_node",
                               "Choose a node kind listed by --surface-capabilities.");
        if (graph_has_field(node, "required_features"))
            return graph_error(d, name, "required_features", "unsupported_feature",
                               "This renderer cannot execute additional required node features.");
        if (n->kind == CORE_SG_IMAGE_COLOR || n->kind == CORE_SG_IMAGE_SCALAR) {
            if (graph_has_field(node, "mapping") || graph_has_field(node, "space") ||
                graph_has_field(node, "coordinates"))
                return graph_error(d, name, "mapping", "unsupported_mapping",
                                   "Image nodes use the shared object surface mapping.");
            if (n->kind == CORE_SG_IMAGE_COLOR && token(node, "resource", "base_color"))
                n->resource = CORE_SG_RESOURCE_BASE_COLOR;
            else if (n->kind == CORE_SG_IMAGE_SCALAR && token(node, "resource", "roughness"))
                n->resource = CORE_SG_RESOURCE_ROUGHNESS;
            else if (n->kind == CORE_SG_IMAGE_SCALAR && token(node, "resource", "height"))
                n->resource = CORE_SG_RESOURCE_HEIGHT;
            else if (n->kind == CORE_SG_IMAGE_SCALAR && token(node, "resource", "base_color_alpha"))
                n->resource = CORE_SG_RESOURCE_BASE_COLOR_ALPHA;
            else
                return graph_error(d, name, "resource", "unsupported_resource",
                                   "Choose base_color for color, or roughness, height, "
                                   "base_color_alpha for scalar.");
        }
        int arity = graph_node_arity(n->kind);
        if ((arity || inputs) && (!json_object_is_type(inputs, json_type_array) ||
                                  json_object_array_length(inputs) != (size_t)arity))
            return graph_error(d, name, "inputs", "input_count",
                               "Provide the declared number of ordered inputs for this node kind.");
        for (int port = 0; port < arity; ++port) {
            n->inputs[port] =
                graph_reference(nodes, json_object_array_get_idx(inputs, (size_t)port));
            if (n->inputs[port] < 0) {
                snprintf(property, sizeof(property), "inputs[%d]", port);
                return graph_error(d, name, property, "missing_reference",
                                   "Connect this input to an existing node ID.");
            }
        }
        if (n->kind == CORE_SG_SCALAR || n->kind == CORE_SG_COLOR) {
            if ((n->kind == CORE_SG_SCALAR && !number(field(node, "value"), &n->value[0])) ||
                (n->kind == CORE_SG_COLOR && !vector(node, "value", n->value, 3)))
                return graph_error(d, name, "value", "invalid_type",
                                   "Provide finite scalar or three-component color values.");
            for (int component = 0; component < (n->kind == CORE_SG_COLOR ? 3 : 1); ++component)
                if (n->value[component] < 0 || n->value[component] > 1) {
                    snprintf(property, sizeof(property), "value[%d]", component);
                    return graph_error(d, name, n->kind == CORE_SG_SCALAR ? "value" : property,
                                       "parameter_range", "Use a value between 0 and 1.");
                }
        }
        if (n->kind == CORE_SG_COORDINATE) {
            if (!token(node, "space", "object_rest") && !token(node, "space", "world"))
                return graph_error(d, name, "space", "unsupported_space",
                                   "Choose object_rest or world coordinates.");
            n->world_space = token(node, "space", "world");
            if (!number(field(node, "scale_m"), &n->scale_m) || n->scale_m < 1e-6 ||
                n->scale_m > 1e6)
                return graph_error(d, name, "scale_m", "parameter_range",
                                   "Use a finite scale from 1e-6 to 1e6 meters.");
            if (!vector(node, "offset", n->value, 3))
                return graph_error(d, name, "offset", "invalid_type",
                                   "Provide three finite dimensionless offsets.");
            for (int component = 0; component < 3; ++component)
                if (fabs(n->value[component]) > 1e6) {
                    snprintf(property, sizeof(property), "offset[%d]", component);
                    return graph_error(d, name, property, "parameter_range",
                                       "Use an offset between -1e6 and 1e6.");
                }
        }
        if (n->kind == CORE_SG_NOISE3D) {
            double seed = 0;
            if (!number(field(node, "seed"), &seed) || seed < 0 || seed > 4294967295.0 ||
                floor(seed) != seed)
                return graph_error(d, name, "seed", "parameter_range",
                                   "Use an integer seed from 0 to 4294967295.");
            n->seed = (uint32_t)seed;
        }
        if (n->kind == CORE_SG_TRIPLANAR && (!number(field(node, "sharpness"), &n->sharpness) ||
                                             n->sharpness < 1 || n->sharpness > 16))
            return graph_error(d, name, "sharpness", "parameter_range",
                               "Use finite projection sharpness from 1 to 16.");
    }
    /* Check input types after all declared node kinds are known. */
    for (size_t i = 0; i < g.count; ++i) {
        const CoreSurfaceGraphNode *n = &g.nodes[i];
        for (int port = 0; port < graph_node_arity(n->kind); ++port) {
            int expected = n->kind == CORE_SG_MIX ? (port < 2 ? 1 : 0)
                           : (n->kind == CORE_SG_MULTIPLY || n->kind == CORE_SG_ROUGHNESS_MIX) ? 0
                                                                                               : 2;
            if (graph_node_type(g.nodes[n->inputs[port]].kind) != expected) {
                char property[32];
                snprintf(property, sizeof(property), "inputs[%d]", port);
                return graph_error(d, graph_node_id(nodes, (int)i), property, "input_type",
                                   expected == 0   ? "Connect a scalar-output node."
                                   : expected == 1 ? "Connect a color-output node."
                                                   : "Connect a coordinate-output node.");
            }
        }
    }
    g.color_output = graph_reference(nodes, field(outputs, "base_color"));
    if (g.color_output < 0)
        return graph_error(d, NULL, "outputs.base_color", "missing_reference",
                           "Reference an existing color-output node.");
    if (graph_node_type(g.nodes[g.color_output].kind) != 1)
        return graph_error(d, graph_node_id(nodes, g.color_output), "outputs.base_color",
                           "output_type", "Reference a color-output node.");
    if (field(outputs, "roughness")) {
        g.roughness_output = graph_reference(nodes, field(outputs, "roughness"));
        if (g.roughness_output < 0)
            return graph_error(d, NULL, "outputs.roughness", "missing_reference",
                               "Reference an existing scalar-output node.");
        if (graph_node_type(g.nodes[g.roughness_output].kind) != 0)
            return graph_error(d, graph_node_id(nodes, g.roughness_output), "outputs.roughness",
                               "output_type", "Reference a scalar-output node.");
    }
    json_object_object_foreach(outputs, key, value) {
        (void)value;
        if (strcmp(key, "base_color") && strcmp(key, "roughness")) {
            char property[96];
            snprintf(property, sizeof(property), "outputs.%s", key);
            return graph_error(d, NULL, property, "unsupported_output",
                               "This renderer supports base_color and roughness outputs.");
        }
    }
    unsigned char state[CORE_SURFACE_GRAPH_MAX_NODES] = {0};
    for (size_t i = 0; i < g.count; ++i)
        if (!graph_check_cycle(&g, nodes, (int)i, state, d))
            return false;
    char shared_diagnostic[160] = {0};
    if (!core_surface_graph_prepare(&g, shared_diagnostic, sizeof(shared_diagnostic)))
        return graph_error(d, NULL, "surface_graph", "compiler_rejected", shared_diagnostic);
    if (composition) {
        CoreSurfaceGraph regions[6];
        bool active[6] = {false};
        if (!graph_regions(source, NULL, object_id, &g, regions, active, d))
            return false;
    }
    if (out)
        *out = g;
    memset(d, 0, sizeof(*d));
    return true;
}
bool RuntimeSurfaceGraphParse(json_object *source, CoreSurfaceGraph *out, char *diagnostic,
                              size_t size) {
    RuntimeSurfaceGraphDiagnostic detail;
    bool ok = RuntimeSurfaceGraphParseDetailed(source, NULL, out, &detail);
    graph_format_error(&detail, diagnostic, size);
    return ok;
}
/* Region outputs override each channel independently; node program remains shared in meaning. */
static bool graph_regions(json_object *source, json_object *object, const char *id,
                          const CoreSurfaceGraph *base, CoreSurfaceGraph programs[6],
                          bool active[6], RuntimeSurfaceGraphDiagnostic *d) {
    json_object *regions = field(source, "regions"), *nodes = field(source, "nodes");
    if (!regions)
        return true;
    if (!json_object_is_type(regions, json_type_array) || json_object_array_length(regions) > 6)
        return graph_error(d, NULL, "regions", "region_limit",
                           "Provide at most six primitive face overrides.");
    if (token(object, "object_type", "mesh_asset_instance"))
        return graph_error(d, NULL, "regions", "unsupported_geometry",
                           "Face-role overrides require a plane or prism.");
    for (size_t i = 0; i < json_object_array_length(regions); ++i) {
        json_object *region = json_object_array_get_idx(regions, i),
                    *outputs = field(region, "outputs");
        int face = region_face(region);
        if (face < 0 || (token(object, "object_type", "plane_primitive") && face != 0) ||
            active[face])
            return graph_error(d, NULL, "regions.face_role", "invalid_region",
                               "Use a unique supported primitive face role (plane: front only).");
        if (!json_object_is_type(outputs, json_type_object) || !json_object_object_length(outputs))
            return graph_error(d, NULL, "regions.outputs", "invalid_type",
                               "Provide at least one named output override.");
        programs[face] = *base;
        json_object_object_foreach(outputs, key, value) {
            int expected = !strcmp(key, "base_color") ? 1 : !strcmp(key, "roughness") ? 0 : -1;
            int reference = graph_reference(nodes, value);
            if (expected < 0 || reference < 0 ||
                graph_node_type(base->nodes[reference].kind) != expected) {
                char property[96];
                snprintf(property, sizeof(property), "regions[%zu].outputs.%s", i, key);
                return graph_error(
                    d, reference >= 0 ? graph_node_id(nodes, reference) : NULL, property,
                    "invalid_region_output",
                    "Reference an existing node with the output's color or scalar type.");
            }
            if (expected == 1)
                programs[face].color_output = reference;
            else
                programs[face].roughness_output = reference;
        }
        if (!core_surface_graph_prepare(&programs[face], NULL, 0))
            return graph_error(d, NULL, "regions.outputs", "compiler_rejected",
                               "Invalid region graph outputs.");
        active[face] = true;
    }
    (void)id;
    return true;
}
static bool graph_validate_scene(json_object *root, char *diagnostic, size_t size) {
    json_object *rows = field(field(field(field(root, "extensions"), "ray_tracing"), "authoring"),
                              "object_materials"),
                *objects = field(root, "objects");
    for (size_t i = 0;
         json_object_is_type(rows, json_type_array) && i < json_object_array_length(rows); ++i) {
        json_object *row = json_object_array_get_idx(rows, i), *source = NULL;
        if (!json_object_object_get_ex(row, "surface_graph", &source))
            continue;
        RuntimeSurfaceGraphDiagnostic d = {0};
        CoreSurfaceGraph g;
        json_object *identity = field(row, "object_id");
        const char *id = json_object_is_type(identity, json_type_string)
                             ? json_object_get_string(identity)
                             : NULL;
        snprintf(d.object_id, sizeof(d.object_id), "%s", id ? id : "");
        if (!id || !id[0]) {
            graph_error(&d, NULL, "object_id", "invalid_object_id",
                        "Name the scene object receiving this graph.");
            goto failed;
        }
        if (!RuntimeSurfaceGraphParseDetailed(source, id, &g, &d))
            goto failed;
        /* Successful graph parsing clears diagnostics; restore the enclosing
         * identity. */
        snprintf(d.object_id, sizeof(d.object_id), "%s", id);
        int matches = 0, row_matches = 0;
        json_object *object = NULL;
        for (size_t j = 0; j < json_object_array_length(rows); ++j)
            if (token(json_object_array_get_idx(rows, j), "object_id", id))
                ++row_matches;
        for (size_t j = 0;
             json_object_is_type(objects, json_type_array) && j < json_object_array_length(objects);
             ++j) {
            json_object *candidate = json_object_array_get_idx(objects, j);
            if (token(candidate, "object_id", id)) {
                object = candidate;
                ++matches;
            }
        }
        if (matches != 1 || row_matches != 1) {
            graph_error(&d, NULL, "object_id",
                        matches == 0 ? "missing_object" : "duplicate_object_id",
                        "Provide exactly one matching scene object and one material row.");
            goto failed;
        }
        if (!token(object, "object_type", "mesh_asset_instance") &&
            !token(object, "object_type", "plane_primitive") &&
            !token(object, "object_type", "rect_prism_primitive")) {
            graph_error(&d, NULL, "object_type", "unsupported_geometry",
                        "Use a mesh instance, plane or rectangular prism.");
            goto failed;
        }
        double dims[3];
        if (!token(object, "object_type", "mesh_asset_instance") &&
            (!dimensions(object, dims) || !frame_supported(object))) {
            graph_error(&d, NULL, "primitive", "unsupported_frame",
                        "Use valid primitive dimensions and an orthonormal frame.");
            goto failed;
        }
        bool composition = token(source, "required_capability", "optic.surface_composition_v1");
        CoreSurfaceGraph region_programs[6];
        bool region_active[6] = {false};
        if (!graph_regions(source, object, id, &g, region_programs, region_active, &d))
            goto failed;
        json_object *ray = field(field(object, "extensions"), "ray_tracing");
        if (composition) {
            if (!field(ray, "surface_mapping") || !field(ray, "surface_sampling")) {
                graph_error(&d, NULL, "surface_sampling", "missing_mapping",
                            "Composition requires surface_mapping and surface_sampling.");
                goto failed;
            }
            json_object *channels = field(field(ray, "surface_sampling"), "channels");
            const char *resources[] = {"base_color", "roughness", "height", "base_color"};
            for (size_t n = 0; n < g.count; ++n)
                if (g.nodes[n].kind == CORE_SG_IMAGE_COLOR ||
                    g.nodes[n].kind == CORE_SG_IMAGE_SCALAR)
                    if (!field(channels, resources[g.nodes[n].resource])) {
                        graph_error(
                            &d, graph_node_id(field(source, "nodes"), (int)n), "resource",
                            "missing_resource",
                            "Declare this image channel in object surface_sampling.channels.");
                        goto failed;
                    }
        }
        const char *object_conflicts[] = {"surface_mapping", "surface_sampling",
                                          "procedural_solid_material_ref"};
        for (size_t c = 0; c < sizeof(object_conflicts) / sizeof(*object_conflicts); ++c)
            if ((!composition || c == 2) && field(c == 2 ? object : ray, object_conflicts[c])) {
                graph_error(&d, NULL, object_conflicts[c], "mixed_source",
                            "Keep one supported material source; replace this "
                            "conflicting declaration explicitly.");
                goto failed;
            }
        json_object *transform = field(object, "transform");
        const char *components[] = {"scale", "position", "rotation"};
        for (int c = 0; c < 3; ++c)
            for (int axis = 0; axis < 3; ++axis) {
                char key[] = {"xyz"[axis], 0};
                json_object *value = field(field(transform, components[c]), key);
                double number_value = 0;
                if (value &&
                    (!number(value, &number_value) || fabs(number_value) > (c == 1 ? 1e9 : 1e6) ||
                     (c == 0 && fabs(number_value) < 1e-6))) {
                    char property[64];
                    snprintf(property, sizeof(property), "transform.%s.%s", components[c], key);
                    graph_error(&d, NULL, property, "transform_range",
                                "Use a finite supported transform; scale magnitudes must "
                                "be 1e-6 to 1e6.");
                    goto failed;
                }
            }
        if (field(transform, "pivot_policy") &&
            !token(transform, "pivot_policy", "authored_origin")) {
            graph_error(&d, NULL, "transform.pivot_policy", "unsupported_pivot",
                        "Use authored_origin for graph coordinates.");
            goto failed;
        }
        const char *conflicts[] = {"material_graph",           "materialGraph",
                                   "material_texture_stack",   "materialTextureStack",
                                   "surface_material_binding", "authored_texture",
                                   "procedural_texture",       "texture_id"};
        for (size_t c = 0; c < sizeof(conflicts) / sizeof(*conflicts); ++c)
            if (field(row, conflicts[c])) {
                graph_error(&d, NULL, conflicts[c], "mixed_source",
                            "Keep one supported material source; replace this "
                            "conflicting declaration explicitly.");
                goto failed;
            }
        double scale = 0;
        if (!number(field(root, "world_scale"), &scale) || scale < 1e-6 || scale > 1e6) {
            graph_error(&d, NULL, "world_scale", "parameter_range",
                        "Use a finite world scale from 1e-6 to 1e6.");
            goto failed;
        }
        continue;
    failed:
        graph_format_error(&d, diagnostic, size);
        return false;
    }
    return true;
}
static bool graph_prepare_scene(json_object *root, double world_scale,
                                SurfaceGraph *surface_graphs) {
    memset(surface_graphs, 0, sizeof(*surface_graphs) * MAX_OBJECTS);
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    json_object *objects = field(root, "objects");
    for (int index = 0; index < sceneSettings.objectCount; ++index) {
        char id[64] = {0};
        runtime_scene_bridge_get_last_object_id_for_scene_index(index, id, sizeof(id));
        json_object *row = material_row(root, id), *source = field(row, "surface_graph");
        if (!source)
            continue;
        SurfaceGraph *s = &surface_graphs[index];
        if (!RuntimeSurfaceGraphParse(source, &s->program, NULL, 0) ||
            !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(index, &s->base))
            goto failed;
        json_object *o = NULL;
        for (size_t j = 0; j < json_object_array_length(objects); ++j)
            if (token(json_object_array_get_idx(objects, j), "object_id", id))
                o = json_object_array_get_idx(objects, j);
        s->composition = token(source, "required_capability", "optic.surface_composition_v1");
        RuntimeSurfaceGraphDiagnostic region_diagnostic = {0};
        if (!graph_regions(source, o, id, &s->program, s->regions, s->region_active,
                           &region_diagnostic))
            goto failed;
        double angles[3] = {0}, scale[3] = {1, 1, 1}, pos[3] = {0};
        json_object *t = field(o, "transform");
        for (int a = 0; a < 3; ++a) {
            char key[] = {"xyz"[a], 0};
            number(field(field(t, "rotation"), key), &angles[a]);
            angles[a] *= .01745329251994329577;
            number(field(field(t, "scale"), key), &scale[a]);
            number(field(field(t, "position"), key), &pos[a]);
        }
        for (int a = 0; a < 3; ++a) {
            double x = a == 0, y = a == 1, z = a == 2, v;
            v = y * cos(angles[0]) - z * sin(angles[0]);
            z = y * sin(angles[0]) + z * cos(angles[0]);
            y = v;
            v = x * cos(angles[1]) + z * sin(angles[1]);
            z = -x * sin(angles[1]) + z * cos(angles[1]);
            x = v;
            v = x * cos(angles[2]) - y * sin(angles[2]);
            y = x * sin(angles[2]) + y * cos(angles[2]);
            x = v;
            s->axes[a] = vec3(x, y, z);
            s->lengths[a] = scale[a] * world_scale;
        }
        Vec3 origin = vec3(pos[0] * world_scale, pos[1] * world_scale, pos[2] * world_scale);
        for (int j = 0; j < seeds.primitive_count; ++j)
            if (seeds.primitives[j].scene_object_index == index) {
                const RuntimeSceneBridgePrimitiveSeed *p = &seeds.primitives[j];
                double dims[3];
                if (!dimensions(o, dims))
                    goto failed;
                origin = vec3(p->origin_x, p->origin_y, p->origin_z);
                s->axes[0] = vec3(p->axis_u_x, p->axis_u_y, p->axis_u_z);
                s->axes[1] = vec3(p->axis_v_x, p->axis_v_y, p->axis_v_z);
                s->axes[2] = vec3(p->normal_x, p->normal_y, p->normal_z);
                s->lengths[0] = p->width / dims[0];
                s->lengths[1] = p->height / dims[1];
                s->lengths[2] = p->depth > 1e-12 ? p->depth / dims[2] : world_scale;
            }
        for (int a = 0; a < 3; ++a) {
            if (fabs(s->lengths[a]) < 1e-12)
                goto failed;
            s->gradient[a] = vec3_scale(s->axes[a], 1 / s->lengths[a]);
            s->offset[a] = -vec3_dot(origin, s->gradient[a]);
        }
        s->world_scale = world_scale;
        s->active = true;
    }
    return true;
failed:
    memset(surface_graphs, 0, sizeof(*surface_graphs) * MAX_OBJECTS);
    return false;
}
bool RuntimeSurfaceGraphActive(int index) {
    return animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE && index >= 0 &&
           index < MAX_OBJECTS && surface_graphs[index].active;
}
bool RuntimeSurfaceGraphCompositionActive(int index) {
    return RuntimeSurfaceGraphActive(index) && surface_graphs[index].composition;
}
bool RuntimeSurfaceGraphResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *out) {
    if (!hit || !out || !RuntimeSurfaceGraphActive(hit->sceneObjectIndex))
        return false;
    const SurfaceGraph *s = &surface_graphs[hit->sceneObjectIndex];
    CoreSurfaceGraphQuery q = {0};
    double p[] = {hit->position.x, hit->position.y, hit->position.z};
    Vec3 normal =
        hit->hasUnperturbedShadingNormal ? hit->unperturbedShadingNormal : hit->shadingNormal;
    double n[] = {normal.x, normal.y, normal.z},
           dx[] = {hit->pixelDpDx.x, hit->pixelDpDx.y, hit->pixelDpDx.z},
           dy[] = {hit->pixelDpDy.x, hit->pixelDpDy.y, hit->pixelDpDy.z};
    for (int a = 0; a < 3; ++a) {
        q.position[1][a] = p[a] / s->world_scale;
        q.normal[1][a] = n[a];
        q.position[0][a] = vec3_dot(hit->position, s->gradient[a]) + s->offset[a];
        q.normal[0][a] = vec3_dot(normal, s->axes[a]) * s->lengths[a];
        if (hit->hasPixelFootprint) {
            q.dx[1][a] = dx[a] / s->world_scale;
            q.dy[1][a] = dy[a] / s->world_scale;
            q.dx[0][a] = vec3_dot(hit->pixelDpDx, s->gradient[a]);
            q.dy[0][a] = vec3_dot(hit->pixelDpDy, s->gradient[a]);
        }
    }
    double length = hypot(hypot(q.normal[0][0], q.normal[0][1]), q.normal[0][2]);
    if (length > 0)
        for (int a = 0; a < 3; ++a)
            q.normal[0][a] /= length;
    q.unbounded = hit->footprintUnbounded;
    CoreSurfaceGraphResult result;
    const CoreSurfaceGraph *program = &s->program;
    bool has_regions = false;
    for (int i = 0; i < 6; ++i)
        has_regions |= s->region_active[i];
    if (has_regions) {
        int face;
        double u, v;
        if (!RuntimeSurfaceMaterialPrimitiveIsland(hit->sceneObjectIndex, hit->position,
                                                   hit->geometricNormal, &face, &u, &v))
            return false;
        if (face >= 0 && face < 6 && s->region_active[face])
            program = &s->regions[face];
    }
    CoreSurfaceGraphInputs inputs = {0};
    if (s->composition && !RuntimeSurfaceSamplingGraphInputs(hit, program, &inputs))
        return false;
    if (!core_surface_graph_evaluate_with_inputs(program, &q, s->composition ? &inputs : NULL,
                                                 &result))
        return false;
    *out = s->base;
    out->baseColorR = out->bsdf.baseColorR = result.color[0];
    out->baseColorG = out->bsdf.baseColorG = result.color[1];
    out->baseColorB = out->bsdf.baseColorB = result.color[2];
    if (result.has_roughness)
        out->bsdf.roughness = result.roughness;
    return !s->composition || RuntimeSurfaceSamplingApplyResponse(hit, out);
}
