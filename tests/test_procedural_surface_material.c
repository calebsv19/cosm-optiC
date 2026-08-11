#include "procedural/procedural_surface_material.h"
#include "procedural/procedural_surface_material_runtime_adapter.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <json-c/json.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIXTURE_ROOT
#define PROCEDURAL_SURFACE_FIXTURE_ROOT \
    "tests/fixtures/procedural_surface_rock_prism_psg0"
#endif

typedef struct MaterialFixture {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfacePrismVertex *vertices;
    ProceduralSurfacePrismTriangle *triangles;
    ProceduralSurfaceMaterialSample *samples;
    const char **sample_ids;
    ProceduralSurfacePrismMesh mesh;
    ProceduralSurfacePrismMeshSummary mesh_summary;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY];
    double minimum_snow;
    double maximum_snow;
    double mean_snow;
    double minimum_roughness;
    double maximum_roughness;
    double mean_roughness;
    double top_mean_snow;
    double side_maximum_snow;
    uint64_t snow_vertex_count;
} MaterialFixture;

static void material_fixture_free(MaterialFixture *fixture) {
    if (!fixture) return;
    if (fixture->sample_ids) {
        for (size_t i = 0u; i < fixture->mesh.vertex_count; ++i) {
            free((void *)fixture->sample_ids[i]);
        }
    }
    free(fixture->sample_ids);
    free(fixture->samples);
    free(fixture->triangles);
    free(fixture->vertices);
    memset(fixture, 0, sizeof(*fixture));
}

static MaterialFixture material_fixture_generate(void) {
    MaterialFixture fixture;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceMaterialReport material_report;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfaceCageContract cage;
    double top_snow_sum = 0.0;
    size_t top_count = 0u;

    memset(&fixture, 0, sizeof(fixture));
    fixture.minimum_snow = 1.0;
    fixture.minimum_roughness = 1.0;
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &fixture.recipe, &recipe_report));
    assert(ProceduralSurfaceRecipeV1_Digest(
        &fixture.recipe, fixture.recipe_digest, &recipe_report));
    cage = (ProceduralSurfaceCageContract){
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 4.0,
        .height_units = 3.0,
        .depth_units = 2.0,
        .target_edge_length_units = fixture.recipe.target_edge_length_units};
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        &cage, &fixture.recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &requirements, &mesh_report));
    fixture.vertices = calloc((size_t)requirements.vertex_count,
                              sizeof(*fixture.vertices));
    fixture.triangles = calloc((size_t)requirements.triangle_count,
                               sizeof(*fixture.triangles));
    fixture.samples = calloc((size_t)requirements.vertex_count,
                             sizeof(*fixture.samples));
    fixture.sample_ids = calloc((size_t)requirements.vertex_count,
                                sizeof(*fixture.sample_ids));
    assert(fixture.vertices && fixture.triangles &&
           fixture.samples && fixture.sample_ids);
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = fixture.vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = fixture.triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = fixture.recipe.quality.max_field_evaluations};
    assert(ProceduralSurfacePrismMesh_Generate(
        &cage, &fixture.recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &budget, &buffers, &fixture.mesh_summary, &mesh_report));
    assert(budget.evaluations == requirements.field_evaluation_count);
    fixture.mesh = (ProceduralSurfacePrismMesh){
        fixture.vertices, buffers.vertex_count,
        fixture.triangles, buffers.triangle_count};

    for (size_t i = 0u; i < fixture.mesh.vertex_count; ++i) {
        char *sample_id = malloc(32u);
        const ProceduralSurfacePrismVertex *vertex = &fixture.vertices[i];
        ProceduralSurfaceMaterialSample *sample = &fixture.samples[i];
        assert(sample_id);
        snprintf(sample_id, 32u, "vertex_%04zu", i);
        fixture.sample_ids[i] = sample_id;
        assert(ProceduralSurfaceMaterial_Evaluate(
            &fixture.recipe, &vertex->field, vertex->position, vertex->normal,
            sample, &material_report));
        if (sample->snow_likelihood < fixture.minimum_snow) {
            fixture.minimum_snow = sample->snow_likelihood;
        }
        if (sample->snow_likelihood > fixture.maximum_snow) {
            fixture.maximum_snow = sample->snow_likelihood;
        }
        if (sample->final_roughness < fixture.minimum_roughness) {
            fixture.minimum_roughness = sample->final_roughness;
        }
        if (sample->final_roughness > fixture.maximum_roughness) {
            fixture.maximum_roughness = sample->final_roughness;
        }
        fixture.mean_snow += sample->snow_likelihood;
        fixture.mean_roughness += sample->final_roughness;
        if (sample->snow_likelihood >= 0.05) ++fixture.snow_vertex_count;
        if (vertex->normal.z >= 0.65) {
            top_snow_sum += sample->snow_likelihood;
            ++top_count;
        } else if (sample->snow_likelihood > fixture.side_maximum_snow) {
            fixture.side_maximum_snow = sample->snow_likelihood;
        }
    }
    fixture.mean_snow /= (double)fixture.mesh.vertex_count;
    fixture.mean_roughness /= (double)fixture.mesh.vertex_count;
    fixture.top_mean_snow = top_count ? top_snow_sum / (double)top_count : 0.0;
    assert(ProceduralSurfaceMaterial_SummaryDigest(
        fixture.recipe_digest, fixture.mesh_summary.mesh_digest_sha256,
        fixture.sample_ids, fixture.samples, fixture.mesh.vertex_count,
        fixture.material_digest, &material_report));
    return fixture;
}

static double json_double(struct json_object *root, const char *key) {
    struct json_object *value = NULL;
    assert(json_object_object_get_ex(root, key, &value));
    return json_object_get_double(value);
}

static uint64_t json_u64(struct json_object *root, const char *key) {
    struct json_object *value = NULL;
    assert(json_object_object_get_ex(root, key, &value));
    return (uint64_t)json_object_get_int64(value);
}

static const char *json_string(struct json_object *root, const char *key) {
    struct json_object *value = NULL;
    assert(json_object_object_get_ex(root, key, &value));
    return json_object_get_string(value);
}

static void test_frozen_material_contract(void) {
    MaterialFixture first = material_fixture_generate();
    MaterialFixture second = material_fixture_generate();
    struct json_object *expected = json_object_from_file(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/expected_material_summary.json");
    assert(expected);
    assert(strcmp(first.recipe_digest, second.recipe_digest) == 0);
    assert(strcmp(first.mesh_summary.mesh_digest_sha256,
                  second.mesh_summary.mesh_digest_sha256) == 0);
    assert(strcmp(first.material_digest, second.material_digest) == 0);
    assert(memcmp(first.samples, second.samples,
                  first.mesh.vertex_count * sizeof(*first.samples)) == 0);
    printf("PSG-4 material summary: recipe=%s shell=%s material=%s "
           "snow=[%.17g,%.17g] mean=%.17g count=%llu "
           "roughness=[%.17g,%.17g] mean=%.17g top_mean=%.17g side_max=%.17g\n",
           first.recipe_digest, first.mesh_summary.mesh_digest_sha256,
           first.material_digest, first.minimum_snow, first.maximum_snow,
           first.mean_snow, (unsigned long long)first.snow_vertex_count,
           first.minimum_roughness, first.maximum_roughness,
           first.mean_roughness, first.top_mean_snow,
           first.side_maximum_snow);
    fflush(stdout);
    assert(strcmp(first.recipe_digest,
                  json_string(expected, "recipe_digest_sha256")) == 0);
    assert(strcmp(first.mesh_summary.mesh_digest_sha256,
                  json_string(expected, "shell_digest_sha256")) == 0);
    assert(strcmp(first.material_digest,
                  json_string(expected, "material_digest_sha256")) == 0);
    assert(first.mesh.vertex_count == json_u64(expected, "sample_count"));
    assert(first.snow_vertex_count == json_u64(expected, "snow_vertex_count"));
#define EXPECT_NEAR(field) \
    assert(fabs(first.field - json_double(expected, #field)) <= 1.0e-12)
    EXPECT_NEAR(minimum_snow);
    EXPECT_NEAR(maximum_snow);
    EXPECT_NEAR(mean_snow);
    EXPECT_NEAR(minimum_roughness);
    EXPECT_NEAR(maximum_roughness);
    EXPECT_NEAR(mean_roughness);
    EXPECT_NEAR(top_mean_snow);
    EXPECT_NEAR(side_maximum_snow);
#undef EXPECT_NEAR
    json_object_put(expected);
    material_fixture_free(&second);
    material_fixture_free(&first);
}

static void test_preview_final_adapter_parity(void) {
    MaterialFixture fixture = material_fixture_generate();
    RuntimeMaterialSurfaceEval base = {
        .active = true,
        .colorR = 0.42,
        .colorG = 0.38,
        .colorB = 0.32,
        .roughness = 0.78,
        .reflectivity = 0.03,
        .specWeight = 0.10,
        .diffuseWeight = 0.90,
        .transparency = 0.0};
    RuntimeMaterialSurfaceEval preview;
    RuntimeMaterialPayload3D final_payload;
    size_t selected = 0u;
    while (selected < fixture.mesh.vertex_count &&
           fixture.samples[selected].snow_likelihood < 0.05) {
        ++selected;
    }
    assert(selected < fixture.mesh.vertex_count);
    assert(ProceduralSurfaceMaterial_ToSurfaceEval(
        &fixture.samples[selected], &base, &preview));
    memset(&final_payload, 0, sizeof(final_payload));
    final_payload.valid = true;
    final_payload.baseColorR = base.colorR;
    final_payload.baseColorG = base.colorG;
    final_payload.baseColorB = base.colorB;
    final_payload.bsdf.baseColorR = base.colorR;
    final_payload.bsdf.baseColorG = base.colorG;
    final_payload.bsdf.baseColorB = base.colorB;
    final_payload.bsdf.roughness = base.roughness;
    final_payload.bsdf.reflectivity = base.reflectivity;
    final_payload.bsdf.specWeight = base.specWeight;
    final_payload.bsdf.diffuseWeight = base.diffuseWeight;
    final_payload.transparency = base.transparency;
    assert(ProceduralSurfaceMaterial_ApplyToPayload(
        &fixture.samples[selected], &base, &final_payload));
    assert(fabs(preview.colorR - final_payload.baseColorR) <= 1e-12);
    assert(fabs(preview.colorG - final_payload.baseColorG) <= 1e-12);
    assert(fabs(preview.colorB - final_payload.baseColorB) <= 1e-12);
    assert(fabs(preview.roughness - final_payload.bsdf.roughness) <= 1e-12);
    assert(fabs(preview.reflectivity -
                final_payload.bsdf.reflectivity) <= 1e-12);
    assert(fabs(preview.specWeight - final_payload.bsdf.specWeight) <= 1e-12);
    assert(fabs(preview.diffuseWeight -
                final_payload.bsdf.diffuseWeight) <= 1e-12);
    assert(final_payload.textureMask == 1.0);
    material_fixture_free(&fixture);
}

static void test_rejections_are_transactional(void) {
    MaterialFixture fixture = material_fixture_generate();
    ProceduralSurfaceMaterialSample output;
    ProceduralSurfaceMaterialSample sentinel;
    ProceduralSurfaceMaterialReport report;
    ProceduralSurfaceFieldPoint3D bad_normal = {0.0, 0.0, 2.0};
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    assert(!ProceduralSurfaceMaterial_Evaluate(
        &fixture.recipe, &fixture.vertices[0].field,
        fixture.vertices[0].position, bad_normal, &output, &report));
    assert(report.status == PROCEDURAL_SURFACE_MATERIAL_STATUS_NORMAL);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    output = sentinel;
    fixture.vertices[0].field.roughness = NAN;
    assert(!ProceduralSurfaceMaterial_Evaluate(
        &fixture.recipe, &fixture.vertices[0].field,
        fixture.vertices[0].position, fixture.vertices[0].normal,
        &output, &report));
    assert(report.status == PROCEDURAL_SURFACE_MATERIAL_STATUS_FIELD);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    material_fixture_free(&fixture);
}

int main(void) {
    test_frozen_material_contract();
    test_preview_final_adapter_parity();
    test_rejections_are_transactional();
    puts("procedural surface coupled material contract passed");
    return 0;
}
