#ifndef RUNTIME_SCENE_TIMELINE_H
#define RUNTIME_SCENE_TIMELINE_H
#include <json-c/json.h>
#include "animation/timeline_frame_snapshot.h"
void RuntimeSceneTimelineReset(void);
uint64_t RuntimeSceneTimelineRevision(void);
TimelineStatus RuntimeSceneTimelineCopy(TimelineDocument* out);
TimelineStatus RuntimeSceneTimelineLoad(json_object* authoring, double world_scale);
/* Detached preflight; never changes the runtime cache. */
bool RuntimeSceneTimelineValidateScene(json_object* scene, char* diagnostics, size_t size);
TimelineStatus RuntimeSceneTimelineClock(TimelineRate* rate, TimelineRange* range);
TimelineStatus RuntimeSceneTimelineSample(TimelineSample sample, TimelineFrameSnapshot* out);
/* Borrowed scene-load cache, valid until the next load/reset; read-only. */
const TimelineDocument* RuntimeSceneTimelineRead(void);
#endif
