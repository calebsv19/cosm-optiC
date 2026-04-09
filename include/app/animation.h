#ifndef ANIMATION_H
#define ANIMATION_H

#include <SDL2/SDL.h>
#include <stdbool.h>  // Fix: Include for `bool` type
#include "path/path_system.h"

// Global animation settings (Loaded from config)
extern int WINDOW_WIDTH;
extern int WINDOW_HEIGHT;

extern int maxLoopCount;
extern double TOTAL_TIME;  // Fix for missing TOTAL_TIME
extern int frameCounter;
extern int loopCount;
extern double currentTime;

// Simulation parameters
extern double light_start_x, light_start_y;
extern double light_end_x, light_end_y;

// SDL globals
extern SDL_Window* window;  
extern SDL_Renderer* renderer;  
extern Path* light_path;  // Fix for undeclared `light_path`


// Bézier path storage
extern Path bezierPath;  // Fix: Add missing Bézier path variable

// Initializes SDL and necessary resources.
int AnimationInit(void);

// Runs a lightweight preview (no ray tracing).
void RunPreviewMode(void);
// Runs preview from within editors (assumes SDL already initialized).
void RunPreviewModeEmbedded(void);

// Runs the main loop.
void RunMainLoop(void);

// Cleans up SDL and other resources.
void AnimationCleanup(void);

// Loads scene settings from config file.
void LoadSceneConfig(void);

// Saves animation settings to config file.
void SaveSceneConfig(void);

// Saves a single frame of animation to disk.
void SaveFrame(int frameNumber);

// Indicates whether a fluid scene should override the default scene.
bool AnimationUseFluidScene(void);

// Apply a fluid manifest into the current scene (camera/object placement).
// Returns true on success and updates animSettings.useFluidScene/fluidManifest.
bool AnimationApplyFluidScene(const char *manifest_path);

// Clears the cached grid bounds when fluid scene is disabled.
void AnimationClearFluidGrid(void);

#endif // ANIMATION_H
