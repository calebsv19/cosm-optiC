#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void canvas_error(RayTracingSurfaceAuthoringCanvasSnapshot* s, const char* message) {
    if (s) snprintf(s->error, sizeof(s->error), "%s", message ? message : "invalid canvas");
}

static bool copy_string(char* dst, size_t cap, struct json_object* value) {
    const char* text;
    if (!dst || cap == 0u || !value || json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= cap) return false;
    snprintf(dst, cap, "%s", text);
    return true;
}

static bool required_string(struct json_object* object, const char* key, char* dst, size_t cap) {
    struct json_object* value = NULL;
    return json_object_object_get_ex(object, key, &value) && copy_string(dst, cap, value);
}

static bool required_int(struct json_object* object, const char* key, int32_t* out) {
    struct json_object* value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_int) return false;
    *out = (int32_t)json_object_get_int64(value);
    return true;
}

static bool required_bool(struct json_object* object, const char* key, bool expected) {
    struct json_object* value = NULL;
    return json_object_object_get_ex(object, key, &value) &&
           json_object_get_type(value) == json_type_boolean &&
           json_object_get_boolean(value) == expected;
}

static int node_index(const RayTracingSurfaceAuthoringCanvasSnapshot* s, const char* id) {
    size_t i;
    for (i = 0u; i < s->node_count; ++i) if (strcmp(s->nodes[i].id, id) == 0) return (int)i;
    return -1;
}

void RayTracingSurfaceAuthoringCanvasSnapshot_Init(
    RayTracingSurfaceAuthoringCanvasSnapshot* snapshot) {
    if (snapshot) memset(snapshot, 0, sizeof(*snapshot));
}

static bool add_node(RayTracingSurfaceAuthoringCanvasSnapshot* s, const char* id,
                     const char* kind, const char* label, int32_t x, int32_t y,
                     const char* digest, uint32_t domains) {
    RayTracingSurfaceAuthoringCanvasNode* n;
    if (!s || s->node_count >= RAY_TRACING_SURFACE_AUTHORING_CANVAS_NODE_CAP ||
        node_index(s, id) >= 0) return false;
    n = &s->nodes[s->node_count++];
    snprintf(n->id, sizeof(n->id), "%s", id);
    snprintf(n->kind, sizeof(n->kind), "%s", kind);
    snprintf(n->label, sizeof(n->label), "%s", label);
    if (digest) snprintf(n->digest_sha256, sizeof(n->digest_sha256), "%s", digest);
    n->x = x; n->y = y; n->output_domains = domains;
    return true;
}

static bool add_edge(RayTracingSurfaceAuthoringCanvasSnapshot* s, const char* from, const char* to) {
    RayTracingSurfaceAuthoringCanvasEdge* e;
    if (!s || s->edge_count >= RAY_TRACING_SURFACE_AUTHORING_CANVAS_EDGE_CAP ||
        node_index(s, from) < 0 || node_index(s, to) < 0) return false;
    e = &s->edges[s->edge_count++];
    snprintf(e->from, sizeof(e->from), "%s", from);
    snprintf(e->to, sizeof(e->to), "%s", to);
    return true;
}

bool RayTracingSurfaceAuthoringCanvasSnapshot_DefaultCube(
    RayTracingSurfaceAuthoringCanvasSnapshot* s) {
    static const char* digest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static const char* lanes[] = {"material_graph", "surface_field_graph", "face_region_selector", "attachment_graph"};
    static const char* refs[] = {"Brown / Umber Mix", "Dirt Noise → Microdetail", "positive_z", "Grass Attachment"};
    static const char* kinds[] = {"reference", "reference", "reference", "attachment"};
    static const char* ref_digest[] = {
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
        "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"};
    size_t i;
    RayTracingSurfaceAuthoringCanvasSnapshot_Init(s);
    if (!s || !add_node(s, "source_mesh", "source", "cube", 40, 280, digest, 0u)) return false;
    s->read_only = true;
    s->can_select = true;
    s->can_zoom = true;
    s->can_pan = true;
    s->can_edit = false;
    s->can_save = false;
    s->can_promote = false;
    snprintf(s->document_id, sizeof(s->document_id), "cube_surface_v1");
    snprintf(s->document_digest_sha256, sizeof(s->document_digest_sha256), "bda967cac7da5fcbef85d521daa136acbb45bc082d1db24e78a45f68660709d6");
    snprintf(s->source_object_id, sizeof(s->source_object_id), "cube");
    snprintf(s->source_mesh_digest_sha256, sizeof(s->source_mesh_digest_sha256), "%s", digest);
    for (i = 0u; i < 4u; ++i) {
        if (!add_node(s, lanes[i], "lane", lanes[i], 260, 80 + (int32_t)i * 130, NULL, 0u) ||
            !add_node(s, i == 3u ? "ref:attachment:grass_attachment" :
                            i == 0u ? "ref:material_graph" : i == 1u ? "ref:surface_field_graph" : "ref:face_region_selector",
                      kinds[i], refs[i], 540, 80 + (int32_t)i * 130, ref_digest[i], i == 3u ? 16u : i == 2u ? 17u : i == 0u ? 3u : 2u) ||
            !add_edge(s, "source_mesh", lanes[i])) return false;
        if (!add_edge(s, lanes[i], s->nodes[s->node_count - 1u].id)) return false;
    }
    s->valid = true;
    return true;
}

bool RayTracingSurfaceAuthoringCanvasSnapshot_LoadJsonFile(
    const char* path, RayTracingSurfaceAuthoringCanvasSnapshot* s) {
    struct json_object* root = NULL;
    struct json_object* interaction = NULL;
    struct json_object* nodes = NULL;
    struct json_object* edges = NULL;
    FILE* file;
    long size;
    char* bytes;
    size_t i;
    int32_t schema_version;
    char schema[128];
    char mode[128];
    RayTracingSurfaceAuthoringCanvasSnapshot_Init(s);
    if (!s || !path || !path[0]) return false;
    file = fopen(path, "rb");
    if (!file) { canvas_error(s, "cannot open canvas JSON"); return false; }
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); canvas_error(s, "cannot seek canvas JSON"); return false; }
    size = ftell(file); rewind(file);
    if (size < 0 || size > 1024 * 1024) { fclose(file); canvas_error(s, "canvas JSON is too large"); return false; }
    bytes = (char*)malloc((size_t)size + 1u);
    if (!bytes || fread(bytes, 1u, (size_t)size, file) != (size_t)size) { free(bytes); fclose(file); canvas_error(s, "cannot read canvas JSON"); return false; }
    bytes[size] = '\0'; fclose(file);
    root = json_tokener_parse(bytes); free(bytes);
    if (!root || json_object_get_type(root) != json_type_object) { if (root) json_object_put(root); canvas_error(s, "canvas root is not an object"); return false; }
    if (!required_string(root, "schema", schema, sizeof(schema)) || strcmp(schema, "ray_tracing.surface_authoring_document_canvas") != 0 ||
        !required_int(root, "schema_version", &schema_version) || schema_version != 1 ||
        !required_string(root, "mode", mode, sizeof(mode)) || strcmp(mode, "inspect") != 0 ||
        !json_object_object_get_ex(root, "interaction", &interaction) ||
        !required_bool(interaction, "read_only", true) || !required_bool(interaction, "can_select", true) ||
        !required_bool(interaction, "can_zoom", true) || !required_bool(interaction, "can_pan", true) ||
        !required_bool(interaction, "can_edit", false) || !required_bool(interaction, "can_save", false) ||
        !required_bool(interaction, "can_promote", false)) {
        json_object_put(root); canvas_error(s, "canvas schema is not frozen read-only v1"); return false;
    }
    s->read_only = true;
    s->can_select = true;
    s->can_zoom = true;
    s->can_pan = true;
    s->can_edit = false;
    s->can_save = false;
    s->can_promote = false;
    if (!required_string(root, "document_id", s->document_id, sizeof(s->document_id)) ||
        !required_string(root, "document_digest_sha256", s->document_digest_sha256, sizeof(s->document_digest_sha256)) ||
        !required_string(root, "source_object_id", s->source_object_id, sizeof(s->source_object_id)) ||
        !required_string(root, "source_mesh_digest_sha256", s->source_mesh_digest_sha256, sizeof(s->source_mesh_digest_sha256)) ||
        !json_object_object_get_ex(root, "nodes", &nodes) || json_object_get_type(nodes) != json_type_array ||
        !json_object_object_get_ex(root, "edges", &edges) || json_object_get_type(edges) != json_type_array) {
        json_object_put(root); canvas_error(s, "canvas metadata or arrays are invalid"); return false;
    }
    if (json_object_array_length(nodes) > RAY_TRACING_SURFACE_AUTHORING_CANVAS_NODE_CAP ||
        json_object_array_length(edges) > RAY_TRACING_SURFACE_AUTHORING_CANVAS_EDGE_CAP) {
        json_object_put(root); canvas_error(s, "canvas exceeds fixed capacity"); return false;
    }
    for (i = 0u; i < json_object_array_length(nodes); ++i) {
        struct json_object* n = json_object_array_get_idx(nodes, i);
        struct json_object *digest = NULL, *domains = NULL;
        if (!n || !required_string(n, "id", s->nodes[i].id, sizeof(s->nodes[i].id)) ||
            !required_string(n, "kind", s->nodes[i].kind, sizeof(s->nodes[i].kind)) ||
            !required_string(n, "label", s->nodes[i].label, sizeof(s->nodes[i].label)) ||
            !required_int(n, "x", &s->nodes[i].x) || !required_int(n, "y", &s->nodes[i].y) || node_index(s, s->nodes[i].id) >= 0) {
            json_object_put(root); canvas_error(s, "canvas node is invalid or duplicated"); return false;
        }
        if (json_object_object_get_ex(n, "digest_sha256", &digest) && !copy_string(s->nodes[i].digest_sha256, sizeof(s->nodes[i].digest_sha256), digest)) {
            json_object_put(root); canvas_error(s, "canvas node digest is invalid"); return false;
        }
        if (json_object_object_get_ex(n, "output_domains", &domains)) {
            if (json_object_get_type(domains) != json_type_int) { json_object_put(root); canvas_error(s, "canvas node domains are invalid"); return false; }
            s->nodes[i].output_domains = (uint32_t)json_object_get_int64(domains);
        }
        ++s->node_count;
    }
    for (i = 0u; i < json_object_array_length(edges); ++i) {
        struct json_object* e = json_object_array_get_idx(edges, i);
        if (!e || !required_string(e, "from", s->edges[i].from, sizeof(s->edges[i].from)) ||
            !required_string(e, "to", s->edges[i].to, sizeof(s->edges[i].to)) ||
            node_index(s, s->edges[i].from) < 0 || node_index(s, s->edges[i].to) < 0) {
            json_object_put(root); canvas_error(s, "canvas edge is invalid"); return false;
        }
        ++s->edge_count;
    }
    s->valid = true;
    json_object_put(root);
    return true;
}
