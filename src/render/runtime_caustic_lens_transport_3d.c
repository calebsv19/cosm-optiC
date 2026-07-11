#include "render/runtime_caustic_lens_transport_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_lens_transport_internal_3d.h"

void RuntimeCausticLensTransport3D_DefaultShape(RuntimeCausticLensShape3D* shape) {
    if (!shape) return;
    memset(shape, 0, sizeof(*shape));
    shape->kind = RUNTIME_CAUSTIC_LENS_SHAPE_NONE;
    shape->sceneObjectIndex = -1;
    shape->primitiveIndex = -1;
    shape->boundsMin = vec3(-1.0, -1.0, -1.0);
    shape->boundsMax = vec3(1.0, 1.0, 1.0);
    shape->center = vec3(0.0, 0.0, 0.0);
    shape->axis = vec3(0.0, 0.0, 1.0);
    shape->radius = 1.0;
    shape->height = 2.0;
}
void RuntimeCausticLensTransport3D_DefaultTraversalProfile(
    RuntimeCausticLensTraversalProfile3D* profile) {
    if (!profile) return;
    memset(profile, 0, sizeof(*profile));
    profile->kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_PAYLOAD_DEFAULT;
    profile->outsideIor = 1.0;
    profile->materialIor = 1.5;
    profile->fresnelScale = 1.0;
    profile->transmissionScale = 1.0;
    profile->tint = vec3(1.0, 1.0, 1.0);
    profile->absorptionDistance = 0.0;
    profile->apertureRadiusScale = 1.0;
}

void RuntimeCausticLensTransport3D_NormalizeTraversalProfile(
    RuntimeCausticLensTraversalProfile3D* profile) {
    if (!profile) return;
    if (!(profile->outsideIor > 1.0e-6)) profile->outsideIor = 1.0;
    if (!(profile->materialIor > 1.0e-6)) profile->materialIor = 1.5;
    profile->fresnelScale = lens_transport_clamp(profile->fresnelScale, 0.0, 4.0);
    profile->transmissionScale = lens_transport_clamp(profile->transmissionScale, 0.0, 4.0);
    profile->tint.x = lens_transport_clamp(profile->tint.x, 0.0, 8.0);
    profile->tint.y = lens_transport_clamp(profile->tint.y, 0.0, 8.0);
    profile->tint.z = lens_transport_clamp(profile->tint.z, 0.0, 8.0);
    if (!(profile->absorptionDistance > 0.0)) profile->absorptionDistance = 0.0;
    profile->absorptionDistance = lens_transport_clamp(profile->absorptionDistance,
                                                       0.0,
                                                       1000000.0);
    profile->apertureRadiusScale = lens_transport_clamp(profile->apertureRadiusScale,
                                                        0.0,
                                                        8.0);
}

bool RuntimeCausticLensTransport3D_PresetTraversalProfileFromLabel(
    const char* label,
    RuntimeCausticLensTraversalProfile3D* out_profile) {
    RuntimeCausticLensTraversalProfile3D profile;
    if (!out_profile || !label || !label[0]) return false;

    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&profile);
    profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;

    if (strcmp(label, "clear_glass") == 0 ||
        strcmp(label, "glass") == 0 ||
        strcmp(label, "crown_glass") == 0) {
        profile.materialIor = 1.50;
        profile.tint = vec3(1.0, 1.0, 1.0);
    } else if (strcmp(label, "dense_glass") == 0 ||
               strcmp(label, "flint_glass") == 0 ||
               strcmp(label, "high_ior_glass") == 0) {
        profile.materialIor = 1.62;
        profile.tint = vec3(1.0, 0.97, 0.92);
        profile.absorptionDistance = 12.0;
    } else if (strcmp(label, "water") == 0 ||
               strcmp(label, "water_like") == 0) {
        profile.materialIor = 1.333;
        profile.tint = vec3(0.86, 0.95, 1.0);
        profile.absorptionDistance = 9.0;
    } else if (strcmp(label, "acrylic") == 0 ||
               strcmp(label, "plexiglass") == 0) {
        profile.materialIor = 1.49;
        profile.tint = vec3(1.0, 0.98, 0.96);
        profile.absorptionDistance = 18.0;
    } else if (strcmp(label, "diamond") == 0) {
        profile.materialIor = 2.417;
        profile.tint = vec3(1.0, 1.0, 0.98);
        profile.absorptionDistance = 20.0;
    } else if (strcmp(label, "air_gap") == 0 ||
               strcmp(label, "low_ior") == 0 ||
               strcmp(label, "refraction_reversal") == 0 ||
               strcmp(label, "reversal") == 0) {
        profile.materialIor = 1.0003;
        profile.tint = vec3(1.0, 1.0, 1.0);
        profile.apertureRadiusScale = 0.65;
    } else {
        return false;
    }

    RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&profile);
    *out_profile = profile;
    return true;
}

void RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(
    const RuntimeMaterialPayload3D* payload,
    RuntimeCausticLensTraversalProfile3D* out_profile) {
    RuntimeCausticLensTraversalProfile3D profile;
    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&profile);
    if (payload) {
        profile.materialIor = payload->opticalIor > 1.0001
                                  ? payload->opticalIor
                                  : fmax(payload->bsdf.ior, 1.0);
        profile.tint = vec3(payload->baseColorR,
                            payload->baseColorG,
                            payload->baseColorB);
        profile.absorptionDistance = payload->absorptionDistance;
    }
    RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&profile);
    if (out_profile) *out_profile = profile;
}

void RuntimeCausticLensTransport3D_DefaultLightSample(
    RuntimeCausticLensLightSample3D* light) {
    if (!light) return;
    memset(light, 0, sizeof(*light));
    light->position = vec3(0.0, 0.0, 3.0);
    light->radius = 0.05;
    light->intensity = 1.0;
    light->color = vec3(1.0, 1.0, 1.0);
    light->lightIndex = -1;
}

void RuntimeCausticLensTransport3D_DefaultSample(RuntimeCausticLensSample3D* sample) {
    if (!sample) return;
    memset(sample, 0, sizeof(*sample));
    sample->sampleWeight = 1.0;
    sample->receiverDistance = 3.0;
}

void RuntimeCausticLensTransport3D_DefaultInterfaceEvent(
    RuntimeCausticLensInterfaceEvent3D* event) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->etaFrom = 1.0;
    event->etaTo = 1.0;
}

void RuntimeCausticLensTransport3D_DefaultPath(RuntimeCausticLensPath3D* path) {
    if (!path) return;
    memset(path, 0, sizeof(*path));
    path->shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_NONE;
    path->sceneObjectIndex = -1;
    path->primitiveIndex = -1;
    path->throughput = vec3(1.0, 1.0, 1.0);
    path->sampleWeight = 1.0;
    path->pathPdf = 1.0;
    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&path->traversalProfile);
}

const char* RuntimeCausticLensTransport3D_ShapeKindLabel(
    RuntimeCausticLensShape3DKind kind) {
    switch (kind) {
        case RUNTIME_CAUSTIC_LENS_SHAPE_NONE:
            return "none";
        case RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE:
            return "sphere";
        case RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER:
            return "cylinder";
        case RUNTIME_CAUSTIC_LENS_SHAPE_PRISM:
            return "prism";
        case RUNTIME_CAUSTIC_LENS_SHAPE_BOWL:
            return "bowl";
        case RUNTIME_CAUSTIC_LENS_SHAPE_WATER_SURFACE:
            return "water_surface";
        case RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC:
            return "mesh_dielectric";
        default:
            return "unknown";
    }
}

double RuntimeCausticLensTransport3D_FresnelSchlick(Vec3 incident,
                                                    Vec3 normal,
                                                    double eta_from,
                                                    double eta_to) {
    Vec3 i = vec3_normalize(incident);
    Vec3 n = vec3_normalize(normal);
    double cos_i = 0.0;
    double f0 = 0.0;
    double one_minus_cos = 0.0;
    if (!(vec3_length(i) > 1.0e-9) || !(vec3_length(n) > 1.0e-9) ||
        !(eta_from > 0.0) || !(eta_to > 0.0) ||
        !lens_transport_finite_vec3(i) || !lens_transport_finite_vec3(n)) {
        return 1.0;
    }
    cos_i = fabs(vec3_dot(i, n));
    cos_i = lens_transport_clamp(cos_i, 0.0, 1.0);
    f0 = (eta_from - eta_to) / (eta_from + eta_to);
    f0 *= f0;
    one_minus_cos = 1.0 - cos_i;
    return lens_transport_clamp(f0 + (1.0 - f0) * one_minus_cos * one_minus_cos *
                                         one_minus_cos * one_minus_cos * one_minus_cos,
                                0.0,
                                1.0);
}

bool RuntimeCausticLensTransport3D_Refract(Vec3 incident,
                                           Vec3 normal,
                                           double eta_from,
                                           double eta_to,
                                           Vec3* out_direction,
                                           bool* out_total_internal_reflection) {
    Vec3 i = vec3_normalize(incident);
    Vec3 n = vec3_normalize(normal);
    double cos_i = 0.0;
    double eta = 0.0;
    double k = 0.0;
    Vec3 refracted = vec3(0.0, 0.0, 0.0);
    if (out_direction) *out_direction = vec3(0.0, 0.0, 0.0);
    if (out_total_internal_reflection) *out_total_internal_reflection = false;
    if (!out_direction || !(vec3_length(i) > 1.0e-9) ||
        !(vec3_length(n) > 1.0e-9) || !(eta_from > 0.0) || !(eta_to > 0.0) ||
        !lens_transport_finite_vec3(i) || !lens_transport_finite_vec3(n)) {
        return false;
    }
    cos_i = -vec3_dot(n, i);
    if (cos_i < 0.0) {
        n = vec3_scale(n, -1.0);
        cos_i = -vec3_dot(n, i);
    }
    cos_i = lens_transport_clamp(cos_i, 0.0, 1.0);
    eta = eta_from / eta_to;
    k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
    if (k < 0.0) {
        if (out_total_internal_reflection) *out_total_internal_reflection = true;
        return false;
    }
    refracted = vec3_add(vec3_scale(i, eta),
                         vec3_scale(n, eta * cos_i - sqrt(k)));
    if (!(vec3_length(refracted) > 1.0e-9) || !lens_transport_finite_vec3(refracted)) {
        return false;
    }
    *out_direction = vec3_normalize(refracted);
    return true;
}

bool RuntimeCausticLensTransport3D_AppendInterfaceEvent(
    RuntimeCausticLensPath3D* path,
    const RuntimeCausticLensInterfaceEvent3D* event) {
    if (!path || !event ||
        path->interfaceEventCount >= RUNTIME_CAUSTIC_LENS_TRANSPORT_MAX_INTERFACE_EVENTS) {
        return false;
    }
    path->events[path->interfaceEventCount++] = *event;
    if (event->distanceInMedium > 0.0) {
        path->insideDistance += event->distanceInMedium;
    }
    return true;
}

Vec3 RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(Vec3 throughput,
                                                              double fresnel) {
    const double transmission = 1.0 - lens_transport_clamp(fresnel, 0.0, 1.0);
    return vec3(throughput.x * transmission,
                throughput.y * transmission,
                throughput.z * transmission);
}

Vec3 RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
    Vec3 throughput,
    double fresnel,
    const RuntimeCausticLensTraversalProfile3D* profile) {
    RuntimeCausticLensTraversalProfile3D default_profile;
    const RuntimeCausticLensTraversalProfile3D* active_profile = profile;
    double scaled_fresnel = 0.0;
    double transmission = 0.0;
    if (!active_profile) {
        RuntimeCausticLensTransport3D_DefaultTraversalProfile(&default_profile);
        active_profile = &default_profile;
    }
    scaled_fresnel = lens_transport_clamp(fresnel, 0.0, 1.0) *
                     lens_transport_clamp(active_profile->fresnelScale, 0.0, 8.0);
    transmission = (1.0 - lens_transport_clamp(scaled_fresnel, 0.0, 1.0)) *
                   lens_transport_clamp(active_profile->transmissionScale, 0.0, 8.0);
    return vec3(throughput.x * transmission,
                throughput.y * transmission,
                throughput.z * transmission);
}

Vec3 RuntimeCausticLensTransport3D_ApplyAbsorptionTint(Vec3 throughput,
                                                       Vec3 tint,
                                                       double distance_in_medium,
                                                       double absorption_distance) {
    double absorption = 1.0;
    if (absorption_distance > 1.0e-9 && distance_in_medium > 0.0) {
        absorption = exp(-distance_in_medium / absorption_distance);
    }
    return vec3(throughput.x * lens_transport_clamp(tint.x, 0.0, 1.0) * absorption,
                throughput.y * lens_transport_clamp(tint.y, 0.0, 1.0) * absorption,
                throughput.z * lens_transport_clamp(tint.z, 0.0, 1.0) * absorption);
}

Vec3 RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
    Vec3 throughput,
    double distance_in_medium,
    const RuntimeCausticLensTraversalProfile3D* profile) {
    RuntimeCausticLensTraversalProfile3D default_profile;
    const RuntimeCausticLensTraversalProfile3D* active_profile = profile;
    if (!active_profile) {
        RuntimeCausticLensTransport3D_DefaultTraversalProfile(&default_profile);
        active_profile = &default_profile;
    }
    return RuntimeCausticLensTransport3D_ApplyAbsorptionTint(
        throughput,
        active_profile->tint,
        distance_in_medium,
        active_profile->absorptionDistance);
}

bool RuntimeCausticLensTransport3D_SolveSpherePath(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    int scene_object_index,
    int primitive_index,
    RuntimeCausticLensPath3D* out_path) {
    RuntimeCausticSphereLens3DPath sphere_path;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    RuntimeCausticLensTraversalProfile3D traversal_profile;
    double sample_weight = 1.0;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!sphere || !light || !out_path) return false;
    if (!RuntimeCausticSphereLens3D_SolvePath(sphere, light, sample, &sphere_path) ||
        !sphere_path.valid) {
        return false;
    }
    if (sample) {
        sample_weight = fmax(sample->sampleWeight, 0.0);
    }

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&traversal_profile);
    traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
    traversal_profile.outsideIor = sphere->outsideIor;
    traversal_profile.materialIor = sphere->ior;
    traversal_profile.fresnelScale = sphere->fresnelScale;
    traversal_profile.transmissionScale = sphere->transmissionScale;
    traversal_profile.tint = sphere->tint;
    traversal_profile.absorptionDistance = sphere->absorptionDistance;
    traversal_profile.apertureRadiusScale = sphere->apertureRadiusScale;
    RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE;
    path.sceneObjectIndex = scene_object_index;
    path.primitiveIndex = primitive_index;
    path.lightSamplePosition = sphere_path.lightSamplePosition;
    path.targetPosition = sphere_path.lensTargetPosition;
    path.postExitOrigin = sphere_path.exitPosition;
    path.postExitDirection = sphere_path.exitDirection;
    path.throughput = sphere_path.throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.receiverPlaneT = sphere_path.exitReceiverT;
    path.receiverCrossing = sphere_path.receiverCrossing;
    path.traversalProfile = traversal_profile;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = sphere_path.entryPosition;
    entry_event.normal = sphere_path.entryNormal;
    entry_event.incidentDirection = sphere_path.entryDirection;
    entry_event.outgoingDirection = sphere_path.insideDirection;
    entry_event.etaFrom = traversal_profile.outsideIor;
    entry_event.etaTo = traversal_profile.materialIor;
    entry_event.fresnel = sphere_path.entryFresnel;
    entry_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = sphere_path.exitPosition;
    exit_event.normal = sphere_path.exitNormal;
    exit_event.incidentDirection = sphere_path.insideDirection;
    exit_event.outgoingDirection = sphere_path.exitDirection;
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
    exit_event.fresnel = sphere_path.exitFresnel;
    exit_event.distanceInMedium = sphere_path.insideDistance;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}
