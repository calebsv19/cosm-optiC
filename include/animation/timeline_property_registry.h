#ifndef RAY_TRACING_TIMELINE_PROPERTY_REGISTRY_H
#define RAY_TRACING_TIMELINE_PROPERTY_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "animation/timeline_document.h"

#define TIMELINE_PROPERTY_REGISTRY_CAPACITY 64u

typedef enum TimelinePropertyTargetKind {
    TIMELINE_PROPERTY_TARGET_OBJECT = 0,
    TIMELINE_PROPERTY_TARGET_LIGHT,
    TIMELINE_PROPERTY_TARGET_MATERIAL,
    TIMELINE_PROPERTY_TARGET_CAMERA_RESERVED,
    TIMELINE_PROPERTY_TARGET_VOLUME_RESERVED
} TimelinePropertyTargetKind;

typedef enum TimelinePropertyAccess {
    TIMELINE_PROPERTY_ACCESS_AUTHORABLE = 0,
    TIMELINE_PROPERTY_ACCESS_SIMULATION_OWNED,
    TIMELINE_PROPERTY_ACCESS_DERIVED_READ_ONLY,
    TIMELINE_PROPERTY_ACCESS_STATIC_READ_ONLY
} TimelinePropertyAccess;

typedef enum TimelineInvalidationDomain {
    TIMELINE_INVALIDATION_NONE = 0u,
    TIMELINE_INVALIDATION_CAMERA = 1u << 0,
    TIMELINE_INVALIDATION_LIGHTING = 1u << 1,
    TIMELINE_INVALIDATION_MATERIAL = 1u << 2,
    TIMELINE_INVALIDATION_RIGID_TRANSFORM = 1u << 3,
    TIMELINE_INVALIDATION_DEFORMING_GEOMETRY = 1u << 4,
    TIMELINE_INVALIDATION_VOLUME = 1u << 5,
    TIMELINE_INVALIDATION_SIMULATION_CACHE = 1u << 6
} TimelineInvalidationDomain;

typedef enum TimelineInterpolationMask {
    TIMELINE_INTERPOLATION_MASK_NONE = 0u,
    TIMELINE_INTERPOLATION_MASK_STEP = 1u << 0,
    TIMELINE_INTERPOLATION_MASK_LINEAR = 1u << 1,
    TIMELINE_INTERPOLATION_MASK_CUBIC_BEZIER = 1u << 2
} TimelineInterpolationMask;

typedef struct TimelinePropertyDescriptor {
    char property_id[TIMELINE_ID_CAPACITY];
    TimelinePropertyTargetKind target_kind;
    TimelineValueType value_type;
    TimelineUnit unit;
    TimelinePropertyAccess access;
    uint32_t interpolation_mask;
    uint32_t invalidation_domains;
    bool has_minimum;
    bool has_maximum;
    TimelineValue minimum;
    TimelineValue maximum;
} TimelinePropertyDescriptor;

typedef struct TimelinePropertyRegistry {
    size_t descriptor_count;
    TimelinePropertyDescriptor descriptors[TIMELINE_PROPERTY_REGISTRY_CAPACITY];
} TimelinePropertyRegistry;

typedef struct TimelinePropertyEvaluationResult {
    TimelineEvaluationResult track;
    TimelinePropertyTargetKind target_kind;
    TimelineUnit unit;
    TimelinePropertyAccess access;
    uint32_t invalidation_domains;
} TimelinePropertyEvaluationResult;

const char* TimelinePropertyTargetKindLabel(TimelinePropertyTargetKind kind);
const char* TimelinePropertyAccessLabel(TimelinePropertyAccess access);
TimelineStatus TimelinePropertyDescriptorInit(
    TimelinePropertyDescriptor* descriptor,
    const char* property_id,
    TimelinePropertyTargetKind target_kind,
    TimelineValueType value_type,
    TimelineUnit unit,
    TimelinePropertyAccess access,
    uint32_t interpolation_mask,
    uint32_t invalidation_domains);
TimelineStatus TimelinePropertyDescriptorSetBounds(
    TimelinePropertyDescriptor* descriptor,
    const TimelineValue* minimum,
    const TimelineValue* maximum);
TimelineStatus TimelinePropertyDescriptorValidate(
    const TimelinePropertyDescriptor* descriptor);
TimelineStatus TimelinePropertyDescriptorValidateValue(
    const TimelinePropertyDescriptor* descriptor,
    TimelineValue value);
TimelineStatus TimelinePropertyRegistryInit(TimelinePropertyRegistry* registry);
TimelineStatus TimelinePropertyRegistryInitFoundationDefaults(
    TimelinePropertyRegistry* registry);
TimelineStatus TimelinePropertyRegistryAdd(
    TimelinePropertyRegistry* registry,
    const TimelinePropertyDescriptor* descriptor);
TimelineStatus TimelinePropertyRegistryFind(
    const TimelinePropertyRegistry* registry,
    const char* property_id,
    const TimelinePropertyDescriptor** out_descriptor);
TimelineStatus TimelinePropertyRegistryValidateTrack(
    const TimelinePropertyRegistry* registry,
    const TimelineTrack* track,
    const TimelineRange* range);
TimelineStatus TimelinePropertyRegistryValidateDocument(
    const TimelinePropertyRegistry* registry,
    const TimelineDocument* document);
TimelineStatus TimelinePropertyRegistryEvaluateDocument(
    const TimelinePropertyRegistry* registry,
    const TimelineDocument* document,
    const TimelineEvaluationContext* context,
    TimelinePropertyEvaluationResult* out_results,
    size_t result_capacity,
    size_t* out_result_count);

#endif
