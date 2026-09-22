/* Host boundary: retained JSON -> validated shared program -> immutable per-object frame. */
#include "render/runtime_surface_graph.h"
typedef struct {
    bool active;
    CoreSurfaceGraph program;
    RuntimeMaterialPayload3D base;
    Vec3 gradient[3], axes[3];
    double offset[3], lengths[3], world_scale;
} SurfaceGraph;
static SurfaceGraph surface_graphs[MAX_OBJECTS];
static const char *sg_kinds[] = {"scalar",   "color", "coordinate", "noise3d", "triplanar_checker",
                                 "multiply", "mix"};
const char *RuntimeSurfaceGraphCapabilities(void) {
    return "{\"version\":1,\"capability\":\"optic.surface_graph_v1\",\"max_nodes\":32,"
           "\"nodes\":[\"scalar\",\"color\",\"coordinate\",\"noise3d\",\"triplanar_checker\","
           "\"multiply\",\"mix\"],"
           "\"spaces\":[\"object_rest\",\"world\"],\"outputs\":[\"base_color\",\"roughness\"],"
           "\"color_space\":\"linear\",\"combinations\":\"exclusive_source\","
           "\"filter\":\"box_checker_noise_fade\",\"secondary_footprint\":\"source_mean\"}";
}
static int graph_reference(json_object *nodes, json_object *reference) {
    if (!json_object_is_type(reference, json_type_string))
        return -1;
    for (size_t i = 0; i < json_object_array_length(nodes); ++i)
        if (token(json_object_array_get_idx(nodes, i), "id", json_object_get_string(reference)))
            return (int)i;
    return -1;
}
bool RuntimeSurfaceGraphParse(json_object *source, CoreSurfaceGraph *out, char *diagnostic,
                              size_t size) {
    CoreSurfaceGraph g = {0};
    g.roughness_output = -1;
    json_object *nodes = field(source, "nodes"), *outputs = field(source, "outputs");
    bool ok = token(source, "required_capability", "optic.surface_graph_v1") &&
              json_object_is_type(field(source, "version"), json_type_int) &&
              json_object_get_int(field(source, "version")) == 1 &&
              token(source, "color_space", "linear") &&
              json_object_is_type(nodes, json_type_array) &&
              json_object_is_type(outputs, json_type_object) && !field(source, "required_features");
    if (ok)
        g.count = json_object_array_length(nodes);
    ok = ok && g.count > 0 && g.count <= CORE_SURFACE_GRAPH_MAX_NODES;
    for (size_t i = 0; ok && i < g.count; ++i) {
        json_object *node = json_object_array_get_idx(nodes, i), *id = field(node, "id"),
                    *inputs = field(node, "inputs");
        CoreSurfaceGraphNode *n = &g.nodes[i];
        n->kind = (CoreSurfaceGraphKind)-1;
        ok = json_object_is_type(id, json_type_string) && strlen(json_object_get_string(id)) > 0 &&
             strlen(json_object_get_string(id)) < 64;
        for (size_t j = 0; j < i; ++j)
            if (token(json_object_array_get_idx(nodes, j), "id", json_object_get_string(id)))
                ok = false;
        for (int k = 0; k < 7; ++k)
            if (token(node, "kind", sg_kinds[k]))
                n->kind = (CoreSurfaceGraphKind)k;
        int arity = n->kind == CORE_SG_MIX ? 3
                    : n->kind == CORE_SG_MULTIPLY
                        ? 2
                        : (n->kind == CORE_SG_NOISE3D || n->kind == CORE_SG_TRIPLANAR ? 1 : 0);
        if (arity)
            ok = ok && json_object_is_type(inputs, json_type_array) &&
                 json_object_array_length(inputs) == (size_t)arity;
        else if (inputs)
            ok = ok && json_object_is_type(inputs, json_type_array) &&
                 json_object_array_length(inputs) == 0;
        for (int a = 0; ok && a < arity; ++a)
            n->inputs[a] = graph_reference(nodes, json_object_array_get_idx(inputs, a));
        if (n->kind == CORE_SG_SCALAR)
            ok = ok && number(field(node, "value"), &n->value[0]);
        if (n->kind == CORE_SG_COLOR)
            ok = ok && vector(node, "value", n->value, 3);
        if (n->kind == CORE_SG_COORDINATE) {
            ok = ok && (token(node, "space", "object_rest") || token(node, "space", "world")) &&
                 number(field(node, "scale_m"), &n->scale_m) && vector(node, "offset", n->value, 3);
            n->world_space = token(node, "space", "world");
        }
        if (n->kind == CORE_SG_NOISE3D) {
            double seed = 0;
            ok = ok && number(field(node, "seed"), &seed) && seed >= 0 && seed <= 4294967295.0 &&
                 floor(seed) == seed;
            if (ok)
                n->seed = (uint32_t)seed;
        }
        if (n->kind == CORE_SG_TRIPLANAR)
            ok = ok && number(field(node, "sharpness"), &n->sharpness);
        if (field(node, "required_features"))
            ok = false;
    }
    if (ok) {
        g.color_output = graph_reference(nodes, field(outputs, "base_color"));
        if (field(outputs, "roughness")) {
            g.roughness_output = graph_reference(nodes, field(outputs, "roughness"));
            if (g.roughness_output < 0)
                ok = false;
        }
        json_object_object_foreach(outputs, key, value) {
            (void)value;
            if (strcmp(key, "base_color") && strcmp(key, "roughness"))
                ok = false;
        }
    }
    if (ok)
        ok = core_surface_graph_prepare(&g, diagnostic, size);
    if (!ok && diagnostic && size)
        snprintf(
            diagnostic, size,
            "surface_graph: invalid or unsupported capability, node, edge, parameter or output");
    if (out)
        *out = g;
    return ok;
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
        CoreSurfaceGraph g;
        bool ok = RuntimeSurfaceGraphParse(source, &g, diagnostic, size);
        int matches = 0, row_matches = 0;
        const char *id = json_object_get_string(field(row, "object_id"));
        for (size_t j = 0; id && j < json_object_array_length(rows); ++j)
            if (token(json_object_array_get_idx(rows, j), "object_id", id))
                ++row_matches;
        for (size_t j = 0; id && json_object_is_type(objects, json_type_array) &&
                           j < json_object_array_length(objects);
             ++j) {
            json_object *o = json_object_array_get_idx(objects, j);
            if (!token(o, "object_id", id))
                continue;
            ++matches;
            ok = ok && (token(o, "object_type", "mesh_asset_instance") ||
                        token(o, "object_type", "plane_primitive") ||
                        token(o, "object_type", "rect_prism_primitive"));
            double dims[3];
            if (!token(o, "object_type", "mesh_asset_instance"))
                ok = ok && dimensions(o, dims) && frame_supported(o);
            json_object *ray = field(field(o, "extensions"), "ray_tracing");
            ok = ok && !field(ray, "surface_mapping") && !field(ray, "surface_sampling") &&
                 !field(o, "procedural_solid_material_ref");
            json_object *t = field(o, "transform");
            for (int a = 0; a < 3; ++a) {
                char key[] = {"xyz"[a], 0};
                double s = 1, p = 0, r = 0;
                if (field(field(t, "scale"), key))
                    ok = ok && number(field(field(t, "scale"), key), &s) && fabs(s) >= 1e-6 &&
                         fabs(s) <= 1e6;
                if (field(field(t, "position"), key))
                    ok = ok && number(field(field(t, "position"), key), &p) && fabs(p) <= 1e9;
                if (field(field(t, "rotation"), key))
                    ok = ok && number(field(field(t, "rotation"), key), &r) && fabs(r) <= 1e6;
            }
            if (field(t, "pivot_policy") && !token(t, "pivot_policy", "authored_origin"))
                ok = false;
        }
        const char *conflicts[] = {"material_graph",           "materialGraph",
                                   "material_texture_stack",   "materialTextureStack",
                                   "surface_material_binding", "authored_texture",
                                   "procedural_texture",       "texture_id"};
        for (size_t c = 0; c < sizeof(conflicts) / sizeof(*conflicts); ++c)
            if (field(row, conflicts[c]))
                ok = false;
        double scale = 0;
        ok = ok && number(field(root, "world_scale"), &scale) && scale >= 1e-6 && scale <= 1e6;
        if (!ok || matches != 1 || row_matches != 1) {
            snprintf(
                diagnostic, size,
                "surface_graph: unsupported geometry, transform, mixed source, identity or graph");
            return false;
        }
    }
    return true;
}
static bool graph_prepare_scene(json_object *root, double world_scale) {
    memset(surface_graphs, 0, sizeof(surface_graphs));
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
    memset(surface_graphs, 0, sizeof(surface_graphs));
    return false;
}
bool RuntimeSurfaceGraphActive(int index) {
    return animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE && index >= 0 &&
           index < MAX_OBJECTS && surface_graphs[index].active;
}
bool RuntimeSurfaceGraphResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *out) {
    if (!hit || !out || !RuntimeSurfaceGraphActive(hit->sceneObjectIndex))
        return false;
    const SurfaceGraph *s = &surface_graphs[hit->sceneObjectIndex];
    CoreSurfaceGraphQuery q = {0};
    double p[] = {hit->position.x, hit->position.y, hit->position.z};
    Vec3 normal = hit->shadingNormal;
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
    if (!core_surface_graph_evaluate(&s->program, &q, &result))
        return false;
    *out = s->base;
    out->baseColorR = out->bsdf.baseColorR = result.color[0];
    out->baseColorG = out->bsdf.baseColorG = result.color[1];
    out->baseColorB = out->bsdf.baseColorB = result.color[2];
    if (result.has_roughness)
        out->bsdf.roughness = result.roughness;
    return true;
}
