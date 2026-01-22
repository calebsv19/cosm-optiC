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
#include "render/timer_hud_api.h"
#include "render/timer_hud_adapter.h"
#include "camera/camera.h"
#include "render/render_helper.h"
#include "engine/Render/render_pipeline.h"
#include "import/fluid_import.h"
#include "geo/shape_asset.h"
#include "geo/shape_adapter.h"
#include "render/vk_shared_device.h"
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
#if USE_VULKAN
static VkRenderer renderer_storage;
#endif
SceneObject sceneObjects[10];  // Define object array storage
int objectCount = 0;  // Define object count

bool running;
double accumulator;
double currentTime;
int frameCounter;
int loopCount;
static bool quitRequested = false;

double t_increment;
double t_param = 0.0;  // Parameter (0 to 1) for interpolation along the path.
int direction = 1;      // +1 for forward, -1 for reverse.
static const double kPreviewBg = 60.0;

static const char* s_fluidManifestOverride = NULL;
#include "render/fluid_state.h"

bool AnimationUseFluidScene(void) {
    return animSettings.useFluidScene && animSettings.fluidManifest[0];
}

void AnimationClearFluidGrid(void) {
    g_fluidGrid.valid = false;
    g_fluidGrid.min_x = g_fluidGrid.min_y = 0.0f;
    g_fluidGrid.max_x = g_fluidGrid.max_y = 0.0f;
}

bool AnimationApplyFluidScene(const char *manifest_path) {
    if (!manifest_path || !*manifest_path) return false;
    FluidManifest manifest = {0};
    if (!fluid_manifest_load(manifest_path, &manifest)) {
        printf("[menu] failed to load manifest %s\n", manifest_path);
        return false;
    }

    // Fit grid
    g_fluidGrid.valid = true;
    g_fluidGrid.min_x = manifest.origin_x;
    g_fluidGrid.min_y = manifest.origin_y;
    g_fluidGrid.max_x = manifest.origin_x + manifest.cell_size * (float)manifest.grid_w;
    g_fluidGrid.max_y = manifest.origin_y + manifest.cell_size * (float)manifest.grid_h;

    // Reset scene and place imports
    sceneSettings.objectCount = 0;
    double grid_w_world = g_fluidGrid.max_x - g_fluidGrid.min_x;
    double grid_h_world = g_fluidGrid.max_y - g_fluidGrid.min_y;
    sceneSettings.camera.x = g_fluidGrid.min_x + grid_w_world * 0.5;
    sceneSettings.camera.y = g_fluidGrid.min_y + grid_h_world * 0.5;
    double zoom_x = (grid_w_world > 1e-4) ? ((double)sceneSettings.windowWidth / grid_w_world) : 1.0;
    double zoom_y = (grid_h_world > 1e-4) ? ((double)sceneSettings.windowHeight / grid_h_world) : 1.0;
    sceneSettings.camera.zoom = fmin(zoom_x, zoom_y) * 0.9;

    for (size_t i = 0; i < manifest.import_count; ++i) {
        const FluidImportShape *imp = &manifest.imports[i];
        if (!imp->path) continue;
        double world_x = g_fluidGrid.min_x + (grid_w_world) * imp->pos_x_norm;
        double world_y = g_fluidGrid.min_y + (grid_h_world) * imp->pos_y_norm;
        ShapeAsset asset = {0};
        bool loaded = shape_asset_load_file(imp->path, &asset);
        double angle = imp->rotation_deg * M_PI / 180.0;
        double scale = (imp->scale > 0.0f) ? imp->scale : 1.0;
        if (loaded) {
            int before = sceneSettings.objectCount;
            ShapeToSceneOptions opts = {.scale = scale, .offset_x = world_x, .offset_y = world_y};
            shape_asset_append_to_scene(&asset, &opts);
            int after = sceneSettings.objectCount;
            for (int oi = before; oi < after; ++oi) {
                sceneSettings.sceneObjects[oi].rotation = angle;
                sceneSettings.sceneObjects[oi].dirty = true;
            }
            shape_asset_free(&asset);
        }
    }

    animSettings.useFluidScene = true;
    if (manifest_path != animSettings.fluidManifest) {
        strncpy(animSettings.fluidManifest, manifest_path, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    }
    fluid_manifest_free(&manifest);
    return true;
}

char loopMode[16] = "stop";  // Increased buffer size for safety
int maxLoopCount = 1;  // Default to 1 loop if not set

static void EnsureDirectoryExists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);  // Create directory with full permissions
    }
}
        
void SaveFrame(int frameNumber) {
#if USE_VULKAN
    EnsureDirectoryExists(animSettings.frameDir);

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.bmp", animSettings.frameDir, frameNumber);

    VkResult capture_result = vk_renderer_request_capture((VkRenderer*)renderer, filename);
    if (capture_result != VK_SUCCESS) {
        fprintf(stderr, "SaveFrame failed to request capture: %d\n", capture_result);
    }
    return;
#else
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
#endif
}


int AnimationInit(void) {
    LoadAnimationConfig();
    if (s_fluidManifestOverride && s_fluidManifestOverride[0]) {
        strncpy(animSettings.fluidManifest, s_fluidManifestOverride, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
        animSettings.useFluidScene = true;
    }
    LoadSceneConfig();
    if (animSettings.useFluidScene && animSettings.fluidManifest[0]) {
        AnimationApplyFluidScene(animSettings.fluidManifest);
    } else {
        AnimationClearFluidGrid();
    }
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
                              SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkResult init = vk_renderer_init_with_device(&renderer_storage, shared_device, window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    renderer = (SDL_Renderer*)&renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)renderer, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
#else
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
#endif

    timer_hud_register_backend();
    ts_init();
    setRenderContext(renderer, window, WINDOW_WIDTH, WINDOW_HEIGHT);

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
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
#if USE_VULKAN
    vk_shared_device_shutdown();
#endif
    SDL_Quit();
}

void HandleEvents(bool* running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            *running = false; // return to menu instead of killing app
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            *running = false; // escape exits loop, menu decides next step
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
        Point new_position = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t_param);
        *lightX = new_position.x;
        *lightY = new_position.y;
    }
}

static void UpdateCameraPosition(double t) {
    if (animSettings.interactiveMode) {
        return;
    }
    if (sceneSettings.cameraPath.numPoints < 1) {
        return;
    }
    Point p = (sceneSettings.cameraPath.numPoints >= 2)
                  ? GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t)
                  : sceneSettings.cameraPath.points[0];
    double rot = (sceneSettings.cameraPath.numPoints >= 2)
                     ? GetRotationAlongPathNormalized(&sceneSettings.cameraPath, t)
                     : sceneSettings.cameraPath.rotations[0];
    sceneSettings.camera.x = p.x;
    sceneSettings.camera.y = p.y;
    sceneSettings.camera.rotation = rot;
}

static void DrawPreviewMarker(SDL_Renderer* r, Point world, SDL_Color col, int radius) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    CameraPoint s = CameraWorldToScreen(&sceneSettings.camera, world.x, world.y, sceneSettings.windowWidth, sceneSettings.windowHeight);
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(r, (int)lround(s.x) + dx, (int)lround(s.y) + dy);
            }
        }
    }
}

static void RunPreviewInternal(bool standalone) {
    bool didInit = false;
    if (standalone) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error (preview): %s\n", SDL_GetError());
            return;
        }
        didInit = true;
    }

    WINDOW_WIDTH = sceneSettings.windowWidth;
    WINDOW_HEIGHT = sceneSettings.windowHeight;

    SDL_Window* pWindow = SDL_CreateWindow("Preview",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           WINDOW_WIDTH, WINDOW_HEIGHT,
                                           SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!pWindow) {
        fprintf(stderr, "SDL_CreateWindow Error (preview): %s\n", SDL_GetError());
        if (didInit) SDL_Quit();
        return;
    }

#if USE_VULKAN
    VkRendererConfig preview_cfg;
    vk_renderer_config_set_defaults(&preview_cfg);
    preview_cfg.enable_validation = SDL_FALSE;
    preview_cfg.clear_color[0] = 0.0f;
    preview_cfg.clear_color[1] = 0.0f;
    preview_cfg.clear_color[2] = 0.0f;
    preview_cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(pWindow, &preview_cfg)) {
        fprintf(stderr, "vk_shared_device_init failed (preview).\n");
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed (preview).\n");
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }

    VkRenderer preview_storage;
    VkResult preview_init = vk_renderer_init_with_device(&preview_storage, shared_device, pWindow, &preview_cfg);
    if (preview_init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed (preview): %d\n", preview_init);
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }
    SDL_Renderer* pRenderer = (SDL_Renderer*)&preview_storage;
    vk_renderer_set_logical_size((VkRenderer*)pRenderer, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
#else
    SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!pRenderer) {
        fprintf(stderr, "SDL_CreateRenderer Error (preview): %s\n", SDL_GetError());
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }
#endif

    double duration = (animSettings.previewDuration > 0.1) ? animSettings.previewDuration : 5.0;
    Uint64 prev = SDL_GetPerformanceCounter();
    double elapsed = 0.0;
    bool runningPreview = true;
    Camera savedCam = sceneSettings.camera;

    while (runningPreview && !quitRequested) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                runningPreview = false;
                quitRequested = true;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f) {
                g_fluidOverlayEnabled = !g_fluidOverlayEnabled;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_LEFTBRACKET) {
                if (g_fluidFrameIndex > 0) g_fluidFrameIndex--;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RIGHTBRACKET) {
                g_fluidFrameIndex++;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev) / SDL_GetPerformanceFrequency();
        prev = now;
        elapsed += dt;

        double t = fmod(elapsed, duration) / duration;

        // Update camera + light along paths
        Point lightP = (sceneSettings.bezierPath.numPoints >= 2)
                           ? GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t)
                           : sceneSettings.bezierPath.points[0];
        Point camP = (sceneSettings.cameraPath.numPoints >= 2)
                         ? GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t)
                         : sceneSettings.cameraPath.points[0];
        double camRot = (sceneSettings.cameraPath.numPoints >= 2)
                            ? GetRotationAlongPathNormalized(&sceneSettings.cameraPath, t)
                            : sceneSettings.cameraPath.rotations[0];
        sceneSettings.camera.x = camP.x;
        sceneSettings.camera.y = camP.y;
        sceneSettings.camera.rotation = camRot;

        // Render preview
        setRenderContext(pRenderer, pWindow, sceneSettings.windowWidth, sceneSettings.windowHeight);
        render_set_clear_color(pRenderer, (Uint8)kPreviewBg, (Uint8)kPreviewBg, (Uint8)kPreviewBg + 5, 255);
        if (!render_begin_frame()) {
            if (render_device_lost()) {
                runningPreview = false;
            }
            SDL_Delay(10);
            continue;
        }

        SDL_Color pathColor = {90, 120, 90, 180};
        SDL_Color camPathColor = {60, 140, 220, 220};
        SDL_Color selectColor = {255, 255, 160, 255};
        RenderBezierPathCameraStyled(pRenderer, &sceneSettings.bezierPath, false, &sceneSettings.camera, pathColor, (SDL_Color){0,0,0,0}, -1, selectColor, 3);
        RenderBezierPathCameraStyled(pRenderer, &sceneSettings.cameraPath, false, &sceneSettings.camera, camPathColor, (SDL_Color){0,0,0,0}, -1, selectColor, 4);

        SDL_SetRenderDrawColor(pRenderer, 220, 220, 220, 255);
        RenderSceneObjects(pRenderer, true);

        DrawPreviewMarker(pRenderer, lightP, (SDL_Color){255, 230, 120, 255}, 6);
        DrawPreviewMarker(pRenderer, camP, (SDL_Color){120, 200, 255, 255}, 6);

        render_end_frame();
    }

    sceneSettings.camera = savedCam;
#if USE_VULKAN
    vk_renderer_wait_idle((VkRenderer*)pRenderer);
    vk_renderer_shutdown_surface((VkRenderer*)pRenderer);
#else
    SDL_DestroyRenderer(pRenderer);
#endif
    SDL_DestroyWindow(pWindow);
    if (didInit) SDL_Quit();
}

void RunPreviewMode(void) {
    RunPreviewInternal(true);
}

void RunPreviewModeEmbedded(void) {
    RunPreviewInternal(false);
}


void RenderFrame(double lightX, double lightY, int* frameCounter, bool* running) {
    if (quitRequested) {
        *running = false;
        return;
    }
    ts_frame_start();
    setRenderContext(renderer, window, sceneSettings.windowWidth, sceneSettings.windowHeight);
    render_set_clear_color(renderer, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            *running = false;
        }
        ts_frame_end();
        return;
    }
        
    // Render scene objects
    SetLightPosition(lightX, lightY);
    RenderRayTracingScene(renderer);
        
    // Handle deep render mode frame saving
    if (animSettings.deepRenderMode) {
        ts_start_timer("Frame Save");
        SaveFrame((*frameCounter)++);
        ts_stop_timer("Frame Save");
        if (*frameCounter >= animSettings.frameLimit) {
            printf("Deep render mode complete. Final frame saved.\n");
            SDL_Delay(500);
            *running = false;
        }
    }

    ts_render();
    render_end_frame();
    ts_frame_end();
}

void CheckLoopConditions(bool* running, int loopCount, int frameCounter) {
    if (quitRequested) {
        *running = false;
        return;
    }
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
    
    while (running && !quitRequested) {
        HandleEvents(&running);
        if (!animSettings.interactiveMode || animSettings.deepRenderMode) {
            UpdateSimulation(&accumulator, &currentTime, &loopCount);
        }
        
        double lightX, lightY;
        UpdateLightPosition(&lightX, &lightY);
        UpdateCameraPosition(t_param);
        RenderFrame(lightX, lightY, &frameCounter, &running);
        CheckLoopConditions(&running, loopCount, currentTime);

        SDL_Delay(16);  // Prevent CPU overload, ~60FPS
    }
 
    if (animSettings.deepRenderMode && !quitRequested) {
        printf("Deep render mode complete. Press close on the window to exit.\n");
        bool waitingForExit = true;
        SDL_Event event; 
        while (waitingForExit && !quitRequested) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                waitingForExit = false; // return to menu
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f) {
                g_fluidOverlayEnabled = !g_fluidOverlayEnabled;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFTBRACKET) {
                if (g_fluidFrameIndex > 0) g_fluidFrameIndex--;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                g_fluidFrameIndex++;
            }
        }
        SDL_Delay(10);
    }
    }
    CleanupRayTracing();    
#if USE_VULKAN
    vk_renderer_wait_idle((VkRenderer*)renderer);
    vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
    SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
}


#ifdef MAIN_DRIVER  
static void ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--fluid-manifest") == 0 && i + 1 < argc) {
            s_fluidManifestOverride = argv[++i];
        } else if (strcmp(arg, "--fluid-frame") == 0 && i + 1 < argc) {
            g_fluidFrameIndex = atoi(argv[++i]);
        }
    }
}

int main(int argc, char* argv[]) {
    ParseArgs(argc, argv);
    (void)argc;
    (void)argv;
    // Load animation settings from config file
    LoadAllSettings();
    t_increment = 1.0 / animSettings.framesForTravel;
    printf("Loaded animation config in main.\n");

    // Menu → run loop, allowing return to menu after each run
    while (!quitRequested) {
        if (!RunMenu()) {
            printf("Menu closed. Exiting program.\n");
            return 0;
        }
        if (animSettings.previewMode) {
            SaveAllSettings();
            RunPreviewMode();
            animSettings.previewMode = false; // do not persist
            SaveAllSettings();
            continue; // back to menu for next choice
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

        t_increment = 1.0 / animSettings.framesForTravel;

        printf("Starting animation loop...\n");
        RunMainLoop();

        if (animSettings.autoMP4 && animSettings.deepRenderMode) {
            printf("Generating MP4 automatically...\n");
            MakeVideo("output.mp4");
        }

        AnimationCleanup();
        SaveAllSettings();

        // If run ended without a quit request, drop back to the menu
        if (quitRequested) {
            break;
        }
    }

    return 0;
}
#endif
