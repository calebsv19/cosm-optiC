#ifndef SCENE_EDITOR_SCENE_VIEW_PACKET_H
#define SCENE_EDITOR_SCENE_VIEW_PACKET_H

#include <stdbool.h>
#include <stddef.h>

#include "core_scene_view.h"
#include "editor/scene_editor_digest_overlay.h"

#define SCENE_EDITOR_SCENE_VIEW_PACKET_MAX_TRIANGLES 256
#define SCENE_EDITOR_SCENE_VIEW_PACKET_SCHEMA_VARIANT CORE_SCENE_VIEW_PACKET_RAY_TRACING_VARIANT

typedef CoreSceneViewPreviewQuality SceneEditorSceneViewPreviewQuality;
typedef CoreSceneViewDegradedReason SceneEditorSceneViewDegradedReason;
typedef CoreSceneViewDisplayFlags SceneEditorSceneViewDisplayFlags;
typedef CoreSceneViewPickId SceneEditorSceneViewPickId;

#define SCENE_EDITOR_SCENE_VIEW_PREVIEW_OUTLINE CORE_SCENE_VIEW_PREVIEW_OUTLINE
#define SCENE_EDITOR_SCENE_VIEW_PREVIEW_FLAT_SOLID CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID
#define SCENE_EDITOR_SCENE_VIEW_PREVIEW_MATERIAL CORE_SCENE_VIEW_PREVIEW_MATERIAL
#define SCENE_EDITOR_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY \
    CORE_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY

#define SCENE_EDITOR_SCENE_VIEW_DEGRADED_NONE CORE_SCENE_VIEW_DEGRADED_NONE
#define SCENE_EDITOR_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE \
    CORE_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE
#define SCENE_EDITOR_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND \
    CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND
#define SCENE_EDITOR_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED \
    CORE_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED
#define SCENE_EDITOR_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE \
    CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE

#define SCENE_EDITOR_SCENE_VIEW_DISPLAY_TRANSPARENT CORE_SCENE_VIEW_DISPLAY_TRANSPARENT
#define SCENE_EDITOR_SCENE_VIEW_DISPLAY_EMISSIVE CORE_SCENE_VIEW_DISPLAY_EMISSIVE
#define SCENE_EDITOR_SCENE_VIEW_DISPLAY_MIRROR CORE_SCENE_VIEW_DISPLAY_MIRROR
#define SCENE_EDITOR_SCENE_VIEW_DISPLAY_TEXTURED CORE_SCENE_VIEW_DISPLAY_TEXTURED

typedef struct SceneEditorSceneViewTriangle {
    SceneEditorSceneViewPickId pickId;
    double p0[3];
    double p1[3];
    double p2[3];
    int screen0[2];
    int screen1[2];
    int screen2[2];
    double depth;
    unsigned char rgba[4];
    unsigned int displayFlags;
} SceneEditorSceneViewTriangle;

typedef struct SceneEditorSceneViewPacket {
    int focusedObjectIndex;
    SceneEditorSceneViewPreviewQuality previewQuality;
    SceneEditorSceneViewDegradedReason degradedReason;
    bool projected;
    bool complete;
    int triangleCount;
    int faceGroupCount;
    SceneEditorSceneViewTriangle triangles[SCENE_EDITOR_SCENE_VIEW_PACKET_MAX_TRIANGLES];
} SceneEditorSceneViewPacket;

typedef CoreSceneViewPacketReadback SceneEditorSceneViewPacketReadback;

void SceneEditorSceneViewPacketInit(SceneEditorSceneViewPacket* packet);
const char* SceneEditorSceneViewPreviewQualityLabel(
    SceneEditorSceneViewPreviewQuality quality);
const char* SceneEditorSceneViewDegradedReasonLabel(
    SceneEditorSceneViewDegradedReason reason);

bool SceneEditorSceneViewPacketBuildFocusedObject(
    int focused_object_index,
    SceneEditorSceneViewPreviewQuality preview_quality,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorSceneViewPacket* out_packet);

bool SceneEditorSceneViewPacketToJsonString(const SceneEditorSceneViewPacket* packet,
                                            char* out_json,
                                            size_t out_json_capacity);
bool SceneEditorSceneViewPacketReadbackFromJsonString(
    const char* json,
    SceneEditorSceneViewPacketReadback* out_readback);

#endif
