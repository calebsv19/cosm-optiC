#pragma once

#include <stdbool.h>
#include <stddef.h>

#define RUNTIME_SCENE_BRIDGE_MAX_DIGEST_PRIMITIVES 16

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
    double camera_z;
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

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight);
bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight);

bool runtime_scene_bridge_apply_json(const char *runtime_scene_json,
                                     RuntimeSceneBridgePreflight *out_summary);
bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary);

bool runtime_scene_bridge_writeback_ray_overlay_json(const char *runtime_scene_json,
                                                     const char *overlay_json,
                                                     char **out_runtime_scene_json,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size);

void runtime_scene_bridge_get_last_3d_scaffold_state(RuntimeSceneBridge3DScaffoldState *out_state);
void runtime_scene_bridge_get_last_3d_digest_state(RuntimeSceneBridge3DDigestState *out_state);
bool runtime_scene_bridge_get_last_object_id_for_scene_index(int scene_index,
                                                             char *out_object_id,
                                                             size_t out_object_id_size);
