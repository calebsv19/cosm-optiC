#include "editor/camera_editor.h"
#include "render/render_helper.h"
#include "path/path_system.h"
#include "camera/camera.h"
#include "editor/scene_editor.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

extern SDL_Rect addButton;
extern SDL_Rect deleteButton;
extern SDL_Rect toggleButton;

static bool cameraDragging = false;
static int lastMouseX = 0;
static int lastMouseY = 0;

static const double kWheelZoomFactor = 0.10;   // 10% zoom per wheel tick
static const double kKeyZoomFactor   = 0.02;   // 2% zoom per +/- key press
static const double kMarginStepRatio = 0.02;   // 2% of min dimension per step

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

static void AdjustMargin(double direction) {
    double step = fmin(sceneSettings.windowWidth, sceneSettings.windowHeight) * kMarginStepRatio;
    if (step < 5.0) step = 5.0;
    sceneSettings.cameraMargin += direction * step;
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                        sceneSettings.windowWidth,
                                                        sceneSettings.windowHeight);
}

static bool HandleCameraButtons(int mx, int my) {
    if (mx >= addButton.x && mx <= addButton.x + addButton.w &&
        my >= addButton.y && my <= addButton.y + addButton.h) {
        // Reduce margin (zoom in to camera box)
        AdjustMargin(-1.0);
        return true;
    }
    if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w &&
        my >= deleteButton.y && my <= deleteButton.y + deleteButton.h) {
        // Increase margin (show more outside camera view)
        AdjustMargin(1.0);
        return true;
    }
    if (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w &&
        my >= toggleButton.y && my <= toggleButton.y + toggleButton.h) {
        printf("Camera Editor: Path control coming soon.\n");
        return true;
    }
    return false;
}

static void RenderCameraButtons(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_RenderFillRect(renderer, &addButton);
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &deleteButton);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &toggleButton);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &addButton);
    RenderButtonText(renderer, addButton, "Margin-");
    SDL_RenderDrawRect(renderer, &deleteButton);
    RenderButtonText(renderer, deleteButton, "Margin+");
    SDL_RenderDrawRect(renderer, &toggleButton);
    RenderButtonText(renderer, toggleButton, "Path+");
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
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                        sceneSettings.windowWidth,
                                                        sceneSettings.windowHeight);
}

void RenderCameraEditor(SDL_Renderer* renderer) {
    Camera original = sceneSettings.camera;
    Camera editorCamera = BuildEditorCamera();
    sceneSettings.camera = editorCamera;

    SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderSceneObjects(renderer, true);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    RenderBezierPathCamera(renderer, &sceneSettings.bezierPath, true, &sceneSettings.camera);

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

            cameraDragging = true;
            lastMouseX = mx;
            lastMouseY = my;
            break;
        }
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                cameraDragging = false;
            }
            break;
        case SDL_MOUSEMOTION:
            if (cameraDragging) {
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
                CameraPan(&sceneSettings.camera,
                          prev.x - current.x,
                          prev.y - current.y);
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
