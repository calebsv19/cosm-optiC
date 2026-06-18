#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>
#include <limits.h>
#include <json-c/json.h>
#include "path/path_system.h"  // Ensure Bézier path handling is included
#include "camera/camera.h"
#include "camera/camera_path_3d.h"
#include "scene/object_manager.h"
#include "material/material.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_SHAPE_POINTS 256  // Supports up to 256 points for custom shapes

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

typedef enum {
    SPACE_MODE_2D = 0,
    SPACE_MODE_3D = 1
} SpaceMode;

typedef enum {
    ENVIRONMENT_LIGHT_MODE_OFF = 0,
    ENVIRONMENT_LIGHT_MODE_TOP_FILL = 1,
    ENVIRONMENT_LIGHT_MODE_AMBIENT = 2
} EnvironmentLightMode;

typedef enum {
    ENVIRONMENT_PRESET_NEUTRAL = 0,
    ENVIRONMENT_PRESET_SKY = 1,
    ENVIRONMENT_PRESET_WARM_SKY = 2
} EnvironmentPreset;

typedef enum {
    SCENE_SOURCE_CONFIG_2D = 0,
    SCENE_SOURCE_FLUID_MANIFEST = 1,
    SCENE_SOURCE_RUNTIME_SCENE = 2
} SceneSource;

typedef enum {
    VOLUME_SOURCE_NONE = 0,
    VOLUME_SOURCE_MANIFEST = 1,
    VOLUME_SOURCE_RAW_VF3D = 2,
    VOLUME_SOURCE_PACK = 3
} VolumeSourceKind;

#define RUNTIME_3D_SECONDARY_SAMPLES_MIN 4
#define RUNTIME_3D_SECONDARY_SAMPLES_MAX 64
#define RUNTIME_3D_SECONDARY_SAMPLES_STEP 4
#define RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT 48

#define RUNTIME_3D_BOUNCE_DEPTH_MIN 1
#define RUNTIME_3D_BOUNCE_DEPTH_MAX 8
#define RUNTIME_3D_BOUNCE_DEPTH_DEFAULT 3

#define RUNTIME_3D_SPECULAR_DEPTH_MIN 1
#define RUNTIME_3D_SPECULAR_DEPTH_MAX 8
#define RUNTIME_3D_SPECULAR_DEPTH_DEFAULT 3

#define RUNTIME_3D_TRANSMISSION_DEPTH_MIN 1
#define RUNTIME_3D_TRANSMISSION_DEPTH_MAX 16
#define RUNTIME_3D_TRANSMISSION_DEPTH_DEFAULT 8

#define RUNTIME_3D_ROULETTE_THRESHOLD_MIN 0.0
#define RUNTIME_3D_ROULETTE_THRESHOLD_MAX 0.1
#define RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT 0.01

#define RUNTIME_3D_TRANSMISSION_SAMPLES_MIN 4
#define RUNTIME_3D_TRANSMISSION_SAMPLES_MAX 32
#define RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT 16

#define RUNTIME_3D_TEMPORAL_FRAMES_MIN 1
#define RUNTIME_3D_TEMPORAL_FRAMES_MAX 32
#define RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT 12

#define RUNTIME_3D_RENDER_SCALE_HIDPI 0
#define RUNTIME_3D_RENDER_SCALE_MIN 0
#define RUNTIME_3D_RENDER_SCALE_MAX 8
#define RUNTIME_3D_RENDER_SCALE_DEFAULT 1

typedef enum {
    RUNTIME_3D_UPSCALE_MODE_OFF = 0,
    RUNTIME_3D_UPSCALE_MODE_NEAREST = 1,
    RUNTIME_3D_UPSCALE_MODE_BILINEAR = 2
} Runtime3DUpscaleMode;

#define RUNTIME_3D_UPSCALE_MODE_MIN RUNTIME_3D_UPSCALE_MODE_OFF
#define RUNTIME_3D_UPSCALE_MODE_MAX RUNTIME_3D_UPSCALE_MODE_BILINEAR
#define RUNTIME_3D_UPSCALE_MODE_DEFAULT RUNTIME_3D_UPSCALE_MODE_OFF

#define RAY_TRACING_DEFAULT_LIGHT_INTENSITY 0.4

// **Animation Config Struct**
typedef struct {
    bool interactiveMode;
    bool deepRenderMode;
    bool bounceMode;
    bool autoMP4;
    int bounceLimit;
    int frameLimit;
    int framesForTravel;
    int startFrameIndex;
    bool resumeFromExistingFrames;
    int maxLoopCount;
    int fps;
    double frameDuration;
    char inputRoot[256];
    char meshAssetRoot[PATH_MAX];
    char outputRoot[256];
    char frameDir[256];
    char videoOutputRoot[256];
    char loopMode[64];
    int lightMode;
    int blurMode;
    bool lightDiffusionEnabled;
    int lightDiffusionRadius;
    double lightDiffusionStrength;
    bool useTiledRenderer;
    bool tilePreviewEnabled;
    int tileSize;
    double rouletteThreshold;
    int integratorMode;
    int integratorMode3D;
    bool previewMode;
    double previewDuration;
    int pathSamplesPerPixel;
    int pathMaxDepth;
    bool pathDirectLighting;
    bool pathRussianRoulette;
    bool pathEnableMIS;
    double environmentBrightness;
    int environmentPreset;
    bool environmentBackgroundLightingAuthored;
    bool environmentBackgroundBrightnessAuto;
    double environmentBackgroundBrightness;
    double environmentBackgroundColorR;
    double environmentBackgroundColorG;
    double environmentBackgroundColorB;
    int pathSeed;
    int editorMode;
    SpaceMode spaceMode;
    int textZoomStep;
    double cacheContributionWeight;
    int bsdfModel;
    double lightIntensity;
    double forwardDecay;    // Forward falloff distance (world units)
    ForwardFalloffMode forwardFalloffMode;
    RenderQuality renderQuality;
    double cacheVarianceCutoff;   // Variance rejection threshold for irradiance cache bins
    double cacheHaloRadius;       // Multiplier for light radius to suppress GI near the emitter
    double lightDecaySoftness;    // >1.0 flattens decay, <1.0 steepens decay
    double lightRadius;           // Authored light source radius in world units (0 = auto)
    double lightHeight;           // Z-height of the light above the ground plane for Disney path/2.5D shading
    int environmentLightMode;     // Extra fill-light lane: off, top-fill, or ambient
    double topFillStrength;       // Standalone top-fill strength for the environment-light top-fill mode
    bool disneyDenoiseEnabled;    // Disney-only native 3D denoise post-pass toggle
    int bounceDepth3D;
    int specularDepth3D;
    int transmissionDepth3D;
    double rouletteThreshold3D;
    int secondaryDiffuseSamples3D;
    int transmissionSamples3D;
    int temporalFrames3D;
    int renderScale3D;
    int upscaleMode3D;
    int runtimeWindowWidth;
    int runtimeWindowHeight;
    // Integrator mode: 0 = forward, 1 = hybrid (camera-path GI), 2 = direct light (Disney path paused).
    int cameraIntegratorMode;

    // Legacy scene source lane. This still chooses between the historical
    // 2D config, planar fluid manifest, or runtime geometry scene.
    SceneSource sceneSource;
    bool useFluidScene;
    char fluidManifest[PATH_MAX];
    char runtimeScenePath[PATH_MAX];

    // Native 3D participating-media lane. This remains separate from the
    // legacy sceneSource contract so runtime geometry and optional VF3D
    // atmosphere can coexist.
    bool volumeInteractionEnabled;
    int volumeSourceKind;
    char volumeSourcePath[PATH_MAX];
    bool volumeAffectsLighting;
    bool volumeDebugOverlayEnabled;
} AnimationConfig;


// **Scene Config Struct**
typedef struct {
    int windowWidth;
    int windowHeight;
    SceneObject sceneObjects[MAX_OBJECTS];  // Stores up to MAX_OBJECTS objects
    int objectCount;
    Path bezierPath;   // Light path
    CameraPath3D bezierPath3D; // Light path depth authoring for controlled-3D lane
    Path cameraPath;   // Camera animation path
    CameraPath3D cameraPath3D; // Camera depth authoring path for controlled-3D lane
    int rays;
    Camera camera;
    double cameraZ;
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
void ApplyAnimationWindowSizeOverride(void);


void LoadObjectProperties(struct json_object* obj, SceneObject* sceneObject);
void LoadSceneObjects(struct json_object* config);

int animation_config_text_zoom_step_clamp(int step);
int animation_config_text_zoom_percent_from_step(int step);
int animation_config_scale_text_point_size(const AnimationConfig* cfg,
                                           int base_point_size,
                                           int min_point_size);
int animation_config_environment_light_mode_clamp(int mode);
int animation_config_environment_preset_clamp(int preset);
int animation_config_space_mode_clamp(int mode);
int animation_config_scene_source_clamp(int source);
int animation_config_volume_source_kind_clamp(int kind);
bool animation_config_scene_source_is_fluid(int source);

#endif // CONFIG_MANAGER_H
