#include "import/scene_timeline_document_io.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static json_object* field(json_object* owner, const char* name, enum json_type type) {
    json_object* value = NULL;
    if (!owner || !json_object_is_type(owner, json_type_object) ||
        !json_object_object_get_ex(owner, name, &value) ||
        !json_object_is_type(value, type)) return NULL;
    return value;
}
static bool number(json_object* value, double* out) {
    if (!value || !(json_object_is_type(value, json_type_double) ||
                    json_object_is_type(value, json_type_int))) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}
static bool named_number(json_object* owner, const char* name, double* out) {
    json_object* value = NULL;
    return owner && json_object_object_get_ex(owner, name, &value) && number(value, out);
}
static const char* string(json_object* owner, const char* name) {
    json_object* value = field(owner, name, json_type_string);
    if (!value) return NULL;
    const char* text = json_object_get_string(value);
    return strlen(text) == (size_t)json_object_get_string_len(value) ? text : NULL;
}
static TimelineStatus parse_track(json_object* root, TimelineTrack* out) {
    const char* id = string(root, "id");
    const char* target = string(root, "target_id");
    const char* property = string(root, "property_id");
    const char* unit = string(root, "unit");
    const char* type = string(root, "value_type");
    const char* source = string(root, "source");
    json_object* enabled = field(root, "enabled", json_type_boolean);
    json_object* keys = field(root, "keys", json_type_array);
    if (!id || !target || !property || !type || !unit || !source ||
        strcmp(source, "authored") || !enabled || !keys)
        return TIMELINE_STATUS_INVALID_TRACK;
    TimelineValueType value_type = !strcmp(type, "scalar") ? TIMELINE_VALUE_SCALAR :
        !strcmp(type, "vec3") ? TIMELINE_VALUE_VEC3 : TIMELINE_VALUE_NONE;
    TimelineTrack track;
    TimelineStatus status = TimelineTrackInit(&track, id, target, property, value_type);
    if (status != TIMELINE_STATUS_OK) return status;
    TimelineUnit parsed_unit = TIMELINE_UNIT_UNSPECIFIED;
    for (int i = TIMELINE_UNIT_UNITLESS; i <= TIMELINE_UNIT_DEGREES; ++i)
        if (!strcmp(unit, TimelineUnitLabel((TimelineUnit)i))) parsed_unit = (TimelineUnit)i;
    if (parsed_unit == TIMELINE_UNIT_UNSPECIFIED) return TIMELINE_STATUS_UNIT_MISMATCH;
    status = TimelineTrackSetUnit(&track, parsed_unit);
    if (status != TIMELINE_STATUS_OK) return status;
    track.enabled = json_object_get_boolean(enabled);
    size_t count = json_object_array_length(keys);
    if (count > TIMELINE_TRACK_KEY_CAPACITY) return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    for (size_t i = 0; i < count; ++i) {
        json_object* key = json_object_array_get_idx(keys, i);
        json_object* frame = field(key, "frame", json_type_int);
        json_object* value = NULL;
        const char* interpolation = string(key, "interpolation");
        if (!frame || !interpolation || !json_object_object_get_ex(key, "value", &value))
            return TIMELINE_STATUS_INVALID_TRACK;
        TimelineInterpolation mode = (TimelineInterpolation)-1;
        for (int m = TIMELINE_INTERPOLATION_STEP; m <= TIMELINE_INTERPOLATION_CUBIC_BEZIER; ++m)
            if (!strcmp(interpolation, TimelineInterpolationLabel((TimelineInterpolation)m))) mode = (TimelineInterpolation)m;
        TimelineValue parsed = {.type = value_type};
        if (value_type == TIMELINE_VALUE_SCALAR) {
            if (!number(value, &parsed.as.scalar)) return TIMELINE_STATUS_INVALID_TRACK;
        } else {
            if (!json_object_is_type(value, json_type_array) || json_object_array_length(value) != 3 ||
                !number(json_object_array_get_idx(value, 0), &parsed.as.vec3.x) ||
                !number(json_object_array_get_idx(value, 1), &parsed.as.vec3.y) ||
                !number(json_object_array_get_idx(value, 2), &parsed.as.vec3.z))
                return TIMELINE_STATUS_INVALID_TRACK;
        }
        status = TimelineTrackAddKey(&track, json_object_get_int64(frame), parsed, mode);
        if (status != TIMELINE_STATUS_OK) return status;
        json_object* incoming = field(key, "incoming_handle", json_type_object);
        json_object* outgoing = field(key, "outgoing_handle", json_type_object);
        double fi, vi, fo, vo;
        if (!named_number(incoming, "frame_offset", &fi) || !named_number(incoming, "value_offset", &vi) ||
            !named_number(outgoing, "frame_offset", &fo) || !named_number(outgoing, "value_offset", &vo))
            return TIMELINE_STATUS_INVALID_TRACK;
        if (value_type == TIMELINE_VALUE_SCALAR) {
            status = TimelineTrackSetScalarTemporalHandles(&track, i, fi, vi, fo, vo);
            if (status != TIMELINE_STATUS_OK) return status;
        } else if (fi != 0 || vi != 0 || fo != 0 || vo != 0) {
            return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
        }
    }
    /* Optional policy is metadata; preserve saved handles exactly on load. */
    for(size_t i=0;i<track.key_count;++i) {
        json_object *key=json_object_array_get_idx(keys,i), *policy=NULL;
        if(json_object_object_get_ex(key,"tangent_mode",&policy)) {
            if(!json_object_is_type(policy,json_type_string)) return TIMELINE_STATUS_INVALID_TRACK;
            bool found=false;
            for(int m=0;m<=TIMELINE_TANGENT_FLAT;++m)
                if(!strcmp(json_object_get_string(policy),TimelineTangentModeLabel(m))) {track.keys[i].tangent_mode=m;found=true;}
            if(!found || (value_type!=TIMELINE_VALUE_SCALAR && track.keys[i].tangent_mode!=TIMELINE_TANGENT_BROKEN)) return TIMELINE_STATUS_INVALID_TRACK;
        }
    }
    *out = track;
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneTimelineDocumentFromJson(json_object* root, TimelineDocument* out) {
    if (!root || !out) return TIMELINE_STATUS_INVALID_ARGUMENT;
    json_object* version = field(root, "version", json_type_int);
    json_object* rate = field(root, "rate", json_type_object);
    json_object* range = field(root, "range", json_type_object);
    json_object* numerator = field(rate, "numerator", json_type_int);
    json_object* denominator = field(rate, "denominator", json_type_int);
    json_object* start = field(range, "start_frame", json_type_int);
    json_object* count = field(range, "frame_count", json_type_int);
    json_object* tracks = field(root, "tracks", json_type_array);
    if (!version || json_object_get_int64(version) != 1 || !numerator || !denominator ||
        !start || !count || !tracks) return TIMELINE_STATUS_INVALID_ARGUMENT;
    int64_t n = json_object_get_int64(numerator), d = json_object_get_int64(denominator);
    if (n <= 0 || d <= 0 || n > UINT32_MAX || d > UINT32_MAX) return TIMELINE_STATUS_INVALID_RATE;
    if (json_object_get_int64(count) <= 0) return TIMELINE_STATUS_INVALID_RANGE;
    if (json_object_array_length(tracks) > TIMELINE_DOCUMENT_TRACK_CAPACITY)
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    TimelineDocument* candidate = malloc(sizeof(*candidate));
    if (!candidate) return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    TimelineStatus status = TimelineDocumentInit(candidate, (TimelineRate){(uint32_t)n,(uint32_t)d},
        (TimelineRange){json_object_get_int64(start), (uint64_t)json_object_get_int64(count)});
    for (size_t i = 0; status == TIMELINE_STATUS_OK && i < json_object_array_length(tracks); ++i) {
        TimelineTrack track;
        status = parse_track(json_object_array_get_idx(tracks, i), &track);
        if (status == TIMELINE_STATUS_OK) status = TimelineDocumentAddTrack(candidate, &track);
    }
    TimelinePropertyRegistry registry;
    if (status == TIMELINE_STATUS_OK) status = TimelinePropertyRegistryInitFoundationDefaults(&registry);
    if (status == TIMELINE_STATUS_OK) status = TimelinePropertyRegistryValidateDocument(&registry, candidate);
    if (status == TIMELINE_STATUS_OK) *out = *candidate;
    free(candidate);
    return status;
}

static json_object* handle(double frame, double value) {
    json_object* result = json_object_new_object();
    json_object_object_add(result, "frame_offset", json_object_new_double(frame));
    json_object_object_add(result, "value_offset", json_object_new_double(value));
    return result;
}
json_object* SceneTimelineDocumentToJson(const TimelineDocument* document) {
    TimelinePropertyRegistry registry;
    if (!document || document->range.frame_count > INT64_MAX ||
        TimelinePropertyRegistryInitFoundationDefaults(&registry) != TIMELINE_STATUS_OK ||
        TimelinePropertyRegistryValidateDocument(&registry, document) != TIMELINE_STATUS_OK) return NULL;
    json_object* root = json_object_new_object();
    json_object* rate = json_object_new_object();
    json_object* range = json_object_new_object();
    json_object* tracks = json_object_new_array();
    json_object_object_add(root, "version", json_object_new_int(1));
    json_object_object_add(rate, "numerator", json_object_new_int64(document->rate.frames_per_second_numerator));
    json_object_object_add(rate, "denominator", json_object_new_int64(document->rate.frames_per_second_denominator));
    json_object_object_add(range, "start_frame", json_object_new_int64(document->range.start_frame));
    json_object_object_add(range, "frame_count", json_object_new_int64((int64_t)document->range.frame_count));
    json_object_object_add(root, "rate", rate);
    json_object_object_add(root, "range", range);
    json_object_object_add(root, "tracks", tracks);
    for (size_t i = 0; i < document->track_count; ++i) {
        const TimelineTrack* track = &document->tracks[i];
        json_object* object = json_object_new_object();
        json_object* keys = json_object_new_array();
        json_object_object_add(object, "id", json_object_new_string(track->track_id));
        json_object_object_add(object, "target_id", json_object_new_string(track->target_id));
        json_object_object_add(object, "property_id", json_object_new_string(track->property_id));
        json_object_object_add(object, "value_type", json_object_new_string(TimelineValueTypeLabel(track->value_type)));
        json_object_object_add(object, "unit", json_object_new_string(TimelineUnitLabel(track->unit)));
        json_object_object_add(object, "source", json_object_new_string(TimelineChannelSourceLabel(track->source)));
        json_object_object_add(object, "enabled", json_object_new_boolean(track->enabled));
        json_object_object_add(object, "keys", keys);
        json_object_array_add(tracks, object);
        for (size_t k = 0; k < track->key_count; ++k) {
            const TimelineKeyframe* key = &track->keys[k];
            json_object* item = json_object_new_object();
            json_object* value;
            if (track->value_type == TIMELINE_VALUE_SCALAR) value = json_object_new_double(key->value.as.scalar);
            else {
                value = json_object_new_array();
                json_object_array_add(value, json_object_new_double(key->value.as.vec3.x));
                json_object_array_add(value, json_object_new_double(key->value.as.vec3.y));
                json_object_array_add(value, json_object_new_double(key->value.as.vec3.z));
            }
            json_object_object_add(item, "frame", json_object_new_int64(key->frame));
            json_object_object_add(item, "value", value);
            json_object_object_add(item, "interpolation", json_object_new_string(TimelineInterpolationLabel(key->interpolation_to_next)));
            json_object_object_add(item, "tangent_mode", json_object_new_string(TimelineTangentModeLabel(key->tangent_mode)));
            json_object_object_add(item, "incoming_handle", handle(key->incoming_frame_offset, key->incoming_value_offset));
            json_object_object_add(item, "outgoing_handle", handle(key->outgoing_frame_offset, key->outgoing_value_offset));
            json_object_array_add(keys, item);
        }
    }
    return root;
}
