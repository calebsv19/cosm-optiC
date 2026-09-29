#ifndef SCENE_MOTION_PATHS_H
#define SCENE_MOTION_PATHS_H
#include "animation/timeline_document.h"
#include <json-c/json.h>
#define MOTION_PATH_CAPACITY 16
#define MOTION_POINT_CAPACITY 32
#define MOTION_BINDING_CAPACITY 64
#define MOTION_PROGRESS_PROPERTY "object/path_progress"
typedef struct MotionPathPoint {
  char id[64];
  double position[3], incoming[3], outgoing[3];
  bool linear;
} MotionPathPoint;
typedef struct MotionPath {
  char id[64], name[128];
  size_t count;
  MotionPathPoint points[MOTION_POINT_CAPACITY];
} MotionPath;
typedef struct MotionPathBinding {
  char object_id[64], path_id[64];
  bool enabled;
  /* Optional in v1: absent on older bindings. Empty means prior static source.
   * Keep track identities, not just axes: disabled alternatives must stay off. */
  bool restore_known;
  size_t restore_count;
  char restore_xyz_tracks[3][TIMELINE_ID_CAPACITY];
} MotionPathBinding;
typedef struct MotionPaths {
  size_t count, binding_count;
  MotionPath paths[MOTION_PATH_CAPACITY];
  MotionPathBinding bindings[MOTION_BINDING_CAPACITY];
} MotionPaths;
bool MotionPathsParse(json_object *authoring, MotionPaths *out, char *message,
                      size_t size);
json_object *MotionPathsToJson(const MotionPaths *paths);
/* Geometry in authored world units; handle vectors are offsets from their
 * anchor. */
void MotionPathPointAt(const MotionPath *path, double parameter, double out[3]);
bool MotionPathsValidateScene(json_object *scene,
                              const TimelineDocument *timeline, char *message,
                              size_t size);
bool MotionPathsRuntimeLoad(json_object *authoring, double world_scale);
void MotionPathsRuntimeReset(void);
uint64_t MotionPathsRuntimeRevision(void);
bool MotionPathsRuntimePosition(const char *object_id, double progress,
                                TimelineVec3 *out);
#endif
