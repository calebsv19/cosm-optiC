#include "editor/camera_editor.h"
#include "render/render_helper.h"
#include "path/path_system.h"
#include "camera/camera.h"
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

extern SDL_Rect addButton;
extern SDL_Rect deleteButton;
extern SDL_Rect toggleButton;

static bool cameraDragging = false;
static bool addModeActive = false;
static bool deleteModeActive = false;
static int camDraggingPoint = -1;
static int camDraggingVelocity = -1;
static int lastMouseX = 0;
static int lastMouseY = 0;

static const double kWheelZoomFactor = 0.10;   // 10% zoom per wheel tick
static const double kKeyZoomFactor   = 0.02;   // 2% zoom per +/- key press

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

void RenderEditorHUD(SDL_Renderer* renderer, const char* label) {
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
}

void RenderCameraEditor(SDL_Renderer* renderer) {
    Camera original = sceneSettings.camera;
    Camera editorCamera = BuildEditorCamera();
    sceneSettings.camera = editorCamera;
    SyncCameraPathStart();

    SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderSceneObjects(renderer, true);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Color lightColor = {0, 255, 0, 255};
    SDL_Color camPathColor = {0, 180, 255, 255};
    RenderBezierPathCamera(renderer, &sceneSettings.bezierPath, true, &sceneSettings.camera, lightColor);
    if (sceneSettings.cameraPath.numPoints >= 2) {
        RenderBezierPathCamera(renderer, &sceneSettings.cameraPath, true, &sceneSettings.camera, camPathColor);
    }

    sceneSettings.camera = original;

    RenderCameraViewportRect(renderer);
    RenderEditorHUD(renderer, "Camera");
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
            if (HandleCameraButtons(mx, my))
                return;
            if (IsClickingButtonMain(mx, my))
                return;

            Camera editorCam = BuildEditorCamera();
            CameraPoint worldPoint = CameraScreenToWorld(&editorCam, mx, my, width, height);
            double worldX = worldPoint.x;
            double worldY = worldPoint.y;

            camDraggingPoint = -1;
            camDraggingVelocity = -1;

            // Check for clicks on Bézier points
            for (int i = 0; i < sceneSettings.cameraPath.numPoints; i++) {
                double dx = worldX - sceneSettings.cameraPath.points[i].x;
                double dy = worldY - sceneSettings.cameraPath.points[i].y;
                if (dx * dx + dy * dy <= POINT_RADIUS * POINT_RADIUS) {
                    camDraggingPoint = i;
                    if (deleteModeActive) {
                        RemoveBezierPoint(&sceneSettings.cameraPath, i);
                    }
                    return;
                }
            }

            // Check for clicks on velocity handles
            for (int i = 0; i < sceneSettings.cameraPath.numPoints - 1; i++) {
                for (int j = 0; j < 2; j++) {
                    double vx = (j == 0) ? sceneSettings.cameraPath.points[i].x + sceneSettings.cameraPath.handles[i][0].vx
                                         : sceneSettings.cameraPath.points[i + 1].x + sceneSettings.cameraPath.handles[i][1].vx;
                    double vy = (j == 0) ? sceneSettings.cameraPath.points[i].y + sceneSettings.cameraPath.handles[i][0].vy
                                         : sceneSettings.cameraPath.points[i + 1].y + sceneSettings.cameraPath.handles[i][1].vy;

                    double dx = worldX - vx;
                    double dy = worldY - vy;
                    if (dx * dx + dy * dy <= POINT_RADIUS * POINT_RADIUS) {
                        camDraggingPoint = i;
                        camDraggingVelocity = j;
                        return;
                    }
                }
            }

            if (addModeActive) {
                AddBezierPoint(&sceneSettings.cameraPath, (int)worldX, (int)worldY);
                return;
            }

            cameraDragging = true;
            lastMouseX = mx;
            lastMouseY = my;
            break;
        }
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                cameraDragging = false;
                camDraggingPoint = -1;
                camDraggingVelocity = -1;
            }
            break;
        case SDL_MOUSEMOTION:
            if (camDraggingPoint != -1 && camDraggingVelocity == -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraScreenToWorld(&editorCam,
                                                          event->motion.x,
                                                          event->motion.y,
                                                          width,
                                                          height);
                MoveEndPoint(&sceneSettings.cameraPath, (int)round(current.x), (int)round(current.y), camDraggingPoint);
            } else if (camDraggingPoint != -1 && camDraggingVelocity != -1) {
                Camera editorCam = BuildEditorCamera();
                CameraPoint current = CameraScreenToWorld(&editorCam,
                                                          event->motion.x,
                                                          event->motion.y,
                                                          width,
                                                          height);
                MoveVelocityHandle(&sceneSettings.cameraPath,
                                   (int)round(current.x),
                                   (int)round(current.y),
                                   camDraggingPoint,
                                   camDraggingVelocity);
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
                double dx = prev.x - current.x;
                double dy = prev.y - current.y;
                CameraPan(&sceneSettings.camera, dx, dy);
                OffsetCameraPath(dx, dy);
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
            if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS) {
                ApplyZoomDelta(kKeyZoomFactor);
            } else if (key == SDLK_MINUS || key == SDLK_UNDERSCORE || key == SDLK_KP_MINUS) {
                ApplyZoomDelta(-kKeyZoomFactor);
            } else if (key == SDLK_t) {
                ToggleBezierPathMode(&sceneSettings.cameraPath);
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
