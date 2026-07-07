#ifndef RENDER_RUNTIME_CAUSTIC_TRANSPORT_INTERNAL_3D_H
#define RENDER_RUNTIME_CAUSTIC_TRANSPORT_INTERNAL_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_caustic_transport_debug_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

enum {
    RUNTIME_CAUSTIC_TRANSPORT_DEFAULT_PATH_BUDGET = 256,
    RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET = 4096,
    RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT = 5,
    RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT = 16,
    RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT = 512,
    RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP = 512
};

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleCount;
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeMaterialPayload3D payload;
} RuntimeCausticTransportAnalyticSphere3D;

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleCount;
    RuntimeCausticLensShape3D shape;
    RuntimeMaterialPayload3D payload;
} RuntimeCausticTransportAnalyticCylinder3D;

typedef RuntimeCausticTransportAnalyticCylinder3D RuntimeCausticTransportAnalyticPrism3D;
typedef RuntimeCausticTransportAnalyticCylinder3D RuntimeCausticTransportAnalyticBowl3D;

typedef struct {
    bool hasSurfaceReceiverFallback;
    HitInfo3D surfaceReceiverFallback;
    int receiverCandidateIndices[RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP];
    int receiverCandidateCount;
} RuntimeCausticTransportSurfaceReceiverContext3D;

double runtime_caustic_transport_clamp(double value,
                                       double min_value,
                                       double max_value);
double runtime_caustic_transport_luma(Vec3 rgb);
double runtime_caustic_transport_analytic_sphere_lens_sample_weight(int path_budget,
                                                                    int sample_count);
Vec3 runtime_caustic_transport_triangle_sample_point(const RuntimeTriangle3D* triangle,
                                                     int sample_index);
void runtime_caustic_transport_sphere_lens_sample(int sample_index,
                                                  int sample_count,
                                                  double* out_aperture_u,
                                                  double* out_aperture_v,
                                                  double* out_lens_u,
                                                  double* out_lens_v);
void runtime_caustic_transport_cylinder_lens_focused_sample(int sample_index,
                                                            int sample_count,
                                                            double* out_aperture_u,
                                                            double* out_aperture_v,
                                                            double* out_lens_u,
                                                            double* out_lens_v);
double runtime_caustic_transport_light_attenuation(
    const RuntimeLightSource3D* light,
    double distance_to_target);
const char* runtime_caustic_transport_light_kind_label(RuntimeLightSource3DKind kind);
bool runtime_caustic_transport_payload_is_eligible(
    const RuntimeMaterialPayload3D* payload);
bool runtime_caustic_transport_resolve_analytic_sphere(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticSphere3D* out_sphere);
bool runtime_caustic_transport_resolve_analytic_cylinder(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticCylinder3D* out_cylinder);
bool runtime_caustic_transport_resolve_analytic_prism(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticPrism3D* out_prism);
bool runtime_caustic_transport_resolve_analytic_bowl(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticBowl3D* out_bowl);
void runtime_caustic_transport_prepare_surface_receiver_fallback(
    RuntimeCausticTransportSurfaceReceiverContext3D* context,
    const RuntimeScene3D* scene);
void runtime_caustic_transport_disable_surface_receiver_fallback(
    RuntimeCausticTransportSurfaceReceiverContext3D* context);
Vec3 runtime_caustic_transport_hit_geometric_normal(const RuntimeScene3D* scene,
                                                    const HitInfo3D* hit);
Vec3 runtime_caustic_transport_orient_specular_normal(Vec3 normal,
                                                      Vec3 incident_dir,
                                                      bool inside_specular_object);
bool runtime_caustic_transport_select_direction_with_normal(
    const RuntimeMaterialPayload3D* payload,
    Vec3 surface_normal,
    Vec3 incident_dir,
    Vec3* out_direction,
    Vec3* out_throughput,
    bool* out_is_refraction);
bool runtime_caustic_transport_deposit_surface(
    const RuntimeScene3D* scene,
    RuntimeCausticSurfaceCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    bool inside_specular_object,
    int current_specular_object_index,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);
bool runtime_caustic_transport_deposit_segment(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    double base_footprint_radius,
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticTransportDebugPath3D* debug_path);
bool runtime_caustic_transport_continue_to_outside_medium(
    const RuntimeScene3D* scene,
    Ray3D* io_ray,
    Vec3* io_radiance,
    bool inside_specular_object,
    int current_specular_object_index,
    int max_path_depth,
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticTransportDebugPath3D* debug_path);
void runtime_caustic_transport_emit_analytic_sphere_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticSphere3D* analytic_sphere,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);
void runtime_caustic_transport_emit_analytic_cylinder_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticCylinder3D* analytic_cylinder,
    int path_budget,
    bool focused_profile,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);
void runtime_caustic_transport_emit_analytic_prism_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticPrism3D* analytic_prism,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);
void runtime_caustic_transport_emit_analytic_bowl_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticBowl3D* analytic_bowl,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);
void runtime_caustic_transport_emit_to_triangle(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    int triangle_index,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics);

#endif
