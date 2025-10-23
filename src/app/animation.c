#ifndef MAIN_DRIVER
#define MAIN_DRIVER
#endif

#include "ui/sdl_menu.h"
#include "tools/make_video.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/ray_tracing2.h"
#include "editor/bezier_editor.h"
#include "path/path_system.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // For mkdir()
#include <sys/types.h>

#define SCENE_CONFIG_FILE "Configs/scene_config.json"
#define ANIMATION_CONFIG_FILE "Configs/animation_config.json"

// Global animation settings (Loaded from config)
int WINDOW_WIDTH;
int WINDOW_HEIGHT;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SceneObject sceneObjects[10];  // Define object array storage
int objectCount = 0;  // Define object count

bool running;
double accumulator;
double currentTime;
int frameCounter;
int loopCount;

double t_increment;
double t_param = 0.0;  // Parameter (0 to 1) for interpolation along the path.
int direction = 1;      // +1 for forward, -1 for reverse.

char loopMode[16] = "stop";  // Increased buffer size for safety
int maxLoopCount = 1;  // Default to 1 loop if not set

static void EnsureDirectoryExists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);  // Create directory with full permissions
    }
}       
        
void SaveFrame(int frameNumber) {
    // Ensure the frame directory exists
    EnsureDirectoryExists(animSettings.frameDir);
    
    // Generate filename for output frame
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.bmp", animSettings.frameDir, frameNumber);
                              
    printf("Saving frame to: %s\n", filename);
        
    // Retrieve window dimensions from scene settings
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;

    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                                0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) { 
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        return;
    }
        
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
        
    if (SDL_SaveBMP(surface, filename) != 0) {
        fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
    } else {
        printf("Saved %s\n", filename);
    }

    SDL_FreeSurface(surface);
}


int AnimationInit(void) {
    LoadAnimationConfig();
    LoadSceneConfig();
    UpdateObjects();
    WINDOW_WIDTH = sceneSettings.windowWidth;       
    WINDOW_HEIGHT = sceneSettings.windowHeight; 
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create window
    window = SDL_CreateWindow("Raytracing Animation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Validate Bézier path
    if (sceneSettings.bezierPath.numPoints < 2) {
        fprintf(stderr, "Error: Bézier path is uninitialized or invalid. Check scene_config.json.\n");
        AnimationCleanup();
        return -1;
    }
    // Initialize ray tracing scene
    InitRayTracingScene();
    printf("Completed initialization in animation.c\n");

    return 0;
}



void AnimationCleanup(void) {   
    if (renderer) {     
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

void HandleEvents(bool* running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            *running = false;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            *running = false;
        } else if (animSettings.interactiveMode && (event.type == SDL_MOUSEMOTION ||
                                event.type == SDL_MOUSEBUTTONDOWN)) {
            ProcessRayTracingEvent(&event);
        }
    }
}

void UpdateSimulation(double* accumulator, double* currentTime, int* loopCount) {
    Uint64 currentCounter = SDL_GetPerformanceCounter();
    static Uint64 previousCounter = 0;
    if (previousCounter == 0) {
        previousCounter = currentCounter;
    }
    double deltaTime = (double)(currentCounter - previousCounter) / SDL_GetPerformanceFrequency();
    previousCounter = currentCounter;
    *accumulator += deltaTime;

    // Only process if enough time has passed for one frame
    if (*accumulator < animSettings.frameDuration) {
        return;  // Not enough time passed, exit early
    }

    // Move forward on the Bézier path
    t_param += t_increment * direction;

    // Bounce mode logic
    if (animSettings.bounceMode) {
        if (t_param >= 1.0) {
            t_param = 1.0;
            direction = -1;
            (*loopCount)++;
        } else if (t_param <= 0.0) {
            t_param = 0.0;
            direction = 1;
            (*loopCount)++;   
        }
    } else { // Normal path following
        if (t_param > 1.0) {
            if (strcmp(animSettings.loopMode, "loop") == 0) {
                t_param = 0.0;
                (*loopCount)++;
            } else {
                t_param = 1.0;
            }
        }
    }

    printf("DEBUG: t_param = %.3f\n", t_param);

    *currentTime += animSettings.frameDuration;
    *accumulator -= animSettings.frameDuration;  // Reset accumulator after processing one frame
}


void UpdateLightPosition(double* lightX, double* lightY) {
    if (animSettings.interactiveMode) {
        GetCurrentLightPosition(lightX, lightY);
    } else {
        Point new_position = GetPositionAlongPath(&sceneSettings.bezierPath, t_param);
        *lightX = new_position.x;
        *lightY = new_position.y;
    }
}


void RenderFrame(double lightX, double lightY, int* frameCounter, bool* running) {
    // Clear the screen before drawing new frame
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
        
    // Render scene objects
    SetLightPosition(lightX, lightY);
    RenderRayTracingScene(renderer);
        
    // Handle deep render mode frame saving
    if (animSettings.deepRenderMode) {
        SaveFrame((*frameCounter)++);
        if (*frameCounter >= animSettings.frameLimit) {
            printf("Deep render mode complete. Final frame saved.\n");
            SDL_Delay(500);
            *running = false;
        }
    }
}

void CheckLoopConditions(bool* running, int loopCount, int frameCounter) {
    if (loopCount >= animSettings.maxLoopCount && strcmp(animSettings.loopMode, "loop") == 0) {
        *running = false;
    }
    
    // Stop when the animation reaches the last frame
    if (!animSettings.interactiveMode && !animSettings.deepRenderMode && t_param >= 1.0) {
        *running = false;
    }
    
    // Stop deep render mode when we reach the frame limit
    if (animSettings.deepRenderMode && frameCounter >= animSettings.frameLimit) {
        *running = false;
        printf("Deep render mode reached frame limit: %d/%d\n", frameCounter, animSettings.frameLimit);
    }
}

void RunMainLoop(void) {
    running = true;
    accumulator = 0.0;
    currentTime = 0.0;
    frameCounter = 0;
    loopCount = 0;
    t_param = 1.0 / animSettings.framesForTravel;
    
    printf("DEBUG: RunMainLoop started with interactiveMode=%d, deepRenderMode=%d\n",
           animSettings.interactiveMode, animSettings.deepRenderMode);
    
    while (running) {
        HandleEvents(&running);
        if (!animSettings.interactiveMode || animSettings.deepRenderMode) {
            UpdateSimulation(&accumulator, &currentTime, &loopCount);
        }
        
        double lightX, lightY;
        UpdateLightPosition(&lightX, &lightY);
        RenderFrame(lightX, lightY, &frameCounter, &running);
        CheckLoopConditions(&running, loopCount, currentTime);

        SDL_Delay(16);  // Prevent CPU overload, ~60FPS
    }
 
    if (animSettings.deepRenderMode) {
        printf("Deep render mode complete. Press close on the window to exit.\n");
        bool waitingForExit = true;
        SDL_Event event; 
        while (waitingForExit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT)
                    waitingForExit = false;
            }
            SDL_Delay(10);
        }
    }
    CleanupRayTracing();    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}


#ifdef MAIN_DRIVER  
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // Load animation settings from config file
    LoadAllSettings();
    t_increment = 1.0 / animSettings.framesForTravel;
    printf("Loaded animation config in main.\n");

    // Run the SDL menu to allow user selection
    if (!RunMenu()) {
        printf("Menu closed. Exiting program.\n");
        return 0;
    }

    // Print selected settings
    printf("Selected Mode: %s\n",
           animSettings.interactiveMode ? "Interactive" :
           animSettings.deepRenderMode ? "Deep Render" :
           animSettings.bounceMode ? "Bounce Animation" : "Standard Animation");
    printf("Auto MP4 after render: %s\n", animSettings.autoMP4 ? "Enabled" : "Disabled");
    printf("Saving frames in directory: %s\n", animSettings.frameDir);

    // Initialize animation
    if (AnimationInit() != 0) {
        printf("Error: Animation initialization failed. Exiting program.\n");
        return -1;
    }

    printf("Starting animation loop...\n");
    // Run animation based on settings from the menu
    RunMainLoop();

    // Automatically create MP4 if deep render mode was enabled and autoMP4 is ON
    if (animSettings.autoMP4 && animSettings.deepRenderMode) {
        printf("Generating MP4 automatically...\n");
        MakeVideo("output.mp4");
    }

    // Cleanup animation and save settings
    AnimationCleanup();
    SaveAllSettings();

    return 0;
}
#endif
