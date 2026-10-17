#include "test_support.h"
#include "render/runtime_disney_v2_photon_receiver_3d.h"
#include "render/runtime_disney_v2_receiver_3d.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "render/runtime_specular_reflection_3d.h"
#include "render/runtime_native_3d_render_shading_internal.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "material/material_manager.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void check_rgb(const char* name, Vec3 a, Vec3 b) {
    assert_close(name,a.x,b.x,1e-10);assert_close(name,a.y,b.y,1e-10);assert_close(name,a.z,b.z,1e-10);
}
int run_test_runtime_photon_receiver_routing_tests(void) {
    AnimationConfig saved_anim=animSettings;
    SceneConfig saved_scene=sceneSettings;
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonMapRecord3D record={0};
    RuntimeMaterialPayload3D material={0};
    RuntimeCausticPhotonMapQueryResult3D query;
    RuntimeCausticPhotonSurfaceDiagnosticAggregate3D aggregate;
    RuntimeDisneyV2_3DResult local={0},off={0},on={0},parent={0};
    HitInfo3D hit;HitInfo3D_Reset(&hit);
    hit.position=vec3(0,0,0);hit.normal=hit.geometricNormal=hit.shadingNormal=vec3(0,0,1);
    hit.sceneObjectIndex=0;hit.primitiveIndex=0;hit.triangleIndex=0;hit.t=1;
    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("photon_routing_allocate",RuntimeCausticPhotonMap3D_Allocate(&map,4));
    record.normal=vec3(0,0,1);record.incidentDirection=vec3(0,0,-1);
    record.pathPdf=1;record.queryRadius=.2;record.flux=vec3(.1,.2,.3);
    record.sceneObjectIndex=0;record.materialId=-1;
    for(int i=0;i<4;i++) {record.photonId=(uint64_t)i+1;record.position=vec3((i-1.5)*.005,0,0);
        assert_true("photon_routing_store",RuntimeCausticPhotonMap3D_StoreRecord(&map,&record));}
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    settings.surfaceEstimator=RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    settings.surfaceQueryRadius=.2;
    material.valid=true;material.materialId=-1;material.baseColorR=.2;material.baseColorG=.5;
    material.baseColorB=.8;material.bsdf.diffuseWeight=.6;material.bsdf.roughness=.8;
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP,&map,NULL,&settings);
    Vec3 expected;
    assert_true("photon_routing_resolved_material_query",
        RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithPayload(&hit,&material,&expected,&query));
    Vec3 formula=vec3(query.physicalFlux.x*.2*.6/acos(-1),query.physicalFlux.y*.5*.6/acos(-1),query.physicalFlux.z*.8*.6/acos(-1));
    check_rgb("photon_routing_lambert_once",expected,formula);
    RuntimeCausticPhotonDirectConsumer3D_ResetSurfaceDiagnostics();
    assert_true("photon_routing_first_local",RuntimeDisneyV2PhotonReceiver3D_Apply(&hit,&material,&local));
    assert_true("photon_routing_second_local_refused",!RuntimeDisneyV2PhotonReceiver3D_Apply(&hit,&material,&local));
    check_rgb("photon_routing_local_once",vec3(local.diffuseRadianceR,local.diffuseRadianceG,local.diffuseRadianceB),expected);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&aggregate);
    assert_true("photon_routing_one_local_query",aggregate.queryCount==1);

    /* Shared primary/arriving-glass receiver entry: no lights or geometry can
     * contribute another photon query in this deliberately empty scene. */
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);scene.hasLight=false;
    scene.environment.backgroundIntensity=0;scene.environment.ambientIntensity=0;
    RuntimePrimaryHit3DResult primary={0};primary.hit=true;primary.hitInfo=hit;
    primary.primaryRay=RuntimeRay3D_Make(vec3(0,0,1),vec3(0,0,-1));
    primary.primaryTransmittance=RuntimeVisibility3D_UnitTransmittance();
    RuntimeNative3DSamplingContext sampling={.sampleSequence=17,.temporalSubpassCount=1};
    memset(&animSettings,0,sizeof(animSettings));animSettings.bounceDepth3D=1;
    animSettings.specularDepth3D=1;animSettings.transmissionDepth3D=1;
    RuntimeCausticPhotonDirectConsumer3D_Reset();
    assert_true("photon_routing_primary_off",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(&scene,&primary,&material,&sampling,0,0,1,1,&off));
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP,&map,NULL,&settings);
    assert_true("photon_routing_primary_on",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(&scene,&primary,&material,&sampling,0,0,1,1,&on));
    check_rgb("photon_routing_primary_delta",vec3(on.radianceR-off.radianceR,on.radianceG-off.radianceG,on.radianceB-off.radianceB),expected);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&aggregate);
    assert_true("photon_routing_primary_one_query",aggregate.queryCount==1);
    RuntimeCausticPhotonDirectConsumer3D_ResetSurfaceDiagnostics();
    parent.payload.hasGlassTransportOverride=true;
    assert_true("photon_routing_arriving_transmitted_receiver",runtime_disney_v2_shade_receiver_3d(&scene,&hit,&primary.primaryRay,&material,&sampling,&parent,&on));
    check_rgb("photon_routing_transmitted_delta",vec3(on.radianceR-off.radianceR,on.radianceG-off.radianceG,on.radianceB-off.radianceB),expected);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&aggregate);
    assert_true("photon_routing_transmitted_one_query",aggregate.queryCount==1);

    /* Deliberately contaminated cache must not add a second term in direct-map. */
    RuntimeCausticSurfaceCache3D cache;RuntimeCausticSurfaceCache3D_Init(&cache);
    assert_true("photon_routing_cache_allocate",RuntimeCausticSurfaceCache3D_Allocate(&cache,1));
    assert_true("photon_routing_cache_deposit",RuntimeCausticSurfaceCache3D_DepositAtHit(&cache,&hit,.2,100,200,300));
    double r=expected.x,g=expected.y,b=expected.z,luma=r;bool visible=true;
    RuntimeNative3DRenderStats stats={0};
    runtime_native_3d_render_apply_surface_caustic_cache(&cache,&hit,&r,&g,&b,&luma,&visible,&stats);
    on.primaryTransmissionContinued=true;on.primaryTransmissionPathState.valid=true;
    on.primaryTransmissionPathState.hit=true;on.primaryTransmissionPathState.hitInfo=hit;
    on.primaryTransmissionPathState.throughputR=on.primaryTransmissionPathState.throughputG=on.primaryTransmissionPathState.throughputB=1;
    runtime_native_3d_render_apply_transmitted_surface_caustic_cache(&cache,&on,&r,&g,&b,&luma,&visible,&stats);
    check_rgb("photon_routing_no_host_double_add",vec3(r,g,b),expected);
    assert_true("photon_routing_no_host_cache_lookup",stats.causticSurfaceCacheSampleLookupCount==0);
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE,&map,NULL,&settings);
    RuntimeDisneyV2_3DResult bridge={0};
    assert_true("photon_routing_bridge_no_direct_query",!RuntimeDisneyV2PhotonReceiver3D_Apply(&hit,&material,&bridge));
    r=g=b=0;runtime_native_3d_render_apply_surface_caustic_cache(&cache,&hit,&r,&g,&b,&luma,&visible,&stats);
    assert_true("photon_routing_bridge_cache_separate",r>0&&stats.causticSurfaceCacheSampleContributingCount==1);
    RuntimeCausticSurfaceCache3D_Free(&cache);RuntimeScene3D_Free(&scene);

    /* Actual deterministic mirror trace to a diffuse floor. Its first receiver
     * is locally shaded; the recursive loop must skip that already-owned hit. */
    memset(&sceneSettings,0,sizeof(sceneSettings));sceneSettings.objectCount=1;
    sceneSettings.sceneObjects[0].alpha=1;sceneSettings.sceneObjects[0].material_id=MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color=0xA0BEDC;
    RuntimeRay3DTraceRoute saved_route=RuntimeRay3D_CurrentTraceRoute();
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    RuntimeScene3D_Init(&scene);scene.hasLight=false;scene.environment.backgroundIntensity=0;
    scene.triangleMesh.triangleCount=scene.triangleMesh.triangleCapacity=1;
    scene.triangleMesh.triangles=calloc(1,sizeof(*scene.triangleMesh.triangles));
    RuntimeTriangle3D* floor=&scene.triangleMesh.triangles[0];
    floor->p0=vec3(-2,-2,0);floor->p1=vec3(2,-2,0);floor->p2=vec3(0,2,0);
    floor->normal=vec3(0,0,1);floor->sceneObjectIndex=0;floor->primitiveIndex=0;
    floor->hasRegionMaterial=true;floor->regionMaterialId=MATERIAL_PRESET_DEFAULT;
    assert_true("photon_routing_mirror_bvh",RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    RuntimeMaterialPayload3D mirror=material;mirror.bsdf.reflectivity=1;mirror.bsdf.specWeight=1;
    mirror.bsdf.diffuseWeight=0;mirror.bsdf.roughness=.01;mirror.bsdf.ior=1.5;
    mirror.baseColorR=mirror.baseColorG=mirror.baseColorB=1;
    primary.hitInfo=hit;primary.hitInfo.position=vec3(0,0,1);
    primary.hitInfo.normal=primary.hitInfo.geometricNormal=primary.hitInfo.shadingNormal=vec3(0,-sqrt(.5),-sqrt(.5));
    primary.hitInfo.sceneObjectIndex=1;primary.hitInfo.triangleIndex=1;
    primary.primaryRay=RuntimeRay3D_Make(vec3(0,-1,1),vec3(0,1,0));
    RuntimeSpecularReflection3DResult reflected={0};
    assert_true("photon_routing_actual_mirror_trace",RuntimeSpecularReflection3D_Trace(&scene,&primary.hitInfo,&mirror,vec3(0,-1,0),&sampling,&reflected)&&reflected.geometryHit);
    RuntimeMaterialPayload3D floor_material;
    assert_true("photon_routing_mirror_receiver_material",RuntimeMaterialPayload3D_ResolveFromHit(&reflected.hitInfo,&floor_material));
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP,&map,NULL,&settings);
    Vec3 floor_radiance;
    assert_true("photon_routing_mirror_expected",RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithPayload(&reflected.hitInfo,&floor_material,&floor_radiance,NULL));
    const char* old_mode=getenv("RAY_TRACING_MIRROR_ESTIMATOR_AUDIT");char* saved_mode=old_mode?strdup(old_mode):NULL;
    setenv("RAY_TRACING_MIRROR_ESTIMATOR_AUDIT","dedicated",1);
    RuntimeCausticPhotonDirectConsumer3D_Reset();
    assert_true("photon_routing_mirror_off",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(&scene,&primary,&mirror,&sampling,0,0,1,1,&off));
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP,&map,NULL,&settings);
    assert_true("photon_routing_mirror_on",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(&scene,&primary,&mirror,&sampling,0,0,1,1,&on));
    Vec3 reflected_expected=vec3(floor_radiance.x*reflected.weight*reflected.tintR,floor_radiance.y*reflected.weight*reflected.tintG,floor_radiance.z*reflected.weight*reflected.tintB);
    check_rgb("photon_routing_mirror_delta_once",vec3(on.radianceR-off.radianceR,on.radianceG-off.radianceG,on.radianceB-off.radianceB),reflected_expected);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&aggregate);
    assert_true("photon_routing_mirror_one_receiver_query",aggregate.queryCount==1);
    animSettings.bounceDepth3D=3;animSettings.specularDepth3D=3;
    RuntimeCausticPhotonDirectConsumer3D_ResetSurfaceDiagnostics();
    assert_true("photon_routing_mirror_deeper",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(&scene,&primary,&mirror,&sampling,0,0,1,1,&on));
    check_rgb("photon_routing_mirror_deeper_delta_once",vec3(on.radianceR-off.radianceR,on.radianceG-off.radianceG,on.radianceB-off.radianceB),reflected_expected);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&aggregate);
    assert_true("photon_routing_mirror_recursive_skip_owned_receiver",aggregate.queryCount==1);
    if(saved_mode){setenv("RAY_TRACING_MIRROR_ESTIMATOR_AUDIT",saved_mode,1);free(saved_mode);}else unsetenv("RAY_TRACING_MIRROR_ESTIMATOR_AUDIT");
    RuntimeCausticPhotonDirectConsumer3D_Reset();RuntimeCausticPhotonMap3D_Free(&map);
    RuntimeScene3D_Free(&scene);RuntimeRay3D_SetTraceRouteForTests(saved_route);animSettings=saved_anim;sceneSettings=saved_scene;
    return 0;
}
