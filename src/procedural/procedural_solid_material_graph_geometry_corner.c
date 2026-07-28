#include "procedural/procedural_solid_material_graph.h"

#include <math.h>
#include <stdlib.h>

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double vertex_signed_up_slope(
    const CoreMeshAssetRuntimeVertex *vertex) {
    double length;
    if (!vertex) return 0.0;
    length = sqrt(
        (vertex->normal.x * vertex->normal.x) +
        (vertex->normal.y * vertex->normal.y) +
        (vertex->normal.z * vertex->normal.z));
    if (length <= 1e-12) return 0.0;
    return clamp01(vertex->normal.z / length);
}

bool ProceduralSolidMaterialGeometryCornerInputs_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialGeometryInputs *out_inputs,
    size_t input_count,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialGeometryInputs *triangle_inputs = NULL;
    double min_z;
    double max_z;
    if (!mesh || !out_inputs ||
        input_count != mesh->triangle_count * 3u ||
        !mesh->vertices || !mesh->triangles ||
        mesh->vertex_count == 0u) {
        return false;
    }
    triangle_inputs = calloc(mesh->triangle_count, sizeof(*triangle_inputs));
    if (!triangle_inputs && mesh->triangle_count > 0u) return false;
    if (!ProceduralSolidMaterialGeometryInputs_Build(
            mesh, region_kinds, triangle_inputs, mesh->triangle_count,
            report)) {
        free(triangle_inputs);
        return false;
    }
    min_z = mesh->vertices[0].position.z;
    max_z = min_z;
    for (size_t i = 1u; i < mesh->vertex_count; ++i) {
        double z = mesh->vertices[i].position.z;
        if (z < min_z) min_z = z;
        if (z > max_z) max_z = z;
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &mesh->triangles[i];
        size_t vertices[3] = {triangle->a, triangle->b, triangle->c};
        for (size_t corner = 0u; corner < 3u; ++corner) {
            const CoreMeshAssetRuntimeVertex *vertex;
            ProceduralSolidMaterialGeometryInputs *input;
            if (vertices[corner] >= mesh->vertex_count) {
                free(triangle_inputs);
                return false;
            }
            vertex = &mesh->vertices[vertices[corner]];
            input = &out_inputs[(i * 3u) + corner];
            *input = triangle_inputs[i];
            input->height =
                max_z > min_z
                    ? clamp01((vertex->position.z - min_z) /
                              (max_z - min_z))
                    : 0.5;
            input->slope = vertex_signed_up_slope(vertex);
            input->object_x = vertex->position.x;
            input->object_y = vertex->position.y;
            input->object_z = vertex->position.z;
        }
    }
    free(triangle_inputs);
    return true;
}
