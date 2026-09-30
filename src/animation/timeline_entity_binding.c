#include "animation/timeline_entity_binding.h"

#include <stdio.h>
#include <string.h>

static bool bounded_id(const char* id, size_t capacity) {
    return id && id[0] && strnlen(id, capacity) < capacity;
}

static const char* target_prefix(TimelinePropertyTargetKind kind) {
    switch (kind) {
        case TIMELINE_PROPERTY_TARGET_OBJECT: return "object/";
        case TIMELINE_PROPERTY_TARGET_LIGHT: return "light/";
        case TIMELINE_PROPERTY_TARGET_CAMERA: return "camera/";
        case TIMELINE_PROPERTY_TARGET_MATERIAL: return "material/";
        default: return NULL;
    }
}

TimelineStatus TimelineEntityBindingsFind(const TimelineEntityBindings* bindings,
    const char* target_id, const TimelineEntityBinding** out_binding) {
    if (!bindings || !out_binding || !bounded_id(target_id, TIMELINE_ID_CAPACITY))
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (bindings->count > TIMELINE_ENTITY_BINDING_CAPACITY)
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    const TimelineEntityBinding* found = NULL;
    for (size_t i = 0; i < bindings->count; ++i) {
        const TimelineEntityBinding* entry = &bindings->entries[i];
        if (!bounded_id(entry->target_id, sizeof(entry->target_id)) ||
            !bounded_id(entry->entity_id, sizeof(entry->entity_id)))
            return TIMELINE_STATUS_INVALID_ID;
        if (strcmp(entry->target_id, target_id) != 0) continue;
        if (found) return TIMELINE_STATUS_DUPLICATE_ID;
        found = entry;
    }
    if (!found) return TIMELINE_STATUS_TARGET_NOT_FOUND;
    *out_binding = found;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineEntityBindingsAdd(TimelineEntityBindings* bindings,
    const char* entity_id, const char* target_id, TimelinePropertyTargetKind kind,
    bool editable, bool simulation_owns_transform) {
    const char* prefix = target_prefix(kind);
    const TimelineEntityBinding* existing = NULL;
    if (!bindings || !bounded_id(entity_id, TIMELINE_ENTITY_ID_CAPACITY) ||
        !bounded_id(target_id, TIMELINE_ID_CAPACITY)) return TIMELINE_STATUS_INVALID_ID;
    if (!prefix || strncmp(target_id, prefix, strlen(prefix)) != 0 ||
        !target_id[strlen(prefix)]) return TIMELINE_STATUS_TARGET_KIND_MISMATCH;
    TimelineStatus status = TimelineEntityBindingsFind(bindings, target_id, &existing);
    if (status == TIMELINE_STATUS_OK) return TIMELINE_STATUS_DUPLICATE_ID;
    if (status != TIMELINE_STATUS_TARGET_NOT_FOUND) return status;
    if (bindings->count >= TIMELINE_ENTITY_BINDING_CAPACITY)
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    /* Capabilities on one entity share editing and transform authority. */
    for (size_t i = 0; i < bindings->count; ++i) {
        const TimelineEntityBinding* other = &bindings->entries[i];
        if (strcmp(other->entity_id, entity_id) == 0 &&
            (other->editable != editable ||
             other->simulation_owns_transform != simulation_owns_transform))
            return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    }
    TimelineEntityBinding entry = {0};
    snprintf(entry.entity_id, sizeof(entry.entity_id), "%s", entity_id);
    snprintf(entry.target_id, sizeof(entry.target_id), "%s", target_id);
    entry.kind = kind;
    entry.editable = editable;
    entry.simulation_owns_transform = simulation_owns_transform;
    bindings->entries[bindings->count++] = entry;
    return TIMELINE_STATUS_OK;
}

static bool position_owner(const char* property) {
    return strcmp(property, "light/route_progress") == 0 || strcmp(property, "camera/route_progress") == 0 ||
        strcmp(property, "object/transform/position") == 0 ||
        strcmp(property, "light/position") == 0 ||
        strcmp(property, "light/path_progress") == 0 ||
        strcmp(property, "camera/position") == 0 ||
        strcmp(property, "camera/path_progress") == 0;
}

static unsigned position_axes(const char* property) {
    if(!strcmp(property,"object/transform/position_x")) return 1;
    if(!strcmp(property,"object/transform/position_y")) return 2;
    if(!strcmp(property,"object/transform/position_z")) return 4;
    return position_owner(property)?7:0;
}
static bool transform_owner(const char* property) {
    return !strncmp(property,"object/transform/position_",26) || position_owner(property) || strcmp(property, "camera/yaw") == 0 ||
        strcmp(property, "camera/pitch") == 0;
}

bool TimelineEntityBindingCanAuthor(const TimelineEntityBinding* binding,
    const char* property_id) {
    return binding && property_id && binding->editable &&
        !(binding->simulation_owns_transform &&
          (!property_id[0] || transform_owner(property_id)));
}

TimelineStatus TimelineEntityBindingsValidateDocument(
    const TimelineEntityBindings* bindings,
    const TimelinePropertyRegistry* registry, const TimelineDocument* document) {
    if (!bindings || !registry || !document) return TIMELINE_STATUS_INVALID_ARGUMENT;
    TimelineStatus status = TimelinePropertyRegistryValidateDocument(registry, document);
    if (status != TIMELINE_STATUS_OK) return status;
    const TimelineEntityBinding* resolved[TIMELINE_DOCUMENT_TRACK_CAPACITY] = {0};
    for (size_t i = 0; i < document->track_count; ++i) {
        const TimelineTrack* track = &document->tracks[i];
        if (!track->enabled) continue;
        status = TimelineEntityBindingsFind(bindings, track->target_id, &resolved[i]);
        if (status != TIMELINE_STATUS_OK) return status;
        const TimelinePropertyDescriptor* descriptor = NULL;
        status = TimelinePropertyRegistryFind(registry, track->property_id, &descriptor);
        if (status != TIMELINE_STATUS_OK) return status;
        if (descriptor->target_kind != resolved[i]->kind)
            return TIMELINE_STATUS_TARGET_KIND_MISMATCH;
        if (!TimelineEntityBindingCanAuthor(resolved[i], track->property_id))
            return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
        for (size_t j = 0; j < i; ++j) {
            if (!resolved[j] || strcmp(resolved[i]->entity_id, resolved[j]->entity_id)) continue;
            const char* prior = document->tracks[j].property_id;
            if (strcmp(prior, track->property_id) == 0 ||
                (position_axes(prior) & position_axes(track->property_id)))
                return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
        }
    }
    return TIMELINE_STATUS_OK;
}
