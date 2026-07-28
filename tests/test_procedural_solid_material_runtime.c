#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_scene_3d_builder.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AnimationConfig animSettings;

static int failures;

static void expect_true(int condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

int animation_config_environment_light_mode_clamp(int mode) {
    return mode;
}

bool RuntimeScene3DSampleAuthoredLight(
    double normalized_t,
    RuntimeLight3D *out_light) {
    (void)normalized_t;
    (void)out_light;
    return false;
}

bool RuntimeScene3DSampleAuthoredCamera(
    double normalized_t,
    RuntimeCamera3D *out_camera) {
    (void)normalized_t;
    (void)out_camera;
    return false;
}

void runtime_scene_bridge_get_last_3d_primitive_seed_state(
    RuntimeSceneBridge3DPrimitiveSeedState *out_state) {
    if (out_state) memset(out_state, 0, sizeof(*out_state));
}

void runtime_scene_bridge_get_last_3d_scaffold_state(
    RuntimeSceneBridge3DScaffoldState *out_state) {
    if (out_state) memset(out_state, 0, sizeof(*out_state));
}

void runtime_scene_bridge_get_last_3d_light_seed_state(
    RuntimeSceneBridge3DLightSeedState *out_state) {
    if (out_state) memset(out_state, 0, sizeof(*out_state));
}

bool runtime_scene_bridge_get_last_object_id_for_scene_index(
    int scene_index,
    char *out_object_id,
    size_t out_object_id_size) {
    (void)scene_index;
    if (out_object_id && out_object_id_size) out_object_id[0] = '\0';
    return false;
}

const RayTracingRuntimeMeshAssetSet *
ray_tracing_runtime_mesh_assets_last(void) {
    return NULL;
}

static void configure_asset(RayTracingRuntimeMeshAssetSet *set) {
    RayTracingRuntimeMeshAsset *asset;
    CoreMeshAssetRuntimeDocument *mesh;
    ProceduralSolidMaterialBindingV1 *binding;
    ProceduralSolidAuthoredMaterialBindingV1 *authored_binding;
    ProceduralSolidAuthoredBindingReport authored_report;
    memset(set, 0, sizeof(*set));
    set->asset_count = 1;
    asset = &set->assets[0];
    core_mesh_asset_runtime_document_init(&asset->document);
    snprintf(asset->asset_id, sizeof(asset->asset_id), "psg13_runtime");
    mesh = &asset->document;
    core_mesh_asset_runtime_document_set_vertex_count(mesh, 4u);
    core_mesh_asset_runtime_document_set_triangle_count(mesh, 2u);
    mesh->vertices[0].position = (CoreObjectVec3){0.0, 0.0, 0.0};
    mesh->vertices[1].position = (CoreObjectVec3){1.0, 0.0, 0.0};
    mesh->vertices[2].position = (CoreObjectVec3){0.0, 1.0, 0.0};
    mesh->vertices[3].position = (CoreObjectVec3){1.0, 1.0, 0.0};
    mesh->triangles[0].a = 0u;
    mesh->triangles[0].b = 1u;
    mesh->triangles[0].c = 2u;
    snprintf(
        mesh->triangles[0].surface_group_id,
        sizeof(mesh->triangles[0].surface_group_id), "region_retained");
    mesh->triangles[1].a = 1u;
    mesh->triangles[1].b = 3u;
    mesh->triangles[1].c = 2u;
    snprintf(
        mesh->triangles[1].surface_group_id,
        sizeof(mesh->triangles[1].surface_group_id), "region_cut");

    asset->procedural_solid_material_valid = true;
    binding = &asset->procedural_solid_material_binding;
    ProceduralSolidMaterialBindingV1_Init(binding);
    binding->fallback_material = PROCEDURAL_SOLID_MATERIAL_GLOSSY;
    binding->region_count = 2u;
    snprintf(
        binding->regions[0].region_id,
        sizeof(binding->regions[0].region_id), "region_cut");
    binding->regions[0].kind = PROCEDURAL_SOLID_REGION_CUT;
    binding->regions[0].triangle_count = 1u;
    snprintf(
        binding->regions[1].region_id,
        sizeof(binding->regions[1].region_id), "region_retained");
    binding->regions[1].kind = PROCEDURAL_SOLID_REGION_RETAINED;
    binding->regions[1].triangle_count = 1u;
    binding->assignment_count = 1u;
    snprintf(
        binding->assignments[0].region_id,
        sizeof(binding->assignments[0].region_id), "region_cut");
    binding->assignments[0].material =
        PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL;

    asset->procedural_solid_authored_material_valid = true;
    authored_binding = &asset->procedural_solid_authored_binding;
    expect_true(
        ProceduralSolidAuthoredMaterialBindingV1_FromRegionBinding(
            "psg14_runtime_authored", binding, authored_binding,
            &authored_report),
        "PSG-14 authored binding derives from exact PSG-13 region binding");
    authored_binding->assignment_count = 2u;
    snprintf(
        authored_binding->assignments[0].region_id,
        sizeof(authored_binding->assignments[0].region_id), "region_cut");
    snprintf(
        authored_binding->assignments[1].region_id,
        sizeof(authored_binding->assignments[1].region_id),
        "region_retained");
    asset->procedural_solid_authored_material_count = 2u;
    expect_true(
        ProceduralSolidAuthoredMaterialV1_FromTemplate(
            "emissive_crystal", "cut_crystal",
            &asset->procedural_solid_authored_materials[0], NULL),
        "PSG-14 cut material template initializes");
    expect_true(
        ProceduralSolidAuthoredMaterialV1_FromTemplate(
            "pitted_concrete", "retained_concrete",
            &asset->procedural_solid_authored_materials[1], NULL),
        "PSG-14 retained material template initializes");

    set->instance_count = 1;
    snprintf(
        set->instances[0].object_id,
        sizeof(set->instances[0].object_id), "psg13_object");
    snprintf(
        set->instances[0].asset_id,
        sizeof(set->instances[0].asset_id), "psg13_runtime");
    set->instances[0].asset_index = 0;
    set->instances[0].scene_object_index = 0;
    set->instances[0].scale_x = 1.0;
    set->instances[0].scale_y = 1.0;
    set->instances[0].scale_z = 1.0;
}

static void test_region_material_survives_flattened_and_tlas_hits(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    HitInfo3D retained_hit;
    HitInfo3D cut_hit;
    HitInfo3D tlas_cut_hit;
    Ray3D retained_ray =
        RuntimeRay3D_Make(vec3(0.1, 0.1, -1.0), vec3(0.0, 0.0, 1.0));
    Ray3D cut_ray =
        RuntimeRay3D_Make(vec3(0.9, 0.9, -1.0), vec3(0.0, 0.0, 1.0));
    configure_asset(&set);
    RuntimeScene3D_Init(&scene);
    expect_true(
        RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set),
        "PSG-13 material-bound mesh appends");
    expect_true(
        scene.triangleMesh.triangleCount == 2 &&
            scene.triangleMesh.triangles[0].hasRegionMaterial &&
            scene.triangleMesh.triangles[0].regionMaterialId ==
                PROCEDURAL_SOLID_MATERIAL_GLOSSY &&
            scene.triangleMesh.triangles[1].hasRegionMaterial &&
            scene.triangleMesh.triangles[1].regionMaterialId ==
                PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL &&
            scene.triangleMesh.triangles[0].hasRegionAuthoredMaterial &&
            scene.triangleMesh.triangles[0].regionAuthoredMaterial.roughness >
                0.8 &&
            scene.triangleMesh.triangles[1].hasRegionAuthoredMaterial &&
            scene.triangleMesh.triangles[1]
                    .regionAuthoredMaterial.emission_strength > 0.0,
        "preset fallback and authored region values reach triangles");
    RuntimeRay3D_SetTraceRouteForTests(
        RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    expect_true(
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &retained_ray, 0.001, 10.0, &retained_hit) &&
            retained_hit.hasRegionMaterial &&
            retained_hit.regionMaterialId ==
                PROCEDURAL_SOLID_MATERIAL_GLOSSY &&
            retained_hit.hasRegionAuthoredMaterial &&
            retained_hit.regionAuthoredMaterial.roughness > 0.8,
        "fallback preset and authored concrete survive flattened hit");
    expect_true(
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &cut_ray, 0.001, 10.0, &cut_hit) &&
            cut_hit.hasRegionMaterial &&
            cut_hit.regionMaterialId ==
                PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL &&
            cut_hit.hasRegionAuthoredMaterial &&
            cut_hit.regionAuthoredMaterial.emission_strength > 0.0,
        "explicit preset and authored crystal survive flattened hit");
    RuntimeRay3D_SetTraceRouteForTests(
        RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
    expect_true(
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &cut_ray, 0.001, 10.0, &tlas_cut_hit) &&
            tlas_cut_hit.hasRegionMaterial &&
            tlas_cut_hit.regionMaterialId ==
                PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL &&
            tlas_cut_hit.hasRegionAuthoredMaterial &&
            tlas_cut_hit.regionAuthoredMaterial.emission_strength ==
                cut_hit.regionAuthoredMaterial.emission_strength &&
            tlas_cut_hit.localTriangleIndex == cut_hit.localTriangleIndex,
        "authored values and triangle identity survive TLAS to BLAS remap");
    RuntimeRay3D_ResetTraceRouteForTests();
    RuntimeScene3D_Free(&scene);
    core_mesh_asset_runtime_document_free(&set.assets[0].document);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
}

static void test_partial_authored_overlay_preserves_preset_fallback(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    configure_asset(&set);
    set.assets[0].procedural_solid_authored_binding.assignment_count = 1u;
    set.assets[0].procedural_solid_authored_material_count = 1u;
    RuntimeScene3D_Init(&scene);
    expect_true(
        RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set),
        "partial PSG-14 authored overlay appends");
    expect_true(
        scene.triangleMesh.triangleCount == 2 &&
            scene.triangleMesh.triangles[0].hasRegionMaterial &&
            !scene.triangleMesh.triangles[0].hasRegionAuthoredMaterial &&
            scene.triangleMesh.triangles[1].hasRegionMaterial &&
            scene.triangleMesh.triangles[1].hasRegionAuthoredMaterial,
        "unassigned retained region keeps PSG-13 preset while cut region "
        "receives authored overlay");
    RuntimeScene3D_Free(&scene);
    core_mesh_asset_runtime_document_free(&set.assets[0].document);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
}

static void test_continuous_program_survives_flattened_and_tlas_hits(void) {
    RayTracingRuntimeMeshAssetSet set;
    RayTracingRuntimeMeshAsset *asset;
    CoreMeshAssetRuntimeDocument *mesh;
    RuntimeScene3D scene;
    ProceduralSolidMaterialGraphReport graph_report = {0};
    ProceduralSolidAuthoredMaterialV1 materials[2];
    ProceduralSolidMaterialRuntimeSampleV1 flat_sample;
    ProceduralSolidMaterialRuntimeSampleV1 tlas_sample;
    ProceduralSolidMaterialRuntimeSampleV1 neighbor_sample;
    HitInfo3D flat_hit;
    HitInfo3D tlas_hit;
    HitInfo3D neighbor_hit;
    const char *kinds[2] = {"retained", "retained"};
    const char *binding_digest =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    Ray3D flat_ray =
        RuntimeRay3D_Make(vec3(0.49, 0.49, 2.0), vec3(0.0, 0.0, -1.0));
    Ray3D neighbor_ray =
        RuntimeRay3D_Make(vec3(0.51, 0.51, 2.0), vec3(0.0, 0.0, -1.0));
    configure_asset(&set);
    asset = &set.assets[0];
    mesh = &asset->document;
    mesh->vertices[0].position = (CoreObjectVec3){0.0, 0.0, 0.0};
    mesh->vertices[1].position = (CoreObjectVec3){1.0, 0.0, 0.2};
    mesh->vertices[2].position = (CoreObjectVec3){0.0, 1.0, 0.8};
    mesh->vertices[3].position = (CoreObjectVec3){1.0, 1.0, 1.0};
    mesh->vertex_normal_count = mesh->vertex_count;
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        mesh->vertices[i].normal =
            (CoreObjectVec3){-0.1543033499620919,
                             -0.6172133998483676,
                              0.7715167498104596};
    }
    mesh->contract.local_bounds.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    mesh->contract.local_bounds.max = (CoreObjectVec3){1.0, 1.0, 1.0};
    expect_true(
        ProceduralSolidMaterialGraphV1_FromTemplate(
            "snow_accumulation", "runtime_continuous_snow",
            "runtime_binding", binding_digest,
            &asset->procedural_solid_material_graph, &graph_report),
        "PSG-16A snow graph initializes");
    expect_true(
        ProceduralSolidAuthoredMaterialV1_FromTemplate(
            "weathered_rock", "base_material", &materials[0], NULL) &&
        ProceduralSolidAuthoredMaterialV1_FromTemplate(
            "snow", "snow_material", &materials[1], NULL),
        "PSG-16A graph materials initialize");
    expect_true(
        ProceduralSolidMaterialRuntimeProgramV1_Build(
            &asset->procedural_solid_material_graph, materials, 2u,
            mesh, kinds,
            &asset->procedural_solid_material_runtime_program,
            &graph_report),
        "PSG-16A runtime program builds");
    asset->procedural_solid_material_graph_valid = true;
    asset->procedural_solid_composed_triangle_material_count =
        mesh->triangle_count;
    asset->procedural_solid_composed_triangle_materials =
        calloc(mesh->triangle_count,
               sizeof(*asset->procedural_solid_composed_triangle_materials));
    expect_true(
        asset->procedural_solid_composed_triangle_materials != NULL,
        "PSG-16A centroid compatibility surfaces allocate");
    if (!asset->procedural_solid_composed_triangle_materials) goto cleanup;
    for (size_t i = 0u; i < mesh->triangle_count; ++i)
        asset->procedural_solid_composed_triangle_materials[i] =
            materials[0].surface;

    RuntimeScene3D_Init(&scene);
    expect_true(
        RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set),
        "PSG-16A material program mesh appends");
    RuntimeRay3D_SetTraceRouteForTests(
        RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    expect_true(
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &flat_ray, 0.001, 10.0, &flat_hit) &&
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &neighbor_ray, 0.001, 10.0, &neighbor_hit) &&
        flat_hit.proceduralSolidMaterialRuntimeProgram ==
            &asset->procedural_solid_material_runtime_program &&
        neighbor_hit.proceduralSolidMaterialRuntimeProgram ==
            &asset->procedural_solid_material_runtime_program,
        "PSG-16A flattened hits retain one asset-level runtime program");
    expect_true(
        ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
            flat_hit.proceduralSolidMaterialRuntimeProgram,
            (size_t)flat_hit.localTriangleIndex,
            flat_hit.baryU, flat_hit.baryV, flat_hit.baryW,
            &flat_sample, &graph_report) &&
        ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
            neighbor_hit.proceduralSolidMaterialRuntimeProgram,
            (size_t)neighbor_hit.localTriangleIndex,
            neighbor_hit.baryU, neighbor_hit.baryV, neighbor_hit.baryW,
            &neighbor_sample, &graph_report) &&
        neighbor_sample.geometry.height > flat_sample.geometry.height &&
        fabs(neighbor_sample.primary_layer_weight -
             flat_sample.primary_layer_weight) < 0.08,
        "PSG-16A neighboring triangles expose continuous height and mask");
    RuntimeRay3D_SetTraceRouteForTests(
        RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
    expect_true(
        RuntimeRay3D_TraceSceneFirstHit(
            &scene, &flat_ray, 0.001, 10.0, &tlas_hit) &&
        tlas_hit.proceduralSolidMaterialRuntimeProgram ==
            flat_hit.proceduralSolidMaterialRuntimeProgram &&
        ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
            tlas_hit.proceduralSolidMaterialRuntimeProgram,
            (size_t)tlas_hit.localTriangleIndex,
            tlas_hit.baryU, tlas_hit.baryV, tlas_hit.baryW,
            &tlas_sample, &graph_report) &&
        fabs(tlas_sample.geometry.height -
             flat_sample.geometry.height) < 1e-12 &&
        fabs(tlas_sample.primary_layer_weight -
             flat_sample.primary_layer_weight) < 1e-12,
        "PSG-16A flattened and TLAS hits evaluate identical scalar masks");
    RuntimeRay3D_ResetTraceRouteForTests();
    RuntimeScene3D_Free(&scene);
cleanup:
    free(asset->procedural_solid_composed_triangle_materials);
    asset->procedural_solid_composed_triangle_materials = NULL;
    ProceduralSolidMaterialRuntimeProgramV1_Free(
        &asset->procedural_solid_material_runtime_program);
    core_mesh_asset_runtime_document_free(&asset->document);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
}

int main(void) {
    test_region_material_survives_flattened_and_tlas_hits();
    test_partial_authored_overlay_preserves_preset_fallback();
    test_continuous_program_survives_flattened_and_tlas_hits();
    if (failures) {
        fprintf(stderr, "%d PSG-14 runtime failures\n", failures);
        return 1;
    }
    printf("PSG-14 authored region material runtime tests passed\n");
    return 0;
}
