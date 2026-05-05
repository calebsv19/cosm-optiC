#include "editor/editor_mode_router.h"

#include "render/ray_tracing_mode_backend.h"

static RayTracingRuntimeRoute ResolveRoute(void) {
    return RayTracingModeBackend_ResolveRoute();
}

int EditorModeRouter_ClampEditorMode(int current_mode, bool lock_object_mode) {
    if (!lock_object_mode) {
        if (current_mode < 0 || current_mode >= EDITOR_MODE_COUNT) return EDITOR_MODE_PATH;
        return current_mode;
    }
    if (current_mode == EDITOR_MODE_CAMERA) return EDITOR_MODE_CAMERA;
    return EDITOR_MODE_PATH;
}

int EditorModeRouter_NextEditorMode(int current_mode, bool reverse, bool lock_object_mode) {
    current_mode = EditorModeRouter_ClampEditorMode(current_mode, lock_object_mode);

    if (!lock_object_mode) {
        if (reverse) {
            return (current_mode == EDITOR_MODE_PATH) ? (EDITOR_MODE_COUNT - 1) : (current_mode - 1);
        }
        return (current_mode + 1) % EDITOR_MODE_COUNT;
    }

    if (reverse) {
        return (current_mode == EDITOR_MODE_PATH) ? EDITOR_MODE_CAMERA : EDITOR_MODE_PATH;
    }
    return (current_mode == EDITOR_MODE_PATH) ? EDITOR_MODE_CAMERA : EDITOR_MODE_PATH;
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
    RayTracingRuntimeRoute route = ResolveRoute();
    if (RayTracingModeBackend_IsNative3D(&route)) {
        return "Space: 3D (Native)";
    }
    if (RayTracingModeBackend_IsCompat3DFallback(&route)) {
        return "Space: 3D (Compat Fallback)";
    }
    return "Space: 2D";
}

const char* EditorModeRouter_RuntimeHintLabel(void) {
    RayTracingRuntimeRoute route = ResolveRoute();
    if (RayTracingModeBackend_IsNative3D(&route)) {
        return "3D native route active: bounded runtime scene contracts are ready.";
    }
    if (RayTracingModeBackend_IsCompat3DFallback(&route)) {
        return "3D compat fallback active: editor routes through 2D projection/backend.";
    }
    return "2D backend active.";
}

SpaceModeViewContext EditorModeRouter_BuildViewContext(const Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height) {
    RayTracingRuntimeRoute route = ResolveRoute();
    RayTracingViewCarrier viewCarrier = RayTracingModeBackend_BuildViewCarrier(camera,
                                                                               viewport_width,
                                                                               viewport_height,
                                                                               &route);
    return viewCarrier.viewContext;
}
