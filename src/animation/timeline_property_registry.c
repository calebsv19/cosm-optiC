#include "animation/timeline_property_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool timeline_property_id_is_valid(const char* id) {
    size_t length = 0u;
    if (!id || id[0] == '\0') return false;
    while (length < TIMELINE_ID_CAPACITY && id[length] != '\0') length += 1u;
    return length > 0u && length < TIMELINE_ID_CAPACITY;
}

static bool timeline_property_target_kind_is_valid(TimelinePropertyTargetKind kind) {
    return kind >= TIMELINE_PROPERTY_TARGET_OBJECT &&
           kind <= TIMELINE_PROPERTY_TARGET_VOLUME_RESERVED;
}

static bool timeline_property_access_is_valid(TimelinePropertyAccess access) {
    return access >= TIMELINE_PROPERTY_ACCESS_AUTHORABLE &&
           access <= TIMELINE_PROPERTY_ACCESS_STATIC_READ_ONLY;
}

static bool timeline_property_interpolation_mask_is_valid(uint32_t mask) {
    const uint32_t supported = TIMELINE_INTERPOLATION_MASK_STEP |
                               TIMELINE_INTERPOLATION_MASK_LINEAR |
                               TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER;
    return mask != TIMELINE_INTERPOLATION_MASK_NONE && (mask & ~supported) == 0u;
}

static bool timeline_property_invalidation_mask_is_valid(uint32_t mask) {
    const uint32_t supported = TIMELINE_INVALIDATION_CAMERA |
                               TIMELINE_INVALIDATION_LIGHTING |
                               TIMELINE_INVALIDATION_MATERIAL |
                               TIMELINE_INVALIDATION_RIGID_TRANSFORM |
                               TIMELINE_INVALIDATION_DEFORMING_GEOMETRY |
                               TIMELINE_INVALIDATION_VOLUME |
                               TIMELINE_INVALIDATION_SIMULATION_CACHE;
    return mask != TIMELINE_INVALIDATION_NONE && (mask & ~supported) == 0u;
}

static uint32_t timeline_property_interpolation_bit(TimelineInterpolation interpolation) {
    switch (interpolation) {
        case TIMELINE_INTERPOLATION_STEP: return TIMELINE_INTERPOLATION_MASK_STEP;
        case TIMELINE_INTERPOLATION_LINEAR: return TIMELINE_INTERPOLATION_MASK_LINEAR;
        case TIMELINE_INTERPOLATION_CUBIC_BEZIER:
            return TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER;
        default: return TIMELINE_INTERPOLATION_MASK_NONE;
    }
}

static bool timeline_property_value_less(TimelineValue a, TimelineValue b) {
    if (a.type == TIMELINE_VALUE_SCALAR) return a.as.scalar < b.as.scalar;
    if (a.type == TIMELINE_VALUE_VEC3) {
        return a.as.vec3.x < b.as.vec3.x || a.as.vec3.y < b.as.vec3.y ||
               a.as.vec3.z < b.as.vec3.z;
    }
    return false;
}

static bool timeline_property_value_greater(TimelineValue a, TimelineValue b) {
    if (a.type == TIMELINE_VALUE_SCALAR) return a.as.scalar > b.as.scalar;
    if (a.type == TIMELINE_VALUE_VEC3) {
        return a.as.vec3.x > b.as.vec3.x || a.as.vec3.y > b.as.vec3.y ||
               a.as.vec3.z > b.as.vec3.z;
    }
    return false;
}

static bool timeline_property_bounds_are_ordered(TimelineValue minimum,
                                                 TimelineValue maximum) {
    if (minimum.type != maximum.type) return false;
    if (minimum.type == TIMELINE_VALUE_SCALAR) {
        return minimum.as.scalar <= maximum.as.scalar;
    }
    if (minimum.type == TIMELINE_VALUE_VEC3) {
        return minimum.as.vec3.x <= maximum.as.vec3.x &&
               minimum.as.vec3.y <= maximum.as.vec3.y &&
               minimum.as.vec3.z <= maximum.as.vec3.z;
    }
    return false;
}

static const char* timeline_property_target_prefix(TimelinePropertyTargetKind kind) {
    switch (kind) {
        case TIMELINE_PROPERTY_TARGET_OBJECT: return "object/";
        case TIMELINE_PROPERTY_TARGET_LIGHT: return "light/";
        case TIMELINE_PROPERTY_TARGET_MATERIAL: return "material/";
        case TIMELINE_PROPERTY_TARGET_CAMERA_RESERVED: return "camera/";
        case TIMELINE_PROPERTY_TARGET_VOLUME_RESERVED: return "volume/";
        default: return NULL;
    }
}

static bool timeline_property_target_matches(TimelinePropertyTargetKind kind,
                                             const char* target_id) {
    const char* prefix = timeline_property_target_prefix(kind);
    size_t prefix_length;
    if (!prefix || !timeline_property_id_is_valid(target_id)) return false;
    prefix_length = strlen(prefix);
    return strncmp(target_id, prefix, prefix_length) == 0 &&
           target_id[prefix_length] != '\0';
}

const char* TimelinePropertyTargetKindLabel(TimelinePropertyTargetKind kind) {
    switch (kind) {
        case TIMELINE_PROPERTY_TARGET_OBJECT: return "object";
        case TIMELINE_PROPERTY_TARGET_LIGHT: return "light";
        case TIMELINE_PROPERTY_TARGET_MATERIAL: return "material";
        case TIMELINE_PROPERTY_TARGET_CAMERA: return "camera";
        case TIMELINE_PROPERTY_TARGET_VOLUME_RESERVED: return "volume_reserved";
        default: return "unknown";
    }
}

const char* TimelinePropertyAccessLabel(TimelinePropertyAccess access) {
    switch (access) {
        case TIMELINE_PROPERTY_ACCESS_AUTHORABLE: return "authorable";
        case TIMELINE_PROPERTY_ACCESS_SIMULATION_OWNED: return "simulation_owned";
        case TIMELINE_PROPERTY_ACCESS_DERIVED_READ_ONLY: return "derived_read_only";
        case TIMELINE_PROPERTY_ACCESS_STATIC_READ_ONLY: return "static_read_only";
        default: return "unknown";
    }
}

TimelineStatus TimelinePropertyDescriptorInit(
    TimelinePropertyDescriptor* descriptor,
    const char* property_id,
    TimelinePropertyTargetKind target_kind,
    TimelineValueType value_type,
    TimelineUnit unit,
    TimelinePropertyAccess access,
    uint32_t interpolation_mask,
    uint32_t invalidation_domains) {
    TimelinePropertyDescriptor candidate;
    if (!descriptor) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!timeline_property_id_is_valid(property_id) ||
        !timeline_property_target_kind_is_valid(target_kind) ||
        (value_type != TIMELINE_VALUE_SCALAR && value_type != TIMELINE_VALUE_VEC3) ||
        !TimelineUnitIsValid(unit) || unit == TIMELINE_UNIT_UNSPECIFIED ||
        !timeline_property_access_is_valid(access) ||
        !timeline_property_interpolation_mask_is_valid(interpolation_mask) ||
        !timeline_property_invalidation_mask_is_valid(invalidation_domains)) {
        return TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR;
    }
    memset(&candidate, 0, sizeof(candidate));
    snprintf(candidate.property_id, sizeof(candidate.property_id), "%s", property_id);
    candidate.target_kind = target_kind;
    candidate.value_type = value_type;
    candidate.unit = unit;
    candidate.access = access;
    candidate.interpolation_mask = interpolation_mask;
    candidate.invalidation_domains = invalidation_domains;
    *descriptor = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyDescriptorSetBounds(
    TimelinePropertyDescriptor* descriptor,
    const TimelineValue* minimum,
    const TimelineValue* maximum) {
    TimelinePropertyDescriptor candidate;
    if (!descriptor || (!minimum && !maximum)) return TIMELINE_STATUS_INVALID_ARGUMENT;
    candidate = *descriptor;
    if (minimum) {
        if (minimum->type != candidate.value_type || !TimelineValueIsFinite(*minimum)) {
            return TIMELINE_STATUS_TYPE_MISMATCH;
        }
        candidate.has_minimum = true;
        candidate.minimum = *minimum;
    }
    if (maximum) {
        if (maximum->type != candidate.value_type || !TimelineValueIsFinite(*maximum)) {
            return TIMELINE_STATUS_TYPE_MISMATCH;
        }
        candidate.has_maximum = true;
        candidate.maximum = *maximum;
    }
    if (candidate.has_minimum && candidate.has_maximum &&
        !timeline_property_bounds_are_ordered(candidate.minimum, candidate.maximum)) {
        return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
    }
    *descriptor = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyDescriptorValidate(
    const TimelinePropertyDescriptor* descriptor) {
    if (!descriptor || !timeline_property_id_is_valid(descriptor->property_id) ||
        !timeline_property_target_kind_is_valid(descriptor->target_kind) ||
        (descriptor->value_type != TIMELINE_VALUE_SCALAR &&
         descriptor->value_type != TIMELINE_VALUE_VEC3) ||
        !TimelineUnitIsValid(descriptor->unit) ||
        descriptor->unit == TIMELINE_UNIT_UNSPECIFIED ||
        !timeline_property_access_is_valid(descriptor->access) ||
        !timeline_property_interpolation_mask_is_valid(
            descriptor->interpolation_mask) ||
        !timeline_property_invalidation_mask_is_valid(
            descriptor->invalidation_domains)) {
        return TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR;
    }
    if (descriptor->has_minimum &&
        (descriptor->minimum.type != descriptor->value_type ||
         !TimelineValueIsFinite(descriptor->minimum))) {
        return TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR;
    }
    if (descriptor->has_maximum &&
        (descriptor->maximum.type != descriptor->value_type ||
         !TimelineValueIsFinite(descriptor->maximum))) {
        return TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR;
    }
    if (descriptor->has_minimum && descriptor->has_maximum &&
        !timeline_property_bounds_are_ordered(descriptor->minimum,
                                              descriptor->maximum)) {
        return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyDescriptorValidateValue(
    const TimelinePropertyDescriptor* descriptor,
    TimelineValue value) {
    TimelineStatus status = TimelinePropertyDescriptorValidate(descriptor);
    if (status != TIMELINE_STATUS_OK) return status;
    if (value.type != descriptor->value_type || !TimelineValueIsFinite(value)) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    if ((descriptor->has_minimum &&
         timeline_property_value_less(value, descriptor->minimum)) ||
        (descriptor->has_maximum &&
         timeline_property_value_greater(value, descriptor->maximum))) {
        return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyRegistryInit(TimelinePropertyRegistry* registry) {
    if (!registry) return TIMELINE_STATUS_INVALID_ARGUMENT;
    memset(registry, 0, sizeof(*registry));
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyRegistryAdd(
    TimelinePropertyRegistry* registry,
    const TimelinePropertyDescriptor* descriptor) {
    TimelineStatus status;
    if (!registry || !descriptor) return TIMELINE_STATUS_INVALID_ARGUMENT;
    status = TimelinePropertyDescriptorValidate(descriptor);
    if (status != TIMELINE_STATUS_OK) return status;
    if (registry->descriptor_count >= TIMELINE_PROPERTY_REGISTRY_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    for (size_t i = 0u; i < registry->descriptor_count; ++i) {
        if (strcmp(registry->descriptors[i].property_id, descriptor->property_id) == 0) {
            return TIMELINE_STATUS_DUPLICATE_ID;
        }
    }
    registry->descriptors[registry->descriptor_count++] = *descriptor;
    return TIMELINE_STATUS_OK;
}

static TimelineStatus timeline_property_add_default(
    TimelinePropertyRegistry* registry,
    const char* property_id,
    TimelinePropertyTargetKind target_kind,
    TimelineValueType value_type,
    TimelineUnit unit,
    uint32_t interpolation_mask,
    uint32_t invalidation_domains,
    const TimelineValue* minimum,
    const TimelineValue* maximum) {
    TimelinePropertyDescriptor descriptor;
    TimelineStatus status = TimelinePropertyDescriptorInit(
        &descriptor, property_id, target_kind, value_type, unit,
        TIMELINE_PROPERTY_ACCESS_AUTHORABLE,
        interpolation_mask,
        invalidation_domains);
    if (status != TIMELINE_STATUS_OK) return status;
    if (minimum || maximum) {
        status = TimelinePropertyDescriptorSetBounds(&descriptor, minimum, maximum);
        if (status != TIMELINE_STATUS_OK) return status;
    }
    return TimelinePropertyRegistryAdd(registry, &descriptor);
}

TimelineStatus TimelinePropertyRegistryInitFoundationDefaults(
    TimelinePropertyRegistry* registry) {
    TimelinePropertyRegistry candidate;
    TimelineValue zero = TimelineValueScalar(0.0);
    TimelineValue one = TimelineValueScalar(1.0);
    TimelineStatus status;
    if (!registry) return TIMELINE_STATUS_INVALID_ARGUMENT;
    TimelinePropertyRegistryInit(&candidate);
    status = timeline_property_add_default(
        &candidate, "object/transform/position", TIMELINE_PROPERTY_TARGET_OBJECT,
        TIMELINE_VALUE_VEC3, TIMELINE_UNIT_WORLD_DISTANCE,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR,
        TIMELINE_INVALIDATION_RIGID_TRANSFORM, NULL, NULL);
    if (status != TIMELINE_STATUS_OK) return status;
    const char* axes[]={"object/transform/position_x","object/transform/position_y","object/transform/position_z"};
    for(int axis=0;axis<3;++axis) {
        status=timeline_property_add_default(&candidate,axes[axis],TIMELINE_PROPERTY_TARGET_OBJECT,
            TIMELINE_VALUE_SCALAR,TIMELINE_UNIT_WORLD_DISTANCE,
            TIMELINE_INTERPOLATION_MASK_STEP|TIMELINE_INTERPOLATION_MASK_LINEAR|TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER,
            TIMELINE_INVALIDATION_RIGID_TRANSFORM,NULL,NULL);
        if(status!=TIMELINE_STATUS_OK) return status;
    }
    status = timeline_property_add_default(&candidate, "object/path_progress", TIMELINE_PROPERTY_TARGET_OBJECT,
        TIMELINE_VALUE_SCALAR,TIMELINE_UNIT_UNITLESS,
        TIMELINE_INTERPOLATION_MASK_STEP|TIMELINE_INTERPOLATION_MASK_LINEAR|TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER,
        TIMELINE_INVALIDATION_RIGID_TRANSFORM,&zero,&one);
    if(status!=TIMELINE_STATUS_OK)return status;
    status = timeline_property_add_default(
        &candidate, "light/intensity", TIMELINE_PROPERTY_TARGET_LIGHT,
        TIMELINE_VALUE_SCALAR, TIMELINE_UNIT_RELATIVE_INTENSITY,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR |
            TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER,
        TIMELINE_INVALIDATION_LIGHTING, &zero, NULL);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(
        &candidate, "light/path_progress", TIMELINE_PROPERTY_TARGET_LIGHT,
        TIMELINE_VALUE_SCALAR, TIMELINE_UNIT_UNITLESS,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR |
            TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER,
        TIMELINE_INVALIDATION_LIGHTING, &zero, &one);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(
        &candidate, "light/position", TIMELINE_PROPERTY_TARGET_LIGHT,
        TIMELINE_VALUE_VEC3, TIMELINE_UNIT_WORLD_DISTANCE,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR,
        TIMELINE_INVALIDATION_LIGHTING, NULL, NULL);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(
        &candidate, "material/roughness", TIMELINE_PROPERTY_TARGET_MATERIAL,
        TIMELINE_VALUE_SCALAR, TIMELINE_UNIT_UNITLESS,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR |
            TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER,
        TIMELINE_INVALIDATION_MATERIAL, &zero, &one);
    if (status != TIMELINE_STATUS_OK) return status;
    /* Camera channels describe authored meaning; runtime application remains
     * in the evaluated-scene adapter, never in this registry. */
    const uint32_t scalar_modes = TIMELINE_INTERPOLATION_MASK_STEP |
        TIMELINE_INTERPOLATION_MASK_LINEAR | TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER;
    const TimelineValue min_pitch = TimelineValueScalar(-1.5707963267948966);
    const TimelineValue max_pitch = TimelineValueScalar(1.5707963267948966);
    const TimelineValue min_fov = TimelineValueScalar(1.0);
    const TimelineValue max_fov = TimelineValueScalar(179.0);
    status = timeline_property_add_default(&candidate, "camera/path_progress",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_UNITLESS, scalar_modes, TIMELINE_INVALIDATION_CAMERA, &zero, &one);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "camera/position",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_VEC3,
        TIMELINE_UNIT_WORLD_DISTANCE,
        TIMELINE_INTERPOLATION_MASK_STEP | TIMELINE_INTERPOLATION_MASK_LINEAR,
        TIMELINE_INVALIDATION_CAMERA, NULL, NULL);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "camera/yaw",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_RADIANS, scalar_modes, TIMELINE_INVALIDATION_CAMERA, NULL, NULL);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "camera/pitch",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_RADIANS, scalar_modes, TIMELINE_INVALIDATION_CAMERA,
        &min_pitch, &max_pitch);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "camera/fov_y",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_DEGREES, scalar_modes, TIMELINE_INVALIDATION_CAMERA,
        &min_fov, &max_fov);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "camera/route_progress",
        TIMELINE_PROPERTY_TARGET_CAMERA, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_UNITLESS, scalar_modes, TIMELINE_INVALIDATION_CAMERA, &zero, &one);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_property_add_default(&candidate, "light/route_progress",
        TIMELINE_PROPERTY_TARGET_LIGHT, TIMELINE_VALUE_SCALAR,
        TIMELINE_UNIT_UNITLESS, scalar_modes, TIMELINE_INVALIDATION_LIGHTING, &zero, &one);
    if (status != TIMELINE_STATUS_OK) return status;
    *registry = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyRegistryFind(
    const TimelinePropertyRegistry* registry,
    const char* property_id,
    const TimelinePropertyDescriptor** out_descriptor) {
    if (!registry || !property_id || !out_descriptor) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (registry->descriptor_count > TIMELINE_PROPERTY_REGISTRY_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    for (size_t i = 0u; i < registry->descriptor_count; ++i) {
        if (strcmp(registry->descriptors[i].property_id, property_id) == 0) {
            *out_descriptor = &registry->descriptors[i];
            return TIMELINE_STATUS_OK;
        }
    }
    return TIMELINE_STATUS_UNKNOWN_PROPERTY;
}

TimelineStatus TimelinePropertyRegistryValidateTrack(
    const TimelinePropertyRegistry* registry,
    const TimelineTrack* track,
    const TimelineRange* range) {
    const TimelinePropertyDescriptor* descriptor = NULL;
    TimelineStatus status;
    if (!registry || !track || !range) return TIMELINE_STATUS_INVALID_ARGUMENT;
    status = TimelinePropertyRegistryFind(registry, track->property_id, &descriptor);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelineTrackValidate(track, range);
    if (status != TIMELINE_STATUS_OK) return status;
    if (!timeline_property_target_matches(descriptor->target_kind, track->target_id)) {
        return TIMELINE_STATUS_TARGET_KIND_MISMATCH;
    }
    if (track->value_type != descriptor->value_type) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    if (track->unit != descriptor->unit) return TIMELINE_STATUS_UNIT_MISMATCH;
    if (descriptor->access != TIMELINE_PROPERTY_ACCESS_AUTHORABLE ||
        track->source != TIMELINE_CHANNEL_SOURCE_AUTHORED) {
        return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    }
    for (size_t i = 0u; i < track->key_count; ++i) {
        const uint32_t interpolation_bit =
            timeline_property_interpolation_bit(track->keys[i].interpolation_to_next);
        if (interpolation_bit == TIMELINE_INTERPOLATION_MASK_NONE ||
            (descriptor->interpolation_mask & interpolation_bit) == 0u) {
            return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
        }
        /* New automatic policies must not create invalid bounded values between
         * valid keys. Preserve legacy/manual handle acceptance unchanged. Test
         * cubic value extrema analytically; time handles are monotonic. */
        if(i+1<track->key_count && track->value_type==TIMELINE_VALUE_SCALAR &&
           track->keys[i].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
           (track->keys[i].tangent_mode!=TIMELINE_TANGENT_BROKEN || track->keys[i+1].tangent_mode!=TIMELINE_TANGENT_BROKEN)) {
            const TimelineKeyframe *a=&track->keys[i],*b=&track->keys[i+1];
            double y0=a->value.as.scalar,y1=y0+a->outgoing_value_offset;
            double y3=b->value.as.scalar,y2=y3+b->incoming_value_offset;
            double A=-y0+3*y1-3*y2+y3,B=2*(y0-2*y1+y2),C=y1-y0;
            double roots[2]={-1,-1};
            if(fabs(A)<1e-15) {if(fabs(B)>1e-15)roots[0]=-C/B;}
            else {double d=B*B-4*A*C;if(d>=0){roots[0]=(-B+sqrt(d))/(2*A);roots[1]=(-B-sqrt(d))/(2*A);}}
            for(int r=0;r<2;++r)if(roots[r]>0 && roots[r]<1) {
                double u=roots[r],v=1-u,y=v*v*v*y0+3*v*v*u*y1+3*v*u*u*y2+u*u*u*y3;
                status=TimelinePropertyDescriptorValidateValue(descriptor,TimelineValueScalar(y));
                if(status!=TIMELINE_STATUS_OK)return status;
            }
        }
        status = TimelinePropertyDescriptorValidateValue(descriptor,
                                                         track->keys[i].value);
        if (status != TIMELINE_STATUS_OK) return status;
    }
    return TIMELINE_STATUS_OK;
}

static bool timeline_property_position_driver(const char* property) {
    return strcmp(property,"light/route_progress")==0 || strcmp(property,"camera/route_progress")==0 ||
        strcmp(property,"camera/position")==0 ||
        strcmp(property,"camera/path_progress")==0 ||
        strcmp(property,"light/position")==0 ||
        strcmp(property,"light/path_progress")==0;
}

TimelineStatus TimelinePropertyRegistryValidateDocument(
    const TimelinePropertyRegistry* registry,
    const TimelineDocument* document) {
    TimelineStatus status;
    if (!registry || !document) return TIMELINE_STATUS_INVALID_ARGUMENT;
    status = TimelineDocumentValidate(document);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0u; i < document->track_count; ++i) {
        status = TimelinePropertyRegistryValidateTrack(
            registry, &document->tracks[i], &document->range);
        if (status != TIMELINE_STATUS_OK) return status;
        for (size_t j = i + 1u; j < document->track_count; ++j) {
            if(document->tracks[i].enabled && document->tracks[j].enabled &&
               strcmp(document->tracks[i].target_id,document->tracks[j].target_id)==0 &&
               timeline_property_position_driver(document->tracks[i].property_id) &&
               timeline_property_position_driver(document->tracks[j].property_id))
                return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
            if (strcmp(document->tracks[i].target_id,
                       document->tracks[j].target_id) == 0 &&
                strcmp(document->tracks[i].property_id,
                       document->tracks[j].property_id) == 0) {
                return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
            }
        }
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelinePropertyRegistryEvaluateDocument(
    const TimelinePropertyRegistry* registry,
    const TimelineDocument* document,
    const TimelineEvaluationContext* context,
    TimelinePropertyEvaluationResult* out_results,
    size_t result_capacity,
    size_t* out_result_count) {
    TimelineEvaluationResult track_results[TIMELINE_DOCUMENT_TRACK_CAPACITY];
    TimelinePropertyEvaluationResult results[TIMELINE_DOCUMENT_TRACK_CAPACITY];
    size_t result_count = 0u;
    TimelineStatus status;
    if (!registry || !document || !context || !out_result_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = TimelinePropertyRegistryValidateDocument(registry, document);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0u; i < document->track_count; ++i) {
        if (document->tracks[i].enabled) result_count += 1u;
    }
    if ((result_count > 0u && !out_results) || result_capacity < result_count) {
        return result_count > 0u && !out_results
                   ? TIMELINE_STATUS_INVALID_ARGUMENT
                   : TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    result_count = 0u;
    status = TimelineDocumentEvaluate(document, context, track_results,
                                      TIMELINE_DOCUMENT_TRACK_CAPACITY,
                                      &result_count);
    if (status != TIMELINE_STATUS_OK) return status;
    memset(results, 0, sizeof(results));
    for (size_t i = 0u; i < result_count; ++i) {
        const TimelinePropertyDescriptor* descriptor = NULL;
        status = TimelinePropertyRegistryFind(registry,
                                              track_results[i].property_id,
                                              &descriptor);
        if (status != TIMELINE_STATUS_OK) return status;
        status = TimelinePropertyDescriptorValidateValue(descriptor,
                                                         track_results[i].value);
        if (status != TIMELINE_STATUS_OK) return status;
        results[i].track = track_results[i];
        results[i].target_kind = descriptor->target_kind;
        results[i].unit = descriptor->unit;
        results[i].access = descriptor->access;
        results[i].invalidation_domains = descriptor->invalidation_domains;
    }
    if (result_count > 0u) {
        memcpy(out_results, results, result_count * sizeof(results[0]));
    }
    *out_result_count = result_count;
    return TIMELINE_STATUS_OK;
}
