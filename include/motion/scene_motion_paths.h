#ifndef SCENE_MOTION_PATHS_H
#define SCENE_MOTION_PATHS_H
#include "animation/timeline_document.h"
#include "motion/motion_frame.h"
#include <json-c/json.h>
#define MOTION_PATH_CAPACITY 16
#define MOTION_POINT_CAPACITY 32
#define MOTION_BINDING_CAPACITY 64
#define MOTION_LIGHT_PROGRESS_PROPERTY "light/route_progress"
#define MOTION_CAMERA_PROGRESS_PROPERTY "camera/route_progress"
#define MOTION_PROGRESS_PROPERTY "object/path_progress"
typedef enum MotionHandleMode {
  MOTION_HANDLE_INDEPENDENT, MOTION_HANDLE_LINKED, MOTION_HANDLE_CORNER
} MotionHandleMode;
typedef struct MotionPathPoint {
  char id[64];
  double position[3], incoming[3], outgoing[3];
  bool linear;
  MotionHandleMode handle_mode;
} MotionPathPoint;
typedef struct MotionPath {
  char id[64], name[128];
  size_t count;
  MotionPathPoint points[MOTION_POINT_CAPACITY];
} MotionPath;
typedef struct MotionPathBinding {
  char object_id[64], path_id[64];
  /* Empty for legacy object bindings; otherwise a typed camera or light target. */
  char target_id[TIMELINE_ID_CAPACITY];
  bool enabled;
  bool follow_direction; /* Object-only; default off. */
  int forward_axis; /* +X,-X,+Y,-Y,+Z,-Z */
  double rotation_offset[3]; /* Local XYZ Euler degrees after axis alignment. */
  double start_up[3]; /* Zero means canonical world +Z, projected at route start. */
  double start_roll, end_roll; /* Unwrapped degrees, smoothstep over route distance. */
  bool end_roll_enabled;
  int camera_orientation; /* 0 legacy authored/focus; 1 route; 2 stable target aim. */
  bool use_focus_target; /* Camera-only explicit orientation owner; default off. */
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
/* Policies alter geometry only through explicit edits, never during loading. */
const char *MotionHandleModeLabel(MotionHandleMode mode);
/* Explicit authoring action: seed collapsed tangents and curve adjacent segments. */
bool MotionPathSmoothPoint(MotionPath *path, size_t index);
bool MotionPathSetHandleMode(MotionPathPoint *point, MotionHandleMode mode);
bool MotionPathEditHandle(MotionPathPoint *point, bool incoming, const double value[3]);
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
bool MotionPathsRuntimeBinding(const char *target_id, MotionPathBinding *out);
bool MotionPathsRuntimeTargetSample(const char *target_id, double progress, TimelineVec3 *out, double *length, double *parameter);
bool MotionPathsRuntimeTargetPosition(const char *target_id, double progress,
                                      TimelineVec3 *out);
bool MotionPathsRuntimePosition(const char *object_id, double progress,
                                TimelineVec3 *out);
bool MotionPathsRuntimeFrame(const char *target, double progress, MotionFrame *out);
bool MotionPathsRuntimeRotation(const char *target, double progress, TimelineVec3 *out);
#endif
