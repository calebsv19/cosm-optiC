#ifndef EDITOR_MODE_ROUTER_H
#define EDITOR_MODE_ROUTER_H

#include <stdbool.h>

#include "camera/camera.h"
#include "render/space_mode_adapter.h"

typedef struct {
    bool isControlled3D;
    bool uses2DProjectionFallback;
    bool canEditXY;
    bool canEditZ;
    bool canUse3DGizmos;
    bool canUseFreeCamera3D;
} EditorModeCapabilities;

int EditorModeRouter_ClampEditorMode(int current_mode, bool lock_object_mode);
int EditorModeRouter_NextEditorMode(int current_mode, bool reverse, bool lock_object_mode);

EditorModeCapabilities EditorModeRouter_GetCapabilities(void);
bool EditorModeRouter_IsControlled3D(void);
const char* EditorModeRouter_SpaceButtonLabel(void);
const char* EditorModeRouter_RuntimeHintLabel(void);

SpaceModeViewContext EditorModeRouter_BuildViewContext(const Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height);

#endif
