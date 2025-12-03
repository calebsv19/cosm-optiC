
#include "editor/bezier_editor.h"
#include "app/animation.h"
#include "editor/scene_editor.h"
#include "path/path_system.h"
#include "config/config_manager.h"
#include "camera/camera.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <json-c/json.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern SDL_Rect applyButton;  // the existing definition from scene_editor.c
extern SDL_Rect addButton;  // the existing definition from scene_editor.c
extern SDL_Rect deleteButton;  // the existing definition from scene_editor.c
extern SDL_Rect toggleButton;  // the existing definition from scene_editor.c


// Global mode states
static bool addModeActive = false;
static bool deleteModeActive = false;

// Dragging state
int draggingPoint = -1;
int draggingVelocity = -1;

static Camera BuildBezierEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

static CameraPoint ScreenToWorldBezier(const Camera* camera, int sx, int sy) {
    return CameraScreenToWorld(camera,
                               sx,
                               sy,
                               sceneSettings.windowWidth,
                               sceneSettings.windowHeight);
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

    if (handleIndex == 0) {
        path->handles[segmentIndex][0].vx = mx - path->points[segmentIndex].x;
        path->handles[segmentIndex][0].vy = my - path->points[segmentIndex].y;
    } else {  
        path->handles[segmentIndex][1].vx = mx - path->points[segmentIndex + 1].x;
        path->handles[segmentIndex][1].vy = my - path->points[segmentIndex + 1].y;
    }
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

    // Update segment count
    path->numPoints--;

    // Update end handle value
    path->handles[path->numPoints][0] = (Velocity){0, 0};
        
    printf("Updated Bézier path. New total points: %d\n", path->numPoints);
}


void AddBezierPoint(Path* path, int x, int y) {
    if (!path) return;
    if (path->numPoints >= MAX_BEZIER_POINTS) {
        printf("Max points reached, cannot add more.\n");
        return;
    }
            
    int index = path->numPoints;
    path->points[index].x = x;
    path->points[index].y = y;
         
    // First point gets no previous handle
    if (index == 0) {
        path->handles[0][0].vx = 50;
        path->handles[0][0].vy = 0;
    }
    // Second point initializes first segment
    else if (index == 1) {
        path->handles[0][1].vx = -50;
        path->handles[0][1].vy = 0;
    }   
    // Third point makes the previous endpoint a midpoint 
    else {
        path->handles[index - 1][0].vx = 50;
        path->handles[index - 1][0].vy = 0;
            
        path->handles[index - 1][1].vx = -50;
        path->handles[index - 1][1].vy = 0;
    }
     
    path->numPoints++;
    printf("Added new point at (%d, %d), Segment Count: %d\n", x, y, path->numPoints - 1);
}

bool IsClickingButtonBezier(int mx, int my) {
    (void)mx;
    (void)my;
    return false;  // Click is not inside a UI button
}

void HandleBezierEditorKeyPress(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_a:
                addModeActive = !addModeActive;
                deleteModeActive = false;
                printf("Add Mode: %s\n", addModeActive ? "ON" : "OFF");
                break;

            case SDLK_d:
                deleteModeActive = !deleteModeActive;
                addModeActive = false;
                printf("Delete Mode: %s\n", deleteModeActive ? "ON" : "OFF");
                break;

            case SDLK_t: // Toggle between cubic and quadratic Bézier paths
                ToggleBezierPathMode(&sceneSettings.bezierPath);
                break;
        }
    }
}
			

void HandleBezierEditorMouseClick(SDL_Event* event) {
    int mx = event->button.x;
    int my = event->button.y;
    Camera previewCam = BuildBezierEditorCamera();
    CameraPoint worldPoint = ScreenToWorldBezier(&previewCam, mx, my);
    double worldX = worldPoint.x;
    double worldY = worldPoint.y;

    // Reset dragging states
    draggingPoint = -1;
    draggingVelocity = -1;

    // Prevent accidental shape creation when clicking UI buttons
    bool clickedButton = IsClickingButtonMain(mx, my);
    if(!clickedButton){
	IsClickingButtonBezier(mx, my);
    }

    //  Check if clicking inside the Add or Delete button area
    if (mx >= sceneSettings.windowWidth - addButton.x * 2 - 20 && mx <= sceneSettings.windowWidth - 20) {
	// Handle UI Buttons Clicks 
        if (mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y && 
                my <= addButton.y + addButton.h) {
            addModeActive = !addModeActive;
            deleteModeActive = false;
            return;
        }
        if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y && 
                my <= deleteButton.y + deleteButton.h) {
            deleteModeActive = !deleteModeActive;
            addModeActive = false;
            return;
        }

	if (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w && my >= toggleButton.y &&
                        my <= toggleButton.y + toggleButton.h) {
            ToggleBezierPathMode(&sceneSettings.bezierPath);
            return;
        }
    }

    // Check for clicks on Bézier points
    for (int i = 0; i < sceneSettings.bezierPath.numPoints; i++) {
        if ((worldX - sceneSettings.bezierPath.points[i].x) * (worldX - sceneSettings.bezierPath.points[i].x) +
            (worldY - sceneSettings.bezierPath.points[i].y) * (worldY - sceneSettings.bezierPath.points[i].y) <=
            POINT_RADIUS * POINT_RADIUS) {
            draggingPoint = i;

            //  If in Delete Mode, remove the point
            if (deleteModeActive) {
                RemoveBezierPoint(&sceneSettings.bezierPath, i);
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

            if ((worldX - vx) * (worldX - vx) + (worldY - vy) * (worldY - vy) <= POINT_RADIUS * POINT_RADIUS) {
                draggingPoint = i;
                draggingVelocity = j;
                return;
            }
        }
    }

    //  If in Add Mode, add a new point at the clicked position
    if (addModeActive && !clickedButton) {
        AddBezierPoint(&sceneSettings.bezierPath, (int)worldX, (int)worldY);
    }

}

void HandleBezierEditorEvents(SDL_Event* event, int* draggingPoint, int* draggingVelocity) {
    int screenX = 0;
    int screenY = 0;
    if (event->type == SDL_MOUSEMOTION) {
        screenX = event->motion.x;
        screenY = event->motion.y;
    } else {
        screenX = event->button.x;
        screenY = event->button.y;
    }
    Camera previewCam = BuildBezierEditorCamera();
    CameraPoint worldPoint = ScreenToWorldBezier(&previewCam, screenX, screenY);
    int worldX = (int)round(worldPoint.x);
    int worldY = (int)round(worldPoint.y);

    // ✅ Handle mouse movement for dragging points or velocity handles
    if (event->type == SDL_MOUSEMOTION) {
        if (*draggingPoint != -1 && *draggingVelocity == -1) {
            MoveEndPoint(&sceneSettings.bezierPath, worldX, worldY, *draggingPoint);
        } else if (*draggingPoint != -1 && *draggingVelocity != -1) {
            MoveVelocityHandle(&sceneSettings.bezierPath, worldX, worldY, *draggingPoint, *draggingVelocity);
        }
    }
    // ✅ Forward mouse click events to HandleBezierEditorMouseClick()
    else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        HandleBezierEditorMouseClick(event);
    }
    // ✅ Reset dragging states when releasing the mouse
    else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        *draggingPoint = -1;
        *draggingVelocity = -1;
    }
}



void RenderBezierEditorUI(SDL_Renderer* renderer) {
    // Button colors (white when inactive, green/red when active)
    SDL_SetRenderDrawColor(renderer, addModeActive ? 0 : 255, addModeActive ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &addButton); // Add Point button
    
    SDL_SetRenderDrawColor(renderer, deleteModeActive ? 255 : 255, deleteModeActive ? 0 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &deleteButton); // Delete Point button
        
    // Draw button outlines
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &addButton);
    RenderButtonText(renderer, addButton, "Add");
    SDL_RenderDrawRect(renderer, &deleteButton);
    RenderButtonText(renderer, deleteButton, "Delete");
    SDL_RenderDrawRect(renderer, &toggleButton);
    RenderButtonText(renderer, toggleButton, BEZIER_MODE_STRINGS[sceneSettings.bezierPath.mode]);
}
 
void RenderBezierEditor(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 80, 80, 85, 255);  
    SDL_RenderClear(renderer);  

    Camera preview = BuildBezierEditorCamera();
    Camera original = sceneSettings.camera;
    sceneSettings.camera = preview;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderSceneObjects(renderer, true);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    if (sceneSettings.bezierPath.numPoints >= 2) {
        SDL_Color lightColor = {0, 255, 0, 255};
	RenderBezierPathCamera(renderer, &sceneSettings.bezierPath, true, &preview, lightColor);
    }

    sceneSettings.camera = original;

    RenderBezierViewportOverlay(renderer, GetCurrentMarginPixels());
    RenderEditorHUD(renderer, "Bezier");
    RenderBezierEditorUI(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
}
