#ifndef SCENE_EDITOR_MOTION_PATH_PANEL_INTERNAL_H
#define SCENE_EDITOR_MOTION_PATH_PANEL_INTERNAL_H
#include "editor/scene_editor_motion_paths.h"
#include "scene_editor_motion_point_gizmo.h"
enum {
  NEW,
  NAME,
  PREV,
  NEXT,
  ADD,
  REMOVE,
  MODE,
  DELETE_PATH,
  OBJECT_PREV,
  OBJECT_NEXT,
  ATTACH,
  DETACH,
  TIMING,
  SAVE,
  FRAME,
  SELECT_TOOL,
  PLACE_TOOL,
  FRAME_PATH,
  DEPTH,
  ATTACH_SECTION,
  DELETE_TOOL,
  HANDLE_MODE,
  PATH_ACTIONS,
  LIBRARY_PREV,
  LIBRARY_NEXT,
  FOLLOWER_OBJECT, FOLLOWER_CAMERA, FOLLOWER_LIGHT, FOLLOWER_BACK,
  OBJECT_PICKER, OBJECT_SELECTED,
  HANDLE_CORNER, HANDLE_INDEPENDENT,
  FOLLOWER_PLAN,
  SHAPE_PLAN,
  POINT_DETAILS,
  FOLLOWER_TIMING,
  CONTROL_COUNT
};
typedef struct {
  bool active, dragging, placing, show_followers, show_actions;
  int library_first, follower_type, object_page;
  bool object_picker, point_focus, point_details;
  SDL_Rect object_rows[6], follower_rows[MOTION_BINDING_CAPACITY];
  char picker_ids[6][128];
  double plane_z, gizmo_initial;
  int gizmo_axis;
  SceneEditorObjectTransformHandle gizmo_handle;
  SDL_Rect gizmo_controls[3];
  char path_id[64], object_id[128], draft[128], message[256];
  char labels[128][200];
  char point_labels[MOTION_POINT_CAPACITY][16];
  int label_count, point, editing, offset, max_offset, right_offset, right_max,
      handle;
  unsigned long long revision, message_revision;
  MotionPath drag;
  SDL_Rect controls[CONTROL_COUNT], rows[MOTION_PATH_CAPACITY], fields[9],
      viewport;
  bool enabled[CONTROL_COUNT];
  SceneEditorDigestOverlayProjector projector;
  bool projected;
  int down_x, down_y;
} MotionPathPanelState;
extern MotionPathPanelState motion_path_ui;
#define ui motion_path_ui
MotionPath *MotionPathPanelSelected(MotionPaths *paths);
bool MotionPathPanelAppliedTarget(const MotionPaths *, const MotionPath *, char *, size_t);
bool MotionPathPanelFollowerTarget(const MotionPaths *, const MotionPath *, char *, size_t);
bool MotionPathPanelProject(const double v[3], int *x, int *y);

#endif
