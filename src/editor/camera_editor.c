#include "editor/camera_editor.h"
#include "render/render_helper.h"
#include "path/path_system.h"
#include "camera/camera.h"
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/fluid_state.h"
#include "math/vec2.h"
#include "math/math_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

extern SDL_Rect addButton;
extern SDL_Rect deleteButton;
extern SDL_Rect toggleButton;

static bool cameraDragging = false;
static bool sliderDragging = false;
static bool addModeActive = false;
static bool deleteModeActive = false;
static int camDraggingPoint = -1;
static int camDraggingVelocity = -1;
static int camDraggingRotation = -1;
static int selectedCamPoint = -1;
static int lastMouseX = 0;
static int lastMouseY = 0;
static SDL_Rect rotationSlider = {0};
static int rotationSliderValue = 0; // 0..360 degrees
static const double kRotationHandleLength = 70.0;
static const int kRotationHandleVisRadius = 6;
static const int kRotationHandleHitRadius = 12;
static const double kHalfPi = M_PI * 0.5;
static const SDL_Color kRotHandleColor = {180, 120, 255, 220};

static const double kWheelZoomFactor = 0.10;   // 10% zoom per wheel tick
static const double kKeyZoomFactor   = 0.02;   // 2% zoom per +/- key press

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
    CameraPoint sp = CameraWorldToScreen(camera,
                                         wx,
                                         wy,
                                         sceneSettings.windowWidth,
                                         sceneSettings.windowHeight);
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

static double CameraPointRotation(int index) {
    Path* path = &sceneSettings.cameraPath;
    if (index < 0 || index >= path->numPoints) return sceneSettings.camera.rotation;
    return path->rotationSet[index] ? path->rotations[index] : sceneSettings.camera.rotation;
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

static void SyncCameraPathStart(void) {
    Path* path = &sceneSettings.cameraPath;
    if (path->numPoints <= 0) {
        path->numPoints = 1;
        path->mode = BEZIER_CUBIC;
        path->handles[0][0].vx = 50;
        path->handles[0][0].vy = 0;
    }
    path->points[0].x = sceneSettings.camera.x;
    path->points[0].y = sceneSettings.camera.y;
    path->rotations[0] = 0.0;  // face up by default
    path->rotationSet[0] = true;
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
        addModeActive = !addModeActive;
        if (addModeActive) deleteModeActive = false;
        return true;
    }
    if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w &&
        my >= deleteButton.y && my <= deleteButton.y + deleteButton.h) {
        deleteModeActive = !deleteModeActive;
        if (deleteModeActive) addModeActive = false;
        return true;
    }
    if (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w &&
        my >= toggleButton.y && my <= toggleButton.y + toggleButton.h) {
        ToggleBezierPathMode(&sceneSettings.cameraPath);
        return true;
    }
    return false;
}

static void RenderCameraButtons(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, addModeActive ? 0 : 255, addModeActive ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &addButton);
    SDL_SetRenderDrawColor(renderer, deleteModeActive ? 255 : 255, deleteModeActive ? 0 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &deleteButton);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &toggleButton);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &addButton);
    RenderButtonText(renderer, addButton, "Add");
    SDL_RenderDrawRect(renderer, &deleteButton);
    RenderButtonText(renderer, deleteButton, "Delete");
    SDL_RenderDrawRect(renderer, &toggleButton);
    RenderButtonText(renderer, toggleButton, BEZIER_MODE_STRINGS[sceneSettings.cameraPath.mode]);
}

void RenderEditorHUD(SDL_Renderer* renderer, const char* label, bool showRotation) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s  |  Camera: (%.1f, %.1f)  Zoom: %.2f",
             label, sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.camera.zoom);
    SDL_Rect hud = {20, 20, 420, 30};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(renderer, &hud);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    SDL_RenderDrawRect(renderer, &hud);
    SDL_Color white = {255, 255, 255, 255};
    RenderLabelText(renderer, hud, buffer, white);

    if (showRotation) {
        SDL_Rect sliderBg = rotationSlider;
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 180);
        SDL_RenderFillRect(renderer, &sliderBg);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_RenderDrawRect(renderer, &sliderBg);

        double deg = sceneSettings.camera.rotation * 180.0 / M_PI;
        while (deg < 0) deg += 360.0;
        rotationSliderValue = (int)fmod(deg, 360.0);
        double t = rotationSliderValue / 360.0;
        int knobX = sliderBg.x + (int)(t * sliderBg.w);
        SDL_Rect knob = {knobX - 4, sliderBg.y - 3, 8, sliderBg.h + 6};
        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 220);
        SDL_RenderFillRect(renderer, &knob);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
        SDL_RenderDrawRect(renderer, &knob);
    }
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
    addModeActive = false;
    deleteModeActive = false;
    camDraggingPoint = camDraggingVelocity = -1;
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                        sceneSettings.windowWidth,
                                                        sceneSettings.windowHeight);
    SyncCameraPathStart();
    EnsureCameraRotationsSeeded();
    rotationSlider = (SDL_Rect){460, 25, 200, 10};
}

void RenderCameraEditor(SDL_Renderer* renderer) {
    Camera original = sceneSettings.camera;
    Camera editorCamera = BuildEditorCamera();
    sceneSettings.camera = editorCamera;
    SyncCameraPathStart();

    SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderSceneObjects(renderer, true);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Color lightColor = {110, 130, 110, 140};
    SDL_Color camPathColor = {0, 180, 255, 255};
    SDL_Color lightHandle = {255, 80, 80, 255};
    SDL_Color camHandle = {255, 165, 0, 255};
    SDL_Color selectColor = {255, 255, 160, 255};
    RenderBezierPathCameraStyled(renderer,
                                 &sceneSettings.bezierPath,
                                 false,
                                 &sceneSettings.camera,
                                 lightColor,
                                 lightHandle,
                                 -1,
                                 selectColor,
                                 4);
    if (sceneSettings.cameraPath.numPoints >= 2) {
        RenderBezierPathCamera(renderer, &sceneSettings.cameraPath, true, &sceneSettings.camera, camPathColor, camHandle, selectedCamPoint, selectColor);
    }

    // Rotation handles (purple)
    for (int i = 1; i < sceneSettings.cameraPath.numPoints; i++) {  // skip start
        Vec2 base = vec2(sceneSettings.cameraPath.points[i].x, sceneSettings.cameraPath.points[i].y);
        Vec2 end = RotationHandleEndWorld(i);
        CameraPoint baseS = CameraWorldToScreen(&sceneSettings.camera,
                                                base.x,
                                                base.y,
                                                sceneSettings.windowWidth,
                                                sceneSettings.windowHeight);
        CameraPoint endS = CameraWorldToScreen(&sceneSettings.camera,
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
    RenderEditorHUD(renderer, "Camera", true);
    RenderCameraButtons(renderer);
}

void HandleCameraEditorEvents(SDL_Event* event) {
    const int width = sceneSettings.windowWidth;
    const int height = sceneSettings.windowHeight;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN: {
            if (event->button.button != SDL_BUTTON_LEFT)
                break;

            int mx = event->button.x;
            int my = event->button.y;

            // Slider interaction (start drag)
            if (mx >= rotationSlider.x && mx <= rotationSlider.x + rotationSlider.w &&
                my >= rotationSlider.y - 4 && my <= rotationSlider.y + rotationSlider.h + 4) {
                double t = (double)(mx - rotationSlider.x) / (double)rotationSlider.w;
                t = clampd(t, 0.0, 1.0);
                double radians = t * 2.0 * M_PI;
                CameraSetRotation(&sceneSettings.camera, radians);
                cameraDragging = false;
                sliderDragging = true;
                return;
            }

            if (HandleCameraButtons(mx, my))
                return;
            if (IsClickingButtonMain(mx, my))
                return;

            Camera editorCam = BuildEditorCamera();
            CameraPoint worldPoint = CameraScreenToWorld(&editorCam, mx, my, width, height);
            Vec2 world = vec2(worldPoint.x, worldPoint.y);

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
                        if (deleteModeActive) {
                            RemoveBezierPoint(&sceneSettings.cameraPath, i);
                            selectedCamPoint = -1;
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
                            consumed = true;
                            break;
                        }
                    }
                    if (consumed) break;
                }
            }

            if (addModeActive) {
                AddBezierPoint(&sceneSettings.cameraPath, (int)world.x, (int)world.y);
                double seedRot = (sceneSettings.cameraPath.numPoints >= 2)
                                     ? CameraPointRotation(sceneSettings.cameraPath.numPoints - 2)
                                     : sceneSettings.camera.rotation;
                SetCameraPointRotation(sceneSettings.cameraPath.numPoints - 1, seedRot);
                selectedCamPoint = sceneSettings.cameraPath.numPoints - 1;
                return;
            }

            if (consumed) return;

            cameraDragging = true;
            lastMouseX = mx;
            lastMouseY = my;
            break;
        }
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                cameraDragging = false;
                sliderDragging = false;
                camDraggingPoint = -1;
                camDraggingVelocity = -1;
                camDraggingRotation = -1;
            }
            break;
        case SDL_MOUSEMOTION:
            if (sliderDragging) {
                double t = (double)(event->motion.x - rotationSlider.x) / (double)rotationSlider.w;
                t = clampd(t, 0.0, 1.0);
                double radians = t * 2.0 * M_PI;
                CameraSetRotation(&sceneSettings.camera, radians);
            } else if (camDraggingRotation != -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraScreenToWorld(&editorCam,
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
                CameraPoint current = CameraScreenToWorld(&editorCam,
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
                CameraPoint current = CameraScreenToWorld(&editorCam,
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
                CameraPoint prev = CameraScreenToWorld(&editorCam,
                                                       lastMouseX,
                                                       lastMouseY,
                                                       width,
                                                       height);
                CameraPoint current = CameraScreenToWorld(&editorCam,
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
        case SDL_MOUSEWHEEL:
            if (event->wheel.y > 0) {
                ApplyZoomDelta(kWheelZoomFactor);
            } else if (event->wheel.y < 0) {
                ApplyZoomDelta(-kWheelZoomFactor);
            }
            break;
        case SDL_KEYDOWN: {
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
                    RemoveBezierPoint(&sceneSettings.cameraPath, selectedCamPoint);
                    selectedCamPoint = -1;
                    camDraggingPoint = -1;
                    camDraggingVelocity = -1;
                    camDraggingRotation = -1;
                }
            }
            break;
        }
        case SDL_QUIT:
            SaveAllSettings();
            sceneEditorExitFlag = true;
            break;
        default:
            break;
    }
}
