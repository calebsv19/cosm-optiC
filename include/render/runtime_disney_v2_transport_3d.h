#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSPORT_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSPORT_3D_H

#include <stdbool.h>

#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_emissive_direct_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_specular_reflection_3d.h"

bool RuntimeDisneyV2_3D_AccumulateEmissiveMaterialHit(
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const Ray3D* ray,
    double throughput_r,
    double throughput_g,
    double throughput_b,
    double mis_weight_bsdf,
    int vertex_index,
    bool recursive_channel,
    RuntimeDisneyV2_3DPathState* io_state,
    double* out_contribution_r,
    double* out_contribution_g,
    double* out_contribution_b,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_EvaluateEmissiveAreaLightSample(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissiveDirect3DResult* out_area_sample);

bool RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(
    const RuntimeScene3D* scene,
    bool recursive_channel,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_AccumulateEmissiveAreaLightSample(
    const RuntimeEmissiveDirect3DResult* area_sample,
    double throughput_r,
    double throughput_g,
    double throughput_b,
    double mis_weight_light,
    int vertex_index,
    bool recursive_channel,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoop(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoopFromDirection(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 incoming_dir,
    int start_depth,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_ApplySpecularReflectionRecursion(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    const RuntimeSpecularReflection3DResult* reflection,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result);

#endif
