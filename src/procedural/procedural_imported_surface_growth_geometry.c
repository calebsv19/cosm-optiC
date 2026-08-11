#include "procedural_imported_surface_growth_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GROWTH_PI 3.14159265358979323846264338327950288

typedef struct GrowthTriangleBuild {
    size_t a;
    size_t b;
    size_t c;
    size_t source_triangle;
    size_t element_index;
    ProceduralImportedSurfaceGrowthRole role;
} GrowthTriangleBuild;

static bool recompute_normals(CoreMeshAssetRuntimeDocument *document) {
    if (!document || !document->vertices || !document->triangles) return false;
    for (size_t i = 0u; i < document->vertex_count; ++i)
        document->vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[triangle->a].position;
        const CoreObjectVec3 b = document->vertices[triangle->b].position;
        const CoreObjectVec3 c = document->vertices[triangle->c].position;
        const CoreObjectVec3 normal = growth_vec_cross(
            growth_vec_sub(b, a), growth_vec_sub(c, a));
        document->vertices[triangle->a].normal = growth_vec_add(
            document->vertices[triangle->a].normal, normal);
        document->vertices[triangle->b].normal = growth_vec_add(
            document->vertices[triangle->b].normal, normal);
        document->vertices[triangle->c].normal = growth_vec_add(
            document->vertices[triangle->c].normal, normal);
    }
    for (size_t i = 0u; i < document->vertex_count; ++i)
        if (!growth_vec_normalize(
                document->vertices[i].normal,
                &document->vertices[i].normal)) return false;
    document->vertex_normal_count = document->vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    return true;
}

static double triangle_local_z(
    const SurfaceGrowthElement *element,
    CoreObjectVec3 a,
    CoreObjectVec3 b,
    CoreObjectVec3 c) {
    const CoreObjectVec3 centroid = growth_vec_scale(
        growth_vec_add(growth_vec_add(a, b), c), 1.0 / 3.0);
    return growth_vec_dot(
        growth_vec_sub(centroid, element->anchor), element->normal);
}

static bool append_build_triangle(
    GrowthTriangleBuild *triangles,
    size_t capacity,
    size_t *count,
    size_t a,
    size_t b,
    size_t c,
    size_t source_triangle,
    size_t element_index,
    ProceduralImportedSurfaceGrowthRole role) {
    if (!triangles || !count || *count >= capacity ||
        a == b || b == c || c == a) return false;
    triangles[(*count)++] = (GrowthTriangleBuild){
        a, b, c, source_triangle, element_index, role};
    return true;
}

static CoreObjectVec3 growth_position(
    const SurfaceGrowthElement *element,
    double theta,
    double phi) {
    const double radial = element->radius * sin(theta);
    const double midpoint =
        (element->height - element->attachment_depth) * 0.5;
    const double half_vertical =
        (element->height + element->attachment_depth) * 0.5;
    const double z = midpoint + half_vertical * cos(theta);
    CoreObjectVec3 result = element->anchor;
    result = growth_vec_add(
        result, growth_vec_scale(
            element->tangent, radial * cos(phi)));
    result = growth_vec_add(
        result, growth_vec_scale(
            element->bitangent, radial * element->aspect * sin(phi)));
    return growth_vec_add(result, growth_vec_scale(element->normal, z));
}

static double signed_volume(const CoreMeshAssetRuntimeDocument *document) {
    double volume = 0.0;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[triangle->a].position;
        const CoreObjectVec3 b = document->vertices[triangle->b].position;
        const CoreObjectVec3 c = document->vertices[triangle->c].position;
        volume += growth_vec_dot(a, growth_vec_cross(b, c)) / 6.0;
    }
    return volume;
}

bool surface_growth_build_geometry(
    const CoreMeshAssetRuntimeDocument *source,
    const SurfaceGrowthSelection *selection,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *receipt) {
    CoreMeshAssetRuntimeDocument result;
    ProceduralImportedSurfaceGrowthProvenance provenance;
    GrowthTriangleBuild *build_triangles = NULL;
    size_t vertices_per_element;
    size_t triangles_per_element;
    size_t total_vertices;
    size_t total_triangles;
    size_t build_count = 0u;
    size_t output_index = 0u;
    CoreResult core_result;
    core_mesh_asset_runtime_document_init(&result);
    ProceduralImportedSurfaceGrowthProvenance_Init(&provenance);
    if (!source || !selection || !config || !growth_asset_id ||
        !out_document || !out_provenance || !receipt ||
        selection->count == 0u) return false;
    vertices_per_element =
        2u + (config->latitude_segments - 1u) * config->radial_segments;
    triangles_per_element =
        2u * config->radial_segments * (config->latitude_segments - 1u);
    if (selection->count > SIZE_MAX / vertices_per_element ||
        selection->count > SIZE_MAX / triangles_per_element) return false;
    total_vertices = selection->count * vertices_per_element;
    total_triangles = selection->count * triangles_per_element;
    if (total_vertices > config->max_vertices ||
        total_triangles > config->max_triangles) return false;
    build_triangles = calloc(total_triangles, sizeof(*build_triangles));
    if (!build_triangles) return false;
    core_result = core_mesh_asset_runtime_contract_set_asset_id(
        &result.contract, growth_asset_id);
    if (core_result.code != CORE_OK) goto fail;
    core_result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &result.contract, source->contract.asset_id);
    if (core_result.code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            &result, total_vertices).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            &result, total_triangles).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            &result, 2u).code != CORE_OK) goto fail;
    provenance.triangle_count = total_triangles;
    provenance.source_triangle_indices = calloc(
        total_triangles, sizeof(*provenance.source_triangle_indices));
    provenance.growth_element_indices = calloc(
        total_triangles, sizeof(*provenance.growth_element_indices));
    provenance.roles = calloc(total_triangles, sizeof(*provenance.roles));
    if (!provenance.source_triangle_indices ||
        !provenance.growth_element_indices || !provenance.roles) goto fail;
    for (size_t element_index = 0u;
         element_index < selection->count; ++element_index) {
        const SurfaceGrowthElement *element =
            &selection->elements[element_index];
        const size_t base = element_index * vertices_per_element;
        const size_t top = base;
        const size_t bottom = base + vertices_per_element - 1u;
        result.vertices[top].position =
            growth_position(element, 0.0, 0.0);
        result.vertices[bottom].position =
            growth_position(element, GROWTH_PI, 0.0);
        for (size_t latitude = 1u;
             latitude < config->latitude_segments; ++latitude) {
            const double theta =
                GROWTH_PI * (double)latitude /
                (double)config->latitude_segments;
            const size_t ring =
                base + 1u +
                (latitude - 1u) * config->radial_segments;
            for (size_t radial = 0u;
                 radial < config->radial_segments; ++radial) {
                const double phi =
                    2.0 * GROWTH_PI * (double)radial /
                    (double)config->radial_segments;
                result.vertices[ring + radial].position =
                    growth_position(element, theta, phi);
            }
        }
        {
            const size_t first_ring = base + 1u;
            const size_t last_ring =
                base + 1u +
                (config->latitude_segments - 2u) *
                    config->radial_segments;
            for (size_t radial = 0u;
                 radial < config->radial_segments; ++radial) {
                const size_t next =
                    (radial + 1u) % config->radial_segments;
                const size_t top_a = first_ring + radial;
                const size_t top_b = first_ring + next;
                const size_t bottom_a = last_ring + radial;
                const size_t bottom_b = last_ring + next;
                const CoreObjectVec3 ta = result.vertices[top].position;
                const CoreObjectVec3 tb =
                    result.vertices[top_b].position;
                const CoreObjectVec3 tc =
                    result.vertices[top_a].position;
                const CoreObjectVec3 ba =
                    result.vertices[bottom].position;
                const CoreObjectVec3 bb =
                    result.vertices[bottom_a].position;
                const CoreObjectVec3 bc =
                    result.vertices[bottom_b].position;
                if (!append_build_triangle(
                        build_triangles, total_triangles, &build_count,
                        top, top_b, top_a,
                        element->source_triangle_index, element_index,
                        triangle_local_z(element, ta, tb, tc) >= 0.0
                            ? PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH
                            : PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE) ||
                    !append_build_triangle(
                        build_triangles, total_triangles, &build_count,
                        bottom, bottom_a, bottom_b,
                        element->source_triangle_index, element_index,
                        triangle_local_z(element, ba, bb, bc) >= 0.0
                            ? PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH
                            : PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE)) {
                    goto fail;
                }
            }
        }
        for (size_t latitude = 1u;
             latitude + 1u < config->latitude_segments; ++latitude) {
            const size_t upper =
                base + 1u +
                (latitude - 1u) * config->radial_segments;
            const size_t lower =
                upper + config->radial_segments;
            for (size_t radial = 0u;
                 radial < config->radial_segments; ++radial) {
                const size_t next =
                    (radial + 1u) % config->radial_segments;
                const size_t ids[4] = {
                    upper + radial, upper + next,
                    lower + next, lower + radial};
                const CoreObjectVec3 a =
                    result.vertices[ids[0]].position;
                const CoreObjectVec3 b =
                    result.vertices[ids[1]].position;
                const CoreObjectVec3 c =
                    result.vertices[ids[2]].position;
                const CoreObjectVec3 d =
                    result.vertices[ids[3]].position;
                if (!append_build_triangle(
                        build_triangles, total_triangles, &build_count,
                        ids[0], ids[1], ids[2],
                        element->source_triangle_index, element_index,
                        triangle_local_z(element, a, b, c) >= 0.0
                            ? PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH
                            : PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE) ||
                    !append_build_triangle(
                        build_triangles, total_triangles, &build_count,
                        ids[0], ids[2], ids[3],
                        element->source_triangle_index, element_index,
                        triangle_local_z(element, a, c, d) >= 0.0
                            ? PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH
                            : PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE)) {
                    goto fail;
                }
            }
        }
    }
    if (build_count != total_triangles) goto fail;
    for (unsigned int role = 0u; role < 2u; ++role) {
        for (size_t i = 0u; i < build_count; ++i) {
            CoreMeshAssetRuntimeTriangle *triangle;
            if ((unsigned int)build_triangles[i].role != role) continue;
            triangle = &result.triangles[output_index];
            triangle->a = build_triangles[i].a;
            triangle->b = build_triangles[i].b;
            triangle->c = build_triangles[i].c;
            snprintf(
                triangle->surface_group_id,
                sizeof(triangle->surface_group_id), "%s",
                role == PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH
                    ? "exposed_growth" : "attachment_base");
            provenance.source_triangle_indices[output_index] =
                build_triangles[i].source_triangle;
            provenance.growth_element_indices[output_index] =
                build_triangles[i].element_index;
            provenance.roles[output_index] = build_triangles[i].role;
            if (role ==
                PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH)
                ++receipt->exposed_growth_triangle_count;
            else
                ++receipt->attachment_base_triangle_count;
            ++output_index;
        }
    }
    if (output_index != total_triangles ||
        receipt->exposed_growth_triangle_count == 0u ||
        receipt->attachment_base_triangle_count == 0u) goto fail;
    snprintf(
        result.surface_groups[0].group_id,
        sizeof(result.surface_groups[0].group_id), "exposed_growth");
    result.surface_groups[0].triangle_start = 0u;
    result.surface_groups[0].triangle_count =
        receipt->exposed_growth_triangle_count;
    snprintf(
        result.surface_groups[1].group_id,
        sizeof(result.surface_groups[1].group_id), "attachment_base");
    result.surface_groups[1].triangle_start =
        receipt->exposed_growth_triangle_count;
    result.surface_groups[1].triangle_count =
        receipt->attachment_base_triangle_count;
    result.contract.asset_type = source->contract.asset_type;
    result.contract.pivot = source->contract.pivot;
    result.contract.topology_closed_volume = true;
    result.contract.topology_manifold_expected = true;
    if (signed_volume(&result) < 0.0) {
        for (size_t i = 0u; i < result.triangle_count; ++i) {
            const size_t temporary = result.triangles[i].b;
            result.triangles[i].b = result.triangles[i].c;
            result.triangles[i].c = temporary;
        }
    }
    if (!recompute_normals(&result)) goto fail;
    free(build_triangles);
    *out_document = result;
    *out_provenance = provenance;
    return true;
fail:
    free(build_triangles);
    core_mesh_asset_runtime_document_free(&result);
    ProceduralImportedSurfaceGrowthProvenance_Free(&provenance);
    return false;
}
