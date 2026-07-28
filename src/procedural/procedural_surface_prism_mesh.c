#include "procedural/procedural_surface_prism_mesh.h"

#include "app/ray_tracing_sha256.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRISM_CANONICAL_BASE_CAPACITY 1024u
#define PRISM_CANONICAL_VERTEX_CAPACITY 320u
#define PRISM_CANONICAL_TRIANGLE_CAPACITY 80u

typedef struct PrismEdge {
    uint32_t a;
    uint32_t b;
} PrismEdge;

typedef struct FaceDescriptor {
    ProceduralSurfacePrismFace face;
    unsigned fixed_axis;
    bool fixed_maximum;
    unsigned u_axis;
    unsigned v_axis;
} FaceDescriptor;

static const FaceDescriptor kFaces[PROCEDURAL_SURFACE_PRISM_FACE_COUNT] = {
    {PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_X, 0u, false, 2u, 1u},
    {PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_X, 0u, true, 1u, 2u},
    {PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Y, 1u, false, 0u, 2u},
    {PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Y, 1u, true, 2u, 0u},
    {PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Z, 2u, false, 1u, 0u},
    {PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z, 2u, true, 0u, 1u}
};

static void report_set(ProceduralSurfacePrismMeshReport *report,
                       ProceduralSurfacePrismMeshStatus status,
                       const char *field,
                       const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    if (field) snprintf(report->field, sizeof(report->field), "%s", field);
    if (message) snprintf(report->message, sizeof(report->message), "%s", message);
}

static ProceduralSurfaceFieldPoint3D point_subtract(
    ProceduralSurfaceFieldPoint3D a,
    ProceduralSurfaceFieldPoint3D b) {
    return (ProceduralSurfaceFieldPoint3D){
        .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

static ProceduralSurfaceFieldPoint3D point_cross(
    ProceduralSurfaceFieldPoint3D a,
    ProceduralSurfaceFieldPoint3D b) {
    return (ProceduralSurfaceFieldPoint3D){
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x};
}

static double point_dot(ProceduralSurfaceFieldPoint3D a,
                        ProceduralSurfaceFieldPoint3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static double point_length(ProceduralSurfaceFieldPoint3D value) {
    return sqrt(point_dot(value, value));
}

static bool point_finite(ProceduralSurfaceFieldPoint3D value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static ProceduralSurfaceFieldPoint3D face_outward_normal(
    ProceduralSurfacePrismFace face) {
    switch (face) {
        case PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_X:
            return (ProceduralSurfaceFieldPoint3D){-1.0, 0.0, 0.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_X:
            return (ProceduralSurfaceFieldPoint3D){1.0, 0.0, 0.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Y:
            return (ProceduralSurfaceFieldPoint3D){0.0, -1.0, 0.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Y:
            return (ProceduralSurfaceFieldPoint3D){0.0, 1.0, 0.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Z:
            return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, -1.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z:
            return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0};
        case PROCEDURAL_SURFACE_PRISM_FACE_COUNT:
            break;
    }
    return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 0.0};
}

static bool field_valid(const ProceduralSurfaceFieldOutput *field) {
    return field &&
           isfinite(field->height) && field->height >= -1.0 && field->height <= 1.0 &&
           isfinite(field->macro_variation) &&
           field->macro_variation >= -1.0 && field->macro_variation <= 1.0 &&
           isfinite(field->micro_variation) &&
           field->micro_variation >= -1.0 && field->micro_variation <= 1.0 &&
           isfinite(field->rock_mask) &&
           field->rock_mask >= 0.0 && field->rock_mask <= 1.0 &&
           isfinite(field->roughness) &&
           field->roughness >= 0.0 && field->roughness <= 1.0 &&
           isfinite(field->snow_precursor) &&
           field->snow_precursor >= 0.0 && field->snow_precursor <= 1.0;
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double smoother_step(double value) {
    const double t = clamp01(value);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static uint64_t quality_budget(const ProceduralSurfaceRecipeV1 *recipe,
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

static bool cage_recipe_validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceTopologyExpectation *out_topology,
    ProceduralSurfacePrismMeshReport *report) {
    ProceduralSurfaceRecipeReport recipe_report;
    if (!cage || !recipe || !out_topology) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NULL_ARGUMENT,
                   "", "cage, recipe, and topology output are required");
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_Validate(recipe, &recipe_report)) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_RECIPE,
                   recipe_report.field, recipe_report.message);
        return false;
    }
    if (cage->kind != PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM ||
        !isfinite(cage->target_edge_length_units) ||
        fabs(cage->target_edge_length_units -
             recipe->target_edge_length_units) > 1.0e-12 ||
        !ProceduralSurfaceTopologyContract_Derive(cage, out_topology)) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_CAGE,
                   "cage", "a valid rectangular-prism cage is required");
        return false;
    }
    return true;
}

bool ProceduralSurfacePrismMesh_DeriveRequirements(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshReport *report) {
    ProceduralSurfaceTopologyExpectation topology;
    ProceduralSurfacePrismMeshRequirements requirements = {0};
    const uint64_t triangle_budget = recipe ? quality_budget(recipe, quality) : 0u;
    report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_OK, "", "ok");
    if (!out_requirements) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NULL_ARGUMENT,
                   "requirements", "requirements output is required");
        return false;
    }
    if (!cage_recipe_validate(cage, recipe, &topology, report)) return false;
    if (triangle_budget == 0u || topology.triangle_count > triangle_budget ||
        topology.vertex_count > recipe->quality.max_field_evaluations) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_QUALITY,
                   "quality_budgets", "prism requirements exceed selected quality");
        return false;
    }
    requirements.subdivisions_x = topology.subdivisions_x;
    requirements.subdivisions_y = topology.subdivisions_y;
    requirements.subdivisions_z = topology.subdivisions_z;
    requirements.vertex_count = topology.vertex_count;
    requirements.triangle_count = topology.triangle_count;
    requirements.field_evaluation_count = topology.vertex_count;
    requirements.selected_triangle_budget = triangle_budget;
    *out_requirements = requirements;
    return true;
}

static double lattice_coordinate(uint32_t value, uint32_t maximum,
                                 double extent) {
    return -extent * 0.5 + extent * (double)value / (double)maximum;
}

static uint32_t boundary_vertex_index(
    uint32_t x, uint32_t y, uint32_t z,
    uint32_t nx, uint32_t ny, uint32_t nz) {
    const uint64_t full_face_count =
        ((uint64_t)nx + 1u) * ((uint64_t)ny + 1u);
    const uint64_t middle_layer_count =
        2u * ((uint64_t)nx + (uint64_t)ny);
    uint64_t index;
    if (z == 0u) {
        index = (uint64_t)y * ((uint64_t)nx + 1u) + x;
    } else if (z == nz) {
        index = full_face_count +
                ((uint64_t)nz - 1u) * middle_layer_count +
                (uint64_t)y * ((uint64_t)nx + 1u) + x;
    } else {
        index = full_face_count +
                ((uint64_t)z - 1u) * middle_layer_count;
        if (y == 0u) {
            index += x;
        } else if (y == ny) {
            index += ((uint64_t)nx + 1u) +
                     2u * ((uint64_t)ny - 1u) + x;
        } else {
            index += ((uint64_t)nx + 1u) +
                     2u * ((uint64_t)y - 1u) +
                     (x == nx ? 1u : 0u);
        }
    }
    return (uint32_t)index;
}

static unsigned boundary_axis_count(uint32_t x, uint32_t y, uint32_t z,
                                    uint32_t nx, uint32_t ny, uint32_t nz) {
    return (unsigned)(x == 0u || x == nx) +
           (unsigned)(y == 0u || y == ny) +
           (unsigned)(z == 0u || z == nz);
}

static ProceduralSurfaceFieldPoint3D single_face_normal(
    uint32_t x, uint32_t y, uint32_t z,
    uint32_t nx, uint32_t ny, uint32_t nz) {
    (void)nz;
    if (x == 0u) return (ProceduralSurfaceFieldPoint3D){-1.0, 0.0, 0.0};
    if (x == nx) return (ProceduralSurfaceFieldPoint3D){1.0, 0.0, 0.0};
    if (y == 0u) return (ProceduralSurfaceFieldPoint3D){0.0, -1.0, 0.0};
    if (y == ny) return (ProceduralSurfaceFieldPoint3D){0.0, 1.0, 0.0};
    if (z == 0u) return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, -1.0};
    return (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0};
}

static double face_edge_lock_weight(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D cage_position,
    ProceduralSurfaceFieldPoint3D face_normal) {
    double distance_a;
    double distance_b;
    if (face_normal.x != 0.0) {
        distance_a = cage->height_units * 0.5 - fabs(cage_position.y);
        distance_b = cage->depth_units * 0.5 - fabs(cage_position.z);
    } else if (face_normal.y != 0.0) {
        distance_a = cage->width_units * 0.5 - fabs(cage_position.x);
        distance_b = cage->depth_units * 0.5 - fabs(cage_position.z);
    } else {
        distance_a = cage->width_units * 0.5 - fabs(cage_position.x);
        distance_b = cage->height_units * 0.5 - fabs(cage_position.y);
    }
    return smoother_step(fmin(distance_a, distance_b) /
                         recipe->edge_lock_width_units);
}

static void face_lattice(const FaceDescriptor *face,
                         uint32_t u, uint32_t v,
                         const uint32_t subdivisions[3],
                         uint32_t coordinates[3]) {
    coordinates[0] = coordinates[1] = coordinates[2] = 0u;
    coordinates[face->fixed_axis] =
        face->fixed_maximum ? subdivisions[face->fixed_axis] : 0u;
    coordinates[face->u_axis] = u;
    coordinates[face->v_axis] = v;
}

static uint32_t vertex_lattice_axis(
    const ProceduralSurfacePrismVertex *vertex,
    unsigned axis) {
    if (axis == 0u) return vertex->lattice_x;
    if (axis == 1u) return vertex->lattice_y;
    return vertex->lattice_z;
}

static bool append_face_triangles(
    const FaceDescriptor *face,
    const uint32_t subdivisions[3],
    ProceduralSurfacePrismTriangle *triangles,
    size_t *triangle_count) {
    const uint32_t nu = subdivisions[face->u_axis];
    const uint32_t nv = subdivisions[face->v_axis];
    for (uint32_t v = 0u; v < nv; ++v) {
        for (uint32_t u = 0u; u < nu; ++u) {
            uint32_t c00[3], c10[3], c01[3], c11[3];
            uint32_t i00, i10, i01, i11;
            face_lattice(face, u, v, subdivisions, c00);
            face_lattice(face, u + 1u, v, subdivisions, c10);
            face_lattice(face, u, v + 1u, subdivisions, c01);
            face_lattice(face, u + 1u, v + 1u, subdivisions, c11);
            i00 = boundary_vertex_index(
                c00[0], c00[1], c00[2],
                subdivisions[0], subdivisions[1], subdivisions[2]);
            i10 = boundary_vertex_index(
                c10[0], c10[1], c10[2],
                subdivisions[0], subdivisions[1], subdivisions[2]);
            i01 = boundary_vertex_index(
                c01[0], c01[1], c01[2],
                subdivisions[0], subdivisions[1], subdivisions[2]);
            i11 = boundary_vertex_index(
                c11[0], c11[1], c11[2],
                subdivisions[0], subdivisions[1], subdivisions[2]);
            triangles[(*triangle_count)++] = (ProceduralSurfacePrismTriangle){
                i00, i10, i11, face->face};
            triangles[(*triangle_count)++] = (ProceduralSurfacePrismTriangle){
                i00, i11, i01, face->face};
        }
    }
    return true;
}

static int edge_compare(const void *left, const void *right) {
    const PrismEdge *a = left;
    const PrismEdge *b = right;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    if (a->b != b->b) return a->b < b->b ? -1 : 1;
    return 0;
}

static bool append_text(char *output, size_t capacity, size_t *length,
                        const char *format, ...) {
    va_list arguments;
    int written;
    if (!output || !length || *length >= capacity) return false;
    va_start(arguments, format);
    written = vsnprintf(output + *length, capacity - *length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

static bool mesh_digest(const char *recipe_digest,
                        const ProceduralSurfacePrismMesh *mesh,
                        const ProceduralSurfacePrismMeshSummary *summary,
                        char out_digest[PROCEDURAL_SURFACE_PRISM_MESH_DIGEST_CAPACITY]) {
    size_t capacity = PRISM_CANONICAL_BASE_CAPACITY;
    if (mesh->vertex_count >
            (SIZE_MAX - capacity) / PRISM_CANONICAL_VERTEX_CAPACITY) {
        return false;
    }
    capacity += mesh->vertex_count * PRISM_CANONICAL_VERTEX_CAPACITY;
    if (mesh->triangle_count >
            (SIZE_MAX - capacity) / PRISM_CANONICAL_TRIANGLE_CAPACITY) {
        return false;
    }
    capacity += mesh->triangle_count * PRISM_CANONICAL_TRIANGLE_CAPACITY;
    char *canonical = calloc(capacity, 1u);
    size_t length = 0u;
    bool ok = canonical &&
        append_text(canonical, capacity, &length,
                    "{\"schema\":\"ray_tracing.procedural_surface_prism_mesh\","
                    "\"schema_version\":1,\"recipe_digest_sha256\":\"%s\","
                    "\"subdivisions\":[%llu,%llu,%llu],\"vertices\":[",
                    recipe_digest,
                    (unsigned long long)summary->subdivisions_x,
                    (unsigned long long)summary->subdivisions_y,
                    (unsigned long long)summary->subdivisions_z);
    for (size_t i = 0u; ok && i < mesh->vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *v = &mesh->vertices[i];
        ok = append_text(canonical, capacity, &length,
                         "%s[%u,%u,%u,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
                         "%.17g,%.17g,%.17g,%.17g]",
                         i ? "," : "", v->lattice_x, v->lattice_y, v->lattice_z,
                         v->position.x, v->position.y, v->position.z,
                         v->normal.x, v->normal.y, v->normal.z,
                         v->field.height, v->field.rock_mask,
                         v->field.roughness, v->field.snow_precursor);
    }
    if (ok) ok = append_text(canonical, capacity, &length,
                             "],\"triangles\":[");
    for (size_t i = 0u; ok && i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *t = &mesh->triangles[i];
        ok = append_text(canonical, capacity, &length,
                         "%s[%u,%u,%u,%u]", i ? "," : "",
                         t->a, t->b, t->c, (unsigned)t->surface_group);
    }
    if (ok) ok = append_text(canonical, capacity, &length, "]}");
    if (ok) ok = ray_tracing_sha256_bytes(canonical, length, out_digest);
    free(canonical);
    return ok;
}

bool ProceduralSurfacePrismMesh_Validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePrismMesh *mesh,
    uint64_t field_evaluation_count,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report) {
    ProceduralSurfaceTopologyExpectation expected;
    ProceduralSurfacePrismMeshSummary summary = {0};
    ProceduralSurfaceRecipeReport recipe_report;
    PrismEdge *edges = NULL;
    uint32_t *parents = NULL;
    ProceduralSurfaceFieldPoint3D *normal_sums = NULL;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    uint64_t group_counts[PROCEDURAL_SURFACE_PRISM_FACE_COUNT] = {0};
    const double tolerance = 1.0e-10;
    bool result = false;

    report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_OK, "", "ok");
    if (!mesh || !out_summary || !mesh->vertices || !mesh->triangles) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NULL_ARGUMENT,
                   "mesh", "mesh arrays and summary output are required");
        return false;
    }
    if (!cage_recipe_validate(cage, recipe, &expected, report)) return false;
    if (mesh->vertex_count != expected.vertex_count ||
        mesh->triangle_count != expected.triangle_count ||
        field_evaluation_count != expected.vertex_count) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "counts", "prism mesh counts do not match frozen topology");
        return false;
    }
    summary.subdivisions_x = expected.subdivisions_x;
    summary.subdivisions_y = expected.subdivisions_y;
    summary.subdivisions_z = expected.subdivisions_z;
    summary.vertex_count = mesh->vertex_count;
    summary.triangle_count = mesh->triangle_count;
    summary.field_evaluation_count = field_evaluation_count;
    summary.surface_group_count = PROCEDURAL_SURFACE_PRISM_FACE_COUNT;
    summary.minimum_twice_triangle_area_units2 = INFINITY;
    summary.minimum_outward_winding_dot = INFINITY;
    summary.bounds_min = summary.bounds_max = mesh->vertices[0].position;

    edges = calloc(mesh->triangle_count * 3u, sizeof(*edges));
    parents = calloc(mesh->vertex_count, sizeof(*parents));
    normal_sums = calloc(mesh->vertex_count, sizeof(*normal_sums));
    if (!edges || !parents || !normal_sums) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_ALLOCATION,
                   "validation", "unable to allocate bounded validation storage");
        goto cleanup;
    }
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *v = &mesh->vertices[i];
        const unsigned boundary_count = boundary_axis_count(
            v->lattice_x, v->lattice_y, v->lattice_z,
            expected.subdivisions_x, expected.subdivisions_y,
            expected.subdivisions_z);
        const double normal_length = point_length(v->normal);
        const ProceduralSurfaceFieldPoint3D expected_cage_position = {
            lattice_coordinate(v->lattice_x, expected.subdivisions_x,
                               cage->width_units),
            lattice_coordinate(v->lattice_y, expected.subdivisions_y,
                               cage->height_units),
            lattice_coordinate(v->lattice_z, expected.subdivisions_z,
                               cage->depth_units)};
        parents[i] = (uint32_t)i;
        if (v->lattice_x > expected.subdivisions_x ||
            v->lattice_y > expected.subdivisions_y ||
            v->lattice_z > expected.subdivisions_z ||
            boundary_count == 0u || !point_finite(v->cage_position) ||
            !point_finite(v->position) || !point_finite(v->normal) ||
            !point_finite(v->displacement_direction) ||
            !field_valid(&v->field) || !isfinite(v->edge_lock_weight) ||
            !isfinite(v->displacement_units) ||
            v->edge_lock_weight < 0.0 || v->edge_lock_weight > 1.0 ||
            fabs(normal_length - 1.0) > 1.0e-8) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                       "vertices", "prism vertex contract is invalid");
            goto cleanup;
        }
        if (fabs(v->cage_position.x - expected_cage_position.x) > tolerance ||
            fabs(v->cage_position.y - expected_cage_position.y) > tolerance ||
            fabs(v->cage_position.z - expected_cage_position.z) > tolerance) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                       "cage_position", "vertex lattice and cage position disagree");
            goto cleanup;
        }
        if (boundary_count >= 2u &&
            (v->edge_lock_weight != 0.0 || v->displacement_units != 0.0 ||
             point_length(v->displacement_direction) != 0.0 ||
             fabs(v->position.x - v->cage_position.x) > tolerance ||
             fabs(v->position.y - v->cage_position.y) > tolerance ||
             fabs(v->position.z - v->cage_position.z) > tolerance)) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                       "edge_lock", "edge and corner vertices must remain on cage");
            goto cleanup;
        }
        if (boundary_count == 1u) {
            const ProceduralSurfaceFieldPoint3D source_normal =
                single_face_normal(
                v->lattice_x, v->lattice_y, v->lattice_z,
                expected.subdivisions_x, expected.subdivisions_y,
                expected.subdivisions_z);
            const double expected_weight = face_edge_lock_weight(
                cage, recipe, v->cage_position, source_normal);
            const double expected_displacement =
                v->field.height * recipe->displacement_amplitude_units *
                expected_weight;
            if (fabs(point_length(v->displacement_direction) - 1.0) >
                    tolerance ||
                fabs(v->edge_lock_weight - expected_weight) > tolerance ||
                fabs(v->displacement_units - expected_displacement) > tolerance ||
                fabs(v->position.x -
                     (v->cage_position.x +
                      v->displacement_direction.x * expected_displacement)) >
                    tolerance ||
                fabs(v->position.y -
                     (v->cage_position.y +
                      v->displacement_direction.y * expected_displacement)) >
                    tolerance ||
                fabs(v->position.z -
                     (v->cage_position.z +
                      v->displacement_direction.z * expected_displacement)) >
                    tolerance) {
                report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                           "displacement", "face displacement contract is invalid");
                goto cleanup;
            }
        }
        summary.maximum_absolute_displacement_units = fmax(
            summary.maximum_absolute_displacement_units,
            fabs(v->displacement_units));
        if (boundary_count >= 2u) {
            summary.maximum_edge_absolute_displacement_units = fmax(
                summary.maximum_edge_absolute_displacement_units,
                fabs(v->displacement_units));
        }
        summary.bounds_min.x = fmin(summary.bounds_min.x, v->position.x);
        summary.bounds_min.y = fmin(summary.bounds_min.y, v->position.y);
        summary.bounds_min.z = fmin(summary.bounds_min.z, v->position.z);
        summary.bounds_max.x = fmax(summary.bounds_max.x, v->position.x);
        summary.bounds_max.y = fmax(summary.bounds_max.y, v->position.y);
        summary.bounds_max.z = fmax(summary.bounds_max.z, v->position.z);
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *t = &mesh->triangles[i];
        ProceduralSurfaceFieldPoint3D a, b, c, cross;
        uint32_t indices[3];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count ||
            t->c >= mesh->vertex_count || t->a == t->b || t->a == t->c ||
            t->b == t->c || t->surface_group >= PROCEDURAL_SURFACE_PRISM_FACE_COUNT) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TRIANGLE,
                       "triangles", "prism triangle indices or group are invalid");
            goto cleanup;
        }
        const FaceDescriptor *face = &kFaces[t->surface_group];
        const uint32_t expected_face_coordinate =
            face->fixed_maximum
                ? (face->fixed_axis == 0u ? expected.subdivisions_x
                   : face->fixed_axis == 1u ? expected.subdivisions_y
                                             : expected.subdivisions_z)
                : 0u;
        if (vertex_lattice_axis(&mesh->vertices[t->a], face->fixed_axis) !=
                expected_face_coordinate ||
            vertex_lattice_axis(&mesh->vertices[t->b], face->fixed_axis) !=
                expected_face_coordinate ||
            vertex_lattice_axis(&mesh->vertices[t->c], face->fixed_axis) !=
                expected_face_coordinate) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                       "surface_groups", "triangle does not belong to named cage face");
            goto cleanup;
        }
        ++group_counts[t->surface_group];
        a = mesh->vertices[t->a].position;
        b = mesh->vertices[t->b].position;
        c = mesh->vertices[t->c].position;
        cross = point_cross(point_subtract(b, a), point_subtract(c, a));
        const double twice_area = point_length(cross);
        const double outward_dot =
            point_dot(cross, face_outward_normal(t->surface_group));
        if (!isfinite(twice_area) || twice_area <= 1.0e-12 ||
            !isfinite(outward_dot) || outward_dot <= 0.0) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TRIANGLE,
                       "winding", "triangles must be nondegenerate and outward");
            goto cleanup;
        }
        summary.minimum_twice_triangle_area_units2 =
            fmin(summary.minimum_twice_triangle_area_units2, twice_area);
        summary.minimum_outward_winding_dot =
            fmin(summary.minimum_outward_winding_dot, outward_dot);
        summary.total_surface_area_units2 += 0.5 * twice_area;
        summary.signed_volume_units3 += point_dot(a, point_cross(b, c)) / 6.0;
        indices[0] = t->a; indices[1] = t->b; indices[2] = t->c;
        for (size_t j = 0u; j < 3u; ++j) {
            normal_sums[indices[j]].x += cross.x;
            normal_sums[indices[j]].y += cross.y;
            normal_sums[indices[j]].z += cross.z;
            uint32_t p = parents[indices[j]];
            while (parents[p] != p) p = parents[p];
            uint32_t q = parents[indices[(j + 1u) % 3u]];
            while (parents[q] != q) q = parents[q];
            if (p != q) parents[q] = p;
        }
        for (size_t j = 0u; j < 3u; ++j) {
            uint32_t x = indices[j], y = indices[(j + 1u) % 3u];
            edges[i * 3u + j] =
                x < y ? (PrismEdge){x, y} : (PrismEdge){y, x};
        }
    }
    if (!(summary.signed_volume_units3 > 0.0)) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "signed_volume", "closed prism must have positive signed volume");
        goto cleanup;
    }
    for (size_t group = 0u;
         group < PROCEDURAL_SURFACE_PRISM_FACE_COUNT;
         ++group) {
        const FaceDescriptor *face = &kFaces[group];
        const uint64_t expected_count =
            2u * (uint64_t)expected.subdivisions_x *
            (uint64_t)expected.subdivisions_y *
            (face->fixed_axis == 2u ? 1u : 0u) +
            2u * (uint64_t)expected.subdivisions_x *
            (uint64_t)expected.subdivisions_z *
            (face->fixed_axis == 1u ? 1u : 0u) +
            2u * (uint64_t)expected.subdivisions_y *
            (uint64_t)expected.subdivisions_z *
            (face->fixed_axis == 0u ? 1u : 0u);
        if (group_counts[group] != expected_count) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                       "surface_groups", "surface-group triangle count is invalid");
            goto cleanup;
        }
    }
    qsort(edges, mesh->triangle_count * 3u, sizeof(*edges), edge_compare);
    for (size_t i = 0u; i < mesh->triangle_count * 3u;) {
        size_t next = i + 1u;
        while (next < mesh->triangle_count * 3u &&
               edge_compare(&edges[i], &edges[next]) == 0) ++next;
        ++summary.unique_edge_count;
        if (next - i != 2u) ++summary.boundary_edge_count;
        i = next;
    }
    if (summary.unique_edge_count != expected.unique_edge_count ||
        summary.boundary_edge_count != 0u) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "edge_incidence", "every closed-shell edge must have two triangles");
        goto cleanup;
    }
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        uint32_t root = (uint32_t)i;
        while (parents[root] != root) root = parents[root];
        if (root == i) ++summary.connected_component_count;
        const double length = point_length(normal_sums[i]);
        if (!(length > 0.0) ||
            fabs(mesh->vertices[i].normal.x - normal_sums[i].x / length) > 1.0e-8 ||
            fabs(mesh->vertices[i].normal.y - normal_sums[i].y / length) > 1.0e-8 ||
            fabs(mesh->vertices[i].normal.z - normal_sums[i].z / length) > 1.0e-8) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NORMAL,
                       "normals", "stored normals must match generated geometry");
            goto cleanup;
        }
    }
    summary.euler_characteristic = (int32_t)(
        (int64_t)summary.vertex_count - (int64_t)summary.unique_edge_count +
        (int64_t)summary.triangle_count);
    if (summary.connected_component_count != 1u ||
        summary.euler_characteristic != 2) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "connectedness", "closed prism must be one Euler-2 component");
        goto cleanup;
    }
    if (!ProceduralSurfaceRecipeV1_Digest(recipe, recipe_digest, &recipe_report) ||
        !mesh_digest(recipe_digest, mesh, &summary, summary.mesh_digest_sha256)) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_SUMMARY,
                   "mesh_digest_sha256", "unable to create prism mesh digest");
        goto cleanup;
    }
    *out_summary = summary;
    result = true;
cleanup:
    free(edges);
    free(parents);
    free(normal_sums);
    return result;
}

static bool evaluate_recipe_field(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    return ProceduralSurfaceField3D_Evaluate(
        context, point, budget, out_field, report);
}

bool ProceduralSurfacePrismMesh_Generate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report) {
    return ProceduralSurfacePrismMesh_GenerateWithEvaluator(
        cage, recipe, quality, evaluate_recipe_field, recipe, field_budget,
        buffers, out_summary, report);
}

bool ProceduralSurfacePrismMesh_GenerateWithEvaluator(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report) {
    return ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
        cage, recipe, quality, evaluator, evaluator_context, NULL, NULL,
        field_budget, buffers, out_summary, report);
}

bool ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfacePrismDisplacementDirectionResolver direction_resolver,
    const void *direction_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report) {
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismVertex *vertices = NULL;
    ProceduralSurfacePrismTriangle *triangles = NULL;
    ProceduralSurfaceFieldPoint3D *normal_sums = NULL;
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfacePrismMesh mesh;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceFieldReport field_report;
    size_t vertex_count = 0u, triangle_count = 0u;
    bool result = false;

    report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_OK, "", "ok");
    if (!evaluator || !field_budget || !buffers || !out_summary ||
        !buffers->vertices || !buffers->triangles) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NULL_ARGUMENT,
                   "buffers", "field budget, buffers, and summary are required");
        return false;
    }
    if (!ProceduralSurfacePrismMesh_DeriveRequirements(
            cage, recipe, quality, &requirements, report)) return false;
    if (requirements.vertex_count > buffers->vertex_capacity ||
        requirements.triangle_count > buffers->triangle_capacity ||
        field_budget->max_evaluations != recipe->quality.max_field_evaluations ||
        field_budget->evaluations > field_budget->max_evaluations ||
        requirements.field_evaluation_count >
        field_budget->max_evaluations - field_budget->evaluations) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_CAPACITY,
                   "buffers", "mesh or evaluation capacity is insufficient");
        return false;
    }
    const uint32_t subdivisions[3] = {
        (uint32_t)requirements.subdivisions_x,
        (uint32_t)requirements.subdivisions_y,
        (uint32_t)requirements.subdivisions_z};
    vertices = calloc((size_t)requirements.vertex_count, sizeof(*vertices));
    triangles = calloc((size_t)requirements.triangle_count, sizeof(*triangles));
    normal_sums = calloc((size_t)requirements.vertex_count, sizeof(*normal_sums));
    if (!vertices || !triangles || !normal_sums) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_ALLOCATION,
                   "temporary_mesh", "unable to allocate bounded prism storage");
        goto cleanup;
    }
    budget = *field_budget;
    for (uint32_t z = 0u; z <= subdivisions[2]; ++z) {
        for (uint32_t y = 0u; y <= subdivisions[1]; ++y) {
            for (uint32_t x = 0u; x <= subdivisions[0]; ++x) {
                const unsigned boundary_count = boundary_axis_count(
                    x, y, z, subdivisions[0], subdivisions[1], subdivisions[2]);
                if (boundary_count == 0u) continue;
                ProceduralSurfacePrismVertex *v = &vertices[vertex_count];
                v->lattice_x = x; v->lattice_y = y; v->lattice_z = z;
                v->cage_position = (ProceduralSurfaceFieldPoint3D){
                    lattice_coordinate(x, subdivisions[0], cage->width_units),
                    lattice_coordinate(y, subdivisions[1], cage->height_units),
                    lattice_coordinate(z, subdivisions[2], cage->depth_units)};
                v->position = v->cage_position;
                if (!evaluator(
                        evaluator_context, v->cage_position, &budget,
                        &v->field, &field_report)) {
                    report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_FIELD,
                               field_report.field, field_report.message);
                    goto cleanup;
                }
                if (boundary_count == 1u) {
                    const ProceduralSurfaceFieldPoint3D source_normal =
                        single_face_normal(x, y, z, subdivisions[0],
                                           subdivisions[1], subdivisions[2]);
                    ProceduralSurfaceFieldPoint3D direction = source_normal;
                    if (direction_resolver &&
                        !direction_resolver(
                            direction_context, v->cage_position,
                            source_normal, &direction)) {
                        report_set(
                            report,
                            PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                            "displacement_direction",
                            "unable to resolve displacement direction");
                        goto cleanup;
                    }
                    {
                        const double length = point_length(direction);
                        if (!point_finite(direction) || !(length > 0.0)) {
                            report_set(
                                report,
                                PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
                                "displacement_direction",
                                "displacement direction must be finite and non-zero");
                            goto cleanup;
                        }
                        direction.x /= length;
                        direction.y /= length;
                        direction.z /= length;
                    }
                    v->displacement_direction = direction;
                    v->edge_lock_weight = face_edge_lock_weight(
                        cage, recipe, v->cage_position, source_normal);
                    v->displacement_units =
                        v->field.height * recipe->displacement_amplitude_units *
                        v->edge_lock_weight;
                    v->position.x += direction.x * v->displacement_units;
                    v->position.y += direction.y * v->displacement_units;
                    v->position.z += direction.z * v->displacement_units;
                }
                if (boundary_vertex_index(
                        x, y, z, subdivisions[0], subdivisions[1],
                        subdivisions[2]) != vertex_count) {
                    report_set(report,
                               PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                               "vertices",
                               "boundary lattice index is not canonical");
                    goto cleanup;
                }
                ++vertex_count;
            }
        }
    }
    if (vertex_count != requirements.vertex_count) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "vertices", "boundary lattice count disagrees with requirement");
        goto cleanup;
    }
    for (size_t face = 0u; face < PROCEDURAL_SURFACE_PRISM_FACE_COUNT; ++face) {
        if (!append_face_triangles(&kFaces[face], subdivisions,
                                   triangles, &triangle_count)) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                       "triangles", "unable to resolve welded face lattice");
            goto cleanup;
        }
    }
    if (triangle_count != requirements.triangle_count) {
        report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
                   "triangles", "face triangle count disagrees with requirement");
        goto cleanup;
    }
    for (size_t i = 0u; i < triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *t = &triangles[i];
        const ProceduralSurfaceFieldPoint3D cross = point_cross(
            point_subtract(vertices[t->b].position, vertices[t->a].position),
            point_subtract(vertices[t->c].position, vertices[t->a].position));
        const uint32_t ids[3] = {t->a, t->b, t->c};
        for (size_t j = 0u; j < 3u; ++j) {
            normal_sums[ids[j]].x += cross.x;
            normal_sums[ids[j]].y += cross.y;
            normal_sums[ids[j]].z += cross.z;
        }
    }
    for (size_t i = 0u; i < vertex_count; ++i) {
        const double length = point_length(normal_sums[i]);
        if (!(length > 0.0) || !isfinite(length)) {
            report_set(report, PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NORMAL,
                       "normals", "unable to normalize prism normal");
            goto cleanup;
        }
        vertices[i].normal = (ProceduralSurfaceFieldPoint3D){
            normal_sums[i].x / length,
            normal_sums[i].y / length,
            normal_sums[i].z / length};
    }
    mesh = (ProceduralSurfacePrismMesh){
        vertices, vertex_count, triangles, triangle_count};
    if (!ProceduralSurfacePrismMesh_Validate(
            cage, recipe, &mesh, requirements.field_evaluation_count,
            &summary, report)) goto cleanup;
    memcpy(buffers->vertices, vertices, vertex_count * sizeof(*vertices));
    memcpy(buffers->triangles, triangles, triangle_count * sizeof(*triangles));
    buffers->vertex_count = vertex_count;
    buffers->triangle_count = triangle_count;
    *field_budget = budget;
    *out_summary = summary;
    result = true;
cleanup:
    free(vertices);
    free(triangles);
    free(normal_sums);
    return result;
}

const char *ProceduralSurfacePrismFace_Name(ProceduralSurfacePrismFace face) {
    static const char *const names[] = {
        "negative_x", "positive_x", "negative_y",
        "positive_y", "negative_z", "positive_z"};
    return face < PROCEDURAL_SURFACE_PRISM_FACE_COUNT ? names[face] : "unknown";
}

const char *ProceduralSurfacePrismMeshStatus_Name(
    ProceduralSurfacePrismMeshStatus status) {
    static const char *const names[] = {
        "ok", "null_argument", "cage", "recipe", "quality", "capacity",
        "allocation", "field", "vertex", "triangle", "topology", "normal",
        "summary"};
    return (unsigned)status < sizeof(names) / sizeof(names[0])
        ? names[status] : "unknown";
}
