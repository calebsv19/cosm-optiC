#ifndef RAY_TRACING_RUNTIME_SCENE_LIGHT_TIMELINE_IO_H
#define RAY_TRACING_RUNTIME_SCENE_LIGHT_TIMELINE_IO_H

#include <stdbool.h>
#include <stddef.h>

#include <json-c/json.h>

#include "animation/timeline_document.h"
#include "animation/timeline_light_motion.h"

#define RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION 1

typedef struct RuntimeSceneLightTimelineDocument {
    bool valid;
    bool migrated_legacy_spatial_path;
    TimelineDocument timeline;
    size_t progress_track_index;
    Path spatial_path;
    CameraPath3D spatial_path_3d;
} RuntimeSceneLightTimelineDocument;

TimelineStatus RuntimeSceneLightTimelineParseAuthoring(
    json_object* authoring,
    double world_scale,
    RuntimeSceneLightTimelineDocument* out_document,
    char* out_diagnostics,
    size_t diagnostics_size);

json_object* RuntimeSceneLightTimelineToJsonObject(
    const RuntimeSceneLightTimelineDocument* document,
    double world_scale);

TimelineStatus RuntimeSceneLightTimelineEvaluate(
    const RuntimeSceneLightTimelineDocument* document,
    TimelineSample sample,
    TimelineLightMotionSample* out_sample);

void RuntimeSceneLightTimelineResetLast(void);
TimelineStatus RuntimeSceneLightTimelineApplyAuthoring(
    json_object* authoring,
    double world_scale,
    char* out_diagnostics,
    size_t diagnostics_size);
bool RuntimeSceneLightTimelineGetLast(RuntimeSceneLightTimelineDocument* out_document);
TimelineStatus RuntimeSceneLightTimelineInspectLast(
    TimelineSample sample,
    TimelineLightMotionSample* out_sample);

#endif
