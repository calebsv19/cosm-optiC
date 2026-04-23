#include "editor/camera_editor.h"
#include "camera/camera_path_3d.h"
#include "render/render_helper.h"
#include "path/path_system.h"
#include "camera/camera.h"
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_tool_state.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/fluid/fluid_state.h"
#include "math/vec2.h"
#include "math/math_utils.h"
#include "ui/shared_theme_font_adapter.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

extern SDL_Rect addButton;
extern SDL_Rect deleteButton;
extern SDL_Rect selectButton;
static SDL_Rect cameraModeButton = {0};
static SDL_Rect cameraRotateLeftButton = {0};
static SDL_Rect cameraRotateRightButton = {0};
static SDL_Rect cameraLinkButton = {0};

static bool CameraEditorPointInRect(int x, int y, const SDL_Rect* rect) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void CameraEditorDrawPaneButton(SDL_Renderer* renderer,
                                       SDL_Rect rect,
                                       const char* label,
                                       bool active) {
    RayTracingThemePalette palette = {0};
    SDL_Color fill = {180, 180, 180, 255};
    SDL_Color border = {95, 95, 112, 255};
    SDL_Color text = {0, 0, 0, 255};
    if (!renderer || rect.w <= 0 || rect.h <= 0 || !label) return;
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        fill = active ? ray_tracing_theme_resolve_button_active_fill(palette) : palette.button_fill;
        border = palette.panel_border;
        text = ray_tracing_theme_choose_button_text(fill, palette);
    } else if (active) {
        fill = (SDL_Color){70, 140, 215, 255};
        text = (SDL_Color){245, 247, 250, 255};
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

static bool cameraDragging = false;
static int camDraggingPoint = -1;
static int camDraggingVelocity = -1;
static int camDraggingRotation = -1;
static int selectedCamPoint = -1;
static CameraEditorSelectionKind selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_NONE;
static int selectedCamHandleSegment = -1;
static int selectedCamHandleIndex = -1;
static int lastMouseX = 0;
static int lastMouseY = 0;
static const double kRotationHandleLength = 70.0;
static const int kRotationHandleVisRadius = 6;
static const int kRotationHandleHitRadius = 12;
static const double kHalfPi = M_PI * 0.5;
static const SDL_Color kRotHandleColor = {180, 120, 255, 220};

static const double kWheelZoomFactor = 0.10;   // 10% zoom per wheel tick
static const double kKeyZoomFactor   = 0.02;   // 2% zoom per +/- key press

typedef enum CameraEditorAction {
    CAMERA_EDITOR_ACTION_NONE = 0,
    CAMERA_EDITOR_ACTION_MOUSE_DOWN,
    CAMERA_EDITOR_ACTION_MOUSE_UP,
    CAMERA_EDITOR_ACTION_MOUSE_DRAG,
    CAMERA_EDITOR_ACTION_MOUSE_WHEEL,
    CAMERA_EDITOR_ACTION_KEY_DOWN,
    CAMERA_EDITOR_ACTION_QUIT
} CameraEditorAction;

static CameraEditorAction ResolveCameraEditorAction(const SDL_Event* event) {
    if (!event) return CAMERA_EDITOR_ACTION_NONE;
    if (event->type == SDL_MOUSEBUTTONDOWN) return CAMERA_EDITOR_ACTION_MOUSE_DOWN;
    if (event->type == SDL_MOUSEBUTTONUP) return CAMERA_EDITOR_ACTION_MOUSE_UP;
    if (event->type == SDL_MOUSEMOTION) return CAMERA_EDITOR_ACTION_MOUSE_DRAG;
    if (event->type == SDL_MOUSEWHEEL) return CAMERA_EDITOR_ACTION_MOUSE_WHEEL;
    if (event->type == SDL_KEYDOWN) return CAMERA_EDITOR_ACTION_KEY_DOWN;
    if (event->type == SDL_QUIT) return CAMERA_EDITOR_ACTION_QUIT;
    return CAMERA_EDITOR_ACTION_NONE;
}

static void ClampCameraToFluidBounds(Camera* cam) {
    if (!cam || !g_fluidGrid.valid) return;
    double margin_world = GetCurrentMarginPixels() / cam->zoom;
    double min_x = g_fluidGrid.min_x + margin_world;
    double max_x = g_fluidGrid.max_x - margin_world;
    double min_y = g_fluidGrid.min_y + margin_world;
    double max_y = g_fluidGrid.max_y - margin_world;
    if (min_x > max_x) { double mid = (g_fluidGrid.min_x + g_fluidGrid.max_x) * 0.5; min_x = max_x = mid; }
    if (min_y > max_y) { double mid = (g_fluidGrid.min_y + g_fluidGrid.max_y) * 0.5; min_y = max_y = mid; }
    if (cam->x < min_x) cam->x = min_x;
    if (cam->x > max_x) cam->x = max_x;
    if (cam->y < min_y) cam->y = min_y;
    if (cam->y > max_y) cam->y = max_y;
}

static bool HitOnScreen(const Camera* camera, double wx, double wy, int mx, int my, double radius) {
    if (!camera || radius <= 0.0) return false;
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    CameraPoint sp = SpaceModeAdapter_WorldToScreen(&view_ctx, wx, wy);
    double dx = sp.x - (double)mx;
    double dy = sp.y - (double)my;
    return (dx * dx + dy * dy) <= radius * radius;
}

static void EnforceCamHandleLink(int pointIndex) {
    Path* path = &sceneSettings.cameraPath;
    if (!path || pointIndex < 0 || pointIndex >= path->numPoints) return;
    if (!path->handleLink[pointIndex]) return;
    int outSeg = pointIndex;
    int inSeg = pointIndex - 1;
    bool hasOut = (outSeg >= 0 && outSeg < path->numPoints - 1);
    bool hasIn = (inSeg >= 0 && inSeg < path->numPoints - 1);
    if (!(hasOut && hasIn)) return;
    Velocity* outH = &path->handles[outSeg][0];
    Velocity* inH = &path->handles[inSeg][1];
    Velocity* src = outH;
    Velocity* dst = inH;
    if (fabs(outH->vx) < 1e-6 && fabs(outH->vy) < 1e-6 && (fabs(inH->vx) > 1e-6 || fabs(inH->vy) > 1e-6)) {
        src = inH;
        dst = outH;
    }
    dst->vx = -src->vx;
    dst->vy = -src->vy;
}

static void EnsureCameraRotationsSeeded(void) {
    Path* path = &sceneSettings.cameraPath;
    for (int i = 0; i < path->numPoints; i++) {
        if (!path->rotationSet[i]) {
            path->rotations[i] = sceneSettings.camera.rotation;
            path->rotationSet[i] = true;
        }
    }
}

static void SetCameraPointRotation(int index, double radians) {
    Path* path = &sceneSettings.cameraPath;
    if (index < 0 || index >= path->numPoints) return;
    path->rotations[index] = radians;
    path->rotationSet[index] = true;
}

static void SetCameraPointPitch(int index, double radians) {
    if (index < 0 || index >= sceneSettings.cameraPath.numPoints) return;
    sceneSettings.cameraPath3D.point_pitch[index] = radians;
}

static double CameraPointRotation(int index) {
    Path* path = &sceneSettings.cameraPath;
    if (index < 0 || index >= path->numPoints) return sceneSettings.camera.rotation;
    return path->rotationSet[index] ? path->rotations[index] : sceneSettings.camera.rotation;
}

static double CameraPointPitch(int index) {
    if (index < 0 || index >= sceneSettings.cameraPath.numPoints) return 0.0;
    return sceneSettings.cameraPath3D.point_pitch[index];
}

static Vec2 RotationHandleDir(double rotationRadians) {
    double drawAngle = rotationRadians - kHalfPi; // rotate so zero faces up visually
    return vec2(cos(drawAngle), sin(drawAngle));
}

static Vec2 RotationHandleEndWorld(int index) {
    Path* path = &sceneSettings.cameraPath;
    Vec2 base = vec2(path->points[index].x, path->points[index].y);
    Vec2 dir = RotationHandleDir(CameraPointRotation(index));
    return vec2_add(base, vec2_scale(dir, kRotationHandleLength));
}

static void CameraEditorResetSelectionMetadata(void) {
    selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_NONE;
    selectedCamHandleSegment = -1;
    selectedCamHandleIndex = -1;
}

static void OffsetCameraPath(double dx, double dy) {
    Path* path = &sceneSettings.cameraPath;
    for (int i = 0; i < path->numPoints; i++) {
        path->points[i].x += dx;
        path->points[i].y += dy;
    }
}

double GetCurrentMarginPixels(void) {
    sceneSettings.cameraMargin =
        CameraClampMarginPixels(sceneSettings.cameraMargin,
                                sceneSettings.windowWidth,
                                sceneSettings.windowHeight);
    return sceneSettings.cameraMargin;
}

static Camera BuildEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

static CameraPoint CameraEditorScreenToWorld(const Camera* camera,
                                             double screen_x,
                                             double screen_y,
                                             int width,
                                             int height) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera, width, height);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, screen_x, screen_y);
}

static CameraPoint CameraEditorWorldToScreen(const Camera* camera,
                                             double world_x,
                                             double world_y,
                                             int width,
                                             int height) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera, width, height);
    return SpaceModeAdapter_WorldToScreen(&view_ctx, world_x, world_y);
}

static void ApplyZoomDelta(double factor) {
    double delta = sceneSettings.camera.zoom * factor;
    CameraZoom(&sceneSettings.camera, delta, 0.01, 20.0);
}

static void RotateCamera(double deltaRadians) {
    CameraRotate(&sceneSettings.camera, deltaRadians);
}

static bool HandleCameraButtons(int mx, int my) {
    if (mx >= addButton.x && mx <= addButton.x + addButton.w &&
        my >= addButton.y && my <= addButton.y + addButton.h) {
        SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
        return true;
    }
    if (mx >= selectButton.x && mx <= selectButton.x + selectButton.w &&
        my >= selectButton.y && my <= selectButton.y + selectButton.h) {
        SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
        return true;
    }
    if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w &&
        my >= deleteButton.y && my <= deleteButton.y + deleteButton.h) {
        SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
        return true;
    }
    if (CameraEditorPointInRect(mx, my, &cameraModeButton)) {
        ToggleBezierPathMode(&sceneSettings.cameraPath);
        return true;
    }
    if (CameraEditorPointInRect(mx, my, &cameraRotateLeftButton)) {
        RotateCamera(-0.05);
        return true;
    }
    if (CameraEditorPointInRect(mx, my, &cameraRotateRightButton)) {
        RotateCamera(0.05);
        return true;
    }
    if (CameraEditorPointInRect(mx, my, &cameraLinkButton) &&
        selectedCamPoint >= 0 &&
        selectedCamPoint < sceneSettings.cameraPath.numPoints) {
        sceneSettings.cameraPath.handleLink[selectedCamPoint] = !sceneSettings.cameraPath.handleLink[selectedCamPoint];
        EnforceCamHandleLink(selectedCamPoint);
        return true;
    }
    return false;
}

CameraEditorHitRegion CameraEditorHitRegionAtPoint(int mx, int my) {
    if (IsClickingButtonMain(mx, my) || SceneEditorIsPaneToolButton(mx, my)) {
        return CAMERA_EDITOR_HIT_CONTROLS;
    }
    return CAMERA_EDITOR_HIT_CANVAS;
}

static void RenderCameraViewportRect(SDL_Renderer* renderer) {
    double margin = GetCurrentMarginPixels();
    SDL_Rect rect = {
        (int)lrint(margin),
        (int)lrint(margin),
        sceneSettings.windowWidth - (int)lrint(margin) * 2,
        sceneSettings.windowHeight - (int)lrint(margin) * 2
    };
    if (rect.w < 0) rect.w = 0;
    if (rect.h < 0) rect.h = 0;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
    SDL_RenderDrawRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x + rect.w, rect.y);
    SDL_RenderDrawLine(renderer, rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h);
}

void InitializeCameraEditor(void) {
    cameraDragging = false;
    lastMouseX = lastMouseY = 0;
    SceneEditorToolStateReset();
    camDraggingPoint = camDraggingVelocity = -1;
    CameraEditorResetSelectionMetadata();
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                        sceneSettings.windowWidth,
                                                        sceneSettings.windowHeight);
    EnsureCameraRotationsSeeded();
}

void RenderCameraEditor(SDL_Renderer* renderer) {
    Camera original = sceneSettings.camera;
    Camera editorCamera = BuildEditorCamera();
    SDL_Color objectColor = {255, 255, 255, 255};
    sceneSettings.camera = editorCamera;

    {
        RayTracingThemePalette palette = {0};
        if (ray_tracing_shared_theme_resolve_palette(&palette)) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.background_fill.r,
                                   palette.background_fill.g,
                                   palette.background_fill.b,
                                   255);
            objectColor = palette.text_primary;
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
        }
    }
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, objectColor.r, objectColor.g, objectColor.b, 255);
    RenderSceneObjects(renderer, !AnimationUseFluidScene());

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Color lightColor = {110, 130, 110, 140};
    SDL_Color camPathColor = {0, 180, 255, 255};
    SDL_Color camHandle = {255, 165, 0, 255};
    SDL_Color selectColor = {255, 255, 160, 255};
    RenderBezierPathCameraPassive(renderer,
                                  &sceneSettings.bezierPath,
                                  &sceneSettings.camera,
                                  lightColor,
                                  4);
    if (sceneSettings.cameraPath.numPoints >= 2) {
        RenderBezierPathCamera(renderer, &sceneSettings.cameraPath, true, &sceneSettings.camera, camPathColor, camHandle, selectedCamPoint, selectColor);
    }

    // Rotation handles (purple)
    for (int i = 1; i < sceneSettings.cameraPath.numPoints; i++) {  // skip start
        Vec2 base = vec2(sceneSettings.cameraPath.points[i].x, sceneSettings.cameraPath.points[i].y);
        Vec2 end = RotationHandleEndWorld(i);
        CameraPoint baseS = CameraEditorWorldToScreen(&sceneSettings.camera,
                                                      base.x,
                                                      base.y,
                                                      sceneSettings.windowWidth,
                                                      sceneSettings.windowHeight);
        CameraPoint endS = CameraEditorWorldToScreen(&sceneSettings.camera,
                                                     end.x,
                                                     end.y,
                                                     sceneSettings.windowWidth,
                                                     sceneSettings.windowHeight);
        SDL_Color col = (i == selectedCamPoint) ? (SDL_Color){200, 170, 255, 255} : kRotHandleColor;
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderDrawLine(renderer, (int)baseS.x, (int)baseS.y, (int)endS.x, (int)endS.y);
        for (int dx = -kRotationHandleVisRadius; dx <= kRotationHandleVisRadius; dx++) {
            for (int dy = -kRotationHandleVisRadius; dy <= kRotationHandleVisRadius; dy++) {
                if (dx * dx + dy * dy <= kRotationHandleVisRadius * kRotationHandleVisRadius) {
                    SDL_RenderDrawPoint(renderer, (int)endS.x + dx, (int)endS.y + dy);
                }
            }
        }
    }

    sceneSettings.camera = original;

    RenderCameraViewportRect(renderer);
}

int CameraEditorRenderPaneControls(SDL_Renderer* renderer, SDL_Rect content_bounds, int top_y, int bottom_y) {
    const int gap = 8;
    const int row_gap = 10;
    const int button_h = 34;
    char label[128];
    int cursor_y = top_y;
    int half_w = (content_bounds.w - row_gap) / 2;
    cameraModeButton = (SDL_Rect){0, 0, 0, 0};
    cameraRotateLeftButton = (SDL_Rect){0, 0, 0, 0};
    cameraRotateRightButton = (SDL_Rect){0, 0, 0, 0};
    cameraLinkButton = (SDL_Rect){0, 0, 0, 0};
    if (!renderer || content_bounds.w <= 0 || top_y >= bottom_y) return top_y;
    if (cursor_y + button_h > bottom_y) return cursor_y;
    cameraModeButton = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, button_h};
    snprintf(label,
             sizeof(label),
             "Path Mode: %s",
             (sceneSettings.cameraPath.mode == BEZIER_CUBIC) ? "Cubic" : "Quadratic");
    CameraEditorDrawPaneButton(renderer, cameraModeButton, label, false);
    cursor_y += button_h + gap;
    if (cursor_y + button_h <= bottom_y) {
        cameraRotateLeftButton = (SDL_Rect){content_bounds.x, cursor_y, half_w, button_h};
        cameraRotateRightButton = (SDL_Rect){content_bounds.x + half_w + row_gap,
                                             cursor_y,
                                             content_bounds.w - half_w - row_gap,
                                             button_h};
        CameraEditorDrawPaneButton(renderer, cameraRotateLeftButton, "Rotate -", false);
        CameraEditorDrawPaneButton(renderer, cameraRotateRightButton, "Rotate +", false);
        cursor_y += button_h + gap;
    }
    if (selectedCamPoint >= 0 &&
        selectedCamPoint < sceneSettings.cameraPath.numPoints &&
        cursor_y + button_h <= bottom_y) {
        bool linked = sceneSettings.cameraPath.handleLink[selectedCamPoint];
        cameraLinkButton = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, button_h};
        snprintf(label, sizeof(label), "Handles: %s", linked ? "Linked" : "Independent");
        CameraEditorDrawPaneButton(renderer, cameraLinkButton, label, linked);
        cursor_y += button_h + gap;
    }
    return cursor_y;
}

void HandleCameraEditorEvents(SDL_Event* event) {
    const int width = sceneSettings.windowWidth;
    const int height = sceneSettings.windowHeight;
    CameraEditorAction action = ResolveCameraEditorAction(event);

    switch (action) {
        case CAMERA_EDITOR_ACTION_MOUSE_DOWN: {
            if (event->button.button != SDL_BUTTON_LEFT)
                break;

            int mx = event->button.x;
            int my = event->button.y;

            if (HandleCameraButtons(mx, my))
                return;
            if (IsClickingButtonMain(mx, my))
                return;

            Camera editorCam = BuildEditorCamera();
            CameraPoint worldPoint = CameraEditorScreenToWorld(&editorCam, mx, my, width, height);
            Vec2 world = vec2(worldPoint.x, worldPoint.y);
            SceneEditorTool active_tool = SceneEditorToolStateGetEffective(SDL_GetModState());

            camDraggingPoint = -1;
            camDraggingVelocity = -1;
            camDraggingRotation = -1;
            bool consumed = false;

            // Check rotation handles first
            for (int i = 1; i < sceneSettings.cameraPath.numPoints; i++) { // skip start handle
                Vec2 end = RotationHandleEndWorld(i);
                int radius = kRotationHandleHitRadius;
                if (HitOnScreen(&editorCam, end.x, end.y, mx, my, radius)) {
                    camDraggingRotation = i;
                    selectedCamPoint = i;
                    selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_ROTATION_HANDLE;
                    selectedCamHandleSegment = -1;
                    selectedCamHandleIndex = -1;
                    consumed = true;
                    break;
                }
            }

            // Check for clicks on Bézier points first (priority over camera drag)
            if (!consumed) {
                for (int i = 0; i < sceneSettings.cameraPath.numPoints; i++) {
                    double px = sceneSettings.cameraPath.points[i].x;
                    double py = sceneSettings.cameraPath.points[i].y;
                    if (HitOnScreen(&editorCam, px, py, mx, my, POINT_HIT_RADIUS)) {
                        camDraggingPoint = i;
                        selectedCamPoint = i;
                        selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_POINT;
                        selectedCamHandleSegment = -1;
                        selectedCamHandleIndex = -1;
                        if (active_tool == SCENE_EDITOR_TOOL_DELETE) {
                            int old_count = sceneSettings.cameraPath.numPoints;
                            CameraPath3D_RemovePoint(&sceneSettings.cameraPath3D, i, old_count);
                            RemoveBezierPoint(&sceneSettings.cameraPath, i);
                            selectedCamPoint = -1;
                            CameraEditorResetSelectionMetadata();
                        }
                        consumed = true;
                        break;
                    }
                }
            }

            // Check for clicks on velocity handles
            if (!consumed) {
                for (int i = 0; i < sceneSettings.cameraPath.numPoints - 1; i++) {
                    for (int j = 0; j < 2; j++) {
                        double vx = (j == 0) ? sceneSettings.cameraPath.points[i].x + sceneSettings.cameraPath.handles[i][0].vx
                                             : sceneSettings.cameraPath.points[i + 1].x + sceneSettings.cameraPath.handles[i][1].vx;
                        double vy = (j == 0) ? sceneSettings.cameraPath.points[i].y + sceneSettings.cameraPath.handles[i][0].vy
                                             : sceneSettings.cameraPath.points[i + 1].y + sceneSettings.cameraPath.handles[i][1].vy;

                        if (HitOnScreen(&editorCam, vx, vy, mx, my, POINT_HIT_RADIUS)) {
                            camDraggingPoint = i;
                            camDraggingVelocity = j;
                            selectedCamPoint = (j == 0) ? i : i + 1;
                            selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_BEZIER_HANDLE;
                            selectedCamHandleSegment = i;
                            selectedCamHandleIndex = j;
                            consumed = true;
                            break;
                        }
                    }
                    if (consumed) break;
                }
            }

            if (active_tool == SCENE_EDITOR_TOOL_ADD) {
                CameraPath3D_InsertPoint(&sceneSettings.cameraPath3D,
                                         &sceneSettings.cameraPath,
                                         world.x,
                                         world.y,
                                         sceneSettings.cameraZ,
                                         8.0);
                double seedRot = (sceneSettings.cameraPath.numPoints >= 2)
                                     ? CameraPointRotation(sceneSettings.cameraPath.numPoints - 2)
                                     : sceneSettings.camera.rotation;
                SetCameraPointRotation(sceneSettings.cameraPath.numPoints - 1, seedRot);
                selectedCamPoint = sceneSettings.cameraPath.numPoints - 1;
                selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_POINT;
                selectedCamHandleSegment = -1;
                selectedCamHandleIndex = -1;
                return;
            }

            if (consumed) return;

            cameraDragging = true;
            lastMouseX = mx;
            lastMouseY = my;
            break;
        }
        case CAMERA_EDITOR_ACTION_MOUSE_UP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                cameraDragging = false;
                camDraggingPoint = -1;
                camDraggingVelocity = -1;
                camDraggingRotation = -1;
            }
            break;
        case CAMERA_EDITOR_ACTION_MOUSE_DRAG:
            if (camDraggingRotation != -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraEditorScreenToWorld(&editorCam,
                                                                event->motion.x,
                                                                event->motion.y,
                                                                width,
                                                                height);
                Vec2 cur = vec2(current.x, current.y);
                Vec2 base = vec2(sceneSettings.cameraPath.points[camDraggingRotation].x,
                                 sceneSettings.cameraPath.points[camDraggingRotation].y);
                Vec2 delta = vec2_sub(cur, base);
                double angle = atan2(delta.y, delta.x);
                double rotation = angle + kHalfPi; // map so zero faces up
                SetCameraPointRotation(camDraggingRotation, rotation);
            } else if (camDraggingPoint != -1 && camDraggingVelocity == -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraEditorScreenToWorld(&editorCam,
                                                                event->motion.x,
                                                                event->motion.y,
                                                                width,
                                                                height);
                Vec2 cur = vec2(current.x, current.y);
                MoveEndPoint(&sceneSettings.cameraPath, (int)round(cur.x), (int)round(cur.y), camDraggingPoint);
            } else if (camDraggingPoint == -1 && camDraggingVelocity == -1 && camDraggingRotation == -1 &&
                       (event->motion.state & SDL_BUTTON_LMASK) && selectedCamPoint >= 0) {
                // drag empty space while a point is selected should not pan camera; do nothing
            } else if (camDraggingPoint != -1 && camDraggingVelocity != -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraEditorScreenToWorld(&editorCam,
                                                                event->motion.x,
                                                                event->motion.y,
                                                                width,
                                                                height);
                Vec2 cur = vec2(current.x, current.y);
                MoveVelocityHandle(&sceneSettings.cameraPath,
                                   (int)round(cur.x),
                                   (int)round(cur.y),
                                   camDraggingPoint,
                                   camDraggingVelocity);
                EnforceCamHandleLink(selectedCamPoint);
            } else if ((event->motion.state & SDL_BUTTON_LMASK) &&
                       !cameraDragging &&
                       camDraggingPoint == -1 &&
                       camDraggingVelocity == -1 &&
                       camDraggingRotation == -1) {
                // Start a pan drag if nothing else is being dragged
                cameraDragging = true;
                lastMouseX = event->motion.x;
                lastMouseY = event->motion.y;
            } else if (cameraDragging) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint prev = CameraEditorScreenToWorld(&editorCam,
                                                             lastMouseX,
                                                             lastMouseY,
                                                             width,
                                                             height);
                CameraPoint current = CameraEditorScreenToWorld(&editorCam,
                                                                event->motion.x,
                                                                event->motion.y,
                                                                width,
                                                                height);
                Vec2 delta = vec2_sub(vec2(prev.x, prev.y), vec2(current.x, current.y));
                double beforeX = sceneSettings.camera.x;
                double beforeY = sceneSettings.camera.y;
                CameraPan(&sceneSettings.camera, delta.x, delta.y);
                ClampCameraToFluidBounds(&sceneSettings.camera);
                double usedDx = sceneSettings.camera.x - beforeX;
                double usedDy = sceneSettings.camera.y - beforeY;
                OffsetCameraPath(usedDx, usedDy);
                lastMouseX = event->motion.x;
                lastMouseY = event->motion.y;
            }
            break;
        case CAMERA_EDITOR_ACTION_MOUSE_WHEEL:
            if (event->wheel.y > 0) {
                ApplyZoomDelta(kWheelZoomFactor);
            } else if (event->wheel.y < 0) {
                ApplyZoomDelta(-kWheelZoomFactor);
            }
            break;
        case CAMERA_EDITOR_ACTION_KEY_DOWN: {
            SDL_Keycode key = event->key.keysym.sym;
            const double panStep = 20.0 / sceneSettings.camera.zoom;
            if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS) {
                ApplyZoomDelta(kKeyZoomFactor);
            } else if (key == SDLK_MINUS || key == SDLK_UNDERSCORE || key == SDLK_KP_MINUS) {
                ApplyZoomDelta(-kKeyZoomFactor);
            } else if (key == SDLK_LEFT) {
                double bx = sceneSettings.camera.x, by = sceneSettings.camera.y;
                CameraPan(&sceneSettings.camera, -panStep, 0.0);
                ClampCameraToFluidBounds(&sceneSettings.camera);
                OffsetCameraPath(sceneSettings.camera.x - bx, sceneSettings.camera.y - by);
            } else if (key == SDLK_RIGHT) {
                double bx = sceneSettings.camera.x, by = sceneSettings.camera.y;
                CameraPan(&sceneSettings.camera, panStep, 0.0);
                ClampCameraToFluidBounds(&sceneSettings.camera);
                OffsetCameraPath(sceneSettings.camera.x - bx, sceneSettings.camera.y - by);
            } else if (key == SDLK_UP) {
                double bx = sceneSettings.camera.x, by = sceneSettings.camera.y;
                CameraPan(&sceneSettings.camera, 0.0, -panStep);
                ClampCameraToFluidBounds(&sceneSettings.camera);
                OffsetCameraPath(sceneSettings.camera.x - bx, sceneSettings.camera.y - by);
            } else if (key == SDLK_DOWN) {
                double bx = sceneSettings.camera.x, by = sceneSettings.camera.y;
                CameraPan(&sceneSettings.camera, 0.0, panStep);
                ClampCameraToFluidBounds(&sceneSettings.camera);
                OffsetCameraPath(sceneSettings.camera.x - bx, sceneSettings.camera.y - by);
            } else if (key == SDLK_t) {
                ToggleBezierPathMode(&sceneSettings.cameraPath);
            } else if (key == SDLK_l) {
                if (selectedCamPoint >= 0 && selectedCamPoint < sceneSettings.cameraPath.numPoints) {
                    sceneSettings.cameraPath.handleLink[selectedCamPoint] = !sceneSettings.cameraPath.handleLink[selectedCamPoint];
                    printf("Camera path handle link for point %d: %s\n", selectedCamPoint, sceneSettings.cameraPath.handleLink[selectedCamPoint] ? "ON" : "OFF");
                    EnforceCamHandleLink(selectedCamPoint);
                }
            } else if (key == SDLK_o) {
                RotateCamera(-0.05);
            } else if (key == SDLK_p) {
                RotateCamera(0.05);
            } else if (key == SDLK_BACKSPACE || key == SDLK_DELETE || key == SDLK_KP_PERIOD) {
                if (selectedCamPoint > 0 && selectedCamPoint < sceneSettings.cameraPath.numPoints) {
                    int old_count = sceneSettings.cameraPath.numPoints;
                    CameraPath3D_RemovePoint(&sceneSettings.cameraPath3D, selectedCamPoint, old_count);
                    RemoveBezierPoint(&sceneSettings.cameraPath, selectedCamPoint);
                    selectedCamPoint = -1;
                    CameraEditorResetSelectionMetadata();
                    camDraggingPoint = -1;
                    camDraggingVelocity = -1;
                    camDraggingRotation = -1;
                }
            }
            break;
        }
        case CAMERA_EDITOR_ACTION_QUIT:
            sceneEditorExitFlag = true;
            break;
        case CAMERA_EDITOR_ACTION_NONE:
        default:
            break;
    }
}

int CameraEditorGetSelectedPointIndex(void) {
    return selectedCamPoint;
}

void CameraEditorSetSelectedPointIndex(int index) {
    if (index < 0 || index >= sceneSettings.cameraPath.numPoints) {
        selectedCamPoint = -1;
        CameraEditorResetSelectionMetadata();
        return;
    }
    selectedCamPoint = index;
    selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_POINT;
    selectedCamHandleSegment = -1;
    selectedCamHandleIndex = -1;
}

CameraEditorSelectionKind CameraEditorGetSelectionKind(void) {
    return selectedCamSelectionKind;
}

bool CameraEditorSelectBezierHandle(int segment_index, int handle_index) {
    int point_index = -1;
    if (segment_index < 0 || segment_index >= sceneSettings.cameraPath.numPoints - 1) {
        return false;
    }
    if (handle_index < 0 || handle_index > 1) {
        return false;
    }
    point_index = (handle_index == 0) ? segment_index : (segment_index + 1);
    selectedCamPoint = point_index;
    selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_BEZIER_HANDLE;
    selectedCamHandleSegment = segment_index;
    selectedCamHandleIndex = handle_index;
    return true;
}

bool CameraEditorSelectRotationHandle(int point_index) {
    if (point_index < 0 || point_index >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    selectedCamPoint = point_index;
    selectedCamSelectionKind = CAMERA_EDITOR_SELECTION_ROTATION_HANDLE;
    selectedCamHandleSegment = -1;
    selectedCamHandleIndex = -1;
    return true;
}

void CameraEditorClearSelection(void) {
    selectedCamPoint = -1;
    camDraggingPoint = -1;
    camDraggingVelocity = -1;
    camDraggingRotation = -1;
    CameraEditorResetSelectionMetadata();
}

bool CameraEditorMoveSelectedPointTo(double x, double y) {
    if (selectedCamPoint < 0 || selectedCamPoint >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    MoveEndPoint(&sceneSettings.cameraPath, (int)lround(x), (int)lround(y), selectedCamPoint);
    return true;
}

bool CameraEditorGetSelectedWorldPosition(double* out_x, double* out_y, double* out_z) {
    if (selectedCamPoint < 0 || selectedCamPoint >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    if (out_x) *out_x = sceneSettings.cameraPath.points[selectedCamPoint].x;
    if (out_y) *out_y = sceneSettings.cameraPath.points[selectedCamPoint].y;
    if (out_z) *out_z = sceneSettings.cameraPath3D.point_z[selectedCamPoint];
    return true;
}

bool CameraEditorGetSelectedGizmoWorldPosition(double* out_x, double* out_y, double* out_z) {
    if (selectedCamSelectionKind == CAMERA_EDITOR_SELECTION_BEZIER_HANDLE) {
        int point_index = -1;
        if (selectedCamHandleSegment < 0 ||
            selectedCamHandleSegment >= sceneSettings.cameraPath.numPoints - 1 ||
            selectedCamHandleIndex < 0 ||
            selectedCamHandleIndex > 1) {
            return false;
        }
        point_index = (selectedCamHandleIndex == 0) ? selectedCamHandleSegment : (selectedCamHandleSegment + 1);
        if (out_x) {
            *out_x = sceneSettings.cameraPath.points[point_index].x +
                     sceneSettings.cameraPath.handles[selectedCamHandleSegment][selectedCamHandleIndex].vx;
        }
        if (out_y) {
            *out_y = sceneSettings.cameraPath.points[point_index].y +
                     sceneSettings.cameraPath.handles[selectedCamHandleSegment][selectedCamHandleIndex].vy;
        }
        if (out_z) {
            *out_z = sceneSettings.cameraPath3D.point_z[point_index] +
                     sceneSettings.cameraPath3D.handles_vz[selectedCamHandleSegment][selectedCamHandleIndex];
        }
        return true;
    }
    if (selectedCamSelectionKind == CAMERA_EDITOR_SELECTION_ROTATION_HANDLE) {
        double rotation = 0.0;
        double pitch = 0.0;
        double draw_angle = 0.0;
        double horizontal_len = 0.0;
        if (selectedCamPoint < 0 || selectedCamPoint >= sceneSettings.cameraPath.numPoints) {
            return false;
        }
        rotation = CameraPointRotation(selectedCamPoint);
        pitch = CameraPointPitch(selectedCamPoint);
        draw_angle = rotation - kHalfPi;
        horizontal_len = cos(pitch) * kRotationHandleLength;
        if (out_x) {
            *out_x = sceneSettings.cameraPath.points[selectedCamPoint].x +
                     cos(draw_angle) * horizontal_len;
        }
        if (out_y) {
            *out_y = sceneSettings.cameraPath.points[selectedCamPoint].y +
                     sin(draw_angle) * horizontal_len;
        }
        if (out_z) {
            *out_z = sceneSettings.cameraPath3D.point_z[selectedCamPoint] +
                     sin(pitch) * kRotationHandleLength;
        }
        return true;
    }
    return CameraEditorGetSelectedWorldPosition(out_x, out_y, out_z);
}

bool CameraEditorMoveSelectedGizmoTo(double x, double y, double z) {
    if (selectedCamSelectionKind == CAMERA_EDITOR_SELECTION_BEZIER_HANDLE) {
        int point_index = -1;
        double anchor_x = 0.0;
        double anchor_y = 0.0;
        double anchor_z = 0.0;
        if (selectedCamHandleSegment < 0 ||
            selectedCamHandleSegment >= sceneSettings.cameraPath.numPoints - 1 ||
            selectedCamHandleIndex < 0 ||
            selectedCamHandleIndex > 1) {
            return false;
        }
        point_index = (selectedCamHandleIndex == 0) ? selectedCamHandleSegment : (selectedCamHandleSegment + 1);
        anchor_x = sceneSettings.cameraPath.points[point_index].x;
        anchor_y = sceneSettings.cameraPath.points[point_index].y;
        anchor_z = sceneSettings.cameraPath3D.point_z[point_index];
        sceneSettings.cameraPath.handles[selectedCamHandleSegment][selectedCamHandleIndex].vx = x - anchor_x;
        sceneSettings.cameraPath.handles[selectedCamHandleSegment][selectedCamHandleIndex].vy = y - anchor_y;
        sceneSettings.cameraPath3D.handles_vz[selectedCamHandleSegment][selectedCamHandleIndex] = z - anchor_z;
        EnforceCamHandleLink(point_index);
        return true;
    }
    if (selectedCamSelectionKind == CAMERA_EDITOR_SELECTION_ROTATION_HANDLE) {
        double base_x = 0.0;
        double base_y = 0.0;
        double dx = 0.0;
        double dy = 0.0;
        double draw_angle = 0.0;
        if (selectedCamPoint < 0 || selectedCamPoint >= sceneSettings.cameraPath.numPoints) {
            return false;
        }
        base_x = sceneSettings.cameraPath.points[selectedCamPoint].x;
        base_y = sceneSettings.cameraPath.points[selectedCamPoint].y;
        dx = x - base_x;
        dy = y - base_y;
        {
            double dz = z - sceneSettings.cameraPath3D.point_z[selectedCamPoint];
            double horizontal = sqrt(dx * dx + dy * dy);
            if (fabs(dx) <= 1e-6 && fabs(dy) <= 1e-6 && fabs(dz) <= 1e-6) {
                return false;
            }
            draw_angle = atan2(dy, dx);
            SetCameraPointRotation(selectedCamPoint, draw_angle + kHalfPi);
            SetCameraPointPitch(selectedCamPoint, atan2(dz, fmax(horizontal, 1e-6)));
            return true;
        }
    }
    if (!CameraEditorMoveSelectedPointTo(x, y)) {
        return false;
    }
    CameraEditorSetSelectedPointZ(z);
    return true;
}

void CameraEditorSetSelectedPointZ(double z) {
    if (selectedCamPoint < 0 || selectedCamPoint >= sceneSettings.cameraPath.numPoints) {
        return;
    }
    sceneSettings.cameraPath3D.point_z[selectedCamPoint] = z;
}

double CameraEditorGetPointRotation(int index) {
    return CameraPointRotation(index);
}

double CameraEditorGetPointPitch(int index) {
    return CameraPointPitch(index);
}
