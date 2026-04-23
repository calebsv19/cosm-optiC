
#include "editor/bezier_editor.h"
#include "app/animation.h"
#include "editor/scene_editor.h"
#include "path/path_system.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_tool_state.h"
#include "math/vec2.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <json-c/json.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern SDL_Rect addButton;  // the existing definition from scene_editor.c
extern SDL_Rect deleteButton;  // the existing definition from scene_editor.c
extern SDL_Rect selectButton;  // the visible shared tool button
static SDL_Rect bezierModeButton = {0};
static SDL_Rect bezierLinkButton = {0};

static bool BezierEditorPointInRect(int x, int y, const SDL_Rect* rect) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void BezierEditorDrawPaneButton(SDL_Renderer* renderer,
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


// Dragging state
int draggingPoint = -1;
int draggingVelocity = -1;
static int selectedPoint = -1;
static int selectedHandleSegment = -1;
static int selectedHandleIndex = -1;
static BezierEditorSelectionKind selectionKind = BEZIER_EDITOR_SELECTION_NONE;
static bool viewportPanDragging = false;
static int viewportPanLastMouseX = 0;
static int viewportPanLastMouseY = 0;

typedef enum BezierEditorAction {
    BEZIER_EDITOR_ACTION_NONE = 0,
    BEZIER_EDITOR_ACTION_MOUSE_DRAG,
    BEZIER_EDITOR_ACTION_MOUSE_DOWN_LEFT,
    BEZIER_EDITOR_ACTION_MOUSE_UP_LEFT,
    BEZIER_EDITOR_ACTION_KEY_DOWN
} BezierEditorAction;

static BezierEditorAction ResolveBezierEditorAction(const SDL_Event* event) {
    if (!event) return BEZIER_EDITOR_ACTION_NONE;
    if (event->type == SDL_MOUSEMOTION) return BEZIER_EDITOR_ACTION_MOUSE_DRAG;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        return BEZIER_EDITOR_ACTION_MOUSE_DOWN_LEFT;
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        return BEZIER_EDITOR_ACTION_MOUSE_UP_LEFT;
    }
    if (event->type == SDL_KEYDOWN) return BEZIER_EDITOR_ACTION_KEY_DOWN;
    return BEZIER_EDITOR_ACTION_NONE;
}

static Camera BuildBezierEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

static CameraPoint ScreenToWorldBezier(const Camera* camera, int sx, int sy) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, sx, sy);
}

static void PanViewportBezierByScreenDelta(int prev_x, int prev_y, int cur_x, int cur_y) {
    Camera previewCam = BuildBezierEditorCamera();
    CameraPoint prev = ScreenToWorldBezier(&previewCam, prev_x, prev_y);
    CameraPoint cur = ScreenToWorldBezier(&previewCam, cur_x, cur_y);
    CameraPan(&sceneSettings.camera, prev.x - cur.x, prev.y - cur.y);
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

static void EnforceHandleLink(Path* path, int pointIndex) {
    if (!path || pointIndex < 0 || pointIndex >= path->numPoints) return;
    if (!path->handleLink[pointIndex]) return;

    int outSeg = pointIndex;
    int inSeg = pointIndex - 1;
    bool hasOut = (outSeg >= 0 && outSeg < path->numPoints - 1);
    bool hasIn = (inSeg >= 0 && inSeg < path->numPoints - 1);

    if (!(hasOut && hasIn)) return;

    Velocity* outH = &path->handles[outSeg][0];
    Velocity* inH = &path->handles[inSeg][1];

    // If both are zero, seed a default handle
    if (fabs(outH->vx) < 1e-6 && fabs(outH->vy) < 1e-6 &&
        fabs(inH->vx) < 1e-6 && fabs(inH->vy) < 1e-6) {
        outH->vx = 50.0;
        outH->vy = 0.0;
    }

    // Choose outgoing as source unless zero
    Velocity* src = outH;
    Velocity* dst = inH;
    if (fabs(outH->vx) < 1e-6 && fabs(outH->vy) < 1e-6 && (fabs(inH->vx) > 1e-6 || fabs(inH->vy) > 1e-6)) {
        src = inH;
        dst = outH;
    }
    dst->vx = -src->vx;
    dst->vy = -src->vy;
}

static void RenderBezierViewportOverlay(SDL_Renderer* renderer, double margin) {
    SDL_Rect rect = {
        (int)lrint(margin),
        (int)lrint(margin),
        sceneSettings.windowWidth - (int)lrint(margin) * 2,
        sceneSettings.windowHeight - (int)lrint(margin) * 2
    };
    if (rect.w < 0) rect.w = 0;
    if (rect.h < 0) rect.h = 0;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawRect(renderer, &rect);
}

void InitializeBezierEditor(void) {
    SceneEditorToolStateReset();
    draggingPoint = -1;
    draggingVelocity = -1;
    selectedPoint = -1;
    selectedHandleSegment = -1;
    selectedHandleIndex = -1;
    selectionKind = BEZIER_EDITOR_SELECTION_NONE;
    viewportPanDragging = false;
    viewportPanLastMouseX = 0;
    viewportPanLastMouseY = 0;
}

void ToggleBezierPathMode(Path* path) {
    if (!path) return;
    if (path->mode == BEZIER_CUBIC) {
        path->mode = BEZIER_QUADRATIC;
        printf("Bézier Path Mode switched to QUADRATIC.\n");
    } else {
        path->mode = BEZIER_CUBIC;
        printf("Bézier Path Mode switched to CUBIC.\n");
    }
}


void MoveEndPoint(Path* path, int mx, int my, int pointIndex) {
    if (!path) return;
    if (pointIndex < 0 || pointIndex >= path->numPoints) {
        printf("ERROR: Invalid point index %d in MoveEndPoint.\n", pointIndex);
        return;
    }

    path->points[pointIndex].x = mx;
    path->points[pointIndex].y = my;
}

void MoveVelocityHandle(Path* path, int mx, int my, int segmentIndex, int handleIndex) {
    if (!path) return;
    if (segmentIndex < 0 || segmentIndex >= path->numPoints - 1) {
        printf("ERROR: Invalid segment index %d in MoveVelocityHandle.\n", segmentIndex);
        return;
    }
    if (handleIndex < 0 || handleIndex > 1) {
        printf("ERROR: Invalid velocity handle index %d for segment %d.\n", handleIndex, segmentIndex);
        return;
    }

    int pointIndex = (handleIndex == 0) ? segmentIndex : segmentIndex + 1;
    Velocity* target = &path->handles[segmentIndex][handleIndex];
    target->vx = mx - path->points[pointIndex].x;
    target->vy = my - path->points[pointIndex].y;

    // Mirror opposite handle if linking is enabled and both sides exist
    if (path->handleLink[pointIndex]) {
        int otherSeg = -1;
        int otherHandle = -1;
        if (handleIndex == 0 && pointIndex > 0) {
            otherSeg = segmentIndex - 1;   // incoming handle of previous segment
            otherHandle = 1;
        } else if (handleIndex == 1 && pointIndex < path->numPoints - 1) {
            otherSeg = segmentIndex + 1;   // outgoing handle of next segment
            otherHandle = 0;
        }
        if (otherSeg >= 0 && otherHandle >= 0 && otherSeg < path->numPoints - 1) {
            path->handles[otherSeg][otherHandle].vx = -target->vx;
            path->handles[otherSeg][otherHandle].vy = -target->vy;
        }
    }
    EnforceHandleLink(path, pointIndex);
}
    

void RemoveBezierPoint(Path* path, int index) {
    if (!path) return;
    if (index < 0 || index >= path->numPoints) {
        printf("ERROR: Invalid point index %d in RemoveBezierPoint.\n", index);
        return;
    }
    
    printf("Removing point %d at (%.2f, %.2f)\n", index, 
           path->points[index].x, 
           path->points[index].y);
        
    if (index == 0) {
        // Shift handles correctly
        for (int i = index; i < path->numPoints - 1; i++) {
            path->handles[i][0] = path->handles[i + 1][0];  // Shift outgoing handle
            path->handles[i][1] = path->handles[i + 1][1];  // Shift incoming handle
        }
    } else if (index < path->numPoints - 1) {
        // Shift handles correctly
        path->handles[index - 1][1] = path->handles[index][1];  // Shift incom handle
        
        for (int i = index + 1; i < path->numPoints - 1; i++) {
            path->handles[i][0] = path->handles[i + 1][0];  // Shift outgoing handle
            path->handles[i][1] = path->handles[i + 1][1];  // Shift incoming handle
        }
    }

    // Shift remaining points
    for (int i = index; i < path->numPoints; i++) {
        path->points[i] = path->points[i + 1];
    }

    // Shift handle links
    for (int i = index; i < path->numPoints; i++) {
        path->handleLink[i] = path->handleLink[i + 1];
    }

    // Update segment count
    path->numPoints--;

    // Update end handle value
    path->handles[path->numPoints][0] = (Velocity){0, 0};
    path->handleLink[path->numPoints] = false;
        
    printf("Updated Bézier path. New total points: %d\n", path->numPoints);
}


void AddBezierPointPrecise(Path* path, double x, double y, double default_handle_length) {
    double prev_x = 0.0;
    double prev_y = 0.0;
    if (!path) return;
    if (path->numPoints >= MAX_BEZIER_POINTS) {
        printf("Max points reached, cannot add more.\n");
        return;
    }
    if (!(default_handle_length > 0.0) || !isfinite(default_handle_length)) {
        default_handle_length = 50.0;
    }
            
    int index = path->numPoints;
    if (index > 0) {
        prev_x = path->points[index - 1].x;
        prev_y = path->points[index - 1].y;
    }
    path->points[index].x = x;
    path->points[index].y = y;
    path->rotations[index] = 0.0;
    path->rotationSet[index] = false;

    if (index < MAX_BEZIER_POINTS - 1) {
        path->handles[index][0].vx = 0.0;
        path->handles[index][0].vy = 0.0;
        path->handles[index][1].vx = 0.0;
        path->handles[index][1].vy = 0.0;
    }

    if (index > 0) {
        double dx = x - prev_x;
        double dy = y - prev_y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist > 1e-6) {
            double dir_x = dx / dist;
            double dir_y = dy / dist;
            double handle_length = fmin(default_handle_length, dist * 0.20);
            double min_handle_length = fmin(default_handle_length * 0.35, dist * 0.10);
            if (handle_length < min_handle_length) {
                handle_length = min_handle_length;
            }
            path->handles[index - 1][0].vx = dir_x * handle_length;
            path->handles[index - 1][0].vy = dir_y * handle_length;
            path->handles[index - 1][1].vx = -dir_x * handle_length;
            path->handles[index - 1][1].vy = -dir_y * handle_length;
        } else {
            path->handles[index - 1][0].vx = 0.0;
            path->handles[index - 1][0].vy = 0.0;
            path->handles[index - 1][1].vx = 0.0;
            path->handles[index - 1][1].vy = 0.0;
        }
    } else {
        path->handles[0][0].vx = 0.0;
        path->handles[0][0].vy = 0.0;
        path->handles[0][1].vx = 0.0;
        path->handles[0][1].vy = 0.0;
    }
     
    path->numPoints++;
    path->handleLink[index] = false;
    printf("Added new point at (%.3f, %.3f), Segment Count: %d\n", x, y, path->numPoints - 1);
}

void AddBezierPoint(Path* path, int x, int y) {
    AddBezierPointPrecise(path, (double)x, (double)y, 50.0);
}

bool IsClickingButtonBezier(int mx, int my) {
    return BezierEditorPointInRect(mx, my, &bezierModeButton) ||
           BezierEditorPointInRect(mx, my, &bezierLinkButton);
}

BezierEditorHitRegion BezierEditorHitRegionAtPoint(int mx, int my) {
    if (IsClickingButtonMain(mx, my) || SceneEditorIsPaneToolButton(mx, my)) {
        return BEZIER_EDITOR_HIT_CONTROLS;
    }
    return BEZIER_EDITOR_HIT_CANVAS;
}

void HandleBezierEditorKeyPress(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_a:
                SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
                printf("Add Mode: %s\n",
                       SceneEditorToolStateToolIsActive(SCENE_EDITOR_TOOL_ADD) ? "ON" : "OFF");
                break;

            case SDLK_d:
                SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
                printf("Delete Mode: %s\n",
                       SceneEditorToolStateToolIsActive(SCENE_EDITOR_TOOL_DELETE) ? "ON" : "OFF");
                break;

            case SDLK_t: // Toggle between cubic and quadratic Bézier paths
                ToggleBezierPathMode(&sceneSettings.bezierPath);
                break;

            case SDLK_l:
                if (selectedPoint >= 0 && selectedPoint < sceneSettings.bezierPath.numPoints) {
                    sceneSettings.bezierPath.handleLink[selectedPoint] = !sceneSettings.bezierPath.handleLink[selectedPoint];
                    printf("Handle link for point %d: %s\n", selectedPoint, sceneSettings.bezierPath.handleLink[selectedPoint] ? "ON" : "OFF");
                    EnforceHandleLink(&sceneSettings.bezierPath, selectedPoint);
                }
                break;
        }
    }
}

int BezierEditorGetSelectedPointIndex(void) {
    return selectedPoint;
}

void BezierEditorSetSelectedPointIndex(int index) {
    selectedPoint = index;
    if (index >= 0) {
        selectionKind = BEZIER_EDITOR_SELECTION_POINT;
        selectedHandleSegment = -1;
        selectedHandleIndex = -1;
    } else {
        selectionKind = BEZIER_EDITOR_SELECTION_NONE;
        selectedHandleSegment = -1;
        selectedHandleIndex = -1;
    }
}

BezierEditorSelectionKind BezierEditorGetSelectionKind(void) {
    return selectionKind;
}

void BezierEditorClearSelection(void) {
    selectedPoint = -1;
    selectedHandleSegment = -1;
    selectedHandleIndex = -1;
    selectionKind = BEZIER_EDITOR_SELECTION_NONE;
}

void BezierEditorSelectHandle(int segmentIndex, int handleIndex) {
    selectedHandleSegment = segmentIndex;
    selectedHandleIndex = handleIndex;
    selectedPoint = (handleIndex == 0) ? segmentIndex : (segmentIndex + 1);
    selectionKind = BEZIER_EDITOR_SELECTION_HANDLE;
}

bool BezierEditorGetSelectedHandle(int* out_segment_index, int* out_handle_index) {
    if (selectionKind != BEZIER_EDITOR_SELECTION_HANDLE ||
        selectedHandleSegment < 0 ||
        selectedHandleIndex < 0) {
        return false;
    }
    if (out_segment_index) *out_segment_index = selectedHandleSegment;
    if (out_handle_index) *out_handle_index = selectedHandleIndex;
    return true;
}

bool BezierEditorGetSelectionWorldPosition(double* out_x, double* out_y) {
    if (selectionKind == BEZIER_EDITOR_SELECTION_HANDLE &&
        selectedHandleSegment >= 0 &&
        selectedHandleSegment < sceneSettings.bezierPath.numPoints - 1 &&
        (selectedHandleIndex == 0 || selectedHandleIndex == 1)) {
        int pointIndex = (selectedHandleIndex == 0) ? selectedHandleSegment : (selectedHandleSegment + 1);
        if (out_x) {
            *out_x = sceneSettings.bezierPath.points[pointIndex].x +
                     sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex].vx;
        }
        if (out_y) {
            *out_y = sceneSettings.bezierPath.points[pointIndex].y +
                     sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex].vy;
        }
        return true;
    }
    if (selectionKind == BEZIER_EDITOR_SELECTION_POINT &&
        selectedPoint >= 0 &&
        selectedPoint < sceneSettings.bezierPath.numPoints) {
        if (out_x) *out_x = sceneSettings.bezierPath.points[selectedPoint].x;
        if (out_y) *out_y = sceneSettings.bezierPath.points[selectedPoint].y;
        return true;
    }
    return false;
}

bool BezierEditorGetSelectionWorldPosition3D(double* out_x, double* out_y, double* out_z) {
    if (selectionKind == BEZIER_EDITOR_SELECTION_HANDLE &&
        selectedHandleSegment >= 0 &&
        selectedHandleSegment < sceneSettings.bezierPath.numPoints - 1 &&
        (selectedHandleIndex == 0 || selectedHandleIndex == 1)) {
        int pointIndex = (selectedHandleIndex == 0) ? selectedHandleSegment : (selectedHandleSegment + 1);
        if (out_x) {
            *out_x = sceneSettings.bezierPath.points[pointIndex].x +
                     sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex].vx;
        }
        if (out_y) {
            *out_y = sceneSettings.bezierPath.points[pointIndex].y +
                     sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex].vy;
        }
        if (out_z) {
            *out_z = sceneSettings.bezierPath3D.point_z[pointIndex] +
                     sceneSettings.bezierPath3D.handles_vz[selectedHandleSegment][selectedHandleIndex];
        }
        return true;
    }
    if (selectionKind == BEZIER_EDITOR_SELECTION_POINT &&
        selectedPoint >= 0 &&
        selectedPoint < sceneSettings.bezierPath.numPoints) {
        if (out_x) *out_x = sceneSettings.bezierPath.points[selectedPoint].x;
        if (out_y) *out_y = sceneSettings.bezierPath.points[selectedPoint].y;
        if (out_z) *out_z = sceneSettings.bezierPath3D.point_z[selectedPoint];
        return true;
    }
    return false;
}

bool BezierEditorMoveSelectionTo(double world_x, double world_y) {
    if (selectionKind == BEZIER_EDITOR_SELECTION_HANDLE &&
        selectedHandleSegment >= 0 &&
        selectedHandleSegment < sceneSettings.bezierPath.numPoints - 1 &&
        (selectedHandleIndex == 0 || selectedHandleIndex == 1)) {
        int pointIndex = (selectedHandleIndex == 0) ? selectedHandleSegment : (selectedHandleSegment + 1);
        Velocity* target = &sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex];
        target->vx = world_x - sceneSettings.bezierPath.points[pointIndex].x;
        target->vy = world_y - sceneSettings.bezierPath.points[pointIndex].y;
        if (sceneSettings.bezierPath.handleLink[pointIndex]) {
            EnforceHandleLink(&sceneSettings.bezierPath, pointIndex);
        }
        return true;
    }
    if (selectionKind == BEZIER_EDITOR_SELECTION_POINT &&
        selectedPoint >= 0 &&
        selectedPoint < sceneSettings.bezierPath.numPoints) {
        sceneSettings.bezierPath.points[selectedPoint].x = world_x;
        sceneSettings.bezierPath.points[selectedPoint].y = world_y;
        return true;
    }
    return false;
}

bool BezierEditorMoveSelectionTo3D(double world_x, double world_y, double world_z) {
    if (selectionKind == BEZIER_EDITOR_SELECTION_HANDLE &&
        selectedHandleSegment >= 0 &&
        selectedHandleSegment < sceneSettings.bezierPath.numPoints - 1 &&
        (selectedHandleIndex == 0 || selectedHandleIndex == 1)) {
        int pointIndex = (selectedHandleIndex == 0) ? selectedHandleSegment : (selectedHandleSegment + 1);
        Velocity* target = &sceneSettings.bezierPath.handles[selectedHandleSegment][selectedHandleIndex];
        target->vx = world_x - sceneSettings.bezierPath.points[pointIndex].x;
        target->vy = world_y - sceneSettings.bezierPath.points[pointIndex].y;
        sceneSettings.bezierPath3D.handles_vz[selectedHandleSegment][selectedHandleIndex] =
            world_z - sceneSettings.bezierPath3D.point_z[pointIndex];
        if (sceneSettings.bezierPath.handleLink[pointIndex]) {
            EnforceHandleLink(&sceneSettings.bezierPath, pointIndex);
            sceneSettings.bezierPath3D.handles_vz[selectedHandleSegment][selectedHandleIndex ^ 1] =
                -sceneSettings.bezierPath3D.handles_vz[selectedHandleSegment][selectedHandleIndex];
        }
        return true;
    }
    if (selectionKind == BEZIER_EDITOR_SELECTION_POINT &&
        selectedPoint >= 0 &&
        selectedPoint < sceneSettings.bezierPath.numPoints) {
        sceneSettings.bezierPath.points[selectedPoint].x = world_x;
        sceneSettings.bezierPath.points[selectedPoint].y = world_y;
        sceneSettings.bezierPath3D.point_z[selectedPoint] = world_z;
        return true;
    }
    return false;
}
			

void HandleBezierEditorMouseClick(SDL_Event* event) {
    int mx = event->button.x;
    int my = event->button.y;
    Camera previewCam = BuildBezierEditorCamera();
    CameraPoint worldPoint = ScreenToWorldBezier(&previewCam, mx, my);
    Vec2 world = vec2(worldPoint.x, worldPoint.y);
    SceneEditorTool active_tool = SceneEditorToolStateGetEffective(SDL_GetModState());

    // Reset dragging states
    draggingPoint = -1;
    draggingVelocity = -1;
    viewportPanDragging = false;

    // Prevent accidental shape creation when clicking UI buttons
    bool clickedButton = IsClickingButtonMain(mx, my) || SceneEditorIsPaneToolButton(mx, my);
    if(!clickedButton){
	IsClickingButtonBezier(mx, my);
    }

    // Handle editor control buttons
    if (mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y &&
            my <= addButton.y + addButton.h) {
        SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
        return;
    }
    if (mx >= selectButton.x && mx <= selectButton.x + selectButton.w && my >= selectButton.y &&
            my <= selectButton.y + selectButton.h) {
        SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
        return;
    }
    if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y &&
            my <= deleteButton.y + deleteButton.h) {
        SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
        return;
    }
    if (BezierEditorPointInRect(mx, my, &bezierModeButton)) {
        ToggleBezierPathMode(&sceneSettings.bezierPath);
        return;
    }
    if (BezierEditorPointInRect(mx, my, &bezierLinkButton) &&
        selectedPoint >= 0 && selectedPoint < sceneSettings.bezierPath.numPoints) {
        sceneSettings.bezierPath.handleLink[selectedPoint] = !sceneSettings.bezierPath.handleLink[selectedPoint];
        EnforceHandleLink(&sceneSettings.bezierPath, selectedPoint);
        return;
    }
    // Check for clicks on Bézier points
    for (int i = 0; i < sceneSettings.bezierPath.numPoints; i++) {
        double px = sceneSettings.bezierPath.points[i].x;
        double py = sceneSettings.bezierPath.points[i].y;
        if (HitOnScreen(&previewCam, px, py, mx, my, POINT_HIT_RADIUS)) {
            draggingPoint = i;
            BezierEditorSetSelectedPointIndex(i);

            //  If in Delete Mode, remove the point
            if (active_tool == SCENE_EDITOR_TOOL_DELETE) {
                RemoveBezierPoint(&sceneSettings.bezierPath, i);
                BezierEditorClearSelection();
            }

            return;
        }
    }

    //  Check for clicks on velocity handles
    for (int i = 0; i < sceneSettings.bezierPath.numPoints - 1; i++) {
        for (int j = 0; j < 2; j++) {
            double vx = (j == 0) ? sceneSettings.bezierPath.points[i].x + sceneSettings.bezierPath.handles[i][0].vx
                                 : sceneSettings.bezierPath.points[i + 1].x + sceneSettings.bezierPath.handles[i][1].vx;
            double vy = (j == 0) ? sceneSettings.bezierPath.points[i].y + sceneSettings.bezierPath.handles[i][0].vy
                                 : sceneSettings.bezierPath.points[i + 1].y + sceneSettings.bezierPath.handles[i][1].vy;

            if (HitOnScreen(&previewCam, vx, vy, mx, my, POINT_HIT_RADIUS)) {
                draggingPoint = i;
                draggingVelocity = j;
                BezierEditorSelectHandle(i, j);
                return;
            }
        }
    }

    //  If in Add Mode, add a new point at the clicked position
    if (active_tool == SCENE_EDITOR_TOOL_ADD && !clickedButton) {
        AddBezierPoint(&sceneSettings.bezierPath, (int)world.x, (int)world.y);
        BezierEditorSetSelectedPointIndex(sceneSettings.bezierPath.numPoints - 1);
    } else if (!clickedButton) {
        BezierEditorClearSelection();
        viewportPanDragging = true;
        viewportPanLastMouseX = mx;
        viewportPanLastMouseY = my;
    }

}

void HandleBezierEditorEvents(SDL_Event* event, int* draggingPoint, int* draggingVelocity) {
    BezierEditorAction action = ResolveBezierEditorAction(event);
    if (action == BEZIER_EDITOR_ACTION_NONE) {
        return;
    }

    switch (action) {
        case BEZIER_EDITOR_ACTION_MOUSE_DRAG: {
            int screenX = event->motion.x;
            int screenY = event->motion.y;
            Camera previewCam = BuildBezierEditorCamera();
            CameraPoint worldPoint = ScreenToWorldBezier(&previewCam, screenX, screenY);
            Vec2 world = vec2(worldPoint.x, worldPoint.y);
            if (viewportPanDragging &&
                (*draggingPoint == -1) &&
                (*draggingVelocity == -1) &&
                (event->motion.state & SDL_BUTTON_LMASK)) {
                PanViewportBezierByScreenDelta(viewportPanLastMouseX,
                                               viewportPanLastMouseY,
                                               screenX,
                                               screenY);
                viewportPanLastMouseX = screenX;
                viewportPanLastMouseY = screenY;
            } else if (*draggingPoint != -1 && *draggingVelocity == -1) {
                MoveEndPoint(&sceneSettings.bezierPath, (int)round(world.x), (int)round(world.y), *draggingPoint);
            } else if (*draggingPoint != -1 && *draggingVelocity != -1) {
                MoveVelocityHandle(&sceneSettings.bezierPath,
                                   (int)round(world.x),
                                   (int)round(world.y),
                                   *draggingPoint,
                                   *draggingVelocity);
            }
            break;
        }
        case BEZIER_EDITOR_ACTION_MOUSE_DOWN_LEFT:
            HandleBezierEditorMouseClick(event);
            break;
        case BEZIER_EDITOR_ACTION_KEY_DOWN:
            HandleBezierEditorKeyPress(event);
            break;
        case BEZIER_EDITOR_ACTION_MOUSE_UP_LEFT:
            *draggingPoint = -1;
            *draggingVelocity = -1;
            viewportPanDragging = false;
            // keep selectedPoint as-is after drag to persist selection
            break;
        case BEZIER_EDITOR_ACTION_NONE:
        default:
            break;
    }
}
void RenderBezierEditor(SDL_Renderer* renderer) {
    RayTracingThemePalette palette = {0};
    SDL_Color objectColor = {255, 255, 255, 255};
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r,
                               palette.background_fill.g,
                               palette.background_fill.b,
                               255);
        objectColor = palette.text_primary;
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 85, 255);
    }
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    SDL_RenderFillRect(renderer, &bg);

    Camera preview = BuildBezierEditorCamera();
    Camera original = sceneSettings.camera;
    sceneSettings.camera = preview;

    SDL_SetRenderDrawColor(renderer, objectColor.r, objectColor.g, objectColor.b, 255);
    RenderSceneObjects(renderer, !AnimationUseFluidScene());

    if (sceneSettings.bezierPath.numPoints >= 2) {
        SDL_Color lightColor = {0, 255, 0, 255};
        SDL_Color handleColor = {255, 80, 80, 255};
        SDL_Color selectColor = {255, 255, 160, 255};
        RenderBezierPathCamera(renderer, &sceneSettings.bezierPath, true, &preview, lightColor, handleColor, selectedPoint, selectColor);
    }
    // Show faded camera path for context
    if (sceneSettings.cameraPath.numPoints >= 2) {
        SDL_Color camPathColor = {120, 180, 240, 130};
        RenderBezierPathCameraPassive(renderer,
                                      &sceneSettings.cameraPath,
                                      &preview,
                                      camPathColor,
                                      4);
    }

    sceneSettings.camera = original;

    RenderBezierViewportOverlay(renderer, GetCurrentMarginPixels());
}

int BezierEditorRenderPaneControls(SDL_Renderer* renderer, SDL_Rect content_bounds, int top_y, int bottom_y) {
    const int gap = 8;
    const int button_h = 34;
    char label[128];
    int cursor_y = top_y;
    bezierModeButton = (SDL_Rect){0, 0, 0, 0};
    bezierLinkButton = (SDL_Rect){0, 0, 0, 0};
    if (!renderer || content_bounds.w <= 0 || top_y >= bottom_y) return top_y;
    if (cursor_y + button_h > bottom_y) return cursor_y;
    bezierModeButton = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, button_h};
    snprintf(label,
             sizeof(label),
             "Path Mode: %s",
             (sceneSettings.bezierPath.mode == BEZIER_CUBIC) ? "Cubic" : "Quadratic");
    BezierEditorDrawPaneButton(renderer, bezierModeButton, label, false);
    cursor_y += button_h + gap;
    if (selectedPoint >= 0 &&
        selectedPoint < sceneSettings.bezierPath.numPoints &&
        cursor_y + button_h <= bottom_y) {
        bool linked = sceneSettings.bezierPath.handleLink[selectedPoint];
        bezierLinkButton = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, button_h};
        snprintf(label, sizeof(label), "Handles: %s", linked ? "Linked" : "Independent");
        BezierEditorDrawPaneButton(renderer, bezierLinkButton, label, linked);
        cursor_y += button_h + gap;
    }
    return cursor_y;
}
