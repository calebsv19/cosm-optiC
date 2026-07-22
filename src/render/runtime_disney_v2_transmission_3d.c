#include "render/runtime_disney_v2_transmission_internal_3d.h"

#include <math.h>

#include "config/config_manager.h"
#include "material/material.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

static double runtime_disney_v2_3d_direction_delta_degrees(Vec3 a, Vec3 b) {
    const double kRadiansToDegrees = 57.2957795130823208768;
    double dot = vec3_dot(vec3_normalize(a), vec3_normalize(b));
    dot = runtime_disney_v2_transmission_3d_clamp(dot, -1.0, 1.0);
    return acos(dot) * kRadiansToDegrees;
}

static double runtime_disney_v2_3d_payload_optical_ior(
    const RuntimeMaterialPayload3D* payload) {
    if (!payload || !payload->valid) return 0.0;
    if (payload->opticalIor >= 1.0) return payload->opticalIor;
    if (payload->bsdf.ior >= 1.0) return payload->bsdf.ior;
    return 0.0;
}

static void runtime_disney_v2_3d_medium_stack_init(
    RuntimeDisneyV2_3DMediumStackTracker* stack) {
    if (!stack) return;
    *stack = (RuntimeDisneyV2_3DMediumStackTracker){0};
    for (int i = 0; i < RUNTIME_DISNEY_V2_3D_MEDIUM_STACK_CAP; ++i) {
        stack->objectStack[i] = -1;
    }
}

static void runtime_disney_v2_3d_medium_stack_enter(
    RuntimeDisneyV2_3DMediumStackTracker* stack,
    int scene_object_index) {
    if (!stack || scene_object_index < 0) return;
    if (stack->depth >= RUNTIME_DISNEY_V2_3D_MEDIUM_STACK_CAP) {
        stack->mismatchCount += 1;
        return;
    }
    stack->objectStack[stack->depth] = scene_object_index;
    stack->depth += 1;
    stack->entryCount += 1;
    if (stack->depth > stack->maxDepth) {
        stack->maxDepth = stack->depth;
    }
}

static void runtime_disney_v2_3d_medium_stack_observe_solid_hit(
    RuntimeDisneyV2_3DMediumStackTracker* stack,
    int scene_object_index) {
    if (!stack || scene_object_index < 0) return;
    if (stack->depth > 0 &&
        stack->objectStack[stack->depth - 1] == scene_object_index) {
        stack->objectStack[stack->depth - 1] = -1;
        stack->depth -= 1;
        stack->exitCount += 1;
        return;
    }
    runtime_disney_v2_3d_medium_stack_enter(stack, scene_object_index);
}

static void runtime_disney_v2_3d_medium_stack_commit(
    RuntimeDisneyV2_3DResult* io_result,
    const RuntimeDisneyV2_3DMediumStackTracker* stack) {
    if (!io_result || !stack) return;
    io_result->primaryTransmissionMediumStackDepth = stack->depth;
    io_result->primaryTransmissionMaxMediumStackDepth = stack->maxDepth;
    io_result->primaryTransmissionMediumEntryCount = stack->entryCount;
    io_result->primaryTransmissionMediumExitCount = stack->exitCount;
    io_result->primaryTransmissionMediumMismatchCount =
        stack->mismatchCount + stack->depth;
}

static bool runtime_disney_v2_3d_record_solid_interior_return(
    RuntimeDisneyV2_3DResult* io_result,
    const RuntimeDisneyV2_3DTransparentPolicy* transparent_policy,
    const RuntimeDisneyV2_3DResult* layer_result,
    double throughput_r,
    double throughput_g,
    double throughput_b,
    double blend_weight) {
    double return_weight = 0.0;
    double return_r = 0.0;
    double return_g = 0.0;
    double return_b = 0.0;

    if (!io_result || !transparent_policy || !layer_result ||
        transparent_policy->thinWalled || !transparent_policy->physicalTransmission ||
        !layer_result->visible) {
        return false;
    }

    return_weight =
        runtime_disney_v2_transmission_3d_clamp01(
            transparent_policy->transmissionWeight *
            (1.0 - transparent_policy->visibleWeight)) *
        0.45;
    if (!(return_weight > 1e-9)) {
        return false;
    }

    return_r = layer_result->radianceR * throughput_r * blend_weight *
               transparent_policy->tintR * transparent_policy->tintR *
               return_weight;
    return_g = layer_result->radianceG * throughput_g * blend_weight *
               transparent_policy->tintG * transparent_policy->tintG *
               return_weight;
    return_b = layer_result->radianceB * throughput_b * blend_weight *
               transparent_policy->tintB * transparent_policy->tintB *
               return_weight;
    if (runtime_disney_v2_transmission_3d_peak(return_r, return_g, return_b) <= 1e-9) {
        return false;
    }

    io_result->primaryTransmissionInteriorReturnRadianceR += return_r;
    io_result->primaryTransmissionInteriorReturnRadianceG += return_g;
    io_result->primaryTransmissionInteriorReturnRadianceB += return_b;
    io_result->primaryTransmissionInteriorReturnSurfaceCount += 1;
    return true;
}

static bool runtime_disney_v2_3d_apply_ambient_receiver_fallback(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    RuntimeDirectLight3DResult* io_direct) {
    double intensity = 0.0;
    double bias = 0.0;
    double directional = 0.0;
    double up_facing = 0.0;
    double ambient_r = 0.0;
    double ambient_g = 0.0;
    double ambient_b = 0.0;
    double emissive_strength = 0.0;

    if (!scene || !hit || !payload || !payload->valid || !io_direct ||
        hit->triangleIndex < 0) {
        return false;
    }

    if (scene->environment.lightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT) {
        intensity = RuntimeEnvironment3D_AmbientStrength(&scene->environment);
        if (intensity > 0.0) {
            bias = runtime_disney_v2_transmission_3d_clamp01(scene->environment.topDownBias);
            up_facing = runtime_disney_v2_transmission_3d_clamp01(hit->normal.z);
            directional = (1.0 - bias) + (bias * up_facing);
            ambient_r = intensity * directional * scene->environment.ambientColor.x *
                        payload->baseColorR;
            ambient_g = intensity * directional * scene->environment.ambientColor.y *
                        payload->baseColorG;
            ambient_b = intensity * directional * scene->environment.ambientColor.z *
                        payload->baseColorB;
        }
    }

    emissive_strength = runtime_disney_v2_transmission_3d_clamp01(payload->emissive);
    ambient_r += payload->baseColorR * emissive_strength;
    ambient_g += payload->baseColorG * emissive_strength;
    ambient_b += payload->baseColorB * emissive_strength;
    if (runtime_disney_v2_transmission_3d_peak(ambient_r, ambient_g, ambient_b) <= 1e-9) {
        return false;
    }
    io_direct->radianceR += ambient_r;
    io_direct->radianceG += ambient_g;
    io_direct->radianceB += ambient_b;
    io_direct->radiance = runtime_disney_v2_transmission_3d_peak(io_direct->radianceR,
                                                                 io_direct->radianceG,
                                                                 io_direct->radianceB);
    io_direct->visible = true;
    return true;
}

bool RuntimeDisneyV2_3D_SampleTransmission(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const HitInfo3D* hit,
    Vec3 view_dir,
    double transmission_probability,
    RuntimeDisneyV2_3DTransmissionSample* out_sample) {
    RuntimeDisneyV2_3DTransmissionSample sample = {0};
    double transmission_weight = 0.0;
    double fresnel_transmission = 0.0;

    if (!payload || !principled || !principled->valid || !hit || !out_sample) return false;
    if (!(transmission_probability > 1e-9) || !(principled->transmissionWeight > 1e-9)) {
        *out_sample = sample;
        return false;
    }
    if (!RuntimeDielectricTransport3D_Resolve(payload,
                                              hit->normal,
                                              vec3_scale(vec3_normalize(view_dir), -1.0),
                                              &sample.dielectric) ||
        !sample.dielectric.hasRefraction) {
        *out_sample = sample;
        return false;
    }

    fresnel_transmission = 1.0 -
                           runtime_disney_v2_transmission_3d_clamp01(sample.dielectric.fresnel);
    transmission_weight =
        runtime_disney_v2_transmission_3d_clamp01(principled->transmissionWeight) *
        runtime_disney_v2_transmission_3d_clamp(fresnel_transmission, 0.05, 1.0);

    sample.direction = vec3_normalize(sample.dielectric.refractionDir);
    sample.pdf = fmax(transmission_probability, 1e-6);
    sample.throughputR =
        runtime_disney_v2_transmission_3d_clamp(
            (payload->hasGlassInterfaceTintOverride
                 ? payload->glassInterfaceTintR
                 : principled->baseColorR) *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.throughputG =
        runtime_disney_v2_transmission_3d_clamp(
            (payload->hasGlassInterfaceTintOverride
                 ? payload->glassInterfaceTintG
                 : principled->baseColorG) *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.throughputB =
        runtime_disney_v2_transmission_3d_clamp(
            (payload->hasGlassInterfaceTintOverride
                 ? payload->glassInterfaceTintB
                 : principled->baseColorB) *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.valid = vec3_length(sample.direction) > 1e-9 &&
                   (sample.throughputR > 1e-9 ||
                    sample.throughputG > 1e-9 ||
                    sample.throughputB > 1e-9);
    *out_sample = sample;
    return sample.valid;
}

static bool runtime_disney_v2_3d_trace_transmission_next_hit(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    Vec3 direction,
    RuntimeRenderTraceCostTransmissionSource3D ledger_source,
    int path_depth,
    HitInfo3D* out_hit,
    Ray3D* out_ray,
    int* out_ray_count,
    RuntimeRenderTraceCostTransmissionTermination3D* out_termination) {
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    double remaining_distance = kRuntimeDisneyV2_3DPrimaryTransmissionMaxDistance;
    int ray_count = 0;

    if (out_ray_count) *out_ray_count = 0;
    if (out_termination) {
        *out_termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;
    }
    if (!scene || !source_hit || !out_hit || !out_ray) return false;
    HitInfo3D_Reset(out_hit);
    ray = RuntimeRay3D_MakeOffset(source_hit->position,
                                  HitInfo3D_OffsetNormal(source_hit),
                                  direction,
                                  kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon);
    for (int skip_count = 0;
         skip_count <= RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_SKIP_CAP &&
         remaining_distance > kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon;
         ++skip_count) {
        HitInfo3D_Reset(&hit);
        ray_count += 1;
        RuntimeRenderTraceCostLedger3D_RecordTransmissionRayAtDepth(
            ledger_source,
            path_depth + skip_count);
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &ray,
                                             kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon,
                                             remaining_distance,
                                             &hit)) {
            if (out_ray_count) *out_ray_count = ray_count;
            if (out_termination) {
                *out_termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_HIT;
            }
            *out_ray = ray;
            return false;
        }
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&hit);
        if (hit.sceneObjectIndex != source_hit->sceneObjectIndex ||
            hit.triangleIndex != source_hit->triangleIndex) {
            *out_hit = hit;
            *out_ray = ray;
            if (out_ray_count) *out_ray_count = ray_count;
            return true;
        }
        remaining_distance -= hit.t;
        ray = RuntimeRay3D_MakeOffset(hit.position,
                                      HitInfo3D_OffsetNormal(&hit),
                                      direction,
                                      kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon);
    }
    if (out_ray_count) *out_ray_count = ray_count;
    if (out_termination) {
        *out_termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_SKIP_LIMIT;
    }
    *out_ray = ray;
    return false;
}

static bool runtime_disney_v2_3d_trace_primary_transmission_receiver(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimeDisneyV2_3DTransmissionSample* initial_sample,
    Vec3 sample_direction,
    double blend_weight,
    RuntimeDisneyV2_3DResult* io_result,
    RuntimeDirectLight3DResult* out_direct,
    HitInfo3D* out_hit,
    Ray3D* out_ray,
    double* out_throughput_r,
    double* out_throughput_g,
    double* out_throughput_b,
    int* out_depth,
    bool* out_receiver_found,
    RuntimeRenderTraceCostTransmissionSource3D ledger_source,
    int ledger_sample_index,
    double ledger_direction_alignment,
    RuntimeRenderTraceCostTransmissionScreenRegion3D ledger_screen_region,
    RuntimeRenderTraceCostTransmissionPixelStability3D ledger_pixel_stability,
    RuntimeRenderTraceCostTransmissionTermination3D* out_termination,
    RuntimeDisneyV2_3DTransmissionContinuationMode continuation_mode) {
    HitInfo3D source_hit = {0};
    HitInfo3D hit = {0};
    Ray3D ray = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D principled = {0};
    RuntimeDirectLight3DResult direct = {0};
    RuntimeDisneyV2_3DResult receiver_result = {0};
    RuntimeDisneyV2_3DMediumStackTracker medium_stack = {0};
    Vec3 direction = sample_direction;
    double throughput_r = initial_sample ? initial_sample->throughputR : 0.0;
    double throughput_g = initial_sample ? initial_sample->throughputG : 0.0;
    double throughput_b = initial_sample ? initial_sample->throughputB : 0.0;
    double accum_r = 0.0;
    double accum_g = 0.0;
    double accum_b = 0.0;
    int depth = 1;
    int sample_ray_total = 0;
    int transparent_surface_count = 0;
    RuntimeRenderTraceCostTransmissionTermination3D termination =
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;
    bool contributed = false;
    bool receiver_found = false;
    bool allow_recursive_receiver_shade =
        continuation_mode == RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_PRIMARY;

    if (!scene || !primary_hit || !primary_hit->hit || !initial_sample || !io_result ||
        !out_direct || !out_hit || !out_ray || !out_throughput_r || !out_throughput_g ||
        !out_throughput_b || !out_depth || !out_receiver_found) {
        return false;
    }

    *out_receiver_found = false;
    if (out_termination) {
        *out_termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;
    }
    source_hit = primary_hit->hitInfo;
    runtime_disney_v2_3d_medium_stack_init(&medium_stack);
    if (runtime_disney_v2_3d_policy_is_physical_transmission(&io_result->payload,
                                                             &io_result->principled) &&
        !io_result->payload.thinWalled) {
        runtime_disney_v2_3d_medium_stack_enter(&medium_stack,
                                                primary_hit->hitInfo.sceneObjectIndex);
        runtime_disney_v2_3d_medium_stack_commit(io_result, &medium_stack);
    }
    for (depth = 1; depth <= RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_DEPTH_CAP; ++depth) {
        int ray_count = 0;
        RuntimeRenderTraceCostTransmissionTermination3D trace_termination =
            RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                                  depth)) {
            io_result->primaryTransmissionDepthLimitCount += 1;
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_DEPTH_LIMIT;
            break;
        }
        if (!runtime_disney_v2_3d_trace_transmission_next_hit(scene,
                                                              &source_hit,
                                                              direction,
                                                              ledger_source,
                                                              depth,
                                                              &hit,
                                                              &ray,
                                                              &ray_count,
                                                              &trace_termination)) {
            io_result->primaryTransmissionRayCount += ray_count;
            sample_ray_total += ray_count;
            termination = trace_termination;
            break;
        }
        io_result->primaryTransmissionRayCount += ray_count;
        sample_ray_total += ray_count;
        io_result->primaryTransmissionHitCount += 1;

        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload)) {
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_POLICY_REJECT;
            RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
                ledger_source,
                termination,
                ledger_sample_index,
                ledger_direction_alignment,
                ledger_screen_region,
                ledger_pixel_stability,
                depth,
                sample_ray_total,
                transparent_surface_count,
                receiver_found,
                runtime_disney_v2_transmission_3d_peak(throughput_r,
                                                       throughput_g,
                                                       throughput_b),
                runtime_disney_v2_transmission_3d_peak(accum_r, accum_g, accum_b));
            if (out_termination) {
                *out_termination = termination;
            }
            return false;
        }
        principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
        if (!runtime_disney_v2_3d_payload_is_transparent(&payload, &principled)) {
            double contribution_r = 0.0;
            double contribution_g = 0.0;
            double contribution_b = 0.0;
            bool receiver_shaded = false;
            RuntimeRenderTraceCostLedger3D_RecordTransmissionSurface(
                ledger_source,
                RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_OPAQUE_RECEIVER,
                &hit);
            if (allow_recursive_receiver_shade &&
                RuntimeDisneyV2_3D_ShadeHitWithTraceContext(scene,
                                                            &hit,
                                                            sampling,
                                                            io_result->tracePixelX,
                                                            io_result->tracePixelY,
                                                            io_result->tracePixelWidth,
                                                            io_result->tracePixelHeight,
                                                            &receiver_result) &&
                receiver_result.visible &&
                runtime_disney_v2_transmission_3d_peak(receiver_result.radianceR,
                                                       receiver_result.radianceG,
                                                       receiver_result.radianceB) > 1e-9) {
                io_result->primaryTransmissionReceiverShadeCount += 1;
                io_result->primaryTransmissionReceiverRadianceR += receiver_result.radianceR;
                io_result->primaryTransmissionReceiverRadianceG += receiver_result.radianceG;
                io_result->primaryTransmissionReceiverRadianceB += receiver_result.radianceB;
                receiver_shaded = true;
            }
            if (!receiver_shaded) {
                if (!RuntimeDirectLight3D_ShadeHitWithPayload(scene,
                                                              &hit,
                                                              &payload,
                                                              sampling,
                                                              &direct)) {
                    return false;
                }
                receiver_result.radianceR = direct.radianceR;
                receiver_result.radianceG = direct.radianceG;
                receiver_result.radianceB = direct.radianceB;
                receiver_result.radiance = direct.radiance;
                if (receiver_result.radiance <= 1e-9 &&
                    runtime_disney_v2_3d_apply_ambient_receiver_fallback(scene,
                                                                         &hit,
                                                                         &payload,
                                                                         &direct)) {
                    receiver_result.radianceR = direct.radianceR;
                    receiver_result.radianceG = direct.radianceG;
                    receiver_result.radianceB = direct.radianceB;
                    receiver_result.radiance = direct.radiance;
                }
            }
            contribution_r = receiver_result.radianceR * throughput_r * blend_weight;
            contribution_g = receiver_result.radianceG * throughput_g * blend_weight;
            contribution_b = receiver_result.radianceB * throughput_b * blend_weight;
            RuntimeRenderTraceCostLedger3D_RecordTransmissionReceiverContribution(
                &hit,
                contribution_r,
                contribution_g,
                contribution_b);
            accum_r += contribution_r;
            accum_g += contribution_g;
            accum_b += contribution_b;
            contributed = runtime_disney_v2_transmission_3d_peak(accum_r, accum_g, accum_b) > 1e-9;
            receiver_found = true;
            *out_direct = direct;
            *out_hit = hit;
            *out_ray = ray;
            *out_throughput_r = throughput_r * blend_weight;
            *out_throughput_g = throughput_g * blend_weight;
            *out_throughput_b = throughput_b * blend_weight;
            *out_depth = depth;
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT;
            break;
        }

        io_result->primaryTransmissionTransparentSurfaceCount += 1;
        double segment_distance = vec3_length(vec3_sub(hit.position, source_hit.position));
        RuntimeDisneyV2_3DTransparentPolicy transparent_policy =
            runtime_disney_v2_3d_resolve_transparent_policy(&payload,
                                                            &principled,
                                                            segment_distance);
        RuntimeDielectricTransport3D interface_transport = {0};
        bool interface_transport_resolved = false;
        bool interface_entering = vec3_dot(direction, hit.normal) < 0.0;
        bool interface_direction_changed = false;
        double interface_angle_delta_deg = -1.0;
        RuntimeRenderTraceCostTransmissionSurfaceKind3D interface_surface_kind =
            runtime_disney_v2_3d_transmission_surface_kind(&transparent_policy);

        if (transparent_policy.physicalTransmission) {
            interface_transport_resolved =
                RuntimeDielectricTransport3D_Resolve(&payload,
                                                     hit.normal,
                                                     direction,
                                                     &interface_transport);
            if (interface_transport_resolved) {
                interface_entering = interface_transport.entering;
                if (interface_transport.hasRefraction) {
                    interface_angle_delta_deg =
                        runtime_disney_v2_3d_direction_delta_degrees(
                            direction,
                            interface_transport.refractionDir);
                    if (!transparent_policy.thinWalled &&
                        interface_angle_delta_deg > 1.0e-5) {
                        direction = interface_transport.refractionDir;
                        interface_direction_changed = true;
                    }
                }
            }
        }

        RuntimeRenderTraceCostLedger3D_RecordTransmissionSurface(
            ledger_source,
            interface_surface_kind,
            &hit);
        RuntimeRenderTraceCostLedger3D_RecordTransmissionInterface(
            ledger_source,
            interface_surface_kind,
            &hit,
            &payload,
            runtime_disney_v2_3d_payload_optical_ior(&payload),
            interface_entering,
            transparent_policy.thinWalled,
            transparent_policy.physicalTransmission,
            interface_angle_delta_deg,
            interface_direction_changed);
        transparent_surface_count += 1;
        if (transparent_policy.alphaOnly) {
            io_result->primaryTransmissionAlphaOnlySurfaceCount += 1;
        } else if (transparent_policy.thinWalled) {
            io_result->primaryTransmissionThinWalledSurfaceCount += 1;
        } else {
            io_result->primaryTransmissionSolidSurfaceCount += 1;
            if (transparent_policy.physicalTransmission) {
                runtime_disney_v2_3d_medium_stack_observe_solid_hit(
                    &medium_stack,
                    hit.sceneObjectIndex);
                runtime_disney_v2_3d_medium_stack_commit(io_result, &medium_stack);
            }
        }
        if (transparent_policy.physicalTransmission) {
            io_result->primaryTransmissionPhysicalSurfaceCount += 1;
        }
        if (!transparent_policy.physicalTransmission && allow_recursive_receiver_shade &&
            RuntimeDisneyV2_3D_ShadeHitWithTraceContext(scene,
                                                        &hit,
                                                        sampling,
                                                        io_result->tracePixelX,
                                                        io_result->tracePixelY,
                                                        io_result->tracePixelWidth,
                                                        io_result->tracePixelHeight,
                                                        &receiver_result) &&
            receiver_result.visible) {
            double layer_r = receiver_result.radianceR *
                             throughput_r * blend_weight * transparent_policy.visibleWeight;
            double layer_g = receiver_result.radianceG *
                             throughput_g * blend_weight * transparent_policy.visibleWeight;
            double layer_b = receiver_result.radianceB *
                             throughput_b * blend_weight * transparent_policy.visibleWeight;
            accum_r += layer_r;
            accum_g += layer_g;
            accum_b += layer_b;
            io_result->primaryTransmissionTransparentLayerShadeCount += 1;
            io_result->primaryTransmissionTransparentLayerRadianceR += layer_r;
            io_result->primaryTransmissionTransparentLayerRadianceG += layer_g;
            io_result->primaryTransmissionTransparentLayerRadianceB += layer_b;
            contributed = contributed ||
                          runtime_disney_v2_transmission_3d_peak(layer_r, layer_g, layer_b) > 1e-9;
            double interior_before_r =
                io_result->primaryTransmissionInteriorReturnRadianceR;
            double interior_before_g =
                io_result->primaryTransmissionInteriorReturnRadianceG;
            double interior_before_b =
                io_result->primaryTransmissionInteriorReturnRadianceB;
            if (runtime_disney_v2_3d_record_solid_interior_return(io_result,
                                                                  &transparent_policy,
                                                                  &receiver_result,
                                                                  throughput_r,
                                                                  throughput_g,
                                                                  throughput_b,
                                                                  blend_weight)) {
                accum_r += io_result->primaryTransmissionInteriorReturnRadianceR -
                           interior_before_r;
                accum_g += io_result->primaryTransmissionInteriorReturnRadianceG -
                           interior_before_g;
                accum_b += io_result->primaryTransmissionInteriorReturnRadianceB -
                           interior_before_b;
                io_result->primaryTransmissionInteriorReturnSampleCount += 1;
                contributed = true;
            }
        }

        throughput_r *= transparent_policy.tintR * transparent_policy.transmissionWeight;
        throughput_g *= transparent_policy.tintG * transparent_policy.transmissionWeight;
        throughput_b *= transparent_policy.tintB * transparent_policy.transmissionWeight;

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                                  depth + 1)) {
            io_result->primaryTransmissionDepthLimitCount += 1;
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_DEPTH_LIMIT;
            break;
        }
        source_hit = hit;
    }

    if (!contributed) {
        if (depth > RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_DEPTH_CAP) {
            io_result->primaryTransmissionDepthLimitCount += 1;
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_DEPTH_LIMIT;
        } else if (termination == RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN) {
            termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_CONTRIBUTION;
        }
        RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
            ledger_source,
            termination,
            ledger_sample_index,
            ledger_direction_alignment,
            ledger_screen_region,
            ledger_pixel_stability,
            depth,
            sample_ray_total,
            transparent_surface_count,
            receiver_found,
            runtime_disney_v2_transmission_3d_peak(throughput_r, throughput_g, throughput_b),
            runtime_disney_v2_transmission_3d_peak(accum_r, accum_g, accum_b));
        if (out_termination) {
            *out_termination = termination;
        }
        return false;
    }

    direct.visible = true;
    direct.radianceR = accum_r;
    direct.radianceG = accum_g;
    direct.radianceB = accum_b;
    direct.radiance = runtime_disney_v2_transmission_3d_peak(accum_r, accum_g, accum_b);
    *out_direct = direct;
    *out_hit = hit;
    *out_ray = ray;
    *out_throughput_r = throughput_r * blend_weight;
    *out_throughput_g = throughput_g * blend_weight;
    *out_throughput_b = throughput_b * blend_weight;
    *out_depth = depth;
    *out_receiver_found = receiver_found;
    if (termination == RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN) {
        termination = receiver_found
                          ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT
                          : RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_CONTRIBUTION;
    }
    RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
        ledger_source,
        termination,
        ledger_sample_index,
        ledger_direction_alignment,
        ledger_screen_region,
        ledger_pixel_stability,
        depth,
        sample_ray_total,
        transparent_surface_count,
        receiver_found,
        runtime_disney_v2_transmission_3d_peak(throughput_r, throughput_g, throughput_b),
        runtime_disney_v2_transmission_3d_peak(accum_r, accum_g, accum_b));
    if (out_termination) {
        *out_termination = termination;
    }
    return true;
}

static bool runtime_disney_v2_3d_apply_transmission_continuation(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result,
    RuntimeDisneyV2_3DTransmissionContinuationMode continuation_mode) {
    RuntimeDisneyV2_3DTransmissionSample sample = {0};
    RuntimeDisneyV2_3DTransmissionSample sample_path = {0};
    HitInfo3D continuation_hit = {0};
    RuntimeDirectLight3DResult continuation_direct = {0};
    Ray3D continuation_ray = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 1.0);
    double fresnel_transmission = 0.0;
    double blend_weight = 0.0;
    double front_body_weight = 1.0;
    double front_diffuse_weight = 1.0;
    double front_specular_weight = 1.0;
    double accum_r = 0.0;
    double accum_g = 0.0;
    double accum_b = 0.0;
    double accum_throughput_r = 0.0;
    double accum_throughput_g = 0.0;
    double accum_throughput_b = 0.0;
    double path_throughput_r = 0.0;
    double path_throughput_g = 0.0;
    double path_throughput_b = 0.0;
    double best_throughput_r = 0.0;
    double best_throughput_g = 0.0;
    double best_throughput_b = 0.0;
    int sample_count = 1;
    int contributing_sample_count = 0;
    int receiver_sample_count = 0;
    int receiver_depth = 0;
    int best_depth = 0;
    uint32_t sample_seed = 0U;
    HitInfo3D best_hit = {0};
    Ray3D best_ray = {0};
    bool physical_front_transmission = false;
    RuntimeRenderTraceCostTransmissionSource3D ledger_source =
        continuation_mode == RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED
            ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED
            : RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY;

    if (!scene || !primary_hit || !primary_hit->hit || !io_result ||
        !io_result->payloadResolved || !io_result->principled.valid) {
        return false;
    }
    if (!(io_result->transmissionProbability > 1e-9) ||
        !(io_result->principled.transmissionWeight > 1e-9)) {
        return false;
    }
    if (!io_result->pathPolicyResolved ||
        !RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                              RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                              1)) {
        return false;
    }

    view_dir = vec3_scale(primary_hit->primaryRay.direction, -1.0);
    if (!RuntimeDisneyV2_3D_SampleTransmission(&io_result->payload,
                                               &io_result->principled,
                                               &primary_hit->hitInfo,
                                               view_dir,
                                               io_result->transmissionProbability,
                                               &sample)) {
        return false;
    }

    fresnel_transmission =
        1.0 - runtime_disney_v2_transmission_3d_clamp01(sample.dielectric.fresnel);
    blend_weight =
        runtime_disney_v2_transmission_3d_clamp01(io_result->principled.transmissionWeight) *
        runtime_disney_v2_transmission_3d_clamp(fresnel_transmission, 0.05, 1.0);
    front_diffuse_weight =
        runtime_disney_v2_transmission_3d_clamp((1.0 - blend_weight) *
                                                    (1.0 - blend_weight),
                                                0.015,
                                                1.0);
    front_specular_weight =
        runtime_disney_v2_transmission_3d_clamp(front_diffuse_weight +
                                                    (sample.dielectric.fresnel * 0.80),
                                                0.05,
                                                0.65);
    physical_front_transmission =
        runtime_disney_v2_3d_policy_is_physical_transmission(&io_result->payload,
                                                             &io_result->principled);
    front_body_weight = physical_front_transmission ? 0.0 : front_diffuse_weight;
    if (physical_front_transmission) {
        io_result->primaryTransmissionPhysicalSurfaceCount += 1;
        front_specular_weight =
            runtime_disney_v2_transmission_3d_clamp(sample.dielectric.fresnel,
                                                    0.0,
                                                    0.35);
    }
    sample_count =
        runtime_disney_v2_3d_resolve_transmission_sample_count(
            continuation_mode);
    sample_seed = runtime_disney_v2_3d_transmission_seed_from_hit(&primary_hit->hitInfo,
                                                                  sampling);
    io_result->primaryTransmissionSampleCount = sample_count;
    RuntimeRenderTraceCostTransmissionScreenRegion3D ledger_screen_region =
        runtime_disney_v2_3d_transmission_screen_region(io_result);
    RuntimeRenderTraceCostTransmissionPixelStability3D ledger_pixel_stability =
        runtime_disney_v2_3d_transmission_pixel_stability(sampling);
    RuntimeRenderTraceCostLedger3D_RecordTransmissionPathEvaluation(ledger_source,
                                                                    sample_count);
    RuntimeRenderTraceCostLedger3D_RecordTransmissionInterface(
        ledger_source,
        physical_front_transmission
            ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_PHYSICAL
            : (io_result->payload.thinWalled
                   ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_THIN_WALLED
                   : RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_NONPHYSICAL),
        &primary_hit->hitInfo,
        &io_result->payload,
        runtime_disney_v2_3d_payload_optical_ior(&io_result->payload),
        sample.dielectric.entering,
        io_result->payload.thinWalled,
        physical_front_transmission,
        runtime_disney_v2_3d_direction_delta_degrees(sample.dielectric.incidentDir,
                                                     sample.dielectric.refractionDir),
        runtime_disney_v2_3d_direction_delta_degrees(sample.dielectric.incidentDir,
                                                     sample.dielectric.refractionDir) > 1.0e-5);
    bool reflected_first_subpass_no_hit_reuse = false;

    for (int sample_index = 0; sample_index < sample_count; ++sample_index) {
        RuntimeRenderTraceCostTransmissionTermination3D sample_termination =
            RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;
        Vec3 sample_direction = runtime_disney_v2_3d_roughen_transmission_direction(
            sample.direction,
            io_result->principled.roughness,
            sampling,
            sample_seed,
            sample_count,
            sample_index);
        double sample_direction_alignment =
            vec3_dot(vec3_normalize(sample.direction), vec3_normalize(sample_direction));

        sample_path = sample;
        sample_path.direction = sample_direction;
        continuation_direct = (RuntimeDirectLight3DResult){0};
        continuation_hit = (HitInfo3D){0};
        continuation_ray = (Ray3D){0};
        path_throughput_r = 0.0;
        path_throughput_g = 0.0;
        path_throughput_b = 0.0;
        receiver_depth = 0;
        bool receiver_found = false;
        if (reflected_first_subpass_no_hit_reuse) {
            RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
                ledger_source,
                RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_POLICY_REJECT,
                sample_index,
                sample_direction_alignment,
                ledger_screen_region,
                ledger_pixel_stability,
                1,
                0,
                0,
                false,
                0.0,
                0.0);
            continue;
        }
        if (!runtime_disney_v2_3d_trace_primary_transmission_receiver(scene,
                                                                      primary_hit,
                                                                      sampling,
                                                                      &sample_path,
                                                                      sample_direction,
                                                                      blend_weight,
                                                                      io_result,
                                                                      &continuation_direct,
                                                                      &continuation_hit,
                                                                      &continuation_ray,
                                                                      &path_throughput_r,
                                                                      &path_throughput_g,
                                                                      &path_throughput_b,
                                                                      &receiver_depth,
                                                                      &receiver_found,
                                                                      ledger_source,
                                                                      sample_index,
                                                                      sample_direction_alignment,
                                                                      ledger_screen_region,
                                                                      ledger_pixel_stability,
                                                                      &sample_termination,
                                                                      continuation_mode)) {
            if (runtime_disney_v2_3d_can_reuse_reflected_first_subpass_no_hit(
                    ledger_source,
                    ledger_pixel_stability,
                    sample_index,
                    sample_termination)) {
                reflected_first_subpass_no_hit_reuse = true;
            }
            continue;
        }
        accum_r += continuation_direct.radianceR;
        accum_g += continuation_direct.radianceG;
        accum_b += continuation_direct.radianceB;
        accum_throughput_r += path_throughput_r;
        accum_throughput_g += path_throughput_g;
        accum_throughput_b += path_throughput_b;
        contributing_sample_count += 1;
        if (receiver_found) {
            receiver_sample_count += 1;
        }
        best_depth = receiver_depth;
        best_hit = continuation_hit;
        best_ray = continuation_ray;
        best_throughput_r = path_throughput_r;
        best_throughput_g = path_throughput_g;
        best_throughput_b = path_throughput_b;
    }

    if (contributing_sample_count <= 0) {
        return false;
    }

    io_result->directRadianceR *= front_body_weight;
    io_result->directRadianceG *= front_body_weight;
    io_result->directRadianceB *= front_body_weight;
    io_result->diffuseRadianceR *= front_body_weight;
    io_result->diffuseRadianceG *= front_body_weight;
    io_result->diffuseRadianceB *= front_body_weight;
    io_result->specularRadianceR *= front_specular_weight;
    io_result->specularRadianceG *= front_specular_weight;
    io_result->specularRadianceB *= front_specular_weight;
    io_result->transmissionRadianceR *= front_body_weight;
    io_result->transmissionRadianceG *= front_body_weight;
    io_result->transmissionRadianceB *= front_body_weight;
    io_result->stochasticDirectRadianceR *= front_body_weight;
    io_result->stochasticDirectRadianceG *= front_body_weight;
    io_result->stochasticDirectRadianceB *= front_body_weight;
    io_result->stochasticBsdfRadianceR *= front_body_weight;
    io_result->stochasticBsdfRadianceG *= front_body_weight;
    io_result->stochasticBsdfRadianceB *= front_body_weight;
    io_result->recursiveBsdfRadianceR *= front_body_weight;
    io_result->recursiveBsdfRadianceG *= front_body_weight;
    io_result->recursiveBsdfRadianceB *= front_body_weight;

    io_result->primaryTransmissionRadianceR = accum_r / (double)contributing_sample_count;
    io_result->primaryTransmissionRadianceG = accum_g / (double)contributing_sample_count;
    io_result->primaryTransmissionRadianceB = accum_b / (double)contributing_sample_count;
    io_result->primaryTransmissionRadiance =
        fmax(io_result->primaryTransmissionRadianceR,
             fmax(io_result->primaryTransmissionRadianceG,
                  io_result->primaryTransmissionRadianceB));
    io_result->primaryTransmissionSurfaceWeight = front_diffuse_weight;
    io_result->primaryTransmissionBlendWeight = blend_weight;
    if (io_result->primaryTransmissionReceiverShadeCount > 0) {
        double receiver_count = (double)io_result->primaryTransmissionReceiverShadeCount;
        io_result->primaryTransmissionReceiverRadianceR /= receiver_count;
        io_result->primaryTransmissionReceiverRadianceG /= receiver_count;
        io_result->primaryTransmissionReceiverRadianceB /= receiver_count;
        io_result->primaryTransmissionReceiverRadiance =
            fmax(io_result->primaryTransmissionReceiverRadianceR,
                 fmax(io_result->primaryTransmissionReceiverRadianceG,
                      io_result->primaryTransmissionReceiverRadianceB));
    }
    if (io_result->primaryTransmissionTransparentLayerShadeCount > 0) {
        double layer_count =
            (double)io_result->primaryTransmissionTransparentLayerShadeCount;
        io_result->primaryTransmissionTransparentLayerRadianceR /= layer_count;
        io_result->primaryTransmissionTransparentLayerRadianceG /= layer_count;
        io_result->primaryTransmissionTransparentLayerRadianceB /= layer_count;
    }
    if (io_result->primaryTransmissionInteriorReturnSampleCount > 0) {
        double interior_count =
            (double)io_result->primaryTransmissionInteriorReturnSampleCount;
        io_result->primaryTransmissionInteriorReturnRadianceR /= interior_count;
        io_result->primaryTransmissionInteriorReturnRadianceG /= interior_count;
        io_result->primaryTransmissionInteriorReturnRadianceB /= interior_count;
    }
    io_result->primaryTransmissionFrontDiffuseWeight = front_diffuse_weight;
    io_result->primaryTransmissionFrontSpecularWeight = front_specular_weight;
    io_result->primaryTransmissionCameraThroughputR =
        accum_throughput_r / (double)contributing_sample_count;
    io_result->primaryTransmissionCameraThroughputG =
        accum_throughput_g / (double)contributing_sample_count;
    io_result->primaryTransmissionCameraThroughputB =
        accum_throughput_b / (double)contributing_sample_count;
    io_result->primaryTransmissionContributingSampleCount = contributing_sample_count;
    io_result->primaryTransmissionReceiverSampleCount = receiver_sample_count;
    io_result->primaryTransmissionTransparentLayerRadiance =
        fmax(io_result->primaryTransmissionTransparentLayerRadianceR,
             fmax(io_result->primaryTransmissionTransparentLayerRadianceG,
                  io_result->primaryTransmissionTransparentLayerRadianceB));
    io_result->primaryTransmissionInteriorReturnRadiance =
        fmax(io_result->primaryTransmissionInteriorReturnRadianceR,
             fmax(io_result->primaryTransmissionInteriorReturnRadianceG,
                  io_result->primaryTransmissionInteriorReturnRadianceB));
    io_result->primaryTransmissionContinued = true;
    io_result->primaryTransmissionPathState.valid = true;
    io_result->primaryTransmissionPathState.hit = true;
    io_result->primaryTransmissionPathState.depth = best_depth;
    io_result->primaryTransmissionPathState.sampledLobe =
        RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
    io_result->primaryTransmissionPathState.ray = best_ray;
    io_result->primaryTransmissionPathState.hitInfo = best_hit;
    io_result->primaryTransmissionPathState.throughputR = best_throughput_r;
    io_result->primaryTransmissionPathState.throughputG = best_throughput_g;
    io_result->primaryTransmissionPathState.throughputB = best_throughput_b;
    io_result->primaryTransmissionPathState.pdf = sample.pdf;
    return true;
}

bool RuntimeDisneyV2_3D_ApplyPrimaryTransmissionContinuation(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result) {
    return runtime_disney_v2_3d_apply_transmission_continuation(scene,
                                                               primary_hit,
                                                               sampling,
                                                               io_result,
                                                               RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_PRIMARY);
}

bool RuntimeDisneyV2_3D_ApplyReflectedTransmissionContinuation(
    const RuntimeScene3D* scene,
    const HitInfo3D* reflected_hit,
    Ray3D reflected_ray,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeDisneyV2_3DResult transmission_result = {0};
    RuntimePrimaryHit3DResult reflected_primary = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D principled = {0};
    double contribution_r = 0.0;
    double contribution_g = 0.0;
    double contribution_b = 0.0;

    if (!scene || !reflected_hit || !io_result ||
        !(runtime_disney_v2_transmission_3d_peak(parent_throughput_r,
                                                parent_throughput_g,
                                                parent_throughput_b) > 1e-9)) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(reflected_hit, &payload) ||
        !payload.valid) {
        return false;
    }
    principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    if (!runtime_disney_v2_3d_payload_is_transparent(&payload, &principled)) {
        return false;
    }

    transmission_result.payload = payload;
    transmission_result.payloadResolved = true;
    transmission_result.principled = principled;
    transmission_result.transmissionProbability =
        runtime_disney_v2_transmission_3d_clamp01(principled.transmissionWeight);
    transmission_result.pathPolicy = io_result->pathPolicy;
    transmission_result.pathPolicyResolved = io_result->pathPolicyResolved;
    transmission_result.tracePixelContextResolved = io_result->tracePixelContextResolved;
    transmission_result.tracePixelX = io_result->tracePixelX;
    transmission_result.tracePixelY = io_result->tracePixelY;
    transmission_result.tracePixelWidth = io_result->tracePixelWidth;
    transmission_result.tracePixelHeight = io_result->tracePixelHeight;

    reflected_primary.hit = true;
    reflected_primary.primaryRay = reflected_ray;
    reflected_primary.hitInfo = *reflected_hit;
    if (!runtime_disney_v2_3d_apply_transmission_continuation(scene,
                                                              &reflected_primary,
                                                              sampling,
                                                              &transmission_result,
                                                              RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED)) {
        return false;
    }

    contribution_r = transmission_result.primaryTransmissionRadianceR * parent_throughput_r;
    contribution_g = transmission_result.primaryTransmissionRadianceG * parent_throughput_g;
    contribution_b = transmission_result.primaryTransmissionRadianceB * parent_throughput_b;
    if (!(runtime_disney_v2_transmission_3d_peak(contribution_r,
                                                contribution_g,
                                                contribution_b) > 1e-9)) {
        return false;
    }

    io_result->specularReflectionRecursiveRadianceR += contribution_r;
    io_result->specularReflectionRecursiveRadianceG += contribution_g;
    io_result->specularReflectionRecursiveRadianceB += contribution_b;
    io_result->specularReflectionRadianceR += contribution_r;
    io_result->specularReflectionRadianceG += contribution_g;
    io_result->specularReflectionRadianceB += contribution_b;
    io_result->recursiveBsdfRadianceR += contribution_r;
    io_result->recursiveBsdfRadianceG += contribution_g;
    io_result->recursiveBsdfRadianceB += contribution_b;
    io_result->specularReflectionRecursiveRayCount +=
        transmission_result.primaryTransmissionRayCount;
    io_result->specularReflectionRecursiveGeometryHitCount +=
        transmission_result.primaryTransmissionHitCount;
    io_result->specularReflectionRecursiveContributingHitCount +=
        transmission_result.primaryTransmissionContributingSampleCount;
    io_result->specularReflectionRecursivePolicyTerminationCount +=
        transmission_result.primaryTransmissionDepthLimitCount;
    return true;
}
