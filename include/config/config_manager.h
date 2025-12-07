#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>
#include <json-c/json.h>
#include "path/path_system.h"  // Ensure Bézier path handling is included
#include "camera/camera.h"
#include "scene/object_manager.h"
#include "material/material.h"

#define MAX_SHAPE_POINTS 10  // Supports up to 10 points for custom shapes

typedef enum {
    FORWARD_FALLOFF_MODE_QUADRATIC = 0,
    FORWARD_FALLOFF_MODE_LINEAR = 1,
    FORWARD_FALLOFF_MODE_NONE = 2
} ForwardFalloffMode;

typedef enum {
    RENDER_QUALITY_LOW = 0,
    RENDER_QUALITY_MEDIUM = 1,
    RENDER_QUALITY_HIGH = 2
} RenderQuality;

// **Animation Config Struct**
typedef struct {
    bool interactiveMode;
    bool deepRenderMode;
    bool bounceMode;
    bool autoMP4;
    int bounceLimit;
    int frameLimit;
    int framesForTravel;
    int maxLoopCount;
    int fps;
    double frameDuration;
    char frameDir[256];
    char loopMode[64];
    int lightMode;
    int blurMode;
    bool lightDiffusionEnabled;
    int lightDiffusionRadius;
    double lightDiffusionStrength;
    bool useTiledRenderer;
    int tileSize;
    double rouletteThreshold;
    int integratorMode;
    bool previewMode;
    double previewDuration;
    int pathSamplesPerPixel;
    int pathMaxDepth;
    bool pathDirectLighting;
    bool pathRussianRoulette;
    bool pathEnableMIS;
    double environmentBrightness;
    int pathSeed;
    int editorMode;
    double cacheContributionWeight;
    int bsdfModel;
    double lightIntensity;
    double forwardDecay;    // Forward falloff distance (world units)
    ForwardFalloffMode forwardFalloffMode;
    RenderQuality renderQuality;
    double cacheVarianceCutoff;   // Variance rejection threshold for irradiance cache bins
    double cacheHaloRadius;       // Multiplier for light radius to suppress GI near the emitter
    double lightDecaySoftness;    // >1.0 flattens decay, <1.0 steepens decay
    // Integrator mode: 0 = forward, 1 = hybrid (legacy camera path), 2 = disney path tracer, else direct
    int cameraIntegratorMode;
} AnimationConfig;


// **Scene Config Struct**
typedef struct {
    int windowWidth;
    int windowHeight;
    SceneObject sceneObjects[10];  // Stores up to 10 objects
    int objectCount;
    Path bezierPath;   // Light path
    Path cameraPath;   // Camera animation path
    int rays;
    Camera camera;
    double cameraMargin;
} SceneConfig;

// Global instances
extern AnimationConfig animSettings;
extern SceneConfig sceneSettings;

// Function prototypes
void SaveAllSettings(void);
void LoadAllSettings(void);
void SaveAnimationConfig(void);
void LoadAnimationConfig(void);
void SaveSceneConfig(void);
void LoadSceneConfig(void);


void LoadObjectProperties(struct json_object* obj, SceneObject* sceneObject);

#endif // CONFIG_MANAGER_H
