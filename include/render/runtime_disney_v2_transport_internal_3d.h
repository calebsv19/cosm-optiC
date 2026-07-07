#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSPORT_INTERNAL_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSPORT_INTERNAL_3D_H

#include <stdint.h>

#include "render/runtime_disney_v2_transport_3d.h"
#include "render/runtime_disney_v2_estimator_3d.h"
#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

#define kRuntimeDisneyV2Transport3DEpsilon 1e-4
#define kRuntimeDisneyV2Transport3DMaxDistance 48.0
#define kRuntimeDisneyV2Transport3DNegligibleLuma 1e-9
#define kRuntimeDisneyV2EmissiveAreaRecursiveCandidateCap 16
#define kRuntimeDisneyV2EmissiveAreaRecursiveTriangleCap 8192

typedef struct RuntimeDisneyV2Transport3DVertexSample {
    RuntimeDisneyV2_3DDominantLobe lobe;
    RuntimePathDepthPolicy3DLobe policyLobe;
    Vec3 direction;
    double throughputR;
    double throughputG;
    double throughputB;
    double pdf;
    double cosTheta;
    double lightPdf;
    double directBsdfPdf;
    double misWeightLight;
    double misWeightBsdf;
    RuntimeDisneyV2_3DMisBranch finiteLightMis;
    RuntimeDisneyV2_3DMisBranch emissiveAreaMis;
} RuntimeDisneyV2Transport3DVertexSample;

double runtime_disney_v2_transport_3d_clamp(double value,
                                            double min_value,
                                            double max_value);
double runtime_disney_v2_transport_3d_clamp01(double value);
double runtime_disney_v2_transport_3d_luma(double r, double g, double b);
double runtime_disney_v2_transport_3d_peak(double r, double g, double b);
void runtime_disney_v2_transport_3d_balance_mis(double light_pdf,
                                                double bsdf_pdf,
                                                double* out_light,
                                                double* out_bsdf);
RuntimeDisneyV2_3DMisBranch runtime_disney_v2_transport_3d_make_mis_branch(
    double light_pdf,
    double bsdf_pdf);
void runtime_disney_v2_transport_3d_record_mis_vertex(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index);
void runtime_disney_v2_transport_3d_record_mis_branch_vertex(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index,
    RuntimeDisneyV2_3DMisBranch finite_light_branch,
    RuntimeDisneyV2_3DMisBranch emissive_area_branch);
void runtime_disney_v2_transport_3d_record_bsdf_sample_contribution(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index,
    double r,
    double g,
    double b,
    RuntimeDisneyV2_3DEmitterKind emitter_kind);
uint32_t runtime_disney_v2_transport_3d_hash_u32(uint32_t x);
uint32_t runtime_disney_v2_transport_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling);
double runtime_disney_v2_transport_3d_roulette_sample(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    int depth);
RuntimePathDepthPolicy3DLobe runtime_disney_v2_transport_3d_policy_lobe(
    RuntimeDisneyV2_3DDominantLobe lobe);
Vec3 runtime_disney_v2_transport_3d_default_tangent(Vec3 normal);
void runtime_disney_v2_transport_3d_build_basis(Vec3 normal,
                                                Vec3* out_tangent,
                                                Vec3* out_bitangent);
Vec3 runtime_disney_v2_transport_3d_reflect(Vec3 incident_dir, Vec3 normal);
bool runtime_disney_v2_transport_3d_sample_vertex(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 incoming_dir,
    int depth,
    bool has_emissive_area_direct,
    const RuntimeEmissiveDirect3DResult* emissive_area_direct,
    RuntimeDisneyV2Transport3DVertexSample* out_sample);

#endif
