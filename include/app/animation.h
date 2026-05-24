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
void AnimationParseArgs(int argc, char* argv[]);
void AnimationLoadRuntimeDefaults(void);
int AnimationRunAppSession(void);

// Runs a lightweight preview (no ray tracing).
void RunPreviewMode(void);
// Runs preview from within editors (assumes SDL already initialized).
void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer);

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

// Returns the current normalized playback position used by the live runtime loop.
double AnimationCurrentNormalizedT(void);

// Indicates whether a fluid scene should override the default scene.
bool AnimationUseFluidScene(void);

// Apply a fluid manifest into the current scene (camera/object placement).
// Returns true on success and updates animSettings.useFluidScene/fluidManifest.
bool AnimationApplyFluidScene(const char *manifest_path);

// Apply whichever scene source is currently active in animSettings.sceneSource.
// Returns true when the selected source is in a valid applied state.
bool AnimationApplyActiveSceneSource(void);

// Transactionally set source lane/path and optionally apply immediately.
// On apply failure, previous source lane/path state is restored.
bool AnimationSelectSceneSource(int source, const char *path, bool apply_immediately);

// Transactionally set the native 3D volume source lane and optionally validate/apply it.
// On apply failure, previous volume-source state is restored.
bool AnimationSelectVolumeSource(int kind, const char *path, bool apply_immediately);

// Clears the native 3D volume source lane and disables atmosphere interaction.
void AnimationClearVolumeSource(void);

// Apply persisted active source at startup/editor session boundaries.
// When persist_on_failure is true, fallback corrections are saved for deterministic reopen.
bool AnimationRestoreActiveSceneSource(bool persist_on_failure);

// Clears the cached grid bounds when fluid scene is disabled.
void AnimationClearFluidGrid(void);

#endif // ANIMATION_H
