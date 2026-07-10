#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSMISSION_INTERNAL_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSMISSION_INTERNAL_3D_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

enum {
    RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_SKIP_CAP = 4,
    RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_DEPTH_CAP = 8,
    RUNTIME_DISNEY_V2_3D_MEDIUM_STACK_CAP = 8
};

static const double kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon = 1e-4;
static const double kRuntimeDisneyV2_3DPrimaryTransmissionMaxDistance = 1.0e6;
static const double kRuntimeDisneyV2_3DPrimaryTransmissionRoughConeScale = 0.18;
static const double kRuntimeDisneyV2_3DMinimumTransmittance = 1.0e-4;

typedef struct {
    double transmissionWeight;
    double visibleWeight;
    double tintR;
    double tintG;
    double tintB;
    bool thinWalled;
    bool physicalTransmission;
    bool alphaOnly;
} RuntimeDisneyV2_3DTransparentPolicy;

typedef struct {
    int objectStack[RUNTIME_DISNEY_V2_3D_MEDIUM_STACK_CAP];
    int depth;
    int maxDepth;
    int entryCount;
    int exitCount;
    int mismatchCount;
} RuntimeDisneyV2_3DMediumStackTracker;

static inline double runtime_disney_v2_transmission_3d_clamp(double value,
                                                             double min_value,
                                                             double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline double runtime_disney_v2_transmission_3d_clamp01(double value) {
    return runtime_disney_v2_transmission_3d_clamp(value, 0.0, 1.0);
}

static inline double runtime_disney_v2_transmission_3d_peak(double r,
                                                            double g,
                                                            double b) {
    return fmax(r, fmax(g, b));
}

bool runtime_disney_v2_3d_payload_has_transparent_alpha(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled);

int runtime_disney_v2_3d_resolve_transmission_sample_count(
    bool allow_recursive_receiver_shade);

bool runtime_disney_v2_3d_can_reuse_reflected_first_subpass_no_hit(
    RuntimeRenderTraceCostTransmissionSource3D ledger_source,
    RuntimeRenderTraceCostTransmissionPixelStability3D ledger_pixel_stability,
    int sample_index,
    RuntimeRenderTraceCostTransmissionTermination3D termination);

uint32_t runtime_disney_v2_3d_transmission_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling);

Vec3 runtime_disney_v2_3d_roughen_transmission_direction(
    Vec3 direction,
    double roughness,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int sample_count,
    int sample_index);

bool runtime_disney_v2_3d_payload_is_transparent(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled);

bool runtime_disney_v2_3d_policy_is_physical_transmission(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled);

RuntimeDisneyV2_3DTransparentPolicy runtime_disney_v2_3d_resolve_transparent_policy(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    double segment_distance);

RuntimeRenderTraceCostTransmissionSurfaceKind3D
runtime_disney_v2_3d_transmission_surface_kind(
    const RuntimeDisneyV2_3DTransparentPolicy* transparent_policy);

RuntimeRenderTraceCostTransmissionScreenRegion3D
runtime_disney_v2_3d_transmission_screen_region(const RuntimeDisneyV2_3DResult* result);

RuntimeRenderTraceCostTransmissionPixelStability3D
runtime_disney_v2_3d_transmission_pixel_stability(
    const RuntimeNative3DSamplingContext* sampling);

#endif
