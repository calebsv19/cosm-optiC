#include "procedural_imported_surface_strands_internal.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRAND_PI 3.14159265358979323846264338327950288

typedef struct StrandTriangleBuild {
    size_t a;
    size_t b;
    size_t c;
    size_t source_triangle;
    size_t strand;
    size_t segment;
    ProceduralImportedSurfaceStrandRole role;
} StrandTriangleBuild;

static bool recompute_normals(CoreMeshAssetRuntimeDocument *document) {
    if (!document || !document->vertices || !document->triangles) return false;
    for (size_t i = 0u; i < document->vertex_count; ++i)
        document->vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[triangle->a].position;
        const CoreObjectVec3 b = document->vertices[triangle->b].position;
        const CoreObjectVec3 c = document->vertices[triangle->c].position;
        const CoreObjectVec3 normal = strand_vec_cross(
            strand_vec_sub(b, a), strand_vec_sub(c, a));
        document->vertices[triangle->a].normal = strand_vec_add(
            document->vertices[triangle->a].normal, normal);
        document->vertices[triangle->b].normal = strand_vec_add(
            document->vertices[triangle->b].normal, normal);
        document->vertices[triangle->c].normal = strand_vec_add(
            document->vertices[triangle->c].normal, normal);
    }
    for (size_t i = 0u; i < document->vertex_count; ++i)
        if (!strand_vec_normalize(
                document->vertices[i].normal,
                &document->vertices[i].normal)) return false;
    document->vertex_normal_count = document->vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    return true;
}

static double signed_volume(const CoreMeshAssetRuntimeDocument *document) {
    double volume = 0.0;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[triangle->a].position;
        const CoreObjectVec3 b = document->vertices[triangle->b].position;
        const CoreObjectVec3 c = document->vertices[triangle->c].position;
        volume += strand_vec_dot(a, strand_vec_cross(b, c)) / 6.0;
    }
    return volume;
}

static bool append_triangle(
    StrandTriangleBuild *triangles,
    size_t capacity,
    size_t *count,
    size_t a,
    size_t b,
    size_t c,
    size_t source_triangle,
    size_t strand,
    size_t segment,
    ProceduralImportedSurfaceStrandRole role) {
    if (!triangles || !count || *count >= capacity ||
        a == b || b == c || c == a) return false;
    triangles[(*count)++] = (StrandTriangleBuild){
        a, b, c, source_triangle, strand, segment, role};
    return true;
}

static bool strand_frame(
    const ProceduralImportedSurfaceStrandAsset *asset,
    size_t strand,
    size_t point,
    CoreObjectVec3 *out_u,
    CoreObjectVec3 *out_v) {
    const size_t count = asset->points_per_strand;
    const size_t base = strand * count;
    const CoreObjectVec3 before =
        asset->points[base + (point > 0u ? point - 1u : point)];
    const CoreObjectVec3 after =
        asset->points[base + (point + 1u < count ? point + 1u : point)];
    CoreObjectVec3 direction;
    CoreObjectVec3 u = asset->root_tangents[strand];
    if (!strand_vec_normalize(strand_vec_sub(after, before), &direction))
        return false;
    u = strand_vec_sub(
        u, strand_vec_scale(direction, strand_vec_dot(u, direction)));
    if (!strand_vec_normalize(u, &u)) {
        const CoreObjectVec3 reference =
            fabs(direction.z) < 0.85
                ? (CoreObjectVec3){0.0, 0.0, 1.0}
                : (CoreObjectVec3){0.0, 1.0, 0.0};
        if (!strand_vec_normalize(
                strand_vec_cross(reference, direction), &u)) return false;
    }
    *out_u = u;
    *out_v = strand_vec_cross(direction, u);
    return true;
}

bool surface_strand_build_tubes(
    const CoreMeshAssetRuntimeDocument *source,
    const SurfaceStrandSelection *selection,
    const ProceduralImportedSurfaceStrandConfig *config,
    const char *strand_asset_id,
    const ProceduralImportedSurfaceStrandAsset *asset,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceStrandProvenance *out_provenance,
    ProceduralImportedSurfaceStrandReceipt *receipt) {
    CoreMeshAssetRuntimeDocument result;
    ProceduralImportedSurfaceStrandProvenance provenance;
    StrandTriangleBuild *build = NULL;
    const size_t rings = asset ? asset->points_per_strand : 0u;
    size_t vertices_per_strand;
    size_t triangles_per_strand;
    size_t total_vertices;
    size_t total_triangles;
    size_t build_count = 0u;
    size_t output_index = 0u;
    CoreResult core_result;
    core_mesh_asset_runtime_document_init(&result);
    ProceduralImportedSurfaceStrandProvenance_Init(&provenance);
    if (!source || !selection || !config || !strand_asset_id || !asset ||
        !out_document || !out_provenance || !receipt ||
        asset->strand_count == 0u || rings < 2u) return false;
    vertices_per_strand = rings * config->radial_segments + 2u;
    triangles_per_strand =
        (rings - 1u) * config->radial_segments * 2u +
        config->radial_segments * 2u;
    if (asset->strand_count > SIZE_MAX / vertices_per_strand ||
        asset->strand_count > SIZE_MAX / triangles_per_strand) return false;
    total_vertices = asset->strand_count * vertices_per_strand;
    total_triangles = asset->strand_count * triangles_per_strand;
    if (total_vertices > config->max_vertices ||
        total_triangles > config->max_triangles) return false;
    build = calloc(total_triangles, sizeof(*build));
    if (!build) return false;
    core_result = core_mesh_asset_runtime_contract_set_asset_id(
        &result.contract, strand_asset_id);
    if (core_result.code != CORE_OK) goto fail;
    core_result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &result.contract, source->contract.asset_id);
    if (core_result.code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            &result, total_vertices).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            &result, total_triangles).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            &result, 3u).code != CORE_OK) goto fail;
    provenance.triangle_count = total_triangles;
    provenance.source_triangle_indices = calloc(
        total_triangles, sizeof(*provenance.source_triangle_indices));
    provenance.strand_indices = calloc(
        total_triangles, sizeof(*provenance.strand_indices));
    provenance.segment_indices = calloc(
        total_triangles, sizeof(*provenance.segment_indices));
    provenance.roles = calloc(total_triangles, sizeof(*provenance.roles));
    if (!provenance.source_triangle_indices || !provenance.strand_indices ||
        !provenance.segment_indices || !provenance.roles) goto fail;
    result.contract.local_bounds.min = (CoreObjectVec3){
        DBL_MAX, DBL_MAX, DBL_MAX};
    result.contract.local_bounds.max = (CoreObjectVec3){
        -DBL_MAX, -DBL_MAX, -DBL_MAX};
    for (size_t strand = 0u; strand < asset->strand_count; ++strand) {
        const size_t vertex_base = strand * vertices_per_strand;
        const size_t point_base = strand * rings;
        const size_t root_center =
            vertex_base + rings * config->radial_segments;
        const size_t tip_center = root_center + 1u;
        for (size_t point = 0u; point < rings; ++point) {
            CoreObjectVec3 u;
            CoreObjectVec3 v;
            if (!strand_frame(asset, strand, point, &u, &v)) goto fail;
            for (size_t radial = 0u;
                 radial < config->radial_segments; ++radial) {
                const double angle =
                    2.0 * STRAND_PI * (double)radial /
                    (double)config->radial_segments;
                CoreObjectVec3 position = asset->points[point_base + point];
                position = strand_vec_add(
                    position, strand_vec_scale(
                        u, asset->radii[point_base + point] * cos(angle)));
                position = strand_vec_add(
                    position, strand_vec_scale(
                        v, asset->radii[point_base + point] * sin(angle)));
                result.vertices[
                    vertex_base + point * config->radial_segments +
                    radial].position = position;
                if (position.x < result.contract.local_bounds.min.x)
                    result.contract.local_bounds.min.x = position.x;
                if (position.y < result.contract.local_bounds.min.y)
                    result.contract.local_bounds.min.y = position.y;
                if (position.z < result.contract.local_bounds.min.z)
                    result.contract.local_bounds.min.z = position.z;
                if (position.x > result.contract.local_bounds.max.x)
                    result.contract.local_bounds.max.x = position.x;
                if (position.y > result.contract.local_bounds.max.y)
                    result.contract.local_bounds.max.y = position.y;
                if (position.z > result.contract.local_bounds.max.z)
                    result.contract.local_bounds.max.z = position.z;
            }
        }
        result.vertices[root_center].position = asset->points[point_base];
        result.vertices[tip_center].position =
            asset->points[point_base + rings - 1u];
        for (size_t radial = 0u;
             radial < config->radial_segments; ++radial) {
            const size_t next =
                (radial + 1u) % config->radial_segments;
            if (!append_triangle(
                    build, total_triangles, &build_count,
                    root_center,
                    vertex_base + next,
                    vertex_base + radial,
                    asset->source_triangle_indices[strand], strand, 0u,
                    PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_ROOT_CAP) ||
                !append_triangle(
                    build, total_triangles, &build_count,
                    tip_center,
                    vertex_base + (rings - 1u) *
                        config->radial_segments + radial,
                    vertex_base + (rings - 1u) *
                        config->radial_segments + next,
                    asset->source_triangle_indices[strand], strand,
                    config->curve_segment_count,
                    PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_TIP_CAP)) {
                goto fail;
            }
        }
        for (size_t segment = 0u;
             segment + 1u < rings; ++segment) {
            const size_t first =
                vertex_base + segment * config->radial_segments;
            const size_t second = first + config->radial_segments;
            for (size_t radial = 0u;
                 radial < config->radial_segments; ++radial) {
                const size_t next =
                    (radial + 1u) % config->radial_segments;
                if (!append_triangle(
                        build, total_triangles, &build_count,
                        first + radial, second + radial, second + next,
                        asset->source_triangle_indices[strand], strand,
                        segment,
                        PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_SHAFT) ||
                    !append_triangle(
                        build, total_triangles, &build_count,
                        first + radial, second + next, first + next,
                        asset->source_triangle_indices[strand], strand,
                        segment,
                        PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_SHAFT)) {
                    goto fail;
                }
            }
        }
    }
    if (build_count != total_triangles) goto fail;
    for (unsigned int role = 0u; role < 3u; ++role) {
        const size_t group_start = output_index;
        for (size_t i = 0u; i < build_count; ++i) {
            CoreMeshAssetRuntimeTriangle *triangle;
            if ((unsigned int)build[i].role != role) continue;
            triangle = &result.triangles[output_index];
            triangle->a = build[i].a;
            triangle->b = build[i].b;
            triangle->c = build[i].c;
            snprintf(
                triangle->surface_group_id,
                sizeof(triangle->surface_group_id), "%s",
                ProceduralImportedSurfaceStrandRole_Name(build[i].role));
            provenance.source_triangle_indices[output_index] =
                build[i].source_triangle;
            provenance.strand_indices[output_index] = build[i].strand;
            provenance.segment_indices[output_index] = build[i].segment;
            provenance.roles[output_index] = build[i].role;
            if (role == PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_ROOT_CAP)
                ++receipt->root_cap_triangle_count;
            else if (role == PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_SHAFT)
                ++receipt->shaft_triangle_count;
            else
                ++receipt->tip_cap_triangle_count;
            ++output_index;
        }
        snprintf(
            result.surface_groups[role].group_id,
            sizeof(result.surface_groups[role].group_id), "%s",
            ProceduralImportedSurfaceStrandRole_Name(
                (ProceduralImportedSurfaceStrandRole)role));
        result.surface_groups[role].triangle_start = group_start;
        result.surface_groups[role].triangle_count =
            output_index - group_start;
    }
    if (output_index != total_triangles ||
        receipt->root_cap_triangle_count == 0u ||
        receipt->shaft_triangle_count == 0u ||
        receipt->tip_cap_triangle_count == 0u) goto fail;
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
    free(build);
    *out_document = result;
    *out_provenance = provenance;
    return true;
fail:
    free(build);
    core_mesh_asset_runtime_document_free(&result);
    ProceduralImportedSurfaceStrandProvenance_Free(&provenance);
    return false;
}
