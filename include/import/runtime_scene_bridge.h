#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "scene/object_manager.h"

#define RUNTIME_SCENE_BRIDGE_MAX_DIGEST_PRIMITIVES 16
#define RUNTIME_SCENE_BRIDGE_MAX_PRIMITIVE_SEEDS MAX_OBJECTS

typedef struct RuntimeSceneBridgePreflight {
    bool valid_contract;
    char scene_id[128];
    int object_count;
    int material_count;
    int light_count;
    int camera_count;
    char diagnostics[256];
} RuntimeSceneBridgePreflight;

typedef struct RuntimeSceneBridge3DScaffoldState {
    bool valid;
    bool has_camera_seed;
    bool has_camera_rotation_seed;
    bool has_camera_pitch_seed;
    bool has_camera_focus_target;
    double camera_z;
    double camera_focus_target_x;
    double camera_focus_target_y;
    double camera_focus_target_z;
    int box_count;
    int plane_count;
    int triangle_mesh_count;
} RuntimeSceneBridge3DScaffoldState;

typedef enum RuntimeSceneBridgePrimitiveKind {
    RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN = 0,
    RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE = 1,
    RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM = 2,
    RUNTIME_SCENE_BRIDGE_PRIMITIVE_TRIANGLE_MESH = 3,
    RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX = 4
} RuntimeSceneBridgePrimitiveKind;

typedef struct RuntimeSceneBridgePrimitiveDigest {
    RuntimeSceneBridgePrimitiveKind kind;
    char object_id[64];
    int scene_object_index;
    bool guide_only;
    double origin_x;
    double origin_y;
    double origin_z;
    bool has_dimensions;
    double width;
    double height;
    double depth;
} RuntimeSceneBridgePrimitiveDigest;

typedef struct RuntimeSceneBridge3DDigestState {
    bool valid;
    bool has_scene_bounds;
    bool bounds_enabled;
    bool bounds_clamp_on_edit;
    double bounds_min_x;
    double bounds_min_y;
    double bounds_min_z;
    double bounds_max_x;
    double bounds_max_y;
    double bounds_max_z;
    bool has_construction_plane;
    char construction_plane_mode[24];
    char construction_plane_axis[12];
    double construction_plane_offset;
    int primitive_count;
    int plane_primitive_count;
    int rect_prism_primitive_count;
    RuntimeSceneBridgePrimitiveDigest primitives[RUNTIME_SCENE_BRIDGE_MAX_DIGEST_PRIMITIVES];
} RuntimeSceneBridge3DDigestState;

typedef struct RuntimeSceneBridgePrimitiveSeed {
    RuntimeSceneBridgePrimitiveKind kind;
    char object_id[64];
    int scene_object_index;
    bool guide_only;
    bool has_dimensions;
    double origin_x;
    double origin_y;
    double origin_z;
    double axis_u_x;
    double axis_u_y;
    double axis_u_z;
    double axis_v_x;
    double axis_v_y;
    double axis_v_z;
    double normal_x;
    double normal_y;
    double normal_z;
    double width;
    double height;
    double depth;
} RuntimeSceneBridgePrimitiveSeed;

typedef struct RuntimeSceneBridge3DPrimitiveSeedState {
    bool valid;
    int primitive_count;
    int plane_primitive_count;
    int rect_prism_primitive_count;
    int excluded_primitive_count;
    RuntimeSceneBridgePrimitiveSeed
        primitives[RUNTIME_SCENE_BRIDGE_MAX_PRIMITIVE_SEEDS];
} RuntimeSceneBridge3DPrimitiveSeedState;

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight);
bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight);

bool runtime_scene_bridge_apply_json(const char *runtime_scene_json,
                                     RuntimeSceneBridgePreflight *out_summary);
bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary);
bool runtime_scene_bridge_apply_file_defer_mesh_assets(const char *runtime_scene_path,
                                                       RuntimeSceneBridgePreflight *out_summary);

bool runtime_scene_bridge_writeback_ray_overlay_json(const char *runtime_scene_json,
                                                     const char *overlay_json,
                                                     char **out_runtime_scene_json,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size);

void runtime_scene_bridge_get_last_3d_scaffold_state(RuntimeSceneBridge3DScaffoldState *out_state);
void runtime_scene_bridge_get_last_3d_digest_state(RuntimeSceneBridge3DDigestState *out_state);
void runtime_scene_bridge_get_last_3d_primitive_seed_state(
    RuntimeSceneBridge3DPrimitiveSeedState *out_state);
bool runtime_scene_bridge_get_last_object_id_for_scene_index(int scene_index,
                                                             char *out_object_id,
                                                             size_t out_object_id_size);
