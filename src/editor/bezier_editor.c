
#include "editor/bezier_editor.h"
#include "app/animation.h"
#include "editor/scene_editor.h"
#include "path/path_system.h"
#include "config/config_manager.h"

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

void InitializeBezierEditor(void) {

}

void ToggleBezierPathMode(void) {
    if (sceneSettings.bezierPath.mode == BEZIER_CUBIC) {
        sceneSettings.bezierPath.mode = BEZIER_QUADRATIC;
        printf("Bézier Path Mode switched to QUADRATIC.\n");
    } else {
        sceneSettings.bezierPath.mode = BEZIER_CUBIC;
        printf("Bézier Path Mode switched to CUBIC.\n");
    }
}


void MoveEndPoint(int mx, int my, int pointIndex) {
    if (pointIndex < 0 || pointIndex >= sceneSettings.bezierPath.numPoints) {
        printf("ERROR: Invalid point index %d in MoveEndPoint.\n", pointIndex);
        return;
    }

    // Move the selected Bézier path point
    sceneSettings.bezierPath.points[pointIndex].x = mx;
    sceneSettings.bezierPath.points[pointIndex].y = my;
}

void MoveVelocityHandle(int mx, int my, int segmentIndex, int handleIndex) {
    if (segmentIndex < 0 || segmentIndex >= sceneSettings.bezierPath.numPoints - 1) {
        printf("ERROR: Invalid segment index %d in MoveVelocityHandle.\n", segmentIndex);
        return;
    }
    if (handleIndex < 0 || handleIndex > 1) {
        printf("ERROR: Invalid velocity handle index %d for segment %d.\n", handleIndex, segmentIndex);
        return;
    }

    // ✅ Fix: Both handles should update symmetrically
    if (handleIndex == 0) {
        sceneSettings.bezierPath.handles[segmentIndex][0].vx = mx - sceneSettings.bezierPath.points[segmentIndex].x;
        sceneSettings.bezierPath.handles[segmentIndex][0].vy = my - sceneSettings.bezierPath.points[segmentIndex].y;
    } else {  
        sceneSettings.bezierPath.handles[segmentIndex][1].vx = mx - sceneSettings.bezierPath.points[segmentIndex + 1].x;
        sceneSettings.bezierPath.handles[segmentIndex][1].vy = my - sceneSettings.bezierPath.points[segmentIndex + 1].y;
    }
}
    

void RemoveBezierPoint(int index) {
    if (index < 0 || index >= sceneSettings.bezierPath.numPoints) {
        printf("ERROR: Invalid point index %d in RemoveBezierPoint.\n", index);
        return;
    }
    
    printf("Removing point %d at (%.2f, %.2f)\n", index, 
           sceneSettings.bezierPath.points[index].x, 
           sceneSettings.bezierPath.points[index].y);
        
    if (index == 0) {
        // Shift handles correctly
        for (int i = index; i < sceneSettings.bezierPath.numPoints - 1; i++) {
            sceneSettings.bezierPath.handles[i][0] = sceneSettings.bezierPath.handles[i + 1][0];  // Shift outgoing handle
            sceneSettings.bezierPath.handles[i][1] = sceneSettings.bezierPath.handles[i + 1][1];  // Shift incoming handle
        }
    } else if (index < sceneSettings.bezierPath.numPoints - 1) {
        // Shift handles correctly
        sceneSettings.bezierPath.handles[index - 1][1] = sceneSettings.bezierPath.handles[index][1];  // Shift incom handle
        
        for (int i = index + 1; i < sceneSettings.bezierPath.numPoints - 1; i++) {
            sceneSettings.bezierPath.handles[i][0] = sceneSettings.bezierPath.handles[i + 1][0];  // Shift outgoing handle
            sceneSettings.bezierPath.handles[i][1] = sceneSettings.bezierPath.handles[i + 1][1];  // Shift incoming handle
        }
    }

    // Shift remaining points
    for (int i = index; i < sceneSettings.bezierPath.numPoints; i++) {
        sceneSettings.bezierPath.points[i] = sceneSettings.bezierPath.points[i + 1];
    }

    // Update segment count
    sceneSettings.bezierPath.numPoints--;

    // Update end handle value
    sceneSettings.bezierPath.handles[sceneSettings.bezierPath.numPoints][0] = (Velocity){0, 0};
        
    printf("Updated Bézier path. New total points: %d\n", sceneSettings.bezierPath.numPoints);
}


void AddBezierPoint(int x, int y) {
    if (sceneSettings.bezierPath.numPoints >= MAX_BEZIER_POINTS) {
        printf("Max points reached, cannot add more.\n");
        return;
    }
            
    int index = sceneSettings.bezierPath.numPoints;
    sceneSettings.bezierPath.points[index].x = x;
    sceneSettings.bezierPath.points[index].y = y;
         
    // First point gets no previous handle
    if (index == 0) {
        sceneSettings.bezierPath.handles[0][0].vx = 50;
        sceneSettings.bezierPath.handles[0][0].vy = 0;
    }
    // Second point initializes first segment
    else if (index == 1) {
        sceneSettings.bezierPath.handles[0][1].vx = -50;
        sceneSettings.bezierPath.handles[0][1].vy = 0;
    }   
    // Third point makes the previous endpoint a midpoint 
    else {
        sceneSettings.bezierPath.handles[index - 1][0].vx = 50;
        sceneSettings.bezierPath.handles[index - 1][0].vy = 0;
            
        sceneSettings.bezierPath.handles[index - 1][1].vx = -50;
        sceneSettings.bezierPath.handles[index - 1][1].vy = 0;
    }
     
    sceneSettings.bezierPath.numPoints++;
    printf("Added new point at (%d, %d), Segment Count: %d\n", x, y, sceneSettings.bezierPath.numPoints - 1);
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
                ToggleBezierPathMode();
                break;
        }
    }
}
			

void HandleBezierEditorMouseClick(SDL_Event* event) {
    int mx = event->button.x;
    int my = event->button.y;

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
            ToggleBezierPathMode();
            return;
        }
    }

    // Check for clicks on Bézier points
    for (int i = 0; i < sceneSettings.bezierPath.numPoints; i++) {
        if ((mx - sceneSettings.bezierPath.points[i].x) * (mx - sceneSettings.bezierPath.points[i].x) +
            (my - sceneSettings.bezierPath.points[i].y) * (my - sceneSettings.bezierPath.points[i].y) <=
            POINT_RADIUS * POINT_RADIUS) {
            draggingPoint = i;

            //  If in Delete Mode, remove the point
            if (deleteModeActive) {
                RemoveBezierPoint(i);
            }

            return;
        }
    }

    //  Check for clicks on velocity handles
    for (int i = 0; i < sceneSettings.bezierPath.numPoints - 1; i++) {
        for (int j = 0; j < 2; j++) {
            int vx = (j == 0) ? sceneSettings.bezierPath.points[i].x + sceneSettings.bezierPath.handles[i][0].vx
                              : sceneSettings.bezierPath.points[i + 1].x + sceneSettings.bezierPath.handles[i][1].vx;
            int vy = (j == 0) ? sceneSettings.bezierPath.points[i].y + sceneSettings.bezierPath.handles[i][0].vy
                              : sceneSettings.bezierPath.points[i + 1].y + sceneSettings.bezierPath.handles[i][1].vy;

            if ((mx - vx) * (mx - vx) + (my - vy) * (my - vy) <= POINT_RADIUS * POINT_RADIUS) {
                draggingPoint = i;
                draggingVelocity = j;
                return;
            }
        }
    }

    //  If in Add Mode, add a new point at the clicked position
    if (addModeActive && !clickedButton) {
        AddBezierPoint(mx, my);
    }

}

void HandleBezierEditorEvents(SDL_Event* event, int* draggingPoint, int* draggingVelocity) {
    int mx = event->button.x;
    int my = event->button.y;

    // ✅ Handle mouse movement for dragging points or velocity handles
    if (event->type == SDL_MOUSEMOTION) {
        if (*draggingPoint != -1 && *draggingVelocity == -1) {
            MoveEndPoint(mx, my, *draggingPoint);
        } else if (*draggingPoint != -1 && *draggingVelocity != -1) {
            MoveVelocityHandle(mx, my, *draggingPoint, *draggingVelocity);
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
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);  
    SDL_RenderClear(renderer);  

    RenderStaticScene(renderer);
    RenderBezierEditorUI(renderer);

    if (sceneSettings.bezierPath.numPoints >= 2) {
	RenderBezierPath(renderer, &sceneSettings.bezierPath, true);
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
}
