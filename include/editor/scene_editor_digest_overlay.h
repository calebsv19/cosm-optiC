#ifndef SCENE_EDITOR_DIGEST_OVERLAY_H
#define SCENE_EDITOR_DIGEST_OVERLAY_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "camera/camera_path_3d.h"
#include "import/runtime_scene_bridge.h"
#include "motion/runtime_motion_track_3d.h"

typedef struct SceneEditorDigestOverlayNavState {
    bool orbit_active;
    bool pan_active;
    bool target_valid;
    bool zoom_limits_valid;
    bool zoom_limits_material_focus;
    int last_mouse_x;
    int last_mouse_y;
    double orbit_yaw_deg;
    double orbit_pitch_deg;
    double overlay_zoom;
    double zoom_min;
    double zoom_max;
    double target_x;
    double target_y;
    double target_z;
} SceneEditorDigestOverlayNavState;

typedef enum SceneEditorBezier3DGizmoAxis {
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE = 0,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z
} SceneEditorBezier3DGizmoAxis;

typedef struct SceneEditorBezier3DGizmoState {
    bool dragging;
    bool smooth_drag;
    SceneEditorBezier3DGizmoAxis drag_axis;
    double drag_start_world_x;
    double drag_start_world_y;
    double drag_start_world_z;
    int drag_start_mouse_x;
    int drag_start_mouse_y;
} SceneEditorBezier3DGizmoState;

typedef struct SceneEditorCamera3DGizmoState {
    bool dragging;
    bool smooth_drag;
    SceneEditorBezier3DGizmoAxis drag_axis;
    double drag_start_world_x;
    double drag_start_world_y;
    double drag_start_world_z;
    int drag_start_mouse_x;
    int drag_start_mouse_y;
} SceneEditorCamera3DGizmoState;

typedef struct SceneEditorDigestOverlayProjector {
    SDL_Rect viewport;
    double center_x;
    double center_y;
    double center_z;
    double yaw_rad;
    double pitch_rad;
    double distance;
    double scale;
    double span_max;
} SceneEditorDigestOverlayProjector;

typedef struct SceneEditorBezier3DInteractionMetrics {
    double snap_step;
    double gizmo_world_length;
    double default_handle_length;
} SceneEditorBezier3DInteractionMetrics;

typedef struct SceneEditorMotionOverlayMetrics {
    bool visible;
    bool has_path_curve;
    int control_point_count;
    int projected_control_point_count;
    int sampled_segment_count;
    SDL_Rect center_marker_bounds;
} SceneEditorMotionOverlayMetrics;

bool SceneEditorDigestOverlayResolve(RuntimeSceneBridge3DDigestState* out_digest);
bool SceneEditorDigestOverlayResolveExtents(const RuntimeSceneBridge3DDigestState* digest,
                                            double* out_min_x,
                                            double* out_min_y,
                                            double* out_min_z,
                                            double* out_max_x,
                                            double* out_max_y,
                                            double* out_max_z,
                                            double* out_span_max);
bool SceneEditorDigestOverlayResolveObjectExtents(const RuntimeSceneBridge3DDigestState* digest,
                                                  int scene_object_index,
                                                  double* out_min_x,
                                                  double* out_min_y,
                                                  double* out_min_z,
                                                  double* out_max_x,
                                                  double* out_max_y,
                                                  double* out_max_z,
                                                  double* out_span_max);
bool SceneEditorDigestOverlayBuildProjectorWithView(const RuntimeSceneBridge3DDigestState* digest,
                                                    const SDL_Rect* viewport,
                                                    double yaw_deg,
                                                    double pitch_deg,
                                                    double zoom,
                                                    SceneEditorDigestOverlayProjector* out_projector);
bool SceneEditorDigestOverlayBuildProjector(const RuntimeSceneBridge3DDigestState* digest,
                                            const SDL_Rect* viewport,
                                            const SceneEditorDigestOverlayNavState* nav_state,
                                            SceneEditorDigestOverlayProjector* out_projector);
bool SceneEditorDigestOverlayBuildObjectProjector(const RuntimeSceneBridge3DDigestState* digest,
                                                  const SDL_Rect* viewport,
                                                  const SceneEditorDigestOverlayNavState* nav_state,
                                                  int scene_object_index,
                                                  bool focused_origin,
                                                  SceneEditorDigestOverlayProjector* out_projector);
SceneEditorBezier3DInteractionMetrics SceneEditorDigestOverlayResolveBezierMetrics(
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorDigestOverlayProjector* projector);
double SceneEditorDigestOverlayQuantizeWorldValue(double value, double step);
bool SceneEditorDigestOverlayProjectPointF(const SceneEditorDigestOverlayProjector* projector,
                                           double world_x,
                                           double world_y,
                                           double world_z,
                                           double* out_x,
                                           double* out_y);
double SceneEditorDigestOverlayViewDepth(const SceneEditorDigestOverlayProjector* projector,
                                         double world_x,
                                         double world_y,
                                         double world_z);
bool SceneEditorDigestOverlayProjectPoint(const SceneEditorDigestOverlayProjector* projector,
                                          double world_x,
                                          double world_y,
                                          double world_z,
                                          int* out_x,
                                          int* out_y);
bool SceneEditorDigestOverlayResolveMotionTrackMetrics(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeMotionTrack3D* track,
    SceneEditorMotionOverlayMetrics* out_metrics);
double SceneEditorDigestOverlayResolveEditPlaneZ(const RuntimeSceneBridge3DDigestState* digest,
                                                 const SceneEditorDigestOverlayProjector* projector);
bool SceneEditorDigestOverlayScreenRayToPlanePoint(const SceneEditorDigestOverlayProjector* projector,
                                                   int screen_x,
                                                   int screen_y,
                                                   double plane_z,
                                                   double* out_world_x,
                                                   double* out_world_y,
                                                   double* out_world_z);
int SceneEditorDigestOverlayPickBezierPointIndex(const SceneEditorDigestOverlayProjector* projector,
                                                 const Path* path,
                                                 const CameraPath3D* path3d,
                                                 double plane_z,
                                                 int screen_x,
                                                 int screen_y);
int SceneEditorDigestOverlayPickBezierHandle(const SceneEditorDigestOverlayProjector* projector,
                                             const Path* path,
                                             const CameraPath3D* path3d,
                                             double plane_z,
                                             int screen_x,
                                             int screen_y,
                                             int* out_segment_index,
                                             int* out_handle_index);
SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y);
bool SceneEditorDigestOverlayBezierGizmoAxisLocked(SceneEditorBezier3DGizmoAxis axis);
int SceneEditorDigestOverlayPickCameraPointIndex(const SceneEditorDigestOverlayProjector* projector,
                                                 int screen_x,
                                                 int screen_y);
bool SceneEditorDigestOverlayPickCameraBezierHandle(const SceneEditorDigestOverlayProjector* projector,
                                                    int screen_x,
                                                    int screen_y,
                                                    int* out_segment_index,
                                                    int* out_handle_index);
int SceneEditorDigestOverlayPickCameraRotationHandle(const SceneEditorDigestOverlayProjector* projector,
                                                     const RuntimeSceneBridge3DDigestState* digest,
                                                     int screen_x,
                                                     int screen_y);
bool SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z);
SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickCameraGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y);
bool SceneEditorDigestOverlayApplyCameraGizmoDrag(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorCamera3DGizmoState* gizmo_state,
    int mouse_x,
    int mouse_y);
bool SceneEditorDigestOverlayApplyBezierGizmoDrag(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorBezier3DGizmoState* gizmo_state,
    int mouse_x,
    int mouse_y);
int SceneEditorDigestOverlayPickObjectIndex(const SceneEditorDigestOverlayProjector* projector,
                                            const RuntimeSceneBridge3DDigestState* digest,
                                            int mx,
                                            int my);
int SceneEditorDigestOverlayRender(SDL_Renderer* renderer,
                                   const SDL_Rect* viewport,
                                   const SceneEditorDigestOverlayNavState* nav_state,
                                   int active_mode,
                                   int selected_object_index,
                                   int mouse_x,
                                   int mouse_y,
                                   const SceneEditorBezier3DGizmoState* bezier_gizmo_state,
                                   const SceneEditorCamera3DGizmoState* camera_gizmo_state);

#endif
