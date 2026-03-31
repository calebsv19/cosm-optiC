#include "editor/editor_mode_router.h"

#include "render/ray_tracing_mode_backend.h"

static RayTracingRuntimeRoute ResolveRoute(void) {
    return RayTracingModeBackend_ResolveRoute();
}

int EditorModeRouter_ClampEditorMode(int current_mode, bool lock_object_mode) {
    if (!lock_object_mode) {
        if (current_mode < 0 || current_mode > 2) return 0;
        return current_mode;
    }
    if (current_mode == 2) return 2;
    return 0;
}

int EditorModeRouter_NextEditorMode(int current_mode, bool reverse, bool lock_object_mode) {
    current_mode = EditorModeRouter_ClampEditorMode(current_mode, lock_object_mode);

    if (!lock_object_mode) {
        if (reverse) {
            return (current_mode == 0) ? 2 : (current_mode - 1);
        }
        return (current_mode + 1) % 3;
    }

    if (reverse) {
        return (current_mode == 0) ? 2 : 0;
    }
    return (current_mode == 0) ? 2 : 0;
}

EditorModeCapabilities EditorModeRouter_GetCapabilities(void) {
    RayTracingRuntimeRoute route = ResolveRoute();
    EditorModeCapabilities caps;

    caps.isControlled3D = RayTracingModeBackend_IsControlled3D(&route);
    caps.uses2DProjectionFallback = route.fallbackTo2DProjection;
    caps.canEditXY = true;
    caps.canEditZ = false;
    caps.canUse3DGizmos = false;
    caps.canUseFreeCamera3D = false;

    return caps;
}

bool EditorModeRouter_IsControlled3D(void) {
    EditorModeCapabilities caps = EditorModeRouter_GetCapabilities();
    return caps.isControlled3D;
}

const char* EditorModeRouter_SpaceButtonLabel(void) {
    if (EditorModeRouter_IsControlled3D()) {
        return "Space: 3D (Scaffold)";
    }
    return "Space: 2D";
}

const char* EditorModeRouter_RuntimeHintLabel(void) {
    if (EditorModeRouter_IsControlled3D()) {
        return "3D scaffold active: editor routes through 2D backend.";
    }
    return "2D backend active.";
}

SpaceModeViewContext EditorModeRouter_BuildViewContext(const Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height) {
    RayTracingRuntimeRoute route = ResolveRoute();
    return RayTracingModeBackend_BuildViewContext(camera, viewport_width, viewport_height, &route);
}
