#include "procedural/procedural_surface_plane_mesh.h"

#include "app/ray_tracing_sha256.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(ProceduralSurfacePlaneMeshReport *report,
                       ProceduralSurfacePlaneMeshStatus status,
                       const char *field,
                       const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    if (field) snprintf(report->field, sizeof(report->field), "%s", field);
    if (message) {
        snprintf(report->message, sizeof(report->message), "%s", message);
    }
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double smoother_step(double value) {
    const double t = clamp01(value);
    return t * t * t * ((t * ((t * 6.0) - 15.0)) + 10.0);
}

static ProceduralSurfaceFieldPoint3D vector_subtract(
    ProceduralSurfaceFieldPoint3D a,
    ProceduralSurfaceFieldPoint3D b) {
    ProceduralSurfaceFieldPoint3D result = {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z
    };
    return result;
}

static ProceduralSurfaceFieldPoint3D vector_cross(
    ProceduralSurfaceFieldPoint3D a,
    ProceduralSurfaceFieldPoint3D b) {
    ProceduralSurfaceFieldPoint3D result = {
        .x = (a.y * b.z) - (a.z * b.y),
        .y = (a.z * b.x) - (a.x * b.z),
        .z = (a.x * b.y) - (a.y * b.x)
    };
    return result;
}

static double vector_length(ProceduralSurfaceFieldPoint3D value) {
    return sqrt(
        (value.x * value.x) +
        (value.y * value.y) +
        (value.z * value.z));
}

static bool vector_is_finite(ProceduralSurfaceFieldPoint3D value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool field_output_is_valid(
    const ProceduralSurfaceFieldOutput *field) {
    return field &&
           isfinite(field->height) &&
           field->height >= -1.0 && field->height <= 1.0 &&
           isfinite(field->macro_variation) &&
           field->macro_variation >= -1.0 &&
           field->macro_variation <= 1.0 &&
           isfinite(field->micro_variation) &&
           field->micro_variation >= -1.0 &&
           field->micro_variation <= 1.0 &&
           isfinite(field->rock_mask) &&
           field->rock_mask >= 0.0 && field->rock_mask <= 1.0 &&
           isfinite(field->roughness) &&
           field->roughness >= 0.0 && field->roughness <= 1.0 &&
           isfinite(field->snow_precursor) &&
           field->snow_precursor >= 0.0 &&
           field->snow_precursor <= 1.0;
}

static uint64_t quality_triangle_budget(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality) {
    switch (quality) {
        case PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW:
            return recipe->quality.preview_max_triangles;
        case PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION:
            return recipe->quality.inspection_max_triangles;
        case PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL:
            return recipe->quality.final_max_triangles;
    }
    return 0u;
}

static bool recipe_and_cage_validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceTopologyExpectation *out_topology,
    ProceduralSurfacePlaneMeshReport *report) {
    ProceduralSurfaceRecipeReport recipe_report;
    if (!cage || !recipe || !out_topology) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT,
                   "",
                   "cage, recipe, and topology output are required");
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_Validate(recipe, &recipe_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_RECIPE,
                   recipe_report.field,
                   recipe_report.message);
        return false;
    }
    if (cage->kind != PROCEDURAL_SURFACE_CAGE_PLANE ||
        !isfinite(cage->target_edge_length_units) ||
        fabs(cage->target_edge_length_units -
             recipe->target_edge_length_units) > 1.0e-12 ||
        !ProceduralSurfaceTopologyContract_Derive(cage, out_topology)) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAGE,
                   "cage",
                   "a valid plane cage matching the recipe target edge is required");
        return false;
    }
    return true;
}

bool ProceduralSurfacePlaneMesh_DeriveRequirements(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePlaneMeshRequirements *out_requirements,
    ProceduralSurfacePlaneMeshReport *report) {
    ProceduralSurfaceTopologyExpectation topology;
    ProceduralSurfacePlaneMeshRequirements requirements;
    uint64_t triangle_budget;

    report_set(report, PROCEDURAL_SURFACE_PLANE_MESH_STATUS_OK, "", "ok");
    if (!out_requirements) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT,
                   "requirements",
                   "requirements output is required");
        return false;
    }
    if (!recipe_and_cage_validate(cage, recipe, &topology, report)) {
        return false;
    }
    triangle_budget = quality_triangle_budget(recipe, quality);
    if (triangle_budget == 0u ||
        topology.triangle_count > triangle_budget ||
        topology.vertex_count > recipe->quality.max_field_evaluations) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_QUALITY,
                   "quality_budgets",
                   "plane requirements exceed the selected quality budget");
        return false;
    }
    memset(&requirements, 0, sizeof(requirements));
    requirements.subdivisions_x = topology.subdivisions_x;
    requirements.subdivisions_y = topology.subdivisions_y;
    requirements.vertex_count = topology.vertex_count;
    requirements.triangle_count = topology.triangle_count;
    requirements.field_evaluation_count = topology.vertex_count;
    requirements.selected_triangle_budget = triangle_budget;
    *out_requirements = requirements;
    return true;
}

static size_t vertex_index(uint64_t x, uint64_t y, uint64_t nx) {
    return (size_t)(y * (nx + 1u) + x);
}

static double edge_lock_weight(const ProceduralSurfaceCageContract *cage,
                               const ProceduralSurfaceRecipeV1 *recipe,
                               double x,
                               double y) {
    const double half_width = cage->width_units * 0.5;
    const double half_height = cage->height_units * 0.5;
    const double distance_x = half_width - fabs(x);
    const double distance_y = half_height - fabs(y);
    const double distance = fmin(distance_x, distance_y);
    return smoother_step(distance / recipe->edge_lock_width_units);
}

static bool append_text(char *output,
                        size_t capacity,
                        size_t *length,
                        const char *format,
                        ...) {
    va_list arguments;
    int written;
    if (!output || !length || *length >= capacity) return false;
    va_start(arguments, format);
    written = vsnprintf(
        output + *length, capacity - *length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

static bool summary_digest(
    const char *recipe_digest,
    const ProceduralSurfacePlaneMesh *mesh,
    const ProceduralSurfacePlaneMeshSummary *summary,
    char out_digest[PROCEDURAL_SURFACE_PLANE_MESH_DIGEST_CAPACITY]) {
    char canonical[PROCEDURAL_SURFACE_PLANE_MESH_CANONICAL_CAPACITY];
    size_t length = 0u;
    if (!append_text(
            canonical,
            sizeof(canonical),
            &length,
            "{\"schema\":\"ray_tracing.procedural_surface_plane_mesh\","
            "\"schema_version\":1,\"recipe_digest_sha256\":\"%s\","
            "\"subdivisions\":[%llu,%llu],\"vertex_count\":%llu,"
            "\"triangle_count\":%llu,\"field_evaluation_count\":%llu,"
            "\"bounds_min\":[%.17g,%.17g,%.17g],"
            "\"bounds_max\":[%.17g,%.17g,%.17g],"
            "\"vertices\":[",
            recipe_digest,
            (unsigned long long)summary->subdivisions_x,
            (unsigned long long)summary->subdivisions_y,
            (unsigned long long)summary->vertex_count,
            (unsigned long long)summary->triangle_count,
            (unsigned long long)summary->field_evaluation_count,
            summary->bounds_min.x,
            summary->bounds_min.y,
            summary->bounds_min.z,
            summary->bounds_max.x,
            summary->bounds_max.y,
            summary->bounds_max.z)) {
        return false;
    }
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        const ProceduralSurfacePlaneVertex *vertex = &mesh->vertices[i];
        if (!append_text(
                canonical,
                sizeof(canonical),
                &length,
                "%s[%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
                "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g]",
                i == 0u ? "" : ",",
                vertex->position.x,
                vertex->position.y,
                vertex->position.z,
                vertex->normal.x,
                vertex->normal.y,
                vertex->normal.z,
                vertex->field.height,
                vertex->field.macro_variation,
                vertex->field.micro_variation,
                vertex->field.rock_mask,
                vertex->field.roughness,
                vertex->field.snow_precursor,
                vertex->edge_lock_weight,
                vertex->displacement_units)) {
            return false;
        }
    }
    if (!append_text(canonical, sizeof(canonical), &length, "],\"triangles\":["))
        return false;
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePlaneTriangle *triangle = &mesh->triangles[i];
        if (!append_text(
                canonical,
                sizeof(canonical),
                &length,
                "%s[%u,%u,%u]",
                i == 0u ? "" : ",",
                triangle->a,
                triangle->b,
                triangle->c)) {
            return false;
        }
    }
    if (!append_text(canonical, sizeof(canonical), &length, "]}")) return false;
    return ray_tracing_sha256_bytes(canonical, length, out_digest);
}

bool ProceduralSurfacePlaneMesh_Validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePlaneMesh *mesh,
    uint64_t field_evaluation_count,
    ProceduralSurfacePlaneMeshSummary *out_summary,
    ProceduralSurfacePlaneMeshReport *report) {
    ProceduralSurfaceTopologyExpectation topology;
    ProceduralSurfacePlaneMeshSummary summary;
    ProceduralSurfaceRecipeReport recipe_report;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    const double coordinate_tolerance = 1.0e-12;
    const double normal_tolerance = 1.0e-10;

    report_set(report, PROCEDURAL_SURFACE_PLANE_MESH_STATUS_OK, "", "ok");
    if (!mesh || !mesh->vertices || !mesh->triangles || !out_summary) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT,
                   "mesh",
                   "mesh arrays and summary output are required");
        return false;
    }
    if (!recipe_and_cage_validate(cage, recipe, &topology, report)) {
        return false;
    }
    if (mesh->vertex_count != topology.vertex_count ||
        mesh->triangle_count != topology.triangle_count ||
        field_evaluation_count != topology.vertex_count) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAPACITY,
                   "counts",
                   "mesh counts do not match the plane topology contract");
        return false;
    }

    memset(&summary, 0, sizeof(summary));
    summary.subdivisions_x = topology.subdivisions_x;
    summary.subdivisions_y = topology.subdivisions_y;
    summary.vertex_count = topology.vertex_count;
    summary.triangle_count = topology.triangle_count;
    summary.field_evaluation_count = field_evaluation_count;
    summary.bounds_min = mesh->vertices[0].position;
    summary.bounds_max = mesh->vertices[0].position;
    summary.minimum_twice_triangle_area_units2 = INFINITY;
    summary.minimum_normal_z = INFINITY;

    for (uint64_t y = 0u; y <= topology.subdivisions_y; ++y) {
        for (uint64_t x = 0u; x <= topology.subdivisions_x; ++x) {
            const size_t index = vertex_index(x, y, topology.subdivisions_x);
            const ProceduralSurfacePlaneVertex *vertex = &mesh->vertices[index];
            const double expected_x =
                (-cage->width_units * 0.5) +
                (cage->width_units * (double)x /
                 (double)topology.subdivisions_x);
            const double expected_y =
                (-cage->height_units * 0.5) +
                (cage->height_units * (double)y /
                 (double)topology.subdivisions_y);
            const bool boundary =
                x == 0u || y == 0u ||
                x == topology.subdivisions_x ||
                y == topology.subdivisions_y;
            const double normal_length = vector_length(vertex->normal);
            const double expected_weight =
                edge_lock_weight(cage, recipe, expected_x, expected_y);
            const double expected_displacement =
                vertex->field.height *
                recipe->displacement_amplitude_units *
                expected_weight;

            if (!vector_is_finite(vertex->position) ||
                !vector_is_finite(vertex->normal) ||
                !field_output_is_valid(&vertex->field) ||
                !isfinite(vertex->edge_lock_weight) ||
                !isfinite(vertex->displacement_units) ||
                fabs(vertex->position.x - expected_x) > coordinate_tolerance ||
                fabs(vertex->position.y - expected_y) > coordinate_tolerance ||
                fabs(vertex->position.z - vertex->displacement_units) >
                    coordinate_tolerance ||
                vertex->edge_lock_weight < 0.0 ||
                vertex->edge_lock_weight > 1.0 ||
                fabs(vertex->edge_lock_weight - expected_weight) >
                    coordinate_tolerance ||
                fabs(vertex->displacement_units - expected_displacement) >
                    coordinate_tolerance ||
                fabs(vertex->displacement_units) >
                    recipe->displacement_amplitude_units + coordinate_tolerance) {
                report_set(report,
                           PROCEDURAL_SURFACE_PLANE_MESH_STATUS_VERTEX,
                           "vertices",
                           "plane vertex violates position or displacement contract");
                return false;
            }
            if (boundary &&
                (vertex->edge_lock_weight != 0.0 ||
                 vertex->displacement_units != 0.0)) {
                report_set(report,
                           PROCEDURAL_SURFACE_PLANE_MESH_STATUS_VERTEX,
                           "edge_lock",
                           "boundary vertices must have exact zero displacement");
                return false;
            }
            if (!isfinite(normal_length) ||
                fabs(normal_length - 1.0) > normal_tolerance ||
                !(vertex->normal.z > 0.0)) {
                report_set(report,
                           PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL,
                           "normals",
                           "plane normals must be finite, unit length, and upward");
                return false;
            }
            summary.bounds_min.x = fmin(summary.bounds_min.x, vertex->position.x);
            summary.bounds_min.y = fmin(summary.bounds_min.y, vertex->position.y);
            summary.bounds_min.z = fmin(summary.bounds_min.z, vertex->position.z);
            summary.bounds_max.x = fmax(summary.bounds_max.x, vertex->position.x);
            summary.bounds_max.y = fmax(summary.bounds_max.y, vertex->position.y);
            summary.bounds_max.z = fmax(summary.bounds_max.z, vertex->position.z);
            summary.maximum_absolute_displacement_units = fmax(
                summary.maximum_absolute_displacement_units,
                fabs(vertex->displacement_units));
            if (boundary) {
                summary.maximum_boundary_absolute_displacement_units = fmax(
                    summary.maximum_boundary_absolute_displacement_units,
                    fabs(vertex->displacement_units));
            }
            summary.minimum_normal_z =
                fmin(summary.minimum_normal_z, vertex->normal.z);
        }
    }

    for (uint64_t y = 0u; y < topology.subdivisions_y; ++y) {
        for (uint64_t x = 0u; x < topology.subdivisions_x; ++x) {
            const size_t triangle_index =
                (size_t)((y * topology.subdivisions_x + x) * 2u);
            const uint32_t bottom_left =
                (uint32_t)vertex_index(x, y, topology.subdivisions_x);
            const uint32_t bottom_right =
                (uint32_t)vertex_index(x + 1u, y, topology.subdivisions_x);
            const uint32_t top_left =
                (uint32_t)vertex_index(x, y + 1u, topology.subdivisions_x);
            const uint32_t top_right =
                (uint32_t)vertex_index(x + 1u, y + 1u, topology.subdivisions_x);
            const ProceduralSurfacePlaneTriangle expected[2] = {
                {.a = bottom_left, .b = bottom_right, .c = top_right},
                {.a = bottom_left, .b = top_right, .c = top_left}
            };
            for (size_t local = 0u; local < 2u; ++local) {
                const ProceduralSurfacePlaneTriangle *triangle =
                    &mesh->triangles[triangle_index + local];
                ProceduralSurfaceFieldPoint3D ab;
                ProceduralSurfaceFieldPoint3D ac;
                ProceduralSurfaceFieldPoint3D cross;
                double twice_area;
                if (triangle->a != expected[local].a ||
                    triangle->b != expected[local].b ||
                    triangle->c != expected[local].c ||
                    triangle->a >= mesh->vertex_count ||
                    triangle->b >= mesh->vertex_count ||
                    triangle->c >= mesh->vertex_count) {
                    report_set(report,
                               PROCEDURAL_SURFACE_PLANE_MESH_STATUS_TRIANGLE,
                               "triangles",
                               "plane triangle violates fixed-diagonal indices");
                    return false;
                }
                ab = vector_subtract(
                    mesh->vertices[triangle->b].position,
                    mesh->vertices[triangle->a].position);
                ac = vector_subtract(
                    mesh->vertices[triangle->c].position,
                    mesh->vertices[triangle->a].position);
                cross = vector_cross(ab, ac);
                twice_area = vector_length(cross);
                if (!isfinite(twice_area) || !(twice_area > 0.0) ||
                    !(cross.z > 0.0)) {
                    report_set(report,
                               PROCEDURAL_SURFACE_PLANE_MESH_STATUS_TRIANGLE,
                               "triangles",
                               "plane triangles must have positive area and winding");
                    return false;
                }
                summary.minimum_twice_triangle_area_units2 = fmin(
                    summary.minimum_twice_triangle_area_units2, twice_area);
                summary.total_surface_area_units2 += twice_area * 0.5;
            }
        }
    }
    for (size_t vertex_index_value = 0u;
         vertex_index_value < mesh->vertex_count;
         ++vertex_index_value) {
        ProceduralSurfaceFieldPoint3D expected_normal = {0};
        double expected_length;
        for (size_t triangle_index = 0u;
             triangle_index < mesh->triangle_count;
             ++triangle_index) {
            const ProceduralSurfacePlaneTriangle *triangle =
                &mesh->triangles[triangle_index];
            if (triangle->a == vertex_index_value ||
                triangle->b == vertex_index_value ||
                triangle->c == vertex_index_value) {
                const ProceduralSurfaceFieldPoint3D ab = vector_subtract(
                    mesh->vertices[triangle->b].position,
                    mesh->vertices[triangle->a].position);
                const ProceduralSurfaceFieldPoint3D ac = vector_subtract(
                    mesh->vertices[triangle->c].position,
                    mesh->vertices[triangle->a].position);
                const ProceduralSurfaceFieldPoint3D cross =
                    vector_cross(ab, ac);
                expected_normal.x += cross.x;
                expected_normal.y += cross.y;
                expected_normal.z += cross.z;
            }
        }
        expected_length = vector_length(expected_normal);
        if (!isfinite(expected_length) || !(expected_length > 0.0)) {
            report_set(report,
                       PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL,
                       "normals",
                       "unable to derive expected smooth plane normal");
            return false;
        }
        expected_normal.x /= expected_length;
        expected_normal.y /= expected_length;
        expected_normal.z /= expected_length;
        if (fabs(mesh->vertices[vertex_index_value].normal.x -
                 expected_normal.x) > normal_tolerance ||
            fabs(mesh->vertices[vertex_index_value].normal.y -
                 expected_normal.y) > normal_tolerance ||
            fabs(mesh->vertices[vertex_index_value].normal.z -
                 expected_normal.z) > normal_tolerance) {
            report_set(report,
                       PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL,
                       "normals",
                       "stored plane normal does not match mesh geometry");
            return false;
        }
    }
    if (!ProceduralSurfaceRecipeV1_Digest(
            recipe, recipe_digest, &recipe_report) ||
        !summary_digest(recipe_digest, mesh, &summary, summary.mesh_digest_sha256)) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_SUMMARY,
                   "mesh_digest_sha256",
                   "unable to create canonical plane mesh digest");
        return false;
    }
    *out_summary = summary;
    return true;
}

bool ProceduralSurfacePlaneMesh_Generate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePlaneMeshBuffers *buffers,
    ProceduralSurfacePlaneMeshSummary *out_summary,
    ProceduralSurfacePlaneMeshReport *report) {
    ProceduralSurfacePlaneMeshRequirements requirements;
    ProceduralSurfacePlaneVertex *temporary_vertices = NULL;
    ProceduralSurfacePlaneTriangle *temporary_triangles = NULL;
    ProceduralSurfaceFieldBudget temporary_budget;
    ProceduralSurfacePlaneMesh temporary_mesh;
    ProceduralSurfacePlaneMeshSummary summary;
    ProceduralSurfaceFieldReport field_report;
    bool result = false;

    report_set(report, PROCEDURAL_SURFACE_PLANE_MESH_STATUS_OK, "", "ok");
    if (!field_budget || !buffers || !out_summary ||
        !buffers->vertices || !buffers->triangles) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT,
                   "buffers",
                   "field budget, mesh buffers, and summary are required");
        return false;
    }
    if (!ProceduralSurfacePlaneMesh_DeriveRequirements(
            cage, recipe, quality, &requirements, report)) {
        return false;
    }
    if (requirements.vertex_count > buffers->vertex_capacity ||
        requirements.triangle_count > buffers->triangle_capacity ||
        field_budget->max_evaluations !=
            recipe->quality.max_field_evaluations ||
        field_budget->evaluations > field_budget->max_evaluations ||
        requirements.field_evaluation_count >
            field_budget->max_evaluations - field_budget->evaluations) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAPACITY,
                   "buffers",
                   "mesh or field-evaluation capacity is insufficient");
        return false;
    }

    temporary_vertices = calloc(
        (size_t)requirements.vertex_count, sizeof(*temporary_vertices));
    temporary_triangles = calloc(
        (size_t)requirements.triangle_count, sizeof(*temporary_triangles));
    if (!temporary_vertices || !temporary_triangles) {
        report_set(report,
                   PROCEDURAL_SURFACE_PLANE_MESH_STATUS_ALLOCATION,
                   "temporary_mesh",
                   "unable to allocate bounded temporary plane mesh");
        goto cleanup;
    }
    temporary_budget = *field_budget;
    for (uint64_t y = 0u; y <= requirements.subdivisions_y; ++y) {
        for (uint64_t x = 0u; x <= requirements.subdivisions_x; ++x) {
            const size_t index =
                vertex_index(x, y, requirements.subdivisions_x);
            ProceduralSurfacePlaneVertex *vertex =
                &temporary_vertices[index];
            vertex->position.x =
                (-cage->width_units * 0.5) +
                (cage->width_units * (double)x /
                 (double)requirements.subdivisions_x);
            vertex->position.y =
                (-cage->height_units * 0.5) +
                (cage->height_units * (double)y /
                 (double)requirements.subdivisions_y);
            vertex->position.z = 0.0;
            if (!ProceduralSurfaceField3D_Evaluate(
                    recipe,
                    vertex->position,
                    &temporary_budget,
                    &vertex->field,
                    &field_report)) {
                report_set(report,
                           PROCEDURAL_SURFACE_PLANE_MESH_STATUS_FIELD,
                           field_report.field,
                           field_report.message);
                goto cleanup;
            }
            vertex->edge_lock_weight =
                edge_lock_weight(cage, recipe, vertex->position.x, vertex->position.y);
            vertex->displacement_units =
                vertex->field.height *
                recipe->displacement_amplitude_units *
                vertex->edge_lock_weight;
            if (x == 0u || y == 0u ||
                x == requirements.subdivisions_x ||
                y == requirements.subdivisions_y) {
                vertex->edge_lock_weight = 0.0;
                vertex->displacement_units = 0.0;
            }
            vertex->position.z = vertex->displacement_units;
        }
    }
    for (uint64_t y = 0u; y < requirements.subdivisions_y; ++y) {
        for (uint64_t x = 0u; x < requirements.subdivisions_x; ++x) {
            const size_t triangle_index =
                (size_t)((y * requirements.subdivisions_x + x) * 2u);
            const uint32_t bottom_left =
                (uint32_t)vertex_index(x, y, requirements.subdivisions_x);
            const uint32_t bottom_right =
                (uint32_t)vertex_index(x + 1u, y, requirements.subdivisions_x);
            const uint32_t top_left =
                (uint32_t)vertex_index(x, y + 1u, requirements.subdivisions_x);
            const uint32_t top_right =
                (uint32_t)vertex_index(
                    x + 1u, y + 1u, requirements.subdivisions_x);
            temporary_triangles[triangle_index] =
                (ProceduralSurfacePlaneTriangle){
                    .a = bottom_left, .b = bottom_right, .c = top_right};
            temporary_triangles[triangle_index + 1u] =
                (ProceduralSurfacePlaneTriangle){
                    .a = bottom_left, .b = top_right, .c = top_left};
        }
    }
    for (size_t i = 0u; i < (size_t)requirements.triangle_count; ++i) {
        const ProceduralSurfacePlaneTriangle *triangle =
            &temporary_triangles[i];
        const ProceduralSurfaceFieldPoint3D ab = vector_subtract(
            temporary_vertices[triangle->b].position,
            temporary_vertices[triangle->a].position);
        const ProceduralSurfaceFieldPoint3D ac = vector_subtract(
            temporary_vertices[triangle->c].position,
            temporary_vertices[triangle->a].position);
        const ProceduralSurfaceFieldPoint3D cross = vector_cross(ab, ac);
        const uint32_t indices[3] = {
            triangle->a, triangle->b, triangle->c};
        for (size_t j = 0u; j < 3u; ++j) {
            ProceduralSurfaceFieldPoint3D *normal =
                &temporary_vertices[indices[j]].normal;
            normal->x += cross.x;
            normal->y += cross.y;
            normal->z += cross.z;
        }
    }
    for (size_t i = 0u; i < (size_t)requirements.vertex_count; ++i) {
        ProceduralSurfaceFieldPoint3D *normal =
            &temporary_vertices[i].normal;
        const double length = vector_length(*normal);
        if (!isfinite(length) || !(length > 0.0)) {
            report_set(report,
                       PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL,
                       "normals",
                       "unable to normalize generated plane normal");
            goto cleanup;
        }
        normal->x /= length;
        normal->y /= length;
        normal->z /= length;
    }

    temporary_mesh.vertices = temporary_vertices;
    temporary_mesh.vertex_count = (size_t)requirements.vertex_count;
    temporary_mesh.triangles = temporary_triangles;
    temporary_mesh.triangle_count = (size_t)requirements.triangle_count;
    if (!ProceduralSurfacePlaneMesh_Validate(
            cage,
            recipe,
            &temporary_mesh,
            requirements.field_evaluation_count,
            &summary,
            report)) {
        goto cleanup;
    }

    memcpy(buffers->vertices,
           temporary_vertices,
           (size_t)requirements.vertex_count * sizeof(*temporary_vertices));
    memcpy(buffers->triangles,
           temporary_triangles,
           (size_t)requirements.triangle_count * sizeof(*temporary_triangles));
    buffers->vertex_count = (size_t)requirements.vertex_count;
    buffers->triangle_count = (size_t)requirements.triangle_count;
    *field_budget = temporary_budget;
    *out_summary = summary;
    result = true;

cleanup:
    free(temporary_vertices);
    free(temporary_triangles);
    return result;
}

const char *ProceduralSurfacePlaneMeshStatus_Name(
    ProceduralSurfacePlaneMeshStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAGE: return "cage";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_RECIPE: return "recipe";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_QUALITY: return "quality";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAPACITY:
            return "capacity";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_ALLOCATION:
            return "allocation";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_FIELD: return "field";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_VERTEX: return "vertex";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_TRIANGLE:
            return "triangle";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL: return "normal";
        case PROCEDURAL_SURFACE_PLANE_MESH_STATUS_SUMMARY: return "summary";
    }
    return "unknown";
}
