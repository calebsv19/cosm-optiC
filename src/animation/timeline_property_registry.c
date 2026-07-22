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
        case TIMELINE_PROPERTY_TARGET_CAMERA_RESERVED: return "camera_reserved";
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
        status = TimelinePropertyDescriptorValidateValue(descriptor,
                                                         track->keys[i].value);
        if (status != TIMELINE_STATUS_OK) return status;
    }
    return TIMELINE_STATUS_OK;
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
