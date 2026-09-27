#ifndef SCENE_TIMELINE_DOCUMENT_IO_H
#define SCENE_TIMELINE_DOCUMENT_IO_H
#include <json-c/json.h>
#include "animation/timeline_property_registry.h"

/* Detached codec for authoring.scene_timeline. World-distance values use scene
 * authored units; scale conversion belongs to the scene application adapter.
 * Does not change globals, save files, or discard fields outside this subtree. */
TimelineStatus SceneTimelineDocumentFromJson(json_object* root,
    TimelineDocument* out_document);
json_object* SceneTimelineDocumentToJson(const TimelineDocument* document);
#endif
