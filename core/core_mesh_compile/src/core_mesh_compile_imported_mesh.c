#include "core_mesh_compile.h"

#include "core_io.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct CoreMeshCompileParsedTriangle {
    size_t a;
    size_t b;
    size_t c;
} CoreMeshCompileParsedTriangle;

static CoreResult imported_mesh_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreResult imported_mesh_copy_text(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return imported_mesh_invalid_arg("text field must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return imported_mesh_invalid_arg("text field too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

static const char *imported_mesh_skip_space(const char *text) {
    while (text && *text && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static bool imported_mesh_vec3_near(CoreObjectVec3 a, CoreObjectVec3 b, double tolerance) {
    return fabs(a.x - b.x) <= tolerance && fabs(a.y - b.y) <= tolerance &&
           fabs(a.z - b.z) <= tolerance;
}

static CoreResult imported_mesh_source_path(const CoreMeshAssetImportedMeshSource *source,
                                            const char *source_root,
                                            char *out_path,
                                            size_t out_path_size) {
    int written;
    if (!source || !out_path || out_path_size == 0u) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    if (source->source_uri[0] == '/') {
        return imported_mesh_copy_text(out_path, out_path_size, source->source_uri);
    }
    if (!source_root || source_root[0] == '\0') {
        return imported_mesh_copy_text(out_path, out_path_size, source->source_uri);
    }
    written = snprintf(out_path, out_path_size, "%s/%s", source_root, source->source_uri);
    if (written < 0 || (size_t)written >= out_path_size) {
        return imported_mesh_invalid_arg("source path is too long");
    }
    return core_result_ok();
}

static CoreResult imported_mesh_add_vertex(CoreObjectVec3 *vertices,
                                           size_t *vertex_count,
                                           size_t vertex_capacity,
                                           CoreObjectVec3 vertex,
                                           double weld_tolerance,
                                           bool weld_vertices,
                                           size_t *out_index) {
    size_t i;
    if (!vertices || !vertex_count || !out_index) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    if (weld_vertices) {
        for (i = 0u; i < *vertex_count; ++i) {
            if (imported_mesh_vec3_near(vertices[i], vertex, weld_tolerance)) {
                *out_index = i;
                return core_result_ok();
            }
        }
    }
    if (*vertex_count >= vertex_capacity) {
        return imported_mesh_invalid_arg("vertex capacity exceeded");
    }
    vertices[*vertex_count] = vertex;
    *out_index = *vertex_count;
    *vertex_count += 1u;
    return core_result_ok();
}

static CoreResult imported_mesh_parse_ascii_stl(const char *path,
                                                const CoreMeshAssetImportedMeshSource *source,
                                                CoreObjectVec3 **out_vertices,
                                                size_t *out_vertex_count,
                                                CoreMeshCompileParsedTriangle **out_triangles,
                                                size_t *out_triangle_count) {
    CoreBuffer file_data = {0};
    char *text = NULL;
    const char *cursor = NULL;
    size_t vertex_line_count = 0u;
    size_t parsed_vertex_in_triangle = 0u;
    size_t vertex_count = 0u;
    size_t triangle_count = 0u;
    size_t current_triangle_indices[3] = {0u, 0u, 0u};
    CoreObjectVec3 *vertices = NULL;
    CoreMeshCompileParsedTriangle *triangles = NULL;
    CoreResult r;

    if (!path || !source || !out_vertices || !out_vertex_count || !out_triangles ||
        !out_triangle_count) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    *out_vertices = NULL;
    *out_vertex_count = 0u;
    *out_triangles = NULL;
    *out_triangle_count = 0u;

    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    text = (char *)core_alloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (file_data.size > 0u && file_data.data) {
        memcpy(text, file_data.data, file_data.size);
    }
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    cursor = text;
    while (*cursor) {
        const char *line = imported_mesh_skip_space(cursor);
        if (strncmp(line, "vertex", 6u) == 0 && isspace((unsigned char)line[6])) {
            ++vertex_line_count;
        }
        cursor = strchr(cursor, '\n');
        if (!cursor) {
            break;
        }
        ++cursor;
    }
    if (vertex_line_count == 0u || vertex_line_count % 3u != 0u) {
        core_free(text);
        return imported_mesh_invalid_arg("ASCII STL vertex line count is invalid");
    }
    vertices = (CoreObjectVec3 *)core_alloc(vertex_line_count * sizeof(CoreObjectVec3));
    triangles = (CoreMeshCompileParsedTriangle *)core_alloc((vertex_line_count / 3u) *
                                                            sizeof(CoreMeshCompileParsedTriangle));
    if (!vertices || !triangles) {
        core_free(vertices);
        core_free(triangles);
        core_free(text);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    cursor = text;
    while (*cursor) {
        double x;
        double y;
        double z;
        const char *line = imported_mesh_skip_space(cursor);
        if (sscanf(line, "vertex %lf %lf %lf", &x, &y, &z) == 3) {
            CoreObjectVec3 vertex;
            size_t vertex_index = 0u;
            vertex.x = x * source->source_to_asset_scale;
            vertex.y = y * source->source_to_asset_scale;
            vertex.z = z * source->source_to_asset_scale;
            if (!isfinite(vertex.x) || !isfinite(vertex.y) || !isfinite(vertex.z)) {
                core_free(vertices);
                core_free(triangles);
                core_free(text);
                return imported_mesh_invalid_arg("ASCII STL vertex is not finite");
            }
            r = imported_mesh_add_vertex(vertices,
                                         &vertex_count,
                                         vertex_line_count,
                                         vertex,
                                         source->weld_tolerance,
                                         source->weld_vertices,
                                         &vertex_index);
            if (r.code != CORE_OK) {
                core_free(vertices);
                core_free(triangles);
                core_free(text);
                return r;
            }
            current_triangle_indices[parsed_vertex_in_triangle] = vertex_index;
            parsed_vertex_in_triangle += 1u;
            if (parsed_vertex_in_triangle == 3u) {
                triangles[triangle_count].a = current_triangle_indices[0];
                triangles[triangle_count].b = current_triangle_indices[1];
                triangles[triangle_count].c = current_triangle_indices[2];
                triangle_count += 1u;
                parsed_vertex_in_triangle = 0u;
            }
        }
        cursor = strchr(cursor, '\n');
        if (!cursor) {
            break;
        }
        ++cursor;
    }
    if (parsed_vertex_in_triangle != 0u || triangle_count == 0u || vertex_count == 0u) {
        core_free(vertices);
        core_free(triangles);
        core_free(text);
        return imported_mesh_invalid_arg("ASCII STL triangle stream is invalid");
    }

    core_free(text);
    *out_vertices = vertices;
    *out_vertex_count = vertex_count;
    *out_triangles = triangles;
    *out_triangle_count = triangle_count;
    return core_result_ok();
}

static void imported_mesh_compute_bounds(const CoreObjectVec3 *vertices,
                                         size_t vertex_count,
                                         CoreMeshAssetBounds3 *out_bounds) {
    size_t i;
    out_bounds->min = vertices[0];
    out_bounds->max = vertices[0];
    for (i = 1u; i < vertex_count; ++i) {
        if (vertices[i].x < out_bounds->min.x) out_bounds->min.x = vertices[i].x;
        if (vertices[i].y < out_bounds->min.y) out_bounds->min.y = vertices[i].y;
        if (vertices[i].z < out_bounds->min.z) out_bounds->min.z = vertices[i].z;
        if (vertices[i].x > out_bounds->max.x) out_bounds->max.x = vertices[i].x;
        if (vertices[i].y > out_bounds->max.y) out_bounds->max.y = vertices[i].y;
        if (vertices[i].z > out_bounds->max.z) out_bounds->max.z = vertices[i].z;
    }
}

CoreResult core_mesh_compile_imported_mesh_to_runtime_document(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    CoreMeshAssetRuntimeDocument *out_document) {
    CoreMeshCompileAuthoringContract compile_contract;
    char source_path[512];
    CoreObjectVec3 *parsed_vertices = NULL;
    CoreMeshCompileParsedTriangle *parsed_triangles = NULL;
    size_t parsed_vertex_count = 0u;
    size_t parsed_triangle_count = 0u;
    CoreResult r;
    size_t i;

    if (!document || !runtime_asset_id || !out_document) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    if (document->contract.source_mode != CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        return imported_mesh_invalid_arg("document source_mode must be imported_mesh");
    }
    if (!document->has_imported_mesh_source) {
        return imported_mesh_invalid_arg("imported mesh source is required");
    }
    r = core_mesh_compile_authoring_contract_prepare(&compile_contract, document, runtime_asset_id);
    if (r.code != CORE_OK) {
        return r;
    }
    if (document->imported_mesh_source.source_format !=
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL) {
        return imported_mesh_invalid_arg("only STL imported mesh compile is supported");
    }
    r = imported_mesh_source_path(&document->imported_mesh_source,
                                  source_root,
                                  source_path,
                                  sizeof(source_path));
    if (r.code != CORE_OK) {
        return r;
    }
    r = imported_mesh_parse_ascii_stl(source_path,
                                      &document->imported_mesh_source,
                                      &parsed_vertices,
                                      &parsed_vertex_count,
                                      &parsed_triangles,
                                      &parsed_triangle_count);
    if (r.code != CORE_OK) {
        return r;
    }

    core_mesh_asset_runtime_document_init(out_document);
    r = core_mesh_asset_runtime_contract_set_asset_id(&out_document->contract, runtime_asset_id);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_contract_set_source_asset_id(&out_document->contract,
                                                             document->contract.asset_id);
    if (r.code != CORE_OK) goto fail;
    out_document->contract.asset_type = document->contract.asset_type;
    out_document->contract.pivot = document->contract.pivot;
    out_document->contract.vertex_count = parsed_vertex_count;
    out_document->contract.triangle_count = parsed_triangle_count;
    out_document->contract.topology_closed_volume =
        document->imported_mesh_source.topology_closed_volume_observed;
    out_document->contract.topology_manifold_expected =
        document->imported_mesh_source.topology_manifold_observed;
    imported_mesh_compute_bounds(parsed_vertices,
                                 parsed_vertex_count,
                                 &out_document->contract.local_bounds);

    r = core_mesh_asset_runtime_document_set_vertex_count(out_document, parsed_vertex_count);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_document_set_triangle_count(out_document, parsed_triangle_count);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_document_set_surface_group_count(out_document, 1u);
    if (r.code != CORE_OK) goto fail;

    for (i = 0u; i < parsed_vertex_count; ++i) {
        out_document->vertices[i].position = parsed_vertices[i];
    }
    for (i = 0u; i < parsed_triangle_count; ++i) {
        out_document->triangles[i].a = parsed_triangles[i].a;
        out_document->triangles[i].b = parsed_triangles[i].b;
        out_document->triangles[i].c = parsed_triangles[i].c;
        r = imported_mesh_copy_text(out_document->triangles[i].surface_group_id,
                                    sizeof(out_document->triangles[i].surface_group_id),
                                    document->imported_mesh_source.default_surface_group_id);
        if (r.code != CORE_OK) goto fail;
    }
    r = imported_mesh_copy_text(out_document->surface_groups[0].group_id,
                                sizeof(out_document->surface_groups[0].group_id),
                                document->imported_mesh_source.default_surface_group_id);
    if (r.code != CORE_OK) goto fail;
    out_document->surface_groups[0].triangle_start = 0u;
    out_document->surface_groups[0].triangle_count = parsed_triangle_count;

    r = core_mesh_asset_runtime_document_validate(out_document);
    if (r.code != CORE_OK) goto fail;

    core_free(parsed_vertices);
    core_free(parsed_triangles);
    return core_result_ok();

fail:
    core_free(parsed_vertices);
    core_free(parsed_triangles);
    core_mesh_asset_runtime_document_free(out_document);
    return r;
}

CoreResult core_mesh_compile_imported_mesh_to_runtime_file(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    const char *runtime_output_path) {
    CoreMeshAssetRuntimeDocument runtime_document;
    CoreResult r;

    if (!runtime_output_path) {
        return imported_mesh_invalid_arg("runtime output path is required");
    }
    core_mesh_asset_runtime_document_init(&runtime_document);
    r = core_mesh_compile_imported_mesh_to_runtime_document(document,
                                                            source_root,
                                                            runtime_asset_id,
                                                            &runtime_document);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_runtime_document_save_file(&runtime_document, runtime_output_path);
    core_mesh_asset_runtime_document_free(&runtime_document);
    return r;
}
