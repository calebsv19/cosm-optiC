#ifndef RUNTIME_SCENE_TIMELINE_CONSUMERS_H
#define RUNTIME_SCENE_TIMELINE_CONSUMERS_H
#include "animation/timeline_document.h"
#include <json-c/json.h>
bool RuntimeSceneTimelineValidateConsumers(json_object* authoring, double world_scale,
    const TimelineDocument* document, char* diagnostics, size_t size);
#endif
