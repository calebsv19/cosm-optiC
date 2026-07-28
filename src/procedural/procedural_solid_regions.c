#include "procedural/procedural_solid_regions.h"

#include "app/ray_tracing_sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SolidTriangleRegion {
    size_t region_index;
} SolidTriangleRegion;

static CoreObjectVec3 centroid(
    CoreObjectVec3 a,
    CoreObjectVec3 b,
    CoreObjectVec3 c) {
    return (CoreObjectVec3){
        (a.x + b.x + c.x) / 3.0,
        (a.y + b.y + c.y) / 3.0,
        (a.z + b.z + c.z) / 3.0};
}

static void region_report(
    ProceduralSolidMeshReport *report,
    ProceduralSolidMeshStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field);
    snprintf(report->message, sizeof(report->message), "%s", message);
}

static bool make_region_id(
    ProceduralSolidRegionKind kind,
    const char *primary,
    const char *secondary,
    char out_id[64]) {
    char semantic[256];
    char digest[65];
    const int count = snprintf(
        semantic, sizeof(semantic), "%s|%s|%s",
        ProceduralSolidRegionKind_Name(kind),
        primary ? primary : "", secondary ? secondary : "");
    if (count < 0 || (size_t)count >= sizeof(semantic) ||
        !ray_tracing_sha256_bytes(semantic, (size_t)count, digest)) {
        return false;
    }
    snprintf(
        out_id, 64, "psg11_%s_%.16s",
        ProceduralSolidRegionKind_Name(kind), digest);
    return true;
}

static size_t find_or_add_region(
    ProceduralSolidRegionSummary *summary,
    const ProceduralSolidSample *sample) {
    char region_id[64];
    if (!make_region_id(
            sample->region_kind, sample->contributing_node_id,
            sample->secondary_contributing_node_id, region_id)) {
        return SIZE_MAX;
    }
    for (size_t i = 0u; i < summary->region_count; ++i) {
        if (strcmp(summary->regions[i].region_id, region_id) == 0) {
            return i;
        }
    }
    if (summary->region_count >= PROCEDURAL_SOLID_REGION_MAX) {
        return SIZE_MAX;
    }
    {
        const size_t index = summary->region_count++;
        ProceduralSolidRegionRecord *record = &summary->regions[index];
        memset(record, 0, sizeof(*record));
        snprintf(record->region_id, sizeof(record->region_id), "%s",
                 region_id);
        record->kind = sample->region_kind;
        snprintf(
            record->primary_node_id, sizeof(record->primary_node_id),
            "%s", sample->contributing_node_id);
        snprintf(
            record->secondary_node_id, sizeof(record->secondary_node_id),
            "%s", sample->secondary_contributing_node_id);
        return index;
    }
}

static void sort_region_order(
    const ProceduralSolidRegionSummary *summary,
    size_t *order) {
    for (size_t i = 0u; i < summary->region_count; ++i) order[i] = i;
    for (size_t i = 1u; i < summary->region_count; ++i) {
        const size_t value = order[i];
        size_t j = i;
        while (j > 0u &&
               strcmp(
                   summary->regions[order[j - 1u]].region_id,
                   summary->regions[value].region_id) > 0) {
            order[j] = order[j - 1u];
            --j;
        }
        order[j] = value;
    }
}

static bool compute_region_digest(
    const ProceduralSolidRegionSummary *summary,
    char out_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY]) {
    char canonical[
        PROCEDURAL_SOLID_REGION_MAX *
        (64u + PROCEDURAL_SOLID_GRAPH_ID_CAPACITY * 2u + 64u)];
    size_t length = 0u;
    for (size_t i = 0u; i < summary->region_count; ++i) {
        const ProceduralSolidRegionRecord *record = &summary->regions[i];
        const int count = snprintf(
            canonical + length, sizeof(canonical) - length,
            "%s|%s|%s|%s|%zu;",
            record->region_id,
            ProceduralSolidRegionKind_Name(record->kind),
            record->primary_node_id, record->secondary_node_id,
            record->triangle_count);
        if (count < 0 || (size_t)count >= sizeof(canonical) - length) {
            return false;
        }
        length += (size_t)count;
    }
    return ray_tracing_sha256_bytes(canonical, length, out_digest);
}

bool ProceduralSolidRegions_Assign(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidRegionSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidRegionSummary summary;
    SolidTriangleRegion *triangle_regions = NULL;
    CoreMeshAssetRuntimeTriangle *triangles = NULL;
    CoreMeshAssetSurfaceGroup *groups = NULL;
    size_t order[PROCEDURAL_SOLID_REGION_MAX];
    size_t old_to_new[PROCEDURAL_SOLID_REGION_MAX];
    if (!graph || !mesh_config || !document || !mesh_summary ||
        !out_summary || document->triangle_count == 0u ||
        !document->triangles || !document->vertices) {
        region_report(report, PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
                      "regions", "solid region inputs are required");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    triangle_regions = calloc(
        document->triangle_count, sizeof(*triangle_regions));
    triangles = malloc(document->triangle_count * sizeof(*triangles));
    if (!triangle_regions || !triangles) {
        free(triangle_regions);
        free(triangles);
        region_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                      "regions", "solid region allocation failed");
        return false;
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle =
            &document->triangles[i];
        ProceduralSolidGraphReport graph_report;
        ProceduralSolidSample sample;
        const CoreObjectVec3 point = centroid(
            document->vertices[triangle->a].position,
            document->vertices[triangle->b].position,
            document->vertices[triangle->c].position);
        size_t region_index;
        if (!ProceduralSolidGraphV1_Evaluate(
                graph, sources, point, &sample, &graph_report)) {
            free(triangle_regions);
            free(triangles);
            region_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                          graph_report.field, graph_report.message);
            return false;
        }
        region_index = find_or_add_region(&summary, &sample);
        if (region_index == SIZE_MAX) {
            free(triangle_regions);
            free(triangles);
            region_report(report, PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
                          "regions",
                          "solid region identity capacity exceeded");
            return false;
        }
        triangle_regions[i].region_index = region_index;
        ++summary.regions[region_index].triangle_count;
        if (sample.region_kind == PROCEDURAL_SOLID_REGION_CUT) {
            ++summary.cut_triangle_count;
        } else if (sample.region_kind == PROCEDURAL_SOLID_REGION_BLEND) {
            ++summary.blend_triangle_count;
        } else {
            ++summary.retained_triangle_count;
        }
    }
    sort_region_order(&summary, order);
    for (size_t i = 0u; i < summary.region_count; ++i) {
        old_to_new[order[i]] = i;
    }
    {
        ProceduralSolidRegionSummary sorted = summary;
        for (size_t i = 0u; i < summary.region_count; ++i) {
            sorted.regions[i] = summary.regions[order[i]];
        }
        summary = sorted;
    }
    groups = calloc(summary.region_count, sizeof(*groups));
    if (!groups) {
        free(triangle_regions);
        free(triangles);
        region_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                      "region_groups",
                      "solid surface-group allocation failed");
        return false;
    }
    {
        size_t cursor = 0u;
        for (size_t region = 0u; region < summary.region_count; ++region) {
            groups[region].triangle_start = cursor;
            snprintf(
                groups[region].group_id,
                sizeof(groups[region].group_id), "%s",
                summary.regions[region].region_id);
            for (size_t i = 0u; i < document->triangle_count; ++i) {
                if (old_to_new[triangle_regions[i].region_index] != region) {
                    continue;
                }
                triangles[cursor] = document->triangles[i];
                snprintf(
                    triangles[cursor].surface_group_id,
                    sizeof(triangles[cursor].surface_group_id), "%s",
                    groups[region].group_id);
                ++cursor;
            }
            groups[region].triangle_count =
                cursor - groups[region].triangle_start;
        }
        if (cursor != document->triangle_count) {
            free(triangle_regions);
            free(triangles);
            free(groups);
            region_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                          "region_order",
                          "solid region reorder lost triangles");
            return false;
        }
    }
    free(document->triangles);
    free(document->surface_groups);
    document->triangles = triangles;
    document->surface_groups = groups;
    document->surface_group_count = summary.region_count;
    free(triangle_regions);
    if (!compute_region_digest(&summary, summary.region_digest_sha256) ||
        !ProceduralSolidMesh_Reanalyze(
            mesh_config, document, mesh_summary, report)) {
        if (report && report->status == PROCEDURAL_SOLID_MESH_STATUS_OK) {
            region_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                          "region_digest",
                          "solid region digest failed");
        }
        return false;
    }
    *out_summary = summary;
    return true;
}
