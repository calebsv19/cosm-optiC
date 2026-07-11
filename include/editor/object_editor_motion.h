#ifndef OBJECT_EDITOR_MOTION_H
#define OBJECT_EDITOR_MOTION_H

#include <stdbool.h>
#include <stddef.h>

#include <json-c/json.h>

#include "motion/runtime_motion_track_3d.h"

void ObjectEditorMotionReset(void);
void ObjectEditorMotionHydrateFromRuntimeSummary(const RuntimeMotionTrack3DSummary* summary);
int ObjectEditorMotionTrackCount(void);
const RuntimeMotionTrack3D* ObjectEditorMotionFindTrack(const char* object_id);

bool ObjectEditorMotionSetObjectStatic(const char* object_id);
bool ObjectEditorMotionSetObjectAuthored(const char* object_id,
                                         double x,
                                         double y,
                                         double z,
                                         double yaw_radians);
bool ObjectEditorMotionObjectIdForSceneIndex(int scene_object_index,
                                             char* out_object_id,
                                             size_t out_object_id_size);
bool ObjectEditorMotionSetSelectedObjectStatic(int scene_object_index);
bool ObjectEditorMotionSetSelectedObjectAuthored(int scene_object_index);

json_object* ObjectEditorMotionBuildAuthoringTracksJson(double authored_to_runtime_scale);

#endif
