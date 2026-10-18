#pragma once
#include <stdlib.h>
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_physical_glass_3d.h"
#include "render/runtime_water_material_3d.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "render/runtime_triangle_bvh_3d.h"

static RuntimeLightSource3D emitter_contract_panel(void) {
    RuntimeLightSource3D light;
    RuntimeLightSource3D_Init(&light);
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;
    light.position = vec3(0, 0, 2);
    light.axisU = vec3(1, 0, 0); light.axisV = vec3(0, 1, 0);
    light.normal = vec3(0, 0, -1);
    light.width = light.height = 2;
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
    light.radiometryMode = RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE;
    light.radiance = 4;
    light.color = vec3(1, .5, .25);
    return light;
}

static void test_runtime_light_emitter_contract(void) {
    RuntimeScene3D scene; RuntimeScene3D_Init(&scene);
    RuntimeLightSource3D light = emitter_contract_panel();
    assert_true("emitter_rect_append", RuntimeLightSet3D_Append(&scene.lightSet, &light, NULL));
    RuntimeLightEmitterHit3DResult hit;
    Ray3D ray = RuntimeRay3D_Make(vec3(0,0,0), vec3(0,0,1));
    assert_true("emitter_rect_front", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_rect_t", hit.t, 2, 1e-12);
    assert_close("emitter_rect_non_delta_no_overlap", RuntimeLightEmitter3D_BsdfHitWeight(&hit, false, .3), 0, 0);
    assert_close("emitter_rect_optical_exclusive", RuntimeLightEmitter3D_BsdfHitWeight(&hit, true, .3), 1, 0);
    assert_close("emitter_rect_direct_unit_weight", RuntimeLightEmitter3D_DirectEstimate(4,4,.7), 4, 1e-12);
    assert_close("emitter_rect_mixed_direct_keeps_other_weight", RuntimeLightEmitter3D_DirectEstimate(6,4,.7), 5.4, 1e-12);
    Vec3 rgb = RuntimeLightEmitter3D_RadianceRGB(&hit);
    assert_close("emitter_rect_r", rgb.x, 4, 1e-12);
    assert_close("emitter_rect_g", rgb.y, 2, 1e-12);
    assert_close("emitter_rect_b", rgb.z, 1, 1e-12);
    ray.origin.z = -10;
    assert_true("emitter_rect_far", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 20, &hit));
    assert_close("emitter_radiance_distance_invariant", hit.radiance, 4, 1e-12);
    ray.origin = vec3(.9,0,0); ray.direction = vec3_normalize(vec3(-.9,0,2));
    assert_true("emitter_rect_oblique", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_radiance_angle_invariant", hit.radiance, 4, 1e-12);
    ray = RuntimeRay3D_Make(vec3(0,0,4), vec3(0,0,-1));
    assert_true("emitter_rect_back_is_surface", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_rect_back_dark", hit.radiance, 0, 0);
    scene.lightSet.lights[0].emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED;
    assert_true("emitter_rect_two_sided", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_rect_two_sided_radiance", hit.radiance, 4, 1e-12);
    ray = RuntimeRay3D_Make(vec3(1.01,0,0), vec3(0,0,1));
    assert_true("emitter_rect_outside", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    ray = RuntimeRay3D_Make(vec3(0,0,0), vec3(1,0,0));
    assert_true("emitter_rect_parallel", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    ray.direction = vec3(0,0,1);
    assert_true("emitter_rect_t_range", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 1.9, &hit));
    scene.lightSet.lights[0].axisV = vec3(1,0,0);
    assert_true("emitter_rect_degenerate", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    scene.lightSet.lights[0] = light;
    scene.lightSet.lights[0].axisU = vec3(0,0,1);
    scene.lightSet.lights[0].normal = vec3(-1,0,0);
    scene.lightSet.lights[0].position = vec3(2,0,0);
    ray.direction = vec3(1,0,0);
    assert_true("emitter_rect_rotated", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_rect_rotated_rgb", hit.radianceRGB.y, 2, 1e-12);
    scene.lightSet.lights[0] = light;
    scene.lightSet.lights[0].origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER;
    ray.direction = vec3(0,0,1);
    assert_true("emitter_material_proxy_not_duplicate", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    scene.lightSet.lights[0] = light;
    scene.lightSet.lights[0].radiometryMode = RUNTIME_LIGHT_RADIOMETRY_LEGACY_INTENSITY;
    scene.lightSet.lights[0].intensity = 10;
    scene.lightSet.lights[0].falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.lightSet.lights[0].falloffDistance = 2;
    assert_true("emitter_rect_legacy", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_rect_legacy_convention", hit.radiance, 5, 1e-12);
    assert_close("emitter_rect_legacy_color", hit.radianceRGB.y, 2.5, 1e-12);
    RuntimeLightEmitterHit3DResult camera_hit;
    RuntimeLightEmitterTrace3DResult camera_trace;
    assert_true("legacy_helper_camera_hidden", !RuntimeLightEmitter3D_IntersectCameraRay(&scene,&ray,0,10,&camera_hit));
    assert_true("legacy_helper_camera_first_hit_hidden", !RuntimeLightEmitter3D_ResolveCameraFirstHit(&scene,&ray,0,10,&camera_trace));
    Ray3D back = RuntimeRay3D_Make(vec3(0,0,4),vec3(0,0,-1));
    assert_true("legacy_helper_back_camera_hidden", !RuntimeLightEmitter3D_IntersectCameraRay(&scene,&back,0,10,&camera_hit));
    RuntimeLightSource3D farther = light; farther.position.z = 3;
    assert_true("camera_farther_light_append",RuntimeLightSet3D_Append(&scene.lightSet,&farther,NULL));
    assert_true("camera_sees_past_helper",RuntimeLightEmitter3D_IntersectCameraRay(&scene,&ray,0,10,&camera_hit));
    assert_close("camera_farther_physical_t",camera_hit.t,3,1e-12);
    assert_close("camera_physical_radiance",camera_hit.radiance,4,1e-12);
    assert_true("transport_still_sees_helper",RuntimeLightEmitter3D_IntersectRay(&scene,&ray,0,10,&camera_hit));
    assert_close("transport_helper_t",camera_hit.t,2,1e-12);
    assert_close("transport_helper_unchanged",camera_hit.radiance,5,1e-12);
    scene.lightSet.lightCount = 1;
    RuntimeLightEmitter3D_ApplyTransmittance(&hit, vec3(.25,.5,1));
    assert_close("emitter_volume_rgb_r", hit.radianceRGB.x, 1.25, 1e-12);
    assert_close("emitter_volume_rgb_g", hit.radianceRGB.y, 1.25, 1e-12);
    assert_close("emitter_volume_rgb_b", hit.radianceRGB.z, 1.25, 1e-12);
    scene.hasLight = true; scene.light.position = vec3(0,0,2); scene.light.radius = 1;
    scene.light.intensity = 100;
    scene.lightSet.lights[0].enabled = false;
    assert_true("emitter_disabled_no_compat_ghost", !RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    RuntimeScene3D_Free(&scene);

    /* An unregistered compatibility light retains its scalar/white contract. */
    RuntimeScene3D_Init(&scene); scene.hasLight = true;
    scene.light.position = vec3(0,0,3); scene.light.radius = 1;
    scene.light.intensity = 6; scene.light.falloffMode = FORWARD_FALLOFF_MODE_NONE;
    assert_true("emitter_compat_sphere", RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0, 10, &hit));
    assert_close("emitter_compat_white", RuntimeLightEmitter3D_RadianceRGB(&hit).z, 6, 1e-12);
    RuntimeScene3D_Free(&scene);
}

static void test_runtime_water_emitter_transport(void) {
    AnimationConfig saved = animSettings;
    SceneConfig saved_scene = sceneSettings;
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0x000000;
    sceneSettings.sceneObjects[0].alpha = 1;
    animSettings.bounceDepth3D = animSettings.specularDepth3D = animSettings.transmissionDepth3D = 8;
    animSettings.transmissionSamples3D = 64;
    RuntimeRay3DTraceRoute old_route = RuntimeRay3D_CurrentTraceRoute();
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    RuntimeScene3D scene; RuntimeScene3D_Init(&scene);
    RuntimeLightSource3D light = emitter_contract_panel();
    light.position = vec3(0,0,3); light.width = light.height = 20;
    assert_true("water_emitter_append", RuntimeLightSet3D_Append(&scene.lightSet, &light, NULL));
    RuntimeWaterMaterial3DOverride water = {.valid=true, .ior=1.333, .absorptionDistance=1, .roughness=0};
    RuntimeMaterialPayload3D p = {.valid=true, .materialId=MATERIAL_PRESET_TRANSPARENT};
    RuntimeWaterMaterial3DOverride previous_water;
    bool had_water = RuntimeWaterMaterial3D_Get(127, &previous_water);
    assert_true("water_emitter_material_set", RuntimeWaterMaterial3D_Set(127, &water));
    assert_true("water_emitter_material_apply", RuntimeWaterMaterial3D_ApplyToPayload(127, &p));
    HitInfo3D boundary = {0}; boundary.sceneObjectIndex = 127; boundary.triangleIndex = -1;
    boundary.normal = boundary.geometricNormal = boundary.shadingNormal = vec3(0,0,1);
    boundary.position = vec3(0,0,0); boundary.t = 1;
    Ray3D incident = RuntimeRay3D_Make(vec3(0,0,1), vec3(0,0,-1));
    double red = 0, green = 0, blue = 0;
    for (unsigned i=0; i<16384; ++i) {
        RuntimeNative3DSamplingContext sampling = {.sampleSequence=19+i*7919, .temporalSubpassCount=1};
        RuntimeDisneyV2_3DResult result;
        assert_true("water_emitter_shade", RuntimePhysicalGlass3D_Shade(&scene, &incident, &boundary, &p, &sampling, 0,0,1,1,&result));
        red += result.radianceR/16384; green += result.radianceG/16384; blue += result.radianceB/16384;
    }
    double fresnel = RuntimePhysicalGlass3D_Fresnel(1,1,1.333);
    assert_close("water_emitter_fresnel_energy", red, 4*fresnel, .002);
    assert_close("water_emitter_reflection_color_g", green, red*.5, 1e-12);
    assert_close("water_emitter_reflection_color_b", blue, red*.25, 1e-12);
    printf("water emitter normal reflection: RGB %.9g %.9g %.9g; analytic red %.9g\n",red,green,blue,4*fresnel);
    /* Match the transmitted branch with the identical sampling stream. Radiance
     * scales by eta^-2; conservation is R + eta^2 T, not a raw radiance sum. */
    light.position = vec3(0,0,-3); light.normal = vec3(0,0,1);
    scene.lightSet.lights[0] = light;
    double transmitted_red = 0;
    for (unsigned i=0; i<16384; ++i) {
        RuntimeNative3DSamplingContext sampling = {.sampleSequence=19+i*7919, .temporalSubpassCount=1};
        RuntimeDisneyV2_3DResult result;
        assert_true("water_emitter_transmission_shade", RuntimePhysicalGlass3D_Shade(&scene, &incident, &boundary, &p, &sampling, 0,0,1,1,&result));
        transmitted_red += result.radianceR/16384;
        assert_close("water_emitter_transmitted_color", result.radianceG, .5*result.radianceR, 1e-12);
    }
    assert_close("water_emitter_transmission_expected", transmitted_red, 4*(1-fresnel)/(1.333*1.333), .002);
    assert_close("water_emitter_reflection_transmission_conserve", red + 1.333*1.333*transmitted_red, 4, 1e-9);
    printf("water emitter eta-corrected R+T: %.12g (expected 1)\n",(red+1.333*1.333*transmitted_red)/4);
    /* Total internal reflection gives a deterministic unit-energy optical path. */
    incident = RuntimeRay3D_Make(vec3(-.8,0,-.6), vec3(.8,0,.6));
    light.position = vec3(4,0,-3); light.normal = vec3(0,0,1);
    scene.lightSet.lights[0] = light;
    RuntimeDisneyV2_3DResult result;
    assert_true("water_emitter_tir_shade", RuntimePhysicalGlass3D_Shade(&scene, &incident, &boundary, &p, NULL, 0,0,1,1,&result));
    assert_close("water_emitter_tir_red", result.radianceR, 4, 1e-10);
    assert_close("water_emitter_tir_green", result.radianceG, 2, 1e-10);
    assert_close("water_emitter_tir_blue", result.radianceB, 1, 1e-10);
    light.normal = vec3(0,0,-1); scene.lightSet.lights[0] = light;
    assert_true("water_emitter_back_shade", RuntimePhysicalGlass3D_Shade(&scene, &incident, &boundary, &p, NULL, 0,0,1,1,&result));
    assert_close("water_emitter_back_dark", result.radiance, 0, 1e-12);
    light.normal = vec3(0,0,1); scene.lightSet.lights[0] = light;
    scene.primitives = calloc(1,sizeof(*scene.primitives)); scene.primitiveCapacity=scene.primitiveCount=1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.triangleMesh.triangles = calloc(1,sizeof(*scene.triangleMesh.triangles));
    scene.triangleMesh.triangleCapacity=scene.triangleMesh.triangleCount=1;
    RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[0];
    triangle->p0=vec3(-20,-20,-1); triangle->p1=vec3(20,-20,-1); triangle->p2=vec3(0,20,-1);
    triangle->normal=vec3(0,0,1); triangle->primitiveIndex=0; triangle->sceneObjectIndex=0;
    assert_true("water_emitter_blocker_bvh",RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    RuntimePhysicalGlass3DContinuation continuation;
    assert_true("water_emitter_blocker_advance",RuntimePhysicalGlass3D_Advance(&scene,&incident,&boundary,&p,NULL,NULL,0,&continuation));
    assert_true("water_emitter_blocker_first", continuation.hit &&
        continuation.hitInfo.triangleIndex == 0 && !continuation.emitterWins);
    if (had_water) RuntimeWaterMaterial3D_Set(127, &previous_water);
    else RuntimeWaterMaterial3D_Clear(127);
    RuntimeScene3D_Free(&scene); animSettings=saved; sceneSettings=saved_scene;
    RuntimeRay3D_SetTraceRouteForTests(old_route);
}

static void test_runtime_clear_water_offspecular_light(void) {
    AnimationConfig saved=animSettings;
    animSettings.bounceDepth3D=animSettings.specularDepth3D=animSettings.transmissionDepth3D=8;
    animSettings.transmissionSamples3D=16;
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);
    scene.environment.backgroundIntensity=scene.environment.ambientIntensity=0;
    RuntimeLightSource3D light=emitter_contract_panel();
    light.position=vec3(2,0,2);light.width=light.height=.2;
    assert_true("offspec_water_light_append",RuntimeLightSet3D_Append(&scene.lightSet,&light,NULL));
    scene.hasLight=true;scene.light.position=light.position;scene.light.intensity=4;
    scene.light.falloffDistance=10;scene.light.falloffMode=FORWARD_FALLOFF_MODE_NONE;
    RuntimeWaterMaterial3DOverride prior,water={.valid=true,.ior=1.333,.absorptionDistance=1,.roughness=0};
    bool had=RuntimeWaterMaterial3D_Get(127,&prior);
    assert_true("offspec_water_register",RuntimeWaterMaterial3D_Set(127,&water));
    RuntimeMaterialPayload3D p={.valid=true,.materialId=MATERIAL_PRESET_TRANSPARENT};
    assert_true("offspec_water_apply",RuntimeWaterMaterial3D_ApplyToPayload(127,&p));
    scene.primitives=calloc(1,sizeof(*scene.primitives));
    scene.primitiveCount=scene.primitiveCapacity=1;
    scene.primitives[0].source.kind=RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.triangleMesh.triangles=calloc(1,sizeof(*scene.triangleMesh.triangles));
    scene.triangleMesh.triangleCount=scene.triangleMesh.triangleCapacity=1;
    RuntimeTriangle3D *plane=&scene.triangleMesh.triangles[0];
    plane->p0=vec3(-10,-10,0);plane->p1=vec3(10,-10,0);plane->p2=vec3(0,10,0);
    plane->normal=vec3(0,0,1);plane->primitiveIndex=0;plane->sceneObjectIndex=127;
    assert_true("offspec_water_plane_bvh",RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    RuntimePrimaryHit3DResult primary={0};primary.hit=true;
    primary.primaryRay=RuntimeRay3D_Make(vec3(0,0,1),vec3(0,0,-1));
    primary.primaryTransmittance=RuntimeVisibility3D_UnitTransmittance();
    primary.hitInfo.sceneObjectIndex=127;primary.hitInfo.triangleIndex=0;
    primary.hitInfo.normal=primary.hitInfo.shadingNormal=primary.hitInfo.geometricNormal=vec3(0,0,1);
    primary.hitInfo.position=vec3(0,0,0);primary.hitInfo.t=1;
    for(unsigned i=0;i<64;++i) {
        RuntimeNative3DSamplingContext sampling={.sampleSequence=19+i*7919,.temporalSubpassCount=1};
        RuntimeDisneyV2_3DResult result;
        assert_true("offspec_water_shade",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
            &scene,&primary,&p,&sampling,0,0,1,1,&result));
        assert_close("offspec_water_no_fabricated_light",result.radiance,0,0);
    }
    RuntimeMaterialPayload3D diffuse=p;diffuse.hasGlassTransportOverride=false;
    diffuse.transparency=0;diffuse.bsdf.opacity=1;diffuse.bsdf.diffuseWeight=1;
    diffuse.bsdf.specWeight=diffuse.bsdf.reflectivity=0;diffuse.bsdf.model=MATERIAL_BSDF_LAMBERT;
    RuntimeDirectLight3DResult control={0};
    assert_true("offspec_diffuse_control_shade",RuntimeDirectLight3D_ShadeHitWithPayload(
        &scene,&primary.hitInfo,&diffuse,NULL,&control));
    assert_true("offspec_light_does_illuminate_diffuse_plane",control.radiance>0);
    RuntimeMaterialPayload3D compatibility=p;compatibility.hasGlassTransportOverride=false;
    RuntimeDisneyV2_3DResult historical={0};
    if(RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
        &scene,&primary,&compatibility,NULL,0,0,1,1,&historical))
        printf("off-specular clear-water control: physical 0; compatibility %.12g (diagnostic, not an acceptance target)\n",historical.radiance);
    if(had)RuntimeWaterMaterial3D_Set(127,&prior);else RuntimeWaterMaterial3D_Clear(127);
    RuntimeScene3D_Free(&scene);animSettings=saved;
}
