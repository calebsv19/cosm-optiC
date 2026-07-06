#include "render/runtime_caustic_lens_transport_3d.h"

#include <math.h>
#include <string.h>

static double lens_transport_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool lens_transport_finite_vec3(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static void lens_transport_build_basis(Vec3 axis, Vec3* out_u, Vec3* out_v) {
    Vec3 n = vec3_normalize(axis);
    Vec3 helper = fabs(n.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 u = vec3_normalize(vec3_cross(helper, n));
    Vec3 v = vec3_normalize(vec3_cross(n, u));
    if (!(vec3_length(u) > 1.0e-9) || !(vec3_length(v) > 1.0e-9)) {
        u = vec3(1.0, 0.0, 0.0);
        v = vec3(0.0, 1.0, 0.0);
    }
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
}

static bool lens_transport_intersect_cylinder_side(Vec3 center,
                                                   Vec3 axis,
                                                   double radius,
                                                   double half_height,
                                                   Vec3 origin,
                                                   Vec3 direction,
                                                   double min_t,
                                                   double* out_t,
                                                   Vec3* out_position,
                                                   Vec3* out_normal) {
    Vec3 rel = vec3_sub(origin, center);
    Vec3 rel_perp = vec3_sub(rel, vec3_scale(axis, vec3_dot(rel, axis)));
    Vec3 dir_perp = vec3_sub(direction, vec3_scale(axis, vec3_dot(direction, axis)));
    double a = vec3_dot(dir_perp, dir_perp);
    double b = 2.0 * vec3_dot(rel_perp, dir_perp);
    double c = vec3_dot(rel_perp, rel_perp) - radius * radius;
    double disc = 0.0;
    double root_disc = 0.0;
    double roots[2] = {0.0, 0.0};

    if (out_t) *out_t = 0.0;
    if (out_position) *out_position = vec3(0.0, 0.0, 0.0);
    if (out_normal) *out_normal = vec3(0.0, 0.0, 0.0);
    if (!(a > 1.0e-12) || !(radius > 1.0e-9) || !(half_height > 1.0e-9)) {
        return false;
    }
    disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return false;
    root_disc = sqrt(disc);
    roots[0] = (-b - root_disc) / (2.0 * a);
    roots[1] = (-b + root_disc) / (2.0 * a);
    for (int i = 0; i < 2; ++i) {
        double t = roots[i];
        Vec3 p;
        double axial = 0.0;
        Vec3 radial;
        if (!(t > min_t)) continue;
        p = vec3_add(origin, vec3_scale(direction, t));
        axial = vec3_dot(vec3_sub(p, center), axis);
        if (fabs(axial) > half_height + 1.0e-6) continue;
        radial = vec3_sub(vec3_sub(p, center), vec3_scale(axis, axial));
        if (!(vec3_length(radial) > 1.0e-9)) continue;
        if (out_t) *out_t = t;
        if (out_position) *out_position = p;
        if (out_normal) *out_normal = vec3_normalize(radial);
        return true;
    }
    return false;
}

static bool lens_transport_intersect_plane(Vec3 plane_point,
                                           Vec3 plane_normal,
                                           Vec3 origin,
                                           Vec3 direction,
                                           double min_t,
                                           double* out_t,
                                           Vec3* out_position) {
    double denom = vec3_dot(direction, plane_normal);
    double t = 0.0;
    if (out_t) *out_t = 0.0;
    if (out_position) *out_position = vec3(0.0, 0.0, 0.0);
    if (fabs(denom) <= 1.0e-10) return false;
    t = vec3_dot(vec3_sub(plane_point, origin), plane_normal) / denom;
    if (!(t > min_t)) return false;
    if (out_t) *out_t = t;
    if (out_position) *out_position = vec3_add(origin, vec3_scale(direction, t));
    return true;
}

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

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = sphere_path.entryPosition;
    entry_event.normal = sphere_path.entryNormal;
    entry_event.incidentDirection = sphere_path.entryDirection;
    entry_event.outgoingDirection = sphere_path.insideDirection;
    entry_event.etaFrom = 1.0;
    entry_event.etaTo = sphere->ior;
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
    exit_event.etaFrom = sphere->ior;
    exit_event.etaTo = 1.0;
    exit_event.fresnel = sphere_path.exitFresnel;
    exit_event.distanceInMedium = sphere_path.insideDistance;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}

bool RuntimeCausticLensTransport3D_SolveCylinderPath(
    const RuntimeCausticLensShape3D* cylinder,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path) {
    RuntimeCausticLensSample3D default_sample;
    const RuntimeCausticLensSample3D* active_sample = sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    Vec3 axis = vec3(0.0, 0.0, 1.0);
    Vec3 optical_axis = vec3(0.0, 1.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 rel_light = vec3(0.0, 0.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 target = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    Vec3 entry_position = vec3(0.0, 0.0, 0.0);
    Vec3 entry_normal = vec3(0.0, 0.0, 0.0);
    Vec3 inside_origin = vec3(0.0, 0.0, 0.0);
    Vec3 inside_dir = vec3(0.0, 0.0, 0.0);
    Vec3 exit_position = vec3(0.0, 0.0, 0.0);
    Vec3 exit_normal = vec3(0.0, 0.0, 0.0);
    Vec3 exit_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);
    Vec3 tint = vec3(1.0, 1.0, 1.0);
    double radius = 0.0;
    double half_height = 0.0;
    double ior = 1.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double radial_offset = 0.0;
    double front_depth = 0.0;
    double entry_t = 0.0;
    double exit_t = 0.0;
    double entry_fresnel = 1.0;
    double exit_fresnel = 1.0;
    double sample_weight = 1.0;
    double receiver_distance = 0.0;
    bool tir = false;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!cylinder || !light || !out_path ||
        cylinder->kind != RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER ||
        !(cylinder->radius > 1.0e-9) || !(cylinder->height > 1.0e-9) ||
        !lens_transport_finite_vec3(cylinder->center) ||
        !lens_transport_finite_vec3(cylinder->axis) ||
        !lens_transport_finite_vec3(light->position)) {
        return false;
    }
    if (!active_sample) {
        RuntimeCausticLensTransport3D_DefaultSample(&default_sample);
        active_sample = &default_sample;
    }

    axis = vec3_normalize(cylinder->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = cylinder->radius;
    half_height = cylinder->height * 0.5;
    ior = cylinder->payload.opticalIor > 1.0001
              ? cylinder->payload.opticalIor
              : fmax(cylinder->payload.bsdf.ior, 1.0);
    if (!(ior > 1.0)) return false;

    rel_light = vec3_sub(cylinder->center, light->position);
    optical_axis = vec3_sub(rel_light, vec3_scale(axis, vec3_dot(rel_light, axis)));
    if (!(vec3_length(optical_axis) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, &optical_axis);
    }
    optical_axis = vec3_normalize(optical_axis);
    basis_u = vec3_normalize(vec3_cross(axis, optical_axis));
    if (!(vec3_length(basis_u) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, NULL);
    }

    lens_u = lens_transport_clamp(active_sample->lensU, -0.95, 0.95);
    lens_v = lens_transport_clamp(active_sample->lensV, -0.95, 0.95);
    radial_offset = lens_u * radius;
    front_depth = sqrt(fmax(radius * radius - radial_offset * radial_offset, 0.0));
    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u,
                            lens_transport_clamp(active_sample->apertureU, -1.0, 1.0) *
                                fmax(light->radius, 0.0)),
                 vec3_scale(axis,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0))));
    target = vec3_add(cylinder->center,
                      vec3_add(vec3_scale(basis_u, radial_offset),
                               vec3_add(vec3_scale(optical_axis, -front_depth),
                                        vec3_scale(axis, lens_v * half_height))));
    ray_dir = vec3_normalize(vec3_sub(target, ray_origin));
    if (!(vec3_length(ray_dir) > 1.0e-9)) return false;

    if (!lens_transport_intersect_cylinder_side(cylinder->center,
                                                axis,
                                                radius,
                                                half_height,
                                                ray_origin,
                                                ray_dir,
                                                1.0e-6,
                                                &entry_t,
                                                &entry_position,
                                                &entry_normal)) {
        return false;
    }
    entry_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir, entry_normal, 1.0, ior);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               1.0,
                                               ior,
                                               &inside_dir,
                                               &tir)) {
        return false;
    }

    inside_origin = vec3_add(entry_position, vec3_scale(inside_dir, 1.0e-5));
    if (!lens_transport_intersect_cylinder_side(cylinder->center,
                                                axis,
                                                radius,
                                                half_height,
                                                inside_origin,
                                                inside_dir,
                                                1.0e-5,
                                                &exit_t,
                                                &exit_position,
                                                &exit_normal)) {
        return false;
    }
    exit_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir, exit_normal, ior, 1.0);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               ior,
                                               1.0,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 3.0;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          entry_fresnel);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          exit_fresnel);
    tint = vec3(cylinder->payload.baseColorR,
                cylinder->payload.baseColorG,
                cylinder->payload.baseColorB);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTint(throughput,
                                                                   tint,
                                                                   exit_t,
                                                                   cylinder->payload.absorptionDistance);

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER;
    path.sceneObjectIndex = cylinder->sceneObjectIndex;
    path.primitiveIndex = cylinder->primitiveIndex;
    path.lightSamplePosition = ray_origin;
    path.targetPosition = target;
    path.postExitOrigin = exit_position;
    path.postExitDirection = exit_dir;
    path.throughput = throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.receiverPlaneT = receiver_distance;
    path.receiverCrossing = vec3_add(exit_position, vec3_scale(exit_dir, receiver_distance));

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = 1.0;
    entry_event.etaTo = ior;
    entry_event.fresnel = entry_fresnel;
    entry_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = exit_position;
    exit_event.normal = exit_normal;
    exit_event.incidentDirection = inside_dir;
    exit_event.outgoingDirection = exit_dir;
    exit_event.etaFrom = ior;
    exit_event.etaTo = 1.0;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = exit_t;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}

bool RuntimeCausticLensTransport3D_SolvePrismPath(
    const RuntimeCausticLensShape3D* prism,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path) {
    RuntimeCausticLensSample3D default_sample;
    const RuntimeCausticLensSample3D* active_sample = sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    Vec3 axis = vec3(0.0, 0.0, 1.0);
    Vec3 optical_axis = vec3(0.0, 1.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 rel_light = vec3(0.0, 0.0, 0.0);
    Vec3 entry_plane_point = vec3(0.0, 0.0, 0.0);
    Vec3 entry_normal = vec3(0.0, -1.0, 0.0);
    Vec3 exit_plane_point = vec3(0.0, 0.0, 0.0);
    Vec3 exit_normal = vec3(0.0, 1.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 target = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    Vec3 entry_position = vec3(0.0, 0.0, 0.0);
    Vec3 inside_origin = vec3(0.0, 0.0, 0.0);
    Vec3 inside_dir = vec3(0.0, 0.0, 0.0);
    Vec3 exit_position = vec3(0.0, 0.0, 0.0);
    Vec3 exit_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);
    Vec3 tint = vec3(1.0, 1.0, 1.0);
    double radius = 0.0;
    double half_height = 0.0;
    double ior = 1.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double entry_t = 0.0;
    double exit_t = 0.0;
    double entry_fresnel = 1.0;
    double exit_fresnel = 1.0;
    double sample_weight = 1.0;
    double receiver_distance = 0.0;
    double exit_lateral = 0.0;
    double exit_axial = 0.0;
    bool tir = false;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!prism || !light || !out_path ||
        prism->kind != RUNTIME_CAUSTIC_LENS_SHAPE_PRISM ||
        !(prism->radius > 1.0e-9) || !(prism->height > 1.0e-9) ||
        !lens_transport_finite_vec3(prism->center) ||
        !lens_transport_finite_vec3(prism->axis) ||
        !lens_transport_finite_vec3(light->position)) {
        return false;
    }
    if (!active_sample) {
        RuntimeCausticLensTransport3D_DefaultSample(&default_sample);
        active_sample = &default_sample;
    }

    axis = vec3_normalize(prism->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = prism->radius;
    half_height = prism->height * 0.5;
    ior = prism->payload.opticalIor > 1.0001
              ? prism->payload.opticalIor
              : fmax(prism->payload.bsdf.ior, 1.0);
    if (!(ior > 1.0)) return false;

    rel_light = vec3_sub(prism->center, light->position);
    optical_axis = vec3_sub(rel_light, vec3_scale(axis, vec3_dot(rel_light, axis)));
    if (!(vec3_length(optical_axis) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, &optical_axis);
    }
    optical_axis = vec3_normalize(optical_axis);
    basis_u = vec3_normalize(vec3_cross(axis, optical_axis));
    if (!(vec3_length(basis_u) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, NULL);
    }

    lens_u = lens_transport_clamp(active_sample->lensU, -0.75, 0.75);
    lens_v = lens_transport_clamp(active_sample->lensV, -0.95, 0.95);
    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u,
                            lens_transport_clamp(active_sample->apertureU, -1.0, 1.0) *
                                fmax(light->radius, 0.0)),
                 vec3_scale(axis,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0))));
    entry_plane_point = vec3_add(prism->center, vec3_scale(optical_axis, -radius * 0.45));
    entry_normal = vec3_scale(optical_axis, -1.0);
    exit_plane_point = vec3_add(prism->center,
                                vec3_add(vec3_scale(optical_axis, radius * 0.22),
                                         vec3_scale(basis_u, radius * 0.28)));
    exit_normal = vec3_normalize(vec3_add(vec3_scale(optical_axis, 0.9659258262890683),
                                          vec3_scale(basis_u, 0.25881904510252074)));
    target = vec3_add(entry_plane_point,
                      vec3_add(vec3_scale(basis_u, lens_u * radius),
                               vec3_scale(axis, lens_v * half_height)));
    ray_dir = vec3_normalize(vec3_sub(target, ray_origin));
    if (!(vec3_length(ray_dir) > 1.0e-9)) return false;

    if (!lens_transport_intersect_plane(entry_plane_point,
                                        entry_normal,
                                        ray_origin,
                                        ray_dir,
                                        1.0e-6,
                                        &entry_t,
                                        &entry_position)) {
        return false;
    }
    entry_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir, entry_normal, 1.0, ior);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               1.0,
                                               ior,
                                               &inside_dir,
                                               &tir)) {
        return false;
    }

    inside_origin = vec3_add(entry_position, vec3_scale(inside_dir, 1.0e-5));
    if (!lens_transport_intersect_plane(exit_plane_point,
                                        exit_normal,
                                        inside_origin,
                                        inside_dir,
                                        1.0e-5,
                                        &exit_t,
                                        &exit_position)) {
        return false;
    }
    exit_lateral = fabs(vec3_dot(vec3_sub(exit_position, prism->center), basis_u));
    exit_axial = fabs(vec3_dot(vec3_sub(exit_position, prism->center), axis));
    if (exit_lateral > radius * 1.25 || exit_axial > half_height + 1.0e-6) {
        return false;
    }
    exit_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir, exit_normal, ior, 1.0);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               ior,
                                               1.0,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 4.0;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          entry_fresnel);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          exit_fresnel);
    tint = vec3(prism->payload.baseColorR,
                prism->payload.baseColorG,
                prism->payload.baseColorB);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTint(throughput,
                                                                   tint,
                                                                   exit_t,
                                                                   prism->payload.absorptionDistance);

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_PRISM;
    path.sceneObjectIndex = prism->sceneObjectIndex;
    path.primitiveIndex = prism->primitiveIndex;
    path.lightSamplePosition = ray_origin;
    path.targetPosition = target;
    path.postExitOrigin = exit_position;
    path.postExitDirection = exit_dir;
    path.throughput = throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.receiverPlaneT = receiver_distance;
    path.receiverCrossing = vec3_add(exit_position, vec3_scale(exit_dir, receiver_distance));

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = 1.0;
    entry_event.etaTo = ior;
    entry_event.fresnel = entry_fresnel;
    entry_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = exit_position;
    exit_event.normal = exit_normal;
    exit_event.incidentDirection = inside_dir;
    exit_event.outgoingDirection = exit_dir;
    exit_event.etaFrom = ior;
    exit_event.etaTo = 1.0;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = exit_t;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}

bool RuntimeCausticLensTransport3D_SolveBowlPath(
    const RuntimeCausticLensShape3D* bowl,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path) {
    RuntimeCausticLensSample3D default_sample;
    const RuntimeCausticLensSample3D* active_sample = sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    Vec3 axis = vec3(0.0, 0.0, 1.0);
    Vec3 optical_axis = vec3(0.0, 1.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 basis_v = vec3(0.0, 0.0, 1.0);
    Vec3 rel_light = vec3(0.0, 0.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 entry_position = vec3(0.0, 0.0, 0.0);
    Vec3 entry_normal = vec3(0.0, -1.0, 0.0);
    Vec3 inside_origin = vec3(0.0, 0.0, 0.0);
    Vec3 inside_dir = vec3(0.0, 0.0, 0.0);
    Vec3 exit_plane_point = vec3(0.0, 0.0, 0.0);
    Vec3 exit_normal = vec3(0.0, 1.0, 0.0);
    Vec3 exit_position = vec3(0.0, 0.0, 0.0);
    Vec3 exit_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);
    Vec3 tint = vec3(1.0, 1.0, 1.0);
    double radius = 0.0;
    double thickness = 0.0;
    double half_thickness = 0.0;
    double bowl_depth = 0.0;
    double ior = 1.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double r2 = 0.0;
    double radial_scale = 1.0;
    double sag = 0.0;
    double front_rim_offset = 0.0;
    double exit_t = 0.0;
    double exit_radial_u = 0.0;
    double exit_radial_v = 0.0;
    double entry_fresnel = 1.0;
    double exit_fresnel = 1.0;
    double sample_weight = 1.0;
    double receiver_distance = 0.0;
    bool tir = false;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!bowl || !light || !out_path ||
        bowl->kind != RUNTIME_CAUSTIC_LENS_SHAPE_BOWL ||
        !(bowl->radius > 1.0e-9) || !(bowl->height > 1.0e-9) ||
        !lens_transport_finite_vec3(bowl->center) ||
        !lens_transport_finite_vec3(bowl->axis) ||
        !lens_transport_finite_vec3(light->position)) {
        return false;
    }
    if (!active_sample) {
        RuntimeCausticLensTransport3D_DefaultSample(&default_sample);
        active_sample = &default_sample;
    }

    axis = vec3_normalize(bowl->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = bowl->radius;
    thickness = bowl->height;
    half_thickness = thickness * 0.5;
    bowl_depth = lens_transport_clamp(thickness * 0.42, thickness * 0.08, thickness * 0.70);
    ior = bowl->payload.opticalIor > 1.0001
              ? bowl->payload.opticalIor
              : fmax(bowl->payload.bsdf.ior, 1.0);
    if (!(ior > 1.0)) return false;

    rel_light = vec3_sub(bowl->center, light->position);
    optical_axis = vec3_sub(rel_light, vec3_scale(axis, vec3_dot(rel_light, axis)));
    if (!(vec3_length(optical_axis) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, &optical_axis);
    }
    optical_axis = vec3_normalize(optical_axis);
    basis_u = vec3_normalize(vec3_cross(axis, optical_axis));
    if (!(vec3_length(basis_u) > 1.0e-9)) {
        lens_transport_build_basis(axis, &basis_u, NULL);
    }
    basis_v = axis;

    lens_u = lens_transport_clamp(active_sample->lensU, -0.92, 0.92);
    lens_v = lens_transport_clamp(active_sample->lensV, -0.92, 0.92);
    r2 = lens_u * lens_u + lens_v * lens_v;
    if (r2 > 0.92 * 0.92) {
        radial_scale = 0.92 / sqrt(r2);
        lens_u *= radial_scale;
        lens_v *= radial_scale;
        r2 = lens_u * lens_u + lens_v * lens_v;
    }
    front_rim_offset = -half_thickness;
    sag = bowl_depth * (1.0 - r2);
    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u,
                            lens_transport_clamp(active_sample->apertureU, -1.0, 1.0) *
                                fmax(light->radius, 0.0)),
                 vec3_scale(basis_v,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0))));
    entry_position = vec3_add(
        bowl->center,
        vec3_add(vec3_scale(optical_axis, front_rim_offset + sag),
                 vec3_add(vec3_scale(basis_u, lens_u * radius),
                          vec3_scale(basis_v, lens_v * radius))));
    entry_normal = vec3_normalize(vec3_add(
        vec3_scale(optical_axis, -1.0),
        vec3_add(vec3_scale(basis_u, -2.0 * bowl_depth * lens_u / radius),
                 vec3_scale(basis_v, -2.0 * bowl_depth * lens_v / radius))));
    if (!(vec3_length(entry_normal) > 1.0e-9)) return false;

    {
        Vec3 ray_dir = vec3_normalize(vec3_sub(entry_position, ray_origin));
        if (!(vec3_length(ray_dir) > 1.0e-9)) return false;
        entry_fresnel =
            RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir, entry_normal, 1.0, ior);
        if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                                   entry_normal,
                                                   1.0,
                                                   ior,
                                                   &inside_dir,
                                                   &tir)) {
            return false;
        }
        RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
        entry_event.position = entry_position;
        entry_event.normal = entry_normal;
        entry_event.incidentDirection = ray_dir;
        entry_event.outgoingDirection = inside_dir;
        entry_event.etaFrom = 1.0;
        entry_event.etaTo = ior;
        entry_event.fresnel = entry_fresnel;
        entry_event.refracted = true;
    }

    exit_plane_point = vec3_add(bowl->center, vec3_scale(optical_axis, half_thickness));
    exit_normal = optical_axis;
    inside_origin = vec3_add(entry_position, vec3_scale(inside_dir, 1.0e-5));
    if (!lens_transport_intersect_plane(exit_plane_point,
                                        exit_normal,
                                        inside_origin,
                                        inside_dir,
                                        1.0e-5,
                                        &exit_t,
                                        &exit_position)) {
        return false;
    }
    exit_radial_u = vec3_dot(vec3_sub(exit_position, bowl->center), basis_u);
    exit_radial_v = vec3_dot(vec3_sub(exit_position, bowl->center), basis_v);
    if (exit_radial_u * exit_radial_u + exit_radial_v * exit_radial_v >
        radius * radius * 1.35 * 1.35) {
        return false;
    }
    exit_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir, exit_normal, ior, 1.0);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               ior,
                                               1.0,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 4.5;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          entry_fresnel);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput,
                                                                          exit_fresnel);
    tint = vec3(bowl->payload.baseColorR,
                bowl->payload.baseColorG,
                bowl->payload.baseColorB);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTint(throughput,
                                                                   tint,
                                                                   exit_t,
                                                                   bowl->payload.absorptionDistance);

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_BOWL;
    path.sceneObjectIndex = bowl->sceneObjectIndex;
    path.primitiveIndex = bowl->primitiveIndex;
    path.lightSamplePosition = ray_origin;
    path.targetPosition = entry_position;
    path.postExitOrigin = exit_position;
    path.postExitDirection = exit_dir;
    path.throughput = throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.receiverPlaneT = receiver_distance;
    path.receiverCrossing = vec3_add(exit_position, vec3_scale(exit_dir, receiver_distance));

    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }
    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = exit_position;
    exit_event.normal = exit_normal;
    exit_event.incidentDirection = inside_dir;
    exit_event.outgoingDirection = exit_dir;
    exit_event.etaFrom = ior;
    exit_event.etaTo = 1.0;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = exit_t;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}
