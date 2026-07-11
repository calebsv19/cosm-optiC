#include "render/runtime_caustic_lens_transport_3d.h"

#include <math.h>

#include "render/runtime_caustic_lens_transport_internal_3d.h"

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
    RuntimeCausticLensTraversalProfile3D traversal_profile;
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
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(&cylinder->payload,
                                                                     &traversal_profile);
    if (cylinder->hasTraversalProfileOverride) {
        traversal_profile = cylinder->traversalProfileOverride;
        traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
        RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    }

    axis = vec3_normalize(cylinder->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = cylinder->radius;
    half_height = cylinder->height * 0.5;
    ior = traversal_profile.materialIor;
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
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale),
                 vec3_scale(axis,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale)));
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
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir,
                                                     entry_normal,
                                                     traversal_profile.outsideIor,
                                                     traversal_profile.materialIor);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               traversal_profile.outsideIor,
                                               traversal_profile.materialIor,
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
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir,
                                                     exit_normal,
                                                     traversal_profile.materialIor,
                                                     traversal_profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               traversal_profile.materialIor,
                                               traversal_profile.outsideIor,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 3.0;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        entry_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        exit_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        throughput,
        exit_t,
        &traversal_profile);

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
    path.traversalProfile = traversal_profile;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = traversal_profile.outsideIor;
    entry_event.etaTo = traversal_profile.materialIor;
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
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
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
    RuntimeCausticLensTraversalProfile3D traversal_profile;
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
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(&prism->payload,
                                                                     &traversal_profile);
    if (prism->hasTraversalProfileOverride) {
        traversal_profile = prism->traversalProfileOverride;
        traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
        RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    }

    axis = vec3_normalize(prism->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = prism->radius;
    half_height = prism->height * 0.5;
    ior = traversal_profile.materialIor;
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
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale),
                 vec3_scale(axis,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale)));
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
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir,
                                                     entry_normal,
                                                     traversal_profile.outsideIor,
                                                     traversal_profile.materialIor);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               traversal_profile.outsideIor,
                                               traversal_profile.materialIor,
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
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir,
                                                     exit_normal,
                                                     traversal_profile.materialIor,
                                                     traversal_profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               traversal_profile.materialIor,
                                               traversal_profile.outsideIor,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 4.0;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        entry_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        exit_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        throughput,
        exit_t,
        &traversal_profile);

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
    path.traversalProfile = traversal_profile;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = traversal_profile.outsideIor;
    entry_event.etaTo = traversal_profile.materialIor;
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
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
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
    RuntimeCausticLensTraversalProfile3D traversal_profile;
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
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(&bowl->payload,
                                                                     &traversal_profile);
    if (bowl->hasTraversalProfileOverride) {
        traversal_profile = bowl->traversalProfileOverride;
        traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
        RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    }

    axis = vec3_normalize(bowl->axis);
    if (!(vec3_length(axis) > 1.0e-9)) return false;
    radius = bowl->radius;
    thickness = bowl->height;
    half_thickness = thickness * 0.5;
    bowl_depth = lens_transport_clamp(thickness * 0.42, thickness * 0.08, thickness * 0.70);
    ior = traversal_profile.materialIor;
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
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale),
                 vec3_scale(basis_v,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale)));
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
            RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir,
                                                         entry_normal,
                                                         traversal_profile.outsideIor,
                                                         traversal_profile.materialIor);
        if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                                   entry_normal,
                                                   traversal_profile.outsideIor,
                                                   traversal_profile.materialIor,
                                                   &inside_dir,
                                                   &tir)) {
            return false;
        }
        RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
        entry_event.position = entry_position;
        entry_event.normal = entry_normal;
        entry_event.incidentDirection = ray_dir;
        entry_event.outgoingDirection = inside_dir;
        entry_event.etaFrom = traversal_profile.outsideIor;
        entry_event.etaTo = traversal_profile.materialIor;
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
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir,
                                                     exit_normal,
                                                     traversal_profile.materialIor,
                                                     traversal_profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               traversal_profile.materialIor,
                                               traversal_profile.outsideIor,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : radius * 4.5;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        entry_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        exit_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        throughput,
        exit_t,
        &traversal_profile);

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
    path.traversalProfile = traversal_profile;

    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }
    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = exit_position;
    exit_event.normal = exit_normal;
    exit_event.incidentDirection = inside_dir;
    exit_event.outgoingDirection = exit_dir;
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = exit_t;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}

bool RuntimeCausticLensTransport3D_SolveMeshDielectricPath(
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path) {
    RuntimeCausticLensSample3D default_sample;
    const RuntimeCausticLensSample3D* active_sample = sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    RuntimeCausticLensTraversalProfile3D traversal_profile;
    Vec3 edge_u = vec3(0.0, 0.0, 0.0);
    Vec3 edge_v = vec3(0.0, 0.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 basis_v = vec3(0.0, 0.0, 1.0);
    Vec3 plane_normal = vec3(0.0, 1.0, 0.0);
    Vec3 entry_normal = vec3(0.0, 1.0, 0.0);
    Vec3 exit_normal = vec3(0.0, -1.0, 0.0);
    Vec3 centroid = vec3(0.0, 0.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 target = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    Vec3 entry_position = vec3(0.0, 0.0, 0.0);
    Vec3 inside_dir = vec3(0.0, 0.0, 0.0);
    Vec3 exit_position = vec3(0.0, 0.0, 0.0);
    Vec3 exit_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);
    double aperture_radius = 0.0;
    double thickness = 0.0;
    double entry_t = 0.0;
    double exit_t = 0.0;
    double entry_fresnel = 1.0;
    double exit_fresnel = 1.0;
    double sample_weight = 1.0;
    double receiver_distance = 0.0;
    bool tir = false;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!mesh_dielectric || !entry_triangle || !light || !out_path ||
        mesh_dielectric->kind != RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC ||
        !lens_transport_finite_vec3(entry_triangle->p0) ||
        !lens_transport_finite_vec3(entry_triangle->p1) ||
        !lens_transport_finite_vec3(entry_triangle->p2) ||
        !lens_transport_finite_vec3(light->position)) {
        return false;
    }
    if (!active_sample) {
        RuntimeCausticLensTransport3D_DefaultSample(&default_sample);
        active_sample = &default_sample;
    }
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(
        &mesh_dielectric->payload,
        &traversal_profile);
    if (mesh_dielectric->hasTraversalProfileOverride) {
        traversal_profile = mesh_dielectric->traversalProfileOverride;
        traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
        RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    }
    if (!(traversal_profile.materialIor > 1.0e-6) ||
        !(traversal_profile.outsideIor > 1.0e-6)) {
        return false;
    }

    edge_u = vec3_sub(entry_triangle->p1, entry_triangle->p0);
    edge_v = vec3_sub(entry_triangle->p2, entry_triangle->p0);
    plane_normal = vec3_normalize(entry_triangle->normal);
    if (!(vec3_length(plane_normal) > 1.0e-9)) {
        plane_normal = vec3_normalize(vec3_cross(edge_u, edge_v));
    }
    if (!(vec3_length(edge_u) > 1.0e-9) ||
        !(vec3_length(edge_v) > 1.0e-9) ||
        !(vec3_length(plane_normal) > 1.0e-9)) {
        return false;
    }
    basis_u = vec3_normalize(edge_u);
    basis_v = vec3_normalize(vec3_cross(plane_normal, basis_u));
    if (!(vec3_length(basis_v) > 1.0e-9)) {
        lens_transport_build_basis(plane_normal, &basis_u, &basis_v);
    }
    centroid = vec3_scale(vec3_add(vec3_add(entry_triangle->p0, entry_triangle->p1),
                                   entry_triangle->p2),
                          1.0 / 3.0);
    aperture_radius = mesh_dielectric->radius > 1.0e-9 ? mesh_dielectric->radius : 0.25;
    thickness = mesh_dielectric->height > 1.0e-9 ? mesh_dielectric->height : aperture_radius;
    target = vec3_add(
        centroid,
        vec3_add(vec3_scale(basis_u,
                            lens_transport_clamp(active_sample->lensU, -0.95, 0.95) *
                                aperture_radius * 0.35 *
                                traversal_profile.apertureRadiusScale),
                 vec3_scale(basis_v,
                            lens_transport_clamp(active_sample->lensV, -0.95, 0.95) *
                                aperture_radius * 0.35 *
                                traversal_profile.apertureRadiusScale)));
    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u,
                            lens_transport_clamp(active_sample->apertureU, -1.0, 1.0) *
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale),
                 vec3_scale(basis_v,
                            lens_transport_clamp(active_sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0) *
                                traversal_profile.apertureRadiusScale)));
    ray_dir = vec3_normalize(vec3_sub(target, ray_origin));
    if (!(vec3_length(ray_dir) > 1.0e-9)) return false;

    entry_normal = plane_normal;
    if (vec3_dot(entry_normal, ray_dir) > 0.0) {
        entry_normal = vec3_scale(entry_normal, -1.0);
    }
    if (!lens_transport_intersect_plane(centroid,
                                        entry_normal,
                                        ray_origin,
                                        ray_dir,
                                        1.0e-6,
                                        &entry_t,
                                        &entry_position)) {
        return false;
    }
    entry_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir,
                                                     entry_normal,
                                                     traversal_profile.outsideIor,
                                                     traversal_profile.materialIor);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               traversal_profile.outsideIor,
                                               traversal_profile.materialIor,
                                               &inside_dir,
                                               &tir)) {
        return false;
    }
    exit_t = thickness / fmax(-vec3_dot(inside_dir, entry_normal), 1.0e-6);
    if (!(exit_t > 1.0e-6) || !isfinite(exit_t)) return false;
    exit_position = vec3_add(entry_position, vec3_scale(inside_dir, exit_t));
    exit_normal = vec3_scale(entry_normal, -1.0);
    exit_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir,
                                                     exit_normal,
                                                     traversal_profile.materialIor,
                                                     traversal_profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               traversal_profile.materialIor,
                                               traversal_profile.outsideIor,
                                               &exit_dir,
                                               &tir)) {
        return false;
    }

    sample_weight = fmax(active_sample->sampleWeight, 0.0);
    receiver_distance = active_sample->receiverDistance > 1.0e-9
                            ? active_sample->receiverDistance
                            : aperture_radius * 4.0;
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        entry_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        exit_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        throughput,
        exit_t,
        &traversal_profile);

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    path.sceneObjectIndex = mesh_dielectric->sceneObjectIndex;
    path.primitiveIndex = mesh_dielectric->primitiveIndex;
    path.lightSamplePosition = ray_origin;
    path.targetPosition = target;
    path.postExitOrigin = exit_position;
    path.postExitDirection = exit_dir;
    path.throughput = throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.receiverPlaneT = receiver_distance;
    path.receiverCrossing = vec3_add(exit_position, vec3_scale(exit_dir, receiver_distance));
    path.traversalProfile = traversal_profile;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = traversal_profile.outsideIor;
    entry_event.etaTo = traversal_profile.materialIor;
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
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = exit_t;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    return true;
}
