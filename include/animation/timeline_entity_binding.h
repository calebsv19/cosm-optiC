#ifndef RAY_TRACING_TIMELINE_ENTITY_BINDING_H
#define RAY_TRACING_TIMELINE_ENTITY_BINDING_H

#include "animation/timeline_property_registry.h"

/* An entity may expose multiple target aliases (object/foo and light/foo).
 * The entity identity survives runtime-array reordering and capability changes.
 * Catalogs are copied views of authored state, not a second scene database. */
#define TIMELINE_ENTITY_ID_CAPACITY 128u
#define TIMELINE_ENTITY_BINDING_CAPACITY 128u

typedef struct TimelineEntityBinding {
    char entity_id[TIMELINE_ENTITY_ID_CAPACITY];
    char target_id[TIMELINE_ID_CAPACITY];
    TimelinePropertyTargetKind kind;
    bool editable;
    bool simulation_owns_transform;
} TimelineEntityBinding;

typedef struct TimelineEntityBindings {
    size_t count;
    TimelineEntityBinding entries[TIMELINE_ENTITY_BINDING_CAPACITY];
} TimelineEntityBindings;

TimelineStatus TimelineEntityBindingsAdd(TimelineEntityBindings* bindings,
    const char* entity_id, const char* target_id, TimelinePropertyTargetKind kind,
    bool editable, bool simulation_owns_transform);
TimelineStatus TimelineEntityBindingsFind(const TimelineEntityBindings* bindings,
    const char* target_id, const TimelineEntityBinding** out_binding);
bool TimelineEntityBindingCanAuthor(const TimelineEntityBinding* binding,
    const char* property_id);
/* Resolve every enabled track and refuse aliases competing for one property.
 * Disabled tracks retain their bindings but do not acquire property ownership. */
TimelineStatus TimelineEntityBindingsValidateDocument(
    const TimelineEntityBindings* bindings,
    const TimelinePropertyRegistry* registry, const TimelineDocument* document);

#endif
