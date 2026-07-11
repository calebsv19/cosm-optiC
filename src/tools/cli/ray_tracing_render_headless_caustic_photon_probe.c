#include "tools/ray_tracing_render_headless_internal.h"

#include <stdint.h>
#include <string.h>

#include "app/agent_render_request.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_photon_scene_descriptor_3d.h"

void ray_tracing_headless_probe_caustic_photon_callsite(
    RayTracingHeadlessPreflight* preflight,
    const RuntimeNative3DPreparedFrame* frame,
    const RayTracingAgentRenderRequest* request) {
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticSurfaceCache3D surface_cache;
    const RuntimeLightSource3D* first_light = NULL;
    double query_radius = 0.0;
    uint64_t cache_capacity = 1u;

    if (!preflight || !frame || !request) return;
    if (!request->caustic_photon_populated_callsite_readback_enabled) return;

    settings = request->caustic_photon_integration_settings;
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    first_light = RuntimeLightSet3D_GetEnabled(&frame->scene.lightSet, 0);
    if (first_light) {
        query.surface.position = first_light->position;
        query.surface.sceneObjectIndex = first_light->sourceSceneObjectIndex;
        query.surface.primitiveIndex = first_light->sourcePrimitiveIndex;
        query.surface.triangleIndex = first_light->sourceTriangleIndex;
    }
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query_radius = request->caustic_photon_integration_settings.surfaceQueryRadius > 0.0
                       ? request->caustic_photon_integration_settings.surfaceQueryRadius
                       : query.surface.radius;
    if (first_light) {
        const Vec3 origin = first_light->position;
        for (int i = 0; i < RuntimeLightSet3D_EnabledCount(&frame->scene.lightSet); ++i) {
            const RuntimeLightSource3D* light =
                RuntimeLightSet3D_GetEnabled(&frame->scene.lightSet, i);
            double extent = 0.0;
            double distance = 0.0;
            if (!light) continue;
            if (light->emissiveProxyRadius > extent) {
                extent = light->emissiveProxyRadius;
            }
            if (light->radius > extent) {
                extent = light->radius;
            }
            if (light->width > extent) {
                extent = light->width;
            }
            if (light->height > extent) {
                extent = light->height;
            }
            distance = vec3_length(vec3_sub(light->position, origin));
            if (distance + extent > query_radius) {
                query_radius = distance + extent;
            }
        }
    }
    query.surface.radius = query_radius;
    settings.surfaceQueryRadius = query_radius;
    query.querySurface = true;
    query.queryVolume = false;

    if (request->caustic_photon_integration_settings.sampleBudget > 0) {
        cache_capacity =
            (uint64_t)request->caustic_photon_integration_settings.sampleBudget;
    }

    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    (void)RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, cache_capacity);
    (void)RuntimeCausticPhotonIntegration3D_EvaluatePopulatedRenderCallsite(
        &frame->scene.lightSet,
        NULL,
        &surface_cache,
        NULL,
        &settings,
        &query,
        &preflight->causticPhotonCallsiteReadback);
    preflight->causticPhotonCallsiteReadbackBuilt = true;
    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
}

static RuntimeCausticLensShape3D ray_tracing_headless_trace_fixture_shape(void) {
    RuntimeCausticLensShape3D shape;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    shape.sceneObjectIndex = 3101;
    shape.primitiveIndex = 4101;
    shape.center = vec3(0.0, 0.0, 0.0);
    shape.axis = vec3(0.0, 1.0, 0.0);
    shape.radius = 0.75;
    shape.height = 0.30;
    shape.payload.opticalIor = 1.45;
    shape.payload.bsdf.ior = 1.45;
    shape.payload.baseColorR = 0.96;
    shape.payload.baseColorG = 1.0;
    shape.payload.baseColorB = 0.92;
    shape.payload.absorptionDistance = 8.0;
    return shape;
}

static RuntimeTriangle3D ray_tracing_headless_trace_fixture_triangle(void) {
    RuntimeTriangle3D triangle = {0};

    triangle.p0 = vec3(-0.7, 0.0, -0.5);
    triangle.p1 = vec3(0.7, 0.0, -0.5);
    triangle.p2 = vec3(0.0, 0.0, 0.7);
    triangle.normal = vec3(0.0, 1.0, 0.0);
    triangle.sceneObjectIndex = 3101;
    triangle.primitiveIndex = 4101;
    triangle.localTriangleIndex = 5101;
    return triangle;
}

static bool ray_tracing_headless_harvest_prepared_mesh_dielectric(
    const RuntimeNative3DPreparedFrame* frame,
    RuntimeCausticLensShape3D* out_shape,
    RuntimeTriangle3D* out_entry_triangle,
    RuntimeCausticPhotonMapPopulationReadback3D* io_population) {
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    const RuntimeCausticPhotonMeshDielectricDescriptor3D* selected = NULL;

    if (io_population) {
        io_population->preparedSceneMeshDielectricAttempted = true;
        io_population->preparedSceneMeshDielectricSceneObjectIndex = -1;
        io_population->preparedSceneMeshDielectricPrimitiveIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleCount = 0;
    }
    if (!frame || !out_shape || !out_entry_triangle) return false;
    if (!RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(
            &frame->scene,
            &batch)) {
        if (io_population) {
            io_population->preparedSceneMeshDielectricCandidateCount =
                batch.meshDielectricCandidateCount;
        }
        return false;
    }
    selected = RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(&batch);
    if (!selected) return false;

    *out_shape = selected->shape;
    *out_entry_triangle = selected->entryTriangle;
    if (io_population) {
        io_population->preparedSceneMeshDielectricSucceeded = true;
        io_population->preparedSceneMeshDielectricCandidateCount =
            batch.meshDielectricCandidateCount;
        io_population->preparedSceneMeshDielectricSceneObjectIndex =
            selected->sceneObjectIndex;
        io_population->preparedSceneMeshDielectricPrimitiveIndex =
            selected->primitiveIndex;
        io_population->preparedSceneMeshDielectricTriangleIndex =
            selected->triangleIndex;
        io_population->preparedSceneMeshDielectricTriangleCount =
            selected->triangleCount;
    }
    return true;
}

void ray_tracing_headless_probe_caustic_photon_trace_callsite(
    RayTracingHeadlessPreflight* preflight,
    const RuntimeNative3DPreparedFrame* frame,
    const RayTracingAgentRenderRequest* request) {
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticPhotonMapPopulationReadback3D population;
    RuntimeCausticPhotonMapPopulationReadback3D harvest;
    RuntimeCausticPhotonRenderCallsiteReadback3D readback;
    RuntimeCausticLensShape3D shape;
    RuntimeTriangle3D triangle;
    uint64_t cache_capacity = 1u;

    if (!preflight || !frame || !request) return;
    if (!request->caustic_photon_trace_populated_callsite_readback_enabled) return;

    settings = request->caustic_photon_integration_settings;
    settings.surfaceQueryEnabled = true;
    settings.volumeQueryEnabled = true;
    settings.renderContributionEnabled = true;
    if (settings.sampleBudget <= 0) settings.sampleBudget = 8;
    if (!(settings.surfaceQueryRadius > 0.0)) settings.surfaceQueryRadius = 0.20;
    if (!(settings.volumeQueryRadius > 0.0)) settings.volumeQueryRadius = 0.20;
    if (request->caustic_photon_integration_settings.sampleBudget > 0) {
        cache_capacity =
            (uint64_t)request->caustic_photon_integration_settings.sampleBudget;
    }

    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    query.querySurface = true;
    query.queryVolume = true;
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = settings.surfaceQueryRadius;
    query.surface.sceneObjectIndex = 9101;
    query.surface.primitiveIndex = 9102;
    query.surface.triangleIndex = 9103;
    query.volume.direction = vec3(0.0, -1.0, 0.0);
    query.volume.radius = settings.volumeQueryRadius;
    query.volume.mediumId = 7;
    query.volume.requireMediumId = true;

    memset(&harvest, 0, sizeof(harvest));
    if (!ray_tracing_headless_harvest_prepared_mesh_dielectric(frame,
                                                               &shape,
                                                               &triangle,
                                                               &harvest)) {
        shape = ray_tracing_headless_trace_fixture_shape();
        triangle = ray_tracing_headless_trace_fixture_triangle();
        harvest.fixtureMeshDielectricFallbackUsed = true;
    }

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    (void)RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, cache_capacity);
    (void)RuntimeCausticVolumeCache3D_AllocateFromVolume(&volume_cache, &frame->scene.volume);
    (void)RuntimeCausticPhotonIntegration3D_PopulateMapsFromMeshDielectricFixture(
        &surface_map,
        &beam_map,
        &frame->scene.lightSet,
        &shape,
        &triangle,
        &settings,
        &query,
        &population);
    population.preparedSceneMeshDielectricAttempted =
        harvest.preparedSceneMeshDielectricAttempted;
    population.preparedSceneMeshDielectricSucceeded =
        harvest.preparedSceneMeshDielectricSucceeded;
    population.fixtureMeshDielectricFallbackUsed =
        harvest.fixtureMeshDielectricFallbackUsed;
    population.preparedSceneMeshDielectricCandidateCount =
        harvest.preparedSceneMeshDielectricCandidateCount;
    population.preparedSceneMeshDielectricSceneObjectIndex =
        harvest.preparedSceneMeshDielectricSceneObjectIndex;
    population.preparedSceneMeshDielectricPrimitiveIndex =
        harvest.preparedSceneMeshDielectricPrimitiveIndex;
    population.preparedSceneMeshDielectricTriangleIndex =
        harvest.preparedSceneMeshDielectricTriangleIndex;
    population.preparedSceneMeshDielectricTriangleCount =
        harvest.preparedSceneMeshDielectricTriangleCount;
    if (surface_map.recordCount > 0u) {
        query.surface.position = surface_map.records[0].position;
    }
    if (beam_map.segmentCount > 0u) {
        query.volume.position = vec3_add(
            beam_map.segments[0].start,
            vec3_scale(vec3_sub(beam_map.segments[0].end, beam_map.segments[0].start),
                       0.5));
        query.volume.direction = beam_map.segments[0].direction;
    }
    (void)RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(&surface_map,
                                                                   &beam_map,
                                                                   &surface_cache,
                                                                   &volume_cache,
                                                                   &settings,
                                                                   &query,
                                                                   &readback);
    readback.mapPopulation = population;
    preflight->causticPhotonCallsiteReadback = readback;
    preflight->causticPhotonCallsiteReadbackBuilt = true;
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
}
