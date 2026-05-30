#ifndef RUNTIME_EMISSION_TRANSPARENCY_3D_INTERNAL_H
#define RUNTIME_EMISSION_TRANSPARENCY_3D_INTERNAL_H

#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern const double kRuntimeEmissionTransparency3DEpsilon;
extern const double kRuntimeEmissionTransparency3DMaxDistance;
extern const double kRuntimeEmissionTransparency3DEnergyScale;
extern const double kRuntimeEmissionTransparency3DTransmissionMaxDistance;
extern const int kRuntimeEmissionTransparency3DMaxTransmissionSurfaceSkips;
extern const double kRuntimeEmissionTransparency3DTransmissionConeBase;
extern const double kRuntimeEmissionTransparency3DTransmissionConeScale;

typedef struct {
    Vec3 incidentDir;
    int specularDepth;
    int transmissionDepth;
} RuntimeEmissionTransparency3DPathState;

typedef struct {
    bool hit;
    bool emitterWins;
    bool usedTransparencyTier;
    RuntimeMaterialResponse3DResult materialResult;
    RuntimeEmissionTransparency3DResult transparencyResult;
    RuntimeMaterialPayload3D materialPayload;
    RuntimeLightEmitterHit3DResult emitterHit;
} RuntimeEmissionTransparency3DTransmissionResult;

int runtime_emission_transparency_3d_resolve_secondary_sample_count(void);
int runtime_emission_transparency_3d_resolve_transmission_sample_count(void);
double runtime_emission_transparency_3d_clamp(double value,
                                              double min_value,
                                              double max_value);
void runtime_emission_transparency_3d_resolve_payload_tint(
    const RuntimeMaterialPayload3D* payload,
    double* out_r,
    double* out_g,
    double* out_b);
Vec3 runtime_emission_transparency_3d_default_tangent(Vec3 normal);
void runtime_emission_transparency_3d_build_basis(Vec3 normal,
                                                  Vec3* out_tangent,
                                                  Vec3* out_bitangent);
double runtime_emission_transparency_3d_distance_decay(double distance);
uint32_t runtime_emission_transparency_3d_hash_u32(uint32_t x);
uint32_t runtime_emission_transparency_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling);
Vec3 runtime_emission_transparency_3d_sample_direction(
    const HitInfo3D* hit,
    Vec3 normal,
    Vec3 tangent,
    Vec3 bitangent,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_count,
    int sample_index);
double runtime_emission_transparency_3d_transmission_cone_radius(
    const RuntimeMaterialPayload3D* payload,
    double transparency);
void runtime_emission_transparency_3d_resolve_mix_weights(
    const RuntimeMaterialPayload3D* payload,
    const RuntimeDielectricTransport3D* dielectric,
    double* out_base_front_weight,
    double* out_reflection_weight,
    double* out_transmission_weight);
Vec3 runtime_emission_transparency_3d_sample_transmission_direction(
    const HitInfo3D* hit,
    Vec3 base_dir,
    Vec3 tangent,
    Vec3 bitangent,
    double cone_radius,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_count,
    int sample_index);

#endif
