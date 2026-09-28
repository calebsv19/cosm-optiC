#include "import/runtime_scene_timeline.h"
#include "import/runtime_scene_object_timeline.h"
#include "runtime_scene_timeline_consumers.h"
#include "import/scene_timeline_document_io.h"
#include "animation/evaluated_scene_snapshot.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Derived cache populated only by scene loading, never an editor authoring store. */
static TimelineDocument runtime_document;
static TimelineStatus runtime_status = TIMELINE_STATUS_TARGET_NOT_FOUND;
uint64_t RuntimeSceneTimelineRevision(void) {
    return runtime_status == TIMELINE_STATUS_OK
        ? RayEvaluatedTimelineFingerprint(&runtime_document, NULL, NULL) : 0;
}
TimelineStatus RuntimeSceneTimelineCopy(TimelineDocument* out) {
    if (!out) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (runtime_status != TIMELINE_STATUS_OK) return runtime_status;
    *out = runtime_document;
    return TIMELINE_STATUS_OK;
}
void RuntimeSceneTimelineReset(void) {
    runtime_status = TIMELINE_STATUS_TARGET_NOT_FOUND;
}
static TimelineStatus parse_runtime_document(json_object* authoring, double world_scale, TimelineDocument* out) {
    json_object* root = NULL;
    if (!authoring || !json_object_object_get_ex(authoring, "scene_timeline", &root))
        return TIMELINE_STATUS_TARGET_NOT_FOUND;
    if (!isfinite(world_scale) || world_scale <= 0)
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    TimelineDocument* candidate = malloc(sizeof(*candidate));
    if (!candidate) return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    TimelineStatus status = SceneTimelineDocumentFromJson(root, candidate);
    if (status == TIMELINE_STATUS_OK) {
        for (size_t i = 0; i < candidate->track_count; ++i) {
            TimelineTrack* track = &candidate->tracks[i];
            if (track->unit != TIMELINE_UNIT_WORLD_DISTANCE) continue;
            for (size_t k = 0; k < track->key_count; ++k) {
                TimelineValue* value = &track->keys[k].value;
                if (value->type == TIMELINE_VALUE_VEC3) {
                    value->as.vec3.x *= world_scale;
                    value->as.vec3.y *= world_scale;
                    value->as.vec3.z *= world_scale;
                } else {
                    value->as.scalar *= world_scale;
                    track->keys[k].incoming_value_offset *= world_scale;
                    track->keys[k].outgoing_value_offset *= world_scale;
                }
            }
        }
        TimelinePropertyRegistry registry;
        status = TimelinePropertyRegistryInitFoundationDefaults(&registry);
        if (status == TIMELINE_STATUS_OK)
            status = TimelinePropertyRegistryValidateDocument(&registry, candidate);
        if (status == TIMELINE_STATUS_OK && out) *out = *candidate;
    }
    free(candidate);
    return status;
}
TimelineStatus RuntimeSceneTimelineLoad(json_object* authoring, double world_scale) {
    runtime_status = parse_runtime_document(authoring,world_scale,&runtime_document);
    return runtime_status;
}
bool RuntimeSceneTimelineValidateScene(json_object* scene,char* diagnostics,size_t size) {
    json_object *extensions=NULL,*ray=NULL,*authoring=NULL,*scale=NULL;
    json_object_object_get_ex(scene,"extensions",&extensions);
    if(extensions) json_object_object_get_ex(extensions,"ray_tracing",&ray);
    if(ray) json_object_object_get_ex(ray,"authoring",&authoring);
    json_object* timeline=NULL;
    if(!authoring || !json_object_object_get_ex(authoring,"scene_timeline",&timeline)) return true;
    json_object_object_get_ex(scene,"world_scale",&scale);
    TimelineDocument* candidate=malloc(sizeof(*candidate));
    if(!candidate) {
        if(diagnostics && size) snprintf(diagnostics,size,"scene_timeline validation allocation failed");
        return false;
    }
    TimelineStatus status=parse_runtime_document(authoring,scale?json_object_get_double(scale):1.0,candidate);
    char invalid_target[TIMELINE_ID_CAPACITY]={0};
    for(size_t i=0;status==TIMELINE_STATUS_OK && i<candidate->track_count;++i) {
        const TimelineTrack* track=&candidate->tracks[i];
        if(!track->enabled) continue;
        if(!strncmp(track->target_id,"camera/",7)) {
            if(strcmp(track->target_id,"camera/main")) status=TIMELINE_STATUS_TARGET_NOT_FOUND;
        } else if(!strncmp(track->target_id,"light/",6)) {
            json_object* lights=NULL;size_t matches=0;
            json_object_object_get_ex(scene,"lights",&lights);
            for(size_t k=0;json_object_is_type(lights,json_type_array) && k<json_object_array_length(lights);++k) {
                json_object *entry=json_object_array_get_idx(lights,k),*id=NULL;
                json_object_object_get_ex(entry,"id",&id);
                if(!json_object_is_type(id,json_type_string) || !json_object_get_string(id)[0])
                    json_object_object_get_ex(entry,"light_id",&id);
                if(!json_object_is_type(id,json_type_string) || !json_object_get_string(id)[0])
                    json_object_object_get_ex(entry,"object_id",&id);
                if(json_object_is_type(id,json_type_string) &&
                   !strcmp(json_object_get_string(id),track->target_id+6)) ++matches;
            }
            if(matches!=1) status=matches?TIMELINE_STATUS_DUPLICATE_ID:TIMELINE_STATUS_TARGET_NOT_FOUND;
        }
        if(status!=TIMELINE_STATUS_OK) snprintf(invalid_target,sizeof(invalid_target),"%s",track->target_id);
    }
    if(status==TIMELINE_STATUS_OK) {
        bool supported=RuntimeObjectTimelineValidate(scene,candidate,diagnostics,size) && RuntimeSceneTimelineValidateConsumers(authoring,
            scale?json_object_get_double(scale):1.0,candidate,diagnostics,size);
        free(candidate);
        return supported;
    }
    free(candidate);
    if(diagnostics && size) snprintf(diagnostics,size,"invalid scene_timeline: %s %s",TimelineStatusLabel(status),invalid_target);
    return false;
}
TimelineStatus RuntimeSceneTimelineClock(TimelineRate* rate, TimelineRange* range) {
    if (!rate || !range) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (runtime_status != TIMELINE_STATUS_OK) return runtime_status;
    *rate = runtime_document.rate;
    *range = runtime_document.range;
    return TIMELINE_STATUS_OK;
}
TimelineStatus RuntimeSceneTimelineSample(TimelineSample sample, TimelineFrameSnapshot* out) {
    if (!out) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (runtime_status != TIMELINE_STATUS_OK) return runtime_status;
    TimelineEvaluationContext context;
    TimelinePropertyRegistry registry;
    TimelineStatus status = TimelineEvaluationContextBuild(runtime_document.rate,
        runtime_document.range, sample, &context);
    if (status == TIMELINE_STATUS_OK) status = TimelinePropertyRegistryInitFoundationDefaults(&registry);
    if (status == TIMELINE_STATUS_OK) status = TimelineFrameSnapshotBuild(&registry, &runtime_document, &context, out);
    return status;
}

const TimelineDocument* RuntimeSceneTimelineRead(void) { return runtime_status==TIMELINE_STATUS_OK?&runtime_document:NULL; }
