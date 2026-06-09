#include "core_mesh_asset.h"

#include "core_io.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../shape/external/cjson/cJSON.h"

static CoreResult runtime_doc_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static bool runtime_doc_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static CoreResult runtime_doc_copy_text(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return runtime_doc_invalid_arg("text field must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return runtime_doc_invalid_arg("text field too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

static bool runtime_doc_vec3_is_finite(CoreObjectVec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static bool runtime_doc_vec3_from_json(const cJSON *node, CoreObjectVec3 *out_vec) {
    const cJSON *x = NULL;
    const cJSON *y = NULL;
    const cJSON *z = NULL;
    if (!cJSON_IsObject(node) || !out_vec) {
        return false;
    }
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) {
        return false;
    }
    out_vec->x = x->valuedouble;
    out_vec->y = y->valuedouble;
    out_vec->z = z->valuedouble;
    return true;
}

static cJSON *runtime_doc_vec3_to_json(CoreObjectVec3 v) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }
    cJSON_AddNumberToObject(obj, "x", v.x);
    cJSON_AddNumberToObject(obj, "y", v.y);
    cJSON_AddNumberToObject(obj, "z", v.z);
    return obj;
}

static double runtime_doc_triangle_area_sq(const CoreMeshAssetRuntimeDocument *document,
                                           const CoreMeshAssetRuntimeTriangle *triangle) {
    CoreObjectVec3 a = document->vertices[triangle->a].position;
    CoreObjectVec3 b = document->vertices[triangle->b].position;
    CoreObjectVec3 c = document->vertices[triangle->c].position;
    double ux = b.x - a.x;
    double uy = b.y - a.y;
    double uz = b.z - a.z;
    double vx = c.x - a.x;
    double vy = c.y - a.y;
    double vz = c.z - a.z;
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    return nx * nx + ny * ny + nz * nz;
}

static bool runtime_doc_bounds_contains(CoreMeshAssetBounds3 bounds, CoreObjectVec3 point) {
    const double eps = 1e-7;
    return point.x >= bounds.min.x - eps && point.x <= bounds.max.x + eps &&
           point.y >= bounds.min.y - eps && point.y <= bounds.max.y + eps &&
           point.z >= bounds.min.z - eps && point.z <= bounds.max.z + eps;
}

static bool runtime_doc_surface_group_exists(const CoreMeshAssetRuntimeDocument *document,
                                             const char *group_id) {
    size_t i;
    if (!document || !group_id || group_id[0] == '\0') {
        return false;
    }
    for (i = 0u; i < document->surface_group_count; ++i) {
        if (runtime_doc_text_equals(document->surface_groups[i].group_id, group_id)) {
            return true;
        }
    }
    return false;
}

static CoreResult runtime_doc_parse_root_contract(const cJSON *root,
                                                  CoreMeshAssetRuntimeDocument *document) {
    const cJSON *schema_family = NULL;
    const cJSON *schema_variant = NULL;
    const cJSON *schema_version = NULL;
    const cJSON *asset_id = NULL;
    const cJSON *source_asset_id = NULL;
    const cJSON *asset_type = NULL;
    const cJSON *local_bounds = NULL;
    const cJSON *topology_flags = NULL;
    const cJSON *closed_volume = NULL;
    const cJSON *manifold_expected = NULL;
    CoreResult r;

    if (!cJSON_IsObject(root) || !document) {
        return runtime_doc_invalid_arg("root document is invalid");
    }
    schema_family = cJSON_GetObjectItemCaseSensitive(root, "schema_family");
    schema_variant = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    schema_version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    source_asset_id = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    asset_type = cJSON_GetObjectItemCaseSensitive(root, "asset_type");
    local_bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");
    topology_flags = cJSON_GetObjectItemCaseSensitive(root, "topology_flags");

    if (!cJSON_IsString(schema_family) || !schema_family->valuestring ||
        !runtime_doc_text_equals(schema_family->valuestring, "codework_geometry")) {
        return runtime_doc_invalid_arg("schema_family must be codework_geometry");
    }
    if (!cJSON_IsString(schema_variant) || !schema_variant->valuestring ||
        !runtime_doc_text_equals(schema_variant->valuestring, "mesh_asset_runtime_v1")) {
        return runtime_doc_invalid_arg("schema_variant must be mesh_asset_runtime_v1");
    }
    if (!cJSON_IsNumber(schema_version) ||
        schema_version->valueint != CORE_MESH_ASSET_SCHEMA_VERSION_1) {
        return runtime_doc_invalid_arg("unsupported schema_version");
    }
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring ||
        !cJSON_IsString(source_asset_id) || !source_asset_id->valuestring ||
        !cJSON_IsString(asset_type) || !asset_type->valuestring) {
        return runtime_doc_invalid_arg("runtime root fields are missing");
    }

    r = core_mesh_asset_runtime_contract_set_asset_id(&document->contract, asset_id->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_runtime_contract_set_source_asset_id(&document->contract,
                                                             source_asset_id->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_type_parse(asset_type->valuestring, &document->contract.asset_type);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!cJSON_IsObject(local_bounds) ||
        !runtime_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(local_bounds, "min"),
                                    &document->contract.local_bounds.min) ||
        !runtime_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(local_bounds, "max"),
                                    &document->contract.local_bounds.max)) {
        return runtime_doc_invalid_arg("local_bounds are invalid");
    }

    if (cJSON_IsObject(topology_flags)) {
        closed_volume = cJSON_GetObjectItemCaseSensitive(topology_flags, "closed_volume");
        manifold_expected = cJSON_GetObjectItemCaseSensitive(topology_flags, "manifold_expected");
        if (cJSON_IsBool(closed_volume)) {
            document->contract.topology_closed_volume = cJSON_IsTrue(closed_volume);
        }
        if (cJSON_IsBool(manifold_expected)) {
            document->contract.topology_manifold_expected = cJSON_IsTrue(manifold_expected);
        }
    }
    return core_result_ok();
}

void core_mesh_asset_runtime_document_init(CoreMeshAssetRuntimeDocument *document) {
    if (!document) {
        return;
    }
    memset(document, 0, sizeof(*document));
    core_mesh_asset_runtime_contract_init(&document->contract);
}

void core_mesh_asset_runtime_document_free(CoreMeshAssetRuntimeDocument *document) {
    if (!document) {
        return;
    }
    core_free(document->vertices);
    core_free(document->triangles);
    core_free(document->surface_groups);
    document->vertices = NULL;
    document->triangles = NULL;
    document->surface_groups = NULL;
    document->vertex_count = 0u;
    document->triangle_count = 0u;
    document->surface_group_count = 0u;
    core_mesh_asset_runtime_contract_init(&document->contract);
}

CoreResult core_mesh_asset_runtime_document_set_vertex_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t vertex_count) {
    void *buffer = NULL;
    if (!document) {
        return runtime_doc_invalid_arg("document is null");
    }
    if (vertex_count == 0u) {
        core_free(document->vertices);
        document->vertices = NULL;
        document->vertex_count = 0u;
        return core_result_ok();
    }
    if (vertex_count > (SIZE_MAX / sizeof(CoreMeshAssetRuntimeVertex))) {
        return runtime_doc_invalid_arg("vertex count is too large");
    }
    buffer = core_alloc(vertex_count * sizeof(CoreMeshAssetRuntimeVertex));
    if (!buffer) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memset(buffer, 0, vertex_count * sizeof(CoreMeshAssetRuntimeVertex));
    core_free(document->vertices);
    document->vertices = (CoreMeshAssetRuntimeVertex *)buffer;
    document->vertex_count = vertex_count;
    document->contract.vertex_count = vertex_count;
    return core_result_ok();
}

CoreResult core_mesh_asset_runtime_document_set_triangle_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t triangle_count) {
    void *buffer = NULL;
    if (!document) {
        return runtime_doc_invalid_arg("document is null");
    }
    if (triangle_count == 0u) {
        core_free(document->triangles);
        document->triangles = NULL;
        document->triangle_count = 0u;
        return core_result_ok();
    }
    if (triangle_count > (SIZE_MAX / sizeof(CoreMeshAssetRuntimeTriangle))) {
        return runtime_doc_invalid_arg("triangle count is too large");
    }
    buffer = core_alloc(triangle_count * sizeof(CoreMeshAssetRuntimeTriangle));
    if (!buffer) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memset(buffer, 0, triangle_count * sizeof(CoreMeshAssetRuntimeTriangle));
    core_free(document->triangles);
    document->triangles = (CoreMeshAssetRuntimeTriangle *)buffer;
    document->triangle_count = triangle_count;
    document->contract.triangle_count = triangle_count;
    return core_result_ok();
}

CoreResult core_mesh_asset_runtime_document_set_surface_group_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t surface_group_count) {
    void *buffer = NULL;
    if (!document) {
        return runtime_doc_invalid_arg("document is null");
    }
    if (surface_group_count == 0u) {
        core_free(document->surface_groups);
        document->surface_groups = NULL;
        document->surface_group_count = 0u;
        return core_result_ok();
    }
    if (surface_group_count > (SIZE_MAX / sizeof(CoreMeshAssetSurfaceGroup))) {
        return runtime_doc_invalid_arg("surface group count is too large");
    }
    buffer = core_alloc(surface_group_count * sizeof(CoreMeshAssetSurfaceGroup));
    if (!buffer) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memset(buffer, 0, surface_group_count * sizeof(CoreMeshAssetSurfaceGroup));
    core_free(document->surface_groups);
    document->surface_groups = (CoreMeshAssetSurfaceGroup *)buffer;
    document->surface_group_count = surface_group_count;
    return core_result_ok();
}

CoreResult core_mesh_asset_runtime_document_validate(
    const CoreMeshAssetRuntimeDocument *document) {
    size_t i;
    CoreResult r;
    if (!document) {
        return runtime_doc_invalid_arg("document is null");
    }
    r = core_mesh_asset_runtime_contract_validate(&document->contract);
    if (r.code != CORE_OK) {
        return r;
    }
    if (document->vertex_count == 0u || !document->vertices ||
        document->triangle_count == 0u || !document->triangles) {
        return runtime_doc_invalid_arg("runtime mesh arrays must be present");
    }
    if (document->contract.vertex_count != document->vertex_count ||
        document->contract.triangle_count != document->triangle_count) {
        return runtime_doc_invalid_arg("runtime mesh counts must match arrays");
    }
    if (document->surface_group_count == 0u || !document->surface_groups) {
        return runtime_doc_invalid_arg("surface groups must be present");
    }
    for (i = 0u; i < document->vertex_count; ++i) {
        CoreObjectVec3 p = document->vertices[i].position;
        if (!runtime_doc_vec3_is_finite(p)) {
            return runtime_doc_invalid_arg("runtime mesh vertices must be finite");
        }
        if (!runtime_doc_bounds_contains(document->contract.local_bounds, p)) {
            return runtime_doc_invalid_arg("runtime mesh vertex is outside local bounds");
        }
    }
    for (i = 0u; i < document->surface_group_count; ++i) {
        const CoreMeshAssetSurfaceGroup *group = &document->surface_groups[i];
        if (group->group_id[0] == '\0') {
            return runtime_doc_invalid_arg("surface group id is required");
        }
        if (group->triangle_count == 0u ||
            group->triangle_start > document->triangle_count ||
            group->triangle_count > document->triangle_count - group->triangle_start) {
            return runtime_doc_invalid_arg("surface group triangle span is invalid");
        }
    }
    for (i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        if (triangle->a >= document->vertex_count || triangle->b >= document->vertex_count ||
            triangle->c >= document->vertex_count) {
            return runtime_doc_invalid_arg("runtime mesh triangle index is out of range");
        }
        if (triangle->a == triangle->b || triangle->a == triangle->c || triangle->b == triangle->c) {
            return runtime_doc_invalid_arg("runtime mesh triangle indices must be unique");
        }
        if (runtime_doc_triangle_area_sq(document, triangle) <= 1e-18) {
            return runtime_doc_invalid_arg("runtime mesh triangle is degenerate");
        }
        if (!runtime_doc_surface_group_exists(document, triangle->surface_group_id)) {
            return runtime_doc_invalid_arg("runtime mesh triangle surface group is unresolved");
        }
    }
    return core_result_ok();
}

CoreResult core_mesh_asset_runtime_document_load_file(const char *path,
                                                      CoreMeshAssetRuntimeDocument *out_document) {
    CoreBuffer file_data = {0};
    char *json_text = NULL;
    cJSON *root = NULL;
    const cJSON *mesh = NULL;
    const cJSON *vertex_count = NULL;
    const cJSON *triangle_count = NULL;
    const cJSON *vertices = NULL;
    const cJSON *triangles = NULL;
    const cJSON *surface_groups = NULL;
    const cJSON *array_item = NULL;
    int vertex_array_count;
    int triangle_array_count;
    int surface_group_count;
    int i;
    CoreResult r;
    CoreMeshAssetRuntimeDocument document;

    if (!path || !out_document) {
        return runtime_doc_invalid_arg("invalid argument");
    }
    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    json_text = (char *)core_alloc(file_data.size + 1u);
    if (!json_text) {
        core_io_buffer_free(&file_data);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (file_data.size > 0u && file_data.data) {
        memcpy(json_text, file_data.data, file_data.size);
    }
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    root = cJSON_Parse(json_text);
    core_free(json_text);
    if (!root) {
        return runtime_doc_invalid_arg("mesh asset runtime JSON parse failed");
    }

    core_mesh_asset_runtime_document_init(&document);
    r = runtime_doc_parse_root_contract(root, &document);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return r;
    }

    mesh = cJSON_GetObjectItemCaseSensitive(root, "mesh");
    vertex_count = mesh ? cJSON_GetObjectItemCaseSensitive(mesh, "vertex_count") : NULL;
    triangle_count = mesh ? cJSON_GetObjectItemCaseSensitive(mesh, "triangle_count") : NULL;
    vertices = mesh ? cJSON_GetObjectItemCaseSensitive(mesh, "vertices") : NULL;
    triangles = mesh ? cJSON_GetObjectItemCaseSensitive(mesh, "triangles") : NULL;
    if (!cJSON_IsObject(mesh) || !cJSON_IsNumber(vertex_count) ||
        !cJSON_IsNumber(triangle_count) || !cJSON_IsArray(vertices) ||
        !cJSON_IsArray(triangles)) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("runtime mesh fields are missing");
    }

    vertex_array_count = cJSON_GetArraySize(vertices);
    triangle_array_count = cJSON_GetArraySize(triangles);
    if (vertex_count->valueint <= 0 || triangle_count->valueint <= 0 ||
        vertex_count->valueint != vertex_array_count ||
        triangle_count->valueint != triangle_array_count) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("runtime mesh counts are invalid");
    }
    r = core_mesh_asset_runtime_document_set_vertex_count(&document, (size_t)vertex_array_count);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return r;
    }
    r = core_mesh_asset_runtime_document_set_triangle_count(&document, (size_t)triangle_array_count);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return r;
    }

    i = 0;
    cJSON_ArrayForEach(array_item, vertices) {
        if (i >= vertex_array_count ||
            !runtime_doc_vec3_from_json(array_item, &document.vertices[i].position)) {
            cJSON_Delete(root);
            core_mesh_asset_runtime_document_free(&document);
            return runtime_doc_invalid_arg("runtime mesh vertex is invalid");
        }
        i += 1;
    }
    if (i != vertex_array_count) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("runtime mesh vertex count mismatch");
    }

    i = 0;
    cJSON_ArrayForEach(array_item, triangles) {
        const cJSON *triangle = array_item;
        const cJSON *a = triangle ? cJSON_GetObjectItemCaseSensitive(triangle, "a") : NULL;
        const cJSON *b = triangle ? cJSON_GetObjectItemCaseSensitive(triangle, "b") : NULL;
        const cJSON *c = triangle ? cJSON_GetObjectItemCaseSensitive(triangle, "c") : NULL;
        const cJSON *surface_group_id =
            triangle ? cJSON_GetObjectItemCaseSensitive(triangle, "surface_group_id") : NULL;
        if (i >= triangle_array_count ||
            !cJSON_IsObject(triangle) || !cJSON_IsNumber(a) || !cJSON_IsNumber(b) ||
            !cJSON_IsNumber(c) || !cJSON_IsString(surface_group_id) ||
            !surface_group_id->valuestring || a->valueint < 0 || b->valueint < 0 ||
            c->valueint < 0) {
            cJSON_Delete(root);
            core_mesh_asset_runtime_document_free(&document);
            return runtime_doc_invalid_arg("runtime mesh triangle is invalid");
        }
        document.triangles[i].a = (size_t)a->valueint;
        document.triangles[i].b = (size_t)b->valueint;
        document.triangles[i].c = (size_t)c->valueint;
        r = runtime_doc_copy_text(document.triangles[i].surface_group_id,
                                  sizeof(document.triangles[i].surface_group_id),
                                  surface_group_id->valuestring);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            core_mesh_asset_runtime_document_free(&document);
            return r;
        }
        i += 1;
    }
    if (i != triangle_array_count) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("runtime mesh triangle count mismatch");
    }

    surface_groups = cJSON_GetObjectItemCaseSensitive(root, "surface_groups");
    if (!cJSON_IsArray(surface_groups) || cJSON_GetArraySize(surface_groups) <= 0) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("surface_groups must be non-empty");
    }
    surface_group_count = cJSON_GetArraySize(surface_groups);
    r = core_mesh_asset_runtime_document_set_surface_group_count(&document,
                                                                 (size_t)surface_group_count);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return r;
    }
    i = 0;
    cJSON_ArrayForEach(array_item, surface_groups) {
        const cJSON *group = array_item;
        const cJSON *group_id = group ? cJSON_GetObjectItemCaseSensitive(group, "group_id") : NULL;
        const cJSON *span = group ? cJSON_GetObjectItemCaseSensitive(group, "triangle_span") : NULL;
        const cJSON *start = span ? cJSON_GetObjectItemCaseSensitive(span, "start") : NULL;
        const cJSON *count = span ? cJSON_GetObjectItemCaseSensitive(span, "count") : NULL;
        if (i >= surface_group_count ||
            !cJSON_IsObject(group) || !cJSON_IsString(group_id) || !group_id->valuestring ||
            !cJSON_IsObject(span) || !cJSON_IsNumber(start) || !cJSON_IsNumber(count) ||
            start->valueint < 0 || count->valueint <= 0) {
            cJSON_Delete(root);
            core_mesh_asset_runtime_document_free(&document);
            return runtime_doc_invalid_arg("surface group is invalid");
        }
        r = runtime_doc_copy_text(document.surface_groups[i].group_id,
                                  sizeof(document.surface_groups[i].group_id),
                                  group_id->valuestring);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            core_mesh_asset_runtime_document_free(&document);
            return r;
        }
        document.surface_groups[i].triangle_start = (size_t)start->valueint;
        document.surface_groups[i].triangle_count = (size_t)count->valueint;
        i += 1;
    }
    if (i != surface_group_count) {
        cJSON_Delete(root);
        core_mesh_asset_runtime_document_free(&document);
        return runtime_doc_invalid_arg("surface group count mismatch");
    }

    cJSON_Delete(root);
    r = core_mesh_asset_runtime_document_validate(&document);
    if (r.code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return r;
    }
    *out_document = document;
    return core_result_ok();
}

CoreResult core_mesh_asset_runtime_document_save_file(
    const CoreMeshAssetRuntimeDocument *document,
    const char *path) {
    cJSON *root = NULL;
    cJSON *local_bounds = NULL;
    cJSON *topology_flags = NULL;
    cJSON *mesh = NULL;
    cJSON *vertices = NULL;
    cJSON *triangles = NULL;
    cJSON *surface_groups = NULL;
    char *json_text = NULL;
    CoreResult r;
    size_t i;

    if (!document || !path) {
        return runtime_doc_invalid_arg("invalid argument");
    }
    r = core_mesh_asset_runtime_document_validate(document);
    if (r.code != CORE_OK) {
        return r;
    }

    root = cJSON_CreateObject();
    if (!root) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddStringToObject(root, "schema_family", "codework_geometry");
    cJSON_AddStringToObject(root, "schema_variant", "mesh_asset_runtime_v1");
    cJSON_AddNumberToObject(root, "schema_version", CORE_MESH_ASSET_SCHEMA_VERSION_1);
    cJSON_AddStringToObject(root, "asset_id", document->contract.asset_id);
    cJSON_AddStringToObject(root, "source_asset_id", document->contract.source_asset_id);
    cJSON_AddStringToObject(root, "asset_type",
                            core_mesh_asset_type_name(document->contract.asset_type));

    local_bounds = cJSON_CreateObject();
    if (!local_bounds) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "local_bounds", local_bounds);
    cJSON_AddItemToObject(local_bounds,
                          "min",
                          runtime_doc_vec3_to_json(document->contract.local_bounds.min));
    cJSON_AddItemToObject(local_bounds,
                          "max",
                          runtime_doc_vec3_to_json(document->contract.local_bounds.max));

    topology_flags = cJSON_CreateObject();
    if (!topology_flags) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "topology_flags", topology_flags);
    cJSON_AddBoolToObject(topology_flags,
                          "closed_volume",
                          document->contract.topology_closed_volume);
    cJSON_AddBoolToObject(topology_flags,
                          "manifold_expected",
                          document->contract.topology_manifold_expected);

    mesh = cJSON_CreateObject();
    if (!mesh) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "mesh", mesh);
    cJSON_AddNumberToObject(mesh, "vertex_count", (double)document->vertex_count);
    cJSON_AddNumberToObject(mesh, "triangle_count", (double)document->triangle_count);
    vertices = cJSON_CreateArray();
    triangles = cJSON_CreateArray();
    if (!vertices || !triangles) {
        cJSON_Delete(vertices);
        cJSON_Delete(triangles);
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(mesh, "vertices", vertices);
    cJSON_AddItemToObject(mesh, "triangles", triangles);
    for (i = 0u; i < document->vertex_count; ++i) {
        cJSON_AddItemToArray(vertices, runtime_doc_vec3_to_json(document->vertices[i].position));
    }
    for (i = 0u; i < document->triangle_count; ++i) {
        cJSON *triangle = cJSON_CreateObject();
        if (!triangle) {
            cJSON_Delete(root);
            return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        }
        cJSON_AddItemToArray(triangles, triangle);
        cJSON_AddNumberToObject(triangle, "a", (double)document->triangles[i].a);
        cJSON_AddNumberToObject(triangle, "b", (double)document->triangles[i].b);
        cJSON_AddNumberToObject(triangle, "c", (double)document->triangles[i].c);
        cJSON_AddStringToObject(triangle,
                                "surface_group_id",
                                document->triangles[i].surface_group_id);
    }

    surface_groups = cJSON_CreateArray();
    if (!surface_groups) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "surface_groups", surface_groups);
    for (i = 0u; i < document->surface_group_count; ++i) {
        cJSON *group = cJSON_CreateObject();
        cJSON *span = cJSON_CreateObject();
        if (!group || !span) {
            cJSON_Delete(group);
            cJSON_Delete(span);
            cJSON_Delete(root);
            return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        }
        cJSON_AddItemToArray(surface_groups, group);
        cJSON_AddStringToObject(group, "group_id", document->surface_groups[i].group_id);
        cJSON_AddStringToObject(group, "semantic", document->surface_groups[i].group_id);
        cJSON_AddItemToObject(group, "triangle_span", span);
        cJSON_AddNumberToObject(span, "start",
                                (double)document->surface_groups[i].triangle_start);
        cJSON_AddNumberToObject(span, "count",
                                (double)document->surface_groups[i].triangle_count);
    }
    cJSON_AddItemToObject(root, "extensions", cJSON_CreateObject());

    json_text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_text) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    r = core_io_write_all_atomic(path, json_text, strlen(json_text));
    free(json_text);
    return r;
}
