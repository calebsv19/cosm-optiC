#include "procedural/procedural_surface_feature_selection.h"

#include "procedural/procedural_solid_mesh.h"
#include "core_io.h"

#include <json-c/json.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *mesh_path;
    const char *field_path;
    const char *base_region_path;
    const char *out_path;
    const char *region_id;
    const char *summary_path;
    double minimum_radius;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
        "usage: %s --mesh PATH --field PATH --base-region PATH --out PATH "
        "--region-id ID --minimum-radius VALUE [--summary-out PATH]\n", program);
}

static bool parse_options(int argc, char **argv, Options *out) {
    Options options = {0};
    bool has_radius = false;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc) return false;
        if (strcmp(argv[i], "--mesh") == 0) options.mesh_path = argv[++i];
        else if (strcmp(argv[i], "--field") == 0) options.field_path = argv[++i];
        else if (strcmp(argv[i], "--base-region") == 0) options.base_region_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0) options.out_path = argv[++i];
        else if (strcmp(argv[i], "--region-id") == 0) options.region_id = argv[++i];
        else if (strcmp(argv[i], "--summary-out") == 0) options.summary_path = argv[++i];
        else if (strcmp(argv[i], "--minimum-radius") == 0) {
            char *end = NULL;
            errno = 0;
            options.minimum_radius = strtod(argv[++i], &end);
            has_radius = errno == 0 && end && *end == '\0' &&
                         options.minimum_radius >= 0.0;
            if (!has_radius) return false;
        } else return false;
    }
    if (!options.mesh_path || !options.field_path || !options.base_region_path ||
        !options.out_path || !options.region_id || !has_radius) return false;
    *out = options;
    return true;
}

static bool write_summary(const char *path,
                          const ProceduralSurfaceFeatureFieldV1 *field,
                          const ProceduralSurfaceFeatureSelectionV1 *selection,
                          const ProceduralImportedSurfaceRegionV1 *region) {
    json_object *root;
    const char *text;
    CoreResult result;
    if (!path) return true;
    root = json_object_new_object();
    if (!root) return false;
    json_object_object_add(root, "schema",
        json_object_new_string("surface_feature_selection_receipt_v1"));
    json_object_object_add(root, "field_digest_sha256",
        json_object_new_string(selection->field_digest_sha256));
    json_object_object_add(root, "source_mesh_digest_sha256",
        json_object_new_string(field->source_mesh_digest_sha256));
    json_object_object_add(root, "selected_feature_count",
        json_object_new_int64((int64_t)selection->feature_id_count));
    json_object_object_add(root, "region_id",
        json_object_new_string(region->region_id));
    json_object_object_add(root, "carrier_value_digest_sha256",
        json_object_new_string(region->value_digest_sha256));
    json_object_object_add(root, "transition_vertex_count",
        json_object_new_int64((int64_t)region->transition_vertex_count));
    json_object_object_add(root, "topology_unchanged",
        json_object_new_boolean(region->topology_unchanged));
    json_object_object_add(root, "source_triangle_provenance_retained",
        json_object_new_boolean(region->source_triangle_provenance_retained));
    text = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    return result.code == CORE_OK;
}

static bool derived_vertices_with_normals(
    const CoreMeshAssetRuntimeDocument *mesh,
    CoreMeshAssetRuntimeVertex **out_vertices) {
    CoreMeshAssetRuntimeVertex *vertices;
    if (!mesh || !out_vertices || !mesh->vertices || !mesh->triangles ||
        mesh->vertex_count == 0u) return false;
    vertices = calloc(mesh->vertex_count, sizeof(*vertices));
    if (!vertices) return false;
    memcpy(vertices, mesh->vertices, mesh->vertex_count * sizeof(*vertices));
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &mesh->triangles[i];
        CoreObjectVec3 a, b, c, ab, ac, cross;
        if (triangle->a >= mesh->vertex_count || triangle->b >= mesh->vertex_count ||
            triangle->c >= mesh->vertex_count) { free(vertices); return false; }
        a = mesh->vertices[triangle->a].position;
        b = mesh->vertices[triangle->b].position;
        c = mesh->vertices[triangle->c].position;
        ab = (CoreObjectVec3){b.x-a.x,b.y-a.y,b.z-a.z};
        ac = (CoreObjectVec3){c.x-a.x,c.y-a.y,c.z-a.z};
        cross = (CoreObjectVec3){ab.y*ac.z-ab.z*ac.y,
            ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
        for (size_t j = 0u; j < 3u; ++j) {
            uint32_t index = j == 0u ? triangle->a : j == 1u ? triangle->b : triangle->c;
            vertices[index].normal.x += cross.x;
            vertices[index].normal.y += cross.y;
            vertices[index].normal.z += cross.z;
        }
    }
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        double length = sqrt(vertices[i].normal.x*vertices[i].normal.x +
            vertices[i].normal.y*vertices[i].normal.y + vertices[i].normal.z*vertices[i].normal.z);
        if (length <= 1e-12) { free(vertices); return false; }
        vertices[i].normal.x /= length; vertices[i].normal.y /= length; vertices[i].normal.z /= length;
    }
    *out_vertices = vertices;
    return true;
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument mesh;
    ProceduralSurfaceFeatureFieldV1 field = {0};
    ProceduralSurfaceFeatureSelectionV1 selection = {0};
    ProceduralImportedSurfaceRegionV1 base, output;
    CoreMeshAssetRuntimeVertex *vertices = NULL;
    ProceduralImportedSurfaceRegionReport report = {0};
    char mesh_digest[65] = {0};
    CoreResult result;
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) { usage(argv[0]); return 2; }
    core_mesh_asset_runtime_document_init(&mesh);
    ProceduralImportedSurfaceRegionV1_Init(&base);
    ProceduralImportedSurfaceRegionV1_Init(&output);
    result = core_mesh_asset_runtime_document_load_file(options.mesh_path, &mesh);
    if (result.code != CORE_OK || !ProceduralSolidMesh_Digest(&mesh, mesh_digest) ||
        !ProceduralSurfaceFeatureFieldV1_LoadJsonFile(options.field_path, &field) ||
        strcmp(field.source_mesh_digest_sha256, mesh_digest) != 0 ||
        !ProceduralImportedSurfaceRegionV1_LoadJsonFile(options.base_region_path,
            &mesh, options.mesh_path, &base, &report) ||
        !derived_vertices_with_normals(&mesh, &vertices) ||
        !ProceduralSurfaceFeatureSelectionV1_Build(&field,
            options.minimum_radius, &selection) ||
        !ProceduralSurfaceFeatureSelectionV1_BuildRegion(&base, &field,
            &selection, vertices, mesh.vertex_count, options.region_id,
            &output) ||
        !ProceduralImportedSurfaceRegionV1_SaveJsonFileAtomic(options.out_path,
            &output, &report) ||
        !write_summary(options.summary_path, &field, &selection, &output)) {
        fprintf(stderr, "surface feature selection compile failed: %s\n",
            report.message[0] ? report.message : "field/source/carrier binding failure");
        goto cleanup;
    }
    printf("{\"field_digest_sha256\":\"%s\",\"selected_feature_count\":%zu,"
           "\"carrier_value_digest_sha256\":\"%s\"}\n",
           selection.field_digest_sha256, selection.feature_id_count,
           output.value_digest_sha256);
    exit_code = 0;
cleanup:
    free(vertices);
    core_mesh_asset_runtime_document_free(&mesh);
    ProceduralImportedSurfaceRegionV1_Free(&base);
    ProceduralImportedSurfaceRegionV1_Free(&output);
    return exit_code;
}
