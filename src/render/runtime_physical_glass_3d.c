#include "render/runtime_physical_glass_3d.h"
#include "render/runtime_disney_v2_atmosphere_3d.h"
#include "render/runtime_disney_v2_transmission_internal_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_path_termination_probe_3d.h"
#include "render/runtime_glass_path_termination_3d.h"
#include <limits.h>
#include "runtime_native_3d_render_internal.h"
#include <math.h>
#include <string.h>
/* Dynamic context is per render thread and restored before returning. It carries
 * the same medium into opaque receiver continuations, never scene/global state. */
typedef RuntimePhysicalGlass3DContext GlassContext;
typedef struct {
    bool hit, terminal_only, emitterWins, scattered, deterministicStraight;
    int visibility[5];
    RuntimeLightEmitterHit3DResult emitter;
    HitInfo3D receiver;
    Ray3D ray;
    Vec3 beta;
    RuntimeMaterialPayload3D payload;
    GlassContext context;
} PathProbe;
static _Thread_local const GlassContext *active_context;
const RuntimePhysicalGlass3DContext *
RuntimePhysicalGlass3D_SetContext(const RuntimePhysicalGlass3DContext *context) {
    const GlassContext *old = active_context;
    active_context = context;
    return old;
}
void RuntimePhysicalGlass3D_CurrentMedium(RuntimeCameraMedium3D *medium) {
    if (active_context)
        *medium = active_context->medium;
    else
        RuntimeCameraMedium3D_Init(medium);
}
static Vec3 mul(Vec3 a, Vec3 b) { return vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
static Vec3 radiance(RuntimeDisneyV2_3DResult r) {
    return vec3(r.radianceR, r.radianceG, r.radianceB);
}
static Vec3 receiver(const RuntimeScene3D *scene, Ray3D ray, HitInfo3D hit,
                     RuntimeMaterialPayload3D p, const RuntimeNative3DSamplingContext *sampling,
                     int x, int y, int w, int h, GlassContext *context, PathProbe *probe) {
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeDisneyV2_3DResult r = {0};
    primary.hit = true;
    primary.hitInfo = hit;
    primary.primaryRay = ray;
    primary.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    const GlassContext *saved = active_context;
    active_context = context;
    bool ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
        scene, &primary, &p, sampling, x, y, w, h, &r);
    active_context = saved;
    if (!ok) {
        RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_RECEIVER_FAILED, context->depth, vec3(1, 1, 1));
        return vec3(0, 0, 0);
    }
    probe->visibility[0] = r.directVisibilityOutcomeNoTraceCount;
    probe->visibility[1] = r.directVisibilityOutcomeClearVisibleCount;
    probe->visibility[2] = r.directVisibilityOutcomeClearBlockedCount;
    probe->visibility[3] = r.directVisibilityOutcomeStablePartialCount;
    probe->visibility[4] = r.directVisibilityOutcomeMixedPartialCount;
    runtime_native_3d_render_apply_ambient_hit_lighting(scene, &hit, &r.radianceR, &r.radianceG,
                                                        &r.radianceB, &r.radiance, &r.visible);
    return radiance(r);
}
static Vec3 path(const RuntimeScene3D *scene, Ray3D ray, HitInfo3D hit, RuntimeMaterialPayload3D p,
                 const RuntimeNative3DSamplingContext *sampling, int index, int count, int x, int y,
                 int w, int h, GlassContext context, PathProbe *probe) {
    probe->deterministicStraight = true;
    Vec3 beta = vec3(1, 1, 1), sum = vec3(0, 0, 0);
    double previous = 0;
    double eta_scale = 1;
    unsigned straight_boundaries = 0;
    const uint32_t path_seed = runtime_disney_v2_3d_transmission_seed_from_hit(&hit, sampling);
    bool segment_start = true;
    RuntimePathDepthPolicy3D policy = RuntimePathDepthPolicy3D_Resolve();
    RuntimePathTermination3DReason exit_reason = RUNTIME_PATH_EXIT_BOUNDARY_SAFETY;
    RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_GLASS_START, context.depth, beta);
    for (uint32_t boundary = 0;; ++boundary) {
        /* Only malformed/non-scattering boundary chains have a short guard.
         * Actual scattering paths terminate stochastically, not at 128 hits. */
        if (boundary == UINT32_MAX || context.depth == INT_MAX || straight_boundaries >= 128)
            break;
        if (!p.hasGlassTransportOverride) {
            RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_RECEIVER, context.depth, beta);
            probe->hit = true;
            probe->receiver = hit;
            probe->ray = ray;
            probe->beta = beta;
            probe->payload = p;
            probe->context = context;
            return probe->terminal_only
                       ? sum
                       : vec3_add(sum, mul(beta, receiver(scene, ray, hit, p, sampling, x, y, w, h,
                                                          &context, probe)));
        }
        RuntimeDielectricTransport3D macro;
        if (!(segment_start
                  ? RuntimeCameraDielectric3D_ResolveSegmentStart(
                        &context.medium, &p, &hit, ray.direction, &macro)
                  : RuntimeCameraDielectric3D_Resolve(
                        &context.medium, &p, &hit, ray.direction, &macro))) {
            exit_reason = RUNTIME_PATH_EXIT_INVALID_MEDIUM;
            break;
        }
        Vec3 attenuation;
        if (!RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                RuntimeCausticPhotonMediumStack3D_Top(&context.medium), fmax(0, hit.t - previous),
                &attenuation)) {
            exit_reason = RUNTIME_PATH_EXIT_INVALID_ATTENUATION;
            break;
        }
        beta = mul(beta, attenuation);
        sum = vec3_add(
            sum, vec3_scale(mul(beta, vec3(p.baseColorR, p.baseColorG, p.baseColorB)), p.emissive));
        double u, v, choose, opaque;
        uint32_t seed = runtime_disney_v2_3d_transmission_seed_from_hit(&hit, sampling);
        RuntimeNative3DSampling_Stratified2D(sampling, seed, count, index,
                                             0x50470000u + (uint32_t)boundary * 2, &u, &v);
        RuntimeNative3DSampling_Stratified2D(
            sampling, seed, count, index, 0x50470001u + (uint32_t)boundary * 2, &choose, &opaque);
        if (opaque >= p.transparency) {
            probe->scattered = true;
            RuntimeMaterialPayload3D body = p;
            body.hasGlassTransportOverride = false;
            body.transparency = 0;
            body.emissive = 0;
            body.bsdf.opacity = 1;
            body.bsdf.diffuseWeight = 1;
            body.bsdf.specWeight = body.bsdf.reflectivity = 0;
            if (!RuntimePathDepthPolicy3D_AllowsDepth(
                    &policy, RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE, context.depth + 1)) {
                exit_reason = RUNTIME_PATH_EXIT_DIFFUSE_DEPTH;
                break;
            }
            ++context.depth;
            RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_RECEIVER, context.depth, beta);
            probe->hit = true;
            probe->receiver = hit;
            probe->ray = ray;
            probe->beta = beta;
            probe->payload = body;
            probe->context = context;
            return probe->terminal_only
                       ? sum
                       : vec3_add(sum, mul(beta, receiver(scene, ray, hit, body, sampling, x, y, w,
                                                          h, &context, probe)));
        }
        RuntimePhysicalGlass3DSample event;
        (void)RuntimeMaterialPayload3D_ApplyShadingNormal(&p, &hit);
        Vec3 shading = HitInfo3D_ShadingNormalForReflection(&hit, vec3_scale(ray.direction, -1));
        if (!RuntimePhysicalGlass3D_Sample(&p, shading, ray.direction, macro.etaFrom, macro.etaTo,
                                           u, v, choose, &event)) {
            exit_reason = RUNTIME_PATH_EXIT_SAMPLE_REJECTED;
            break;
        }
        double sides = vec3_dot(ray.direction, HitInfo3D_BoundaryNormal(&hit)) *
                       vec3_dot(event.direction, HitInfo3D_BoundaryNormal(&hit));
        if (event.reflected ? sides >= 0 : sides <= 0) {
            exit_reason = RUNTIME_PATH_EXIT_GEOMETRIC_SIDE;
            break;
        }
        if (!event.delta || event.reflected || event.pdf != 1 || p.transparency != 1)
            probe->deterministicStraight = false;
        beta = vec3_scale(beta, event.weight);
        if (!event.reflected) {
            Vec3 tint =
                p.hasGlassInterfaceTintOverride
                    ? vec3(p.glassInterfaceTintR, p.glassInterfaceTintG, p.glassInterfaceTintB)
                    : vec3(p.baseColorR, p.baseColorG, p.baseColorB);
            beta = mul(beta, tint);
            if (!p.thinWalled)
                eta_scale *= (macro.etaTo / macro.etaFrom) * (macro.etaTo / macro.etaFrom);
            macro.totalInternalReflection = false;
            if (!RuntimeCameraMedium3D_Transmit(&context.medium, &p, &hit, &macro)) {
                exit_reason = RUNTIME_PATH_EXIT_MEDIUM_TRANSITION;
                break;
            }
        }
        bool straight =
            event.delta && !event.reflected && (macro.etaFrom == macro.etaTo || p.thinWalled);
        double min_t;
        if (!straight) {
            RuntimePathDepthPolicy3DLobe lobe =
                event.reflected ? RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR
                                : RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION;
            int prefix = RuntimePathDepthPolicy3D_MaxDepthForLobe(&policy, lobe);
            if (prefix <= 0) {
                exit_reason = event.reflected ? RUNTIME_PATH_EXIT_SPECULAR_DEPTH
                                              : RUNTIME_PATH_EXIT_TRANSMISSION_DEPTH;
                break;
            }
            double survival = RuntimeGlassPathTermination3D_Survival(
                prefix, context.depth + 1, beta, eta_scale);
            if (survival < 1) {
                Vec3 before = beta;
                double sample = RuntimeGlassPathTermination3D_Sample(sampling, path_seed, index, boundary);
                if (!RuntimeGlassPathTermination3D_Apply(survival, sample, &beta)) {
                    exit_reason = RUNTIME_PATH_EXIT_GLASS_ROULETTE_TERMINATED;
                    break;
                }
                RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_GLASS_ROULETTE_SURVIVED,
                                                  context.depth + 1, before);
            }
            straight_boundaries = 0;
            probe->scattered = true;
            probe->deterministicStraight = false;
        }
        if (straight) {
            ++straight_boundaries;
            /* A real intersection and medium transition occurred. Preserve this
             * ray's exact parameterization/differentials for a delta straight event. */
            segment_start = false;
            previous = hit.t;
            min_t = nextafter(hit.t, INFINITY);
        } else {
            Ray3D next = RuntimeRay3D_MakeOffset(hit.position, HitInfo3D_OffsetNormal(&hit),
                                                 event.direction, 1e-6);
            if (event.delta)
                RuntimeRay3D_TransportIdealFootprint(&hit, event.interface.orientedNormal,
                                                     event.reflected ? RUNTIME_RAY_IDEAL_REFLECTION
                                                                     : RUNTIME_RAY_IDEAL_REFRACTION,
                                                     macro.etaFrom, macro.etaTo, &next);
            else {
                next.footprintUnbounded = true;
                next.footprintTransported = true;
                if (hit.hasIncidentDifferentials && hit.hasPixelFootprint &&
                    !hit.footprintUnbounded && hit.constantShadingNormal &&
                    !p.hasMicrodetailNormal) {
                    RuntimePhysicalGlass3DSample dx, dy;
                    if (RuntimePhysicalGlass3D_Sample(&p, shading, hit.incidentDirectionDx,
                                                      macro.etaFrom, macro.etaTo, u, v, choose,
                                                      &dx) &&
                        RuntimePhysicalGlass3D_Sample(&p, shading, hit.incidentDirectionDy,
                                                      macro.etaFrom, macro.etaTo, u, v, choose,
                                                      &dy) &&
                        dx.reflected == event.reflected && dy.reflected == event.reflected) {
                        next.directionDx = dx.direction;
                        next.directionDy = dy.direction;
                        next.originDx = vec3_add(next.origin, hit.pixelDpDx);
                        next.originDy = vec3_add(next.origin, hit.pixelDpDy);
                        next.hasDifferentials = next.hasDifferentialOrigins = true;
                        next.footprintUnbounded = false;
                    }
                }
            }
            segment_start = true;
            ray = next;
            previous = 0;
            min_t = 0;
            ++context.depth;
        }
        RuntimeLightEmitterTrace3DResult next = {0};
        RuntimeLightEmitter3D_ResolveFirstHit(scene, &ray, min_t, HUGE_VAL, &next);
        double distance = next.emitterWins   ? next.emitterHitInfo.t - previous
                          : next.geometryHit ? next.geometryHitInfo.t - previous
                                             : RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE;
        Vec3 scattered = vec3(0, 0, 0);
        Ray3D atmosphere_ray = ray;
        atmosphere_ray.origin = vec3_add(ray.origin, vec3_scale(ray.direction, previous));
        RuntimeDisneyV2Atmosphere3D_Advance(scene, &atmosphere_ray, distance, sampling, &beta,
                                            &scattered);
        sum = vec3_add(sum, scattered);
        if (next.emitterWins || !next.geometryHit) {
            Vec3 tr;
            if (!RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                    RuntimeCausticPhotonMediumStack3D_Top(&context.medium), distance, &tr)) {
                exit_reason = RUNTIME_PATH_EXIT_INVALID_ATTENUATION;
                break;
            }
            beta = mul(beta, tr);
            Vec3 light;
            if (next.emitterWins)
                light = vec3(next.emitterHitInfo.radiance, next.emitterHitInfo.radiance,
                             next.emitterHitInfo.radiance);
            else
                RuntimeEnvironment3D_EvaluateBackgroundRGB(&scene->environment, ray.direction,
                                                           &light.x, &light.y, &light.z);
            RuntimePathTermination3D_Record(next.emitterWins ? RUNTIME_PATH_EXIT_EMITTER
                                                            : RUNTIME_PATH_EXIT_ENVIRONMENT, context.depth, beta);
            if (probe->terminal_only) {
                probe->beta = beta;
                probe->ray = ray;
                probe->context = context;
                probe->emitterWins = next.emitterWins;
                probe->emitter = next.emitterHitInfo;
                return sum;
            }
            return vec3_add(sum, mul(beta, light));
        }
        hit = next.geometryHitInfo;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p)) {
            exit_reason = RUNTIME_PATH_EXIT_SAMPLE_REJECTED;
            break;
        }
        if (!p.hasGlassTransportOverride) {
            Vec3 tr;
            if (!RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                    RuntimeCausticPhotonMediumStack3D_Top(&context.medium),
                    fmax(0, hit.t - previous), &tr)) {
                exit_reason = RUNTIME_PATH_EXIT_INVALID_ATTENUATION;
                break;
            }
            beta = mul(beta, tr);
            RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_RECEIVER, context.depth, beta);
            probe->hit = true;
            probe->receiver = hit;
            probe->ray = ray;
            probe->beta = beta;
            probe->payload = p;
            probe->context = context;
            return probe->terminal_only
                       ? sum
                       : vec3_add(sum, mul(beta, receiver(scene, ray, hit, p, sampling, x, y, w, h,
                                                          &context, probe)));
        }
    }
    RuntimePathTermination3D_Record(exit_reason, context.depth, beta);
    return sum;
}
bool RuntimePhysicalGlass3D_Shade(const RuntimeScene3D *scene, const Ray3D *ray,
                                  const HitInfo3D *hit, const RuntimeMaterialPayload3D *p,
                                  const RuntimeNative3DSamplingContext *sampling, int x, int y,
                                  int w, int h, RuntimeDisneyV2_3DResult *out) {
    if (!scene || !ray || !hit || !p || !p->hasGlassTransportOverride || !out)
        return false;
    GlassContext context = {0};
    if (active_context)
        context = *active_context;
    else
        RuntimeCameraMedium3D_Init(&context.medium);
    int count = active_context ? 1 : animSettings.transmissionSamples3D;
    if (count < 1)
        count = 1;
    if (count > 64)
        count = 64;
    Vec3 total = vec3(0, 0, 0);
    int receivers = 0;
    int visibility[5] = {0};
    PathProbe selected = {0};
    for (int i = 0; i < count; ++i) {
        PathProbe probe = {0};
        total = vec3_add(
            total, path(scene, *ray, *hit, *p, sampling, i, count, x, y, w, h, context, &probe));
        for (int k = 0; k < 5; ++k)
            visibility[k] += probe.visibility[k];
        if (probe.hit) {
            ++receivers;
            selected = probe;
        }
    }
    total = vec3_scale(total, 1.0 / count);
    memset(out, 0, sizeof(*out));
    out->directVisibilityOutcomeNoTraceCount = visibility[0];
    out->directVisibilityOutcomeClearVisibleCount = visibility[1];
    out->directVisibilityOutcomeClearBlockedCount = visibility[2];
    out->directVisibilityOutcomeStablePartialCount = visibility[3];
    out->directVisibilityOutcomeMixedPartialCount = visibility[4];
    out->hit = true;
    out->hitInfo = *hit;
    out->primaryRay = *ray;
    out->payload = *p;
    out->payloadResolved = true;
    out->radianceR = total.x;
    out->radianceG = total.y;
    out->radianceB = total.z;
    out->radiance = fmax(total.x, fmax(total.y, total.z));
    out->visible = out->radiance > 0;
    out->primaryTransmissionContinued = true;
    out->primaryTransmissionPhysicalSurfaceCount = 1;
    out->primaryTransmissionSampleCount = count;
    out->primaryTransmissionRadianceR = total.x;
    out->primaryTransmissionRadianceG = total.y;
    out->primaryTransmissionRadianceB = total.z;
    out->primaryTransmissionReceiverRadiance = out->radiance;
    out->primaryTransmissionReceiverRadianceR = total.x;
    out->primaryTransmissionReceiverRadianceG = total.y;
    out->primaryTransmissionReceiverRadianceB = total.z;
    out->primaryTransmissionReceiverSampleCount = receivers;
    out->primaryTransmissionReceiverShadeCount = receivers;
    out->primaryTransmissionContributingSampleCount = receivers;
    if (selected.hit) {
        out->primaryTransmissionPathState.valid = true;
        out->primaryTransmissionPathState.hit = true;
        out->primaryTransmissionPathState.hitInfo = selected.receiver;
        out->primaryTransmissionPathState.ray = selected.ray;
        out->primaryTransmissionPathState.throughputR = selected.beta.x;
        out->primaryTransmissionPathState.throughputG = selected.beta.y;
        out->primaryTransmissionPathState.throughputB = selected.beta.z;
    }

    return true;
}

bool RuntimePhysicalGlass3D_Advance(const RuntimeScene3D *scene, const Ray3D *ray,
                                    const HitInfo3D *hit, const RuntimeMaterialPayload3D *p,
                                    const RuntimeNative3DSamplingContext *sampling,
                                    const RuntimeCameraMedium3D *medium, int depth,
                                    RuntimePhysicalGlass3DContinuation *out) {
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    if (!scene || !ray || !hit || !p)
        return false;
    GlassContext context = {0};
    if (medium)
        context.medium = *medium;
    else if (active_context)
        context = *active_context;
    else
        RuntimeCameraMedium3D_Init(&context.medium);
    if (depth > context.depth)
        context.depth = depth;
    PathProbe probe = {0};
    probe.terminal_only = true;
    out->radiance = path(scene, *ray, *hit, *p, sampling, 0, 1, -1, -1, 0, 0, context, &probe);
    out->deterministicStraight = probe.deterministicStraight;
    out->scattered = probe.scattered;
    out->hit = probe.hit;
    out->emitterWins = probe.emitterWins;
    out->emitter = probe.emitter;
    out->hitInfo = probe.receiver;
    out->ray = probe.ray;
    out->payload = probe.payload;
    out->medium = probe.context.medium;
    out->depth = probe.context.depth;
    out->throughput = probe.beta;
    return true;
}
