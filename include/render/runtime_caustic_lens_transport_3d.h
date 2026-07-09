#ifndef RENDER_RUNTIME_CAUSTIC_LENS_TRANSPORT_3D_H
#define RENDER_RUNTIME_CAUSTIC_LENS_TRANSPORT_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "render/runtime_material_payload_3d.h"

enum {
    RUNTIME_CAUSTIC_LENS_TRANSPORT_MAX_INTERFACE_EVENTS = 8
};

typedef enum {
    RUNTIME_CAUSTIC_LENS_SHAPE_NONE = 0,
    RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE = 1,
    RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER = 2,
    RUNTIME_CAUSTIC_LENS_SHAPE_PRISM = 3,
    RUNTIME_CAUSTIC_LENS_SHAPE_BOWL = 4,
    RUNTIME_CAUSTIC_LENS_SHAPE_WATER_SURFACE = 5,
    RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC = 6
} RuntimeCausticLensShape3DKind;

typedef enum {
    RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_PAYLOAD_DEFAULT = 0,
    RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM = 1
} RuntimeCausticLensTraversalProfile3DKind;

typedef struct {
    RuntimeCausticLensTraversalProfile3DKind kind;
    double outsideIor;
    double materialIor;
    double fresnelScale;
    double transmissionScale;
    Vec3 tint;
    double absorptionDistance;
    double apertureRadiusScale;
} RuntimeCausticLensTraversalProfile3D;

typedef struct {
    RuntimeCausticLensShape3DKind kind;
    int sceneObjectIndex;
    int primitiveIndex;
    Vec3 boundsMin;
    Vec3 boundsMax;
    Vec3 center;
    Vec3 axis;
    double radius;
    double height;
    RuntimeMaterialPayload3D payload;
    bool hasTraversalProfileOverride;
    RuntimeCausticLensTraversalProfile3D traversalProfileOverride;
} RuntimeCausticLensShape3D;

typedef struct {
    Vec3 position;
    double radius;
    double intensity;
    Vec3 color;
    int lightIndex;
} RuntimeCausticLensLightSample3D;

typedef struct {
    double apertureU;
    double apertureV;
    double lensU;
    double lensV;
    double sampleWeight;
    double receiverDistance;
} RuntimeCausticLensSample3D;

typedef struct {
    Vec3 position;
    Vec3 normal;
    Vec3 incidentDirection;
    Vec3 outgoingDirection;
    double etaFrom;
    double etaTo;
    double fresnel;
    double distanceInMedium;
    bool reflected;
    bool refracted;
    bool totalInternalReflection;
} RuntimeCausticLensInterfaceEvent3D;

typedef struct {
    bool valid;
    uint64_t pathId;
    RuntimeCausticLensShape3DKind shapeKind;
    int sceneObjectIndex;
    int primitiveIndex;
    Vec3 lightSamplePosition;
    Vec3 targetPosition;
    Vec3 postExitOrigin;
    Vec3 postExitDirection;
    Vec3 throughput;
    double sampleWeight;
    double pathPdf;
    double insideDistance;
    double receiverPlaneT;
    Vec3 receiverCrossing;
    RuntimeCausticLensTraversalProfile3D traversalProfile;
    uint32_t interfaceEventCount;
    RuntimeCausticLensInterfaceEvent3D events[
        RUNTIME_CAUSTIC_LENS_TRANSPORT_MAX_INTERFACE_EVENTS];
} RuntimeCausticLensPath3D;

void RuntimeCausticLensTransport3D_DefaultShape(RuntimeCausticLensShape3D* shape);
void RuntimeCausticLensTransport3D_DefaultTraversalProfile(
    RuntimeCausticLensTraversalProfile3D* profile);
void RuntimeCausticLensTransport3D_NormalizeTraversalProfile(
    RuntimeCausticLensTraversalProfile3D* profile);
bool RuntimeCausticLensTransport3D_PresetTraversalProfileFromLabel(
    const char* label,
    RuntimeCausticLensTraversalProfile3D* out_profile);
void RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(
    const RuntimeMaterialPayload3D* payload,
    RuntimeCausticLensTraversalProfile3D* out_profile);
void RuntimeCausticLensTransport3D_DefaultLightSample(
    RuntimeCausticLensLightSample3D* light);
void RuntimeCausticLensTransport3D_DefaultSample(RuntimeCausticLensSample3D* sample);
void RuntimeCausticLensTransport3D_DefaultInterfaceEvent(
    RuntimeCausticLensInterfaceEvent3D* event);
void RuntimeCausticLensTransport3D_DefaultPath(RuntimeCausticLensPath3D* path);
const char* RuntimeCausticLensTransport3D_ShapeKindLabel(
    RuntimeCausticLensShape3DKind kind);

double RuntimeCausticLensTransport3D_FresnelSchlick(Vec3 incident,
                                                    Vec3 normal,
                                                    double eta_from,
                                                    double eta_to);
bool RuntimeCausticLensTransport3D_Refract(Vec3 incident,
                                           Vec3 normal,
                                           double eta_from,
                                           double eta_to,
                                           Vec3* out_direction,
                                           bool* out_total_internal_reflection);
bool RuntimeCausticLensTransport3D_AppendInterfaceEvent(
    RuntimeCausticLensPath3D* path,
    const RuntimeCausticLensInterfaceEvent3D* event);
Vec3 RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(Vec3 throughput,
                                                              double fresnel);
Vec3 RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
    Vec3 throughput,
    double fresnel,
    const RuntimeCausticLensTraversalProfile3D* profile);
Vec3 RuntimeCausticLensTransport3D_ApplyAbsorptionTint(Vec3 throughput,
                                                       Vec3 tint,
                                                       double distance_in_medium,
                                                       double absorption_distance);
Vec3 RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
    Vec3 throughput,
    double distance_in_medium,
    const RuntimeCausticLensTraversalProfile3D* profile);
bool RuntimeCausticLensTransport3D_SolveSpherePath(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    int scene_object_index,
    int primitive_index,
    RuntimeCausticLensPath3D* out_path);
bool RuntimeCausticLensTransport3D_SolveCylinderPath(
    const RuntimeCausticLensShape3D* cylinder,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path);
bool RuntimeCausticLensTransport3D_SolvePrismPath(
    const RuntimeCausticLensShape3D* prism,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path);
bool RuntimeCausticLensTransport3D_SolveBowlPath(
    const RuntimeCausticLensShape3D* bowl,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path);
bool RuntimeCausticLensTransport3D_SolveMeshDielectricPath(
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path);

#endif
