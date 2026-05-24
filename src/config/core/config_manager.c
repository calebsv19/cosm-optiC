#include "config/config_manager.h"
#include "config/config_file_io.h"
#include "config/config_scene_path_io.h"
#include "config/core/config_runtime_paths.h"
#include "app/data_paths.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "scene/object_manager.h"
#include "material/material_manager.h"
#include <stdio.h>       // For file handling (fopen, fprintf, fclose, perror)
#include <stdlib.h>      // For memory allocation (malloc, free)
#include <string.h>      // For string functions (strncpy)
#include <json-c/json.h> // For JSON handling
#include <math.h>
#include <limits.h>

#define SCENE_CONFIG_DEFAULT_FILE "config/scene_config.json"
#define SCENE_CONFIG_RUNTIME_FILE "data/runtime/scene_config.json"
#define SCENE_CONFIG_LEGACY_FILE "Configs/scene_config.json"
#define MATERIALS_DEFAULT_DIR "config/materials"
#define MATERIALS_LEGACY_DIR "Configs/materials"
#define FRAME_DIR_DEFAULT "data/runtime/frames/default"

typedef enum {
    LONG_RAYS = 0,         // Original ray casting mode
    REALISTIC_LIGHT = 1    // Inverse-square law lighting
} LightMode;

// **Global config instances**
AnimationConfig animSettings = {
    .interactiveMode = true,
    .deepRenderMode = false,
    .bounceMode = false,
    .autoMP4 = false,
    .bounceLimit = 10,
    .frameLimit = 50,
    .framesForTravel = 40,
    .startFrameIndex = 0,
    .resumeFromExistingFrames = false,
    .maxLoopCount = 1,
    .fps = 30,
    .frameDuration = 1.0 / 30.0,
    .inputRoot = "config",
    .outputRoot = "data/runtime",
    .frameDir = FRAME_DIR_DEFAULT,
    .videoOutputRoot = "data/runtime/videos",
    .loopMode = "Normal",
    .lightMode = 0,
    .blurMode = 0,
    .lightDiffusionEnabled = true,
    .lightDiffusionRadius = 4,
    .lightDiffusionStrength = 0.65,
    .useTiledRenderer = false,
    .tilePreviewEnabled = false,
    .tileSize = 16,
    .rouletteThreshold = 0.01,
    .integratorMode = 0,
    .integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
    .pathSamplesPerPixel = 4,
    .pathMaxDepth = 4,
    .pathDirectLighting = true,
    .pathRussianRoulette = true,
    .pathEnableMIS = true,
    .environmentBrightness = 0.0,
    .pathSeed = 1,
    .editorMode = 0,
    .spaceMode = SPACE_MODE_2D,
    .textZoomStep = 0,
    .cacheContributionWeight = 1.0,
    .bsdfModel = 1,
    .lightIntensity = 5.0,
    .forwardDecay = 0.0,
    .forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC,
    .renderQuality = RENDER_QUALITY_MEDIUM,
    .cacheVarianceCutoff = 0.35,
    .cacheHaloRadius = 3.5,
    .lightDecaySoftness = 1.0,
    .lightRadius = 0.0,
    .lightHeight = 8.0,
    .topFillLightEnabled = false,
    .disneyDenoiseEnabled = true,
    .bounceDepth3D = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT,
    .specularDepth3D = RUNTIME_3D_SPECULAR_DEPTH_DEFAULT,
    .transmissionDepth3D = RUNTIME_3D_TRANSMISSION_DEPTH_DEFAULT,
    .rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT,
    .secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT,
    .transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT,
    .temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT,
    .renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT,
    .upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_DEFAULT,
    .runtimeWindowWidth = 0,
    .runtimeWindowHeight = 0,
    .sceneSource = SCENE_SOURCE_CONFIG_2D,
    .useFluidScene = false,
    .fluidManifest = "",
    .runtimeScenePath = "",
    .volumeInteractionEnabled = false,
    .volumeSourceKind = VOLUME_SOURCE_NONE,
    .volumeSourcePath = "",
    .volumeAffectsLighting = true,
    .volumeDebugOverlayEnabled = false
};

SceneConfig sceneSettings = {
    .windowWidth = 1200,
    .windowHeight = 800,
    .objectCount = 0,  // No objects initially
    .bezierPath = { .numPoints = 0, .mode = BEZIER_CUBIC },
    .bezierPath3D = {0},
    .cameraPath = { .numPoints = 0, .mode = BEZIER_CUBIC },
    .rays = 2000,
    .camera = { .x = 0.0, .y = 0.0, .zoom = 1.0, .rotation = 0.0 },
    .cameraZ = 0.0,
    .cameraMargin = 80.0
};

static bool ConfigJsonGetDouble(struct json_object* obj, const char* key, double* out_value);
static bool ConfigJsonGetInt(struct json_object* obj, const char* key, int* out_value);

static void SyncSceneSourceLegacyFieldsForSave(void) {
    animSettings.sceneSource = animation_config_scene_source_clamp(animSettings.sceneSource);
    if (animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST &&
        animSettings.fluidManifest[0] == '\0') {
        animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    }
    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0] == '\0') {
        animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    }
    animSettings.useFluidScene = animation_config_scene_source_is_fluid(animSettings.sceneSource);
}

void SaveAllSettings(void) {
    MaterialManagerInit();
    SyncSceneSourceLegacyFieldsForSave();
    if (animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D) {
        SaveSceneConfig();
    }
    SaveAnimationConfig();
}

void LoadAllSettings(void) {
    MaterialManagerInit();
    if (config_io_directory_exists(MATERIALS_DEFAULT_DIR)) {
        MaterialManagerLoadDir(MATERIALS_DEFAULT_DIR);
    } else {
        MaterialManagerLoadDir(MATERIALS_LEGACY_DIR);
    }
    LoadSceneConfig();
    LoadAnimationConfig();
    ApplyAnimationWindowSizeOverride();
}

static void ConfigSaveTextureParams(struct json_object* target,
                                    RuntimeMaterialTexture3DParams params) {
    if (!target) return;
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    json_object_object_add(target, "patternMode", json_object_new_int(params.patternMode));
    json_object_object_add(target, "coverage", json_object_new_double(params.coverage));
    json_object_object_add(target, "grain", json_object_new_double(params.grain));
    json_object_object_add(target, "edgeSoftness", json_object_new_double(params.edgeSoftness));
    json_object_object_add(target, "contrast", json_object_new_double(params.contrast));
    json_object_object_add(target, "flow", json_object_new_double(params.flow));
    json_object_object_add(target, "colorDepth", json_object_new_double(params.colorDepth));
    json_object_object_add(target, "surfaceDamage", json_object_new_double(params.surfaceDamage));
    json_object_object_add(target, "seed", json_object_new_int(params.seed));
}

static struct json_object* ConfigSaveTexturePlacement(
    const RuntimeMaterialTexture3DPlacement* placement) {
    struct json_object* placement_obj = json_object_new_object();
    if (!placement_obj || !placement) return placement_obj;
    json_object_object_add(placement_obj, "offsetU", json_object_new_double(placement->offsetU));
    json_object_object_add(placement_obj, "offsetV", json_object_new_double(placement->offsetV));
    json_object_object_add(placement_obj, "scale", json_object_new_double(placement->scale));
    json_object_object_add(placement_obj, "strength", json_object_new_double(placement->strength));
    json_object_object_add(placement_obj, "rotation", json_object_new_double(placement->rotation));
    return placement_obj;
}

static struct json_object* ConfigSaveMaterialTextureStackForObject(int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    bool explicit_stack = SceneEditorMaterialStackGetObjectStack(scene_object_index, &stack);
    bool legacy_texture = false;
    struct json_object* stack_obj = NULL;
    struct json_object* layers = NULL;
    const SceneObject* obj = NULL;

    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) return NULL;
    obj = &sceneSettings.sceneObjects[scene_object_index];
    legacy_texture =
        obj->textureId != 0 ||
        obj->textureOffsetU != 0.0 ||
        obj->textureOffsetV != 0.0 ||
        obj->textureScale != 1.0 ||
        obj->textureStrength != 0.0 ||
        obj->texturePatternMode != 0 ||
        obj->textureCoverage != 0.5 ||
        obj->textureGrain != 0.5 ||
        obj->textureEdgeSoftness != 0.5 ||
        obj->textureContrast != 0.5 ||
        obj->textureFlow != 0.0 ||
        obj->textureColorDepth != 0.5 ||
        obj->textureSurfaceDamage != 0.5 ||
        obj->textureSeed != 0;
    if (!explicit_stack && !legacy_texture) return NULL;
    if (!explicit_stack &&
        !RuntimeMaterialTextureStackBuildLegacyFromObject(obj, &stack)) {
        return NULL;
    }
    stack = RuntimeMaterialTextureStackNormalize(stack);

    stack_obj = json_object_new_object();
    layers = json_object_new_array();
    if (!stack_obj || !layers) {
        if (stack_obj) json_object_put(stack_obj);
        if (layers) json_object_put(layers);
        return NULL;
    }
    json_object_object_add(stack_obj, "version", json_object_new_int(1));
    for (int i = 0; i < stack.layerCount && i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
        RuntimeMaterialTextureLayer layer = RuntimeMaterialTextureLayerNormalize(stack.layers[i]);
        struct json_object* layer_obj = NULL;
        struct json_object* params = NULL;
        if (!layer.enabled || layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;
        layer_obj = json_object_new_object();
        params = json_object_new_object();
        if (!layer_obj || !params) {
            if (layer_obj) json_object_put(layer_obj);
            if (params) json_object_put(params);
            json_object_put(layers);
            json_object_put(stack_obj);
            return NULL;
        }
        json_object_object_add(layer_obj, "id", json_object_new_string(layer.layerId));
        json_object_object_add(layer_obj, "name", json_object_new_string(layer.displayName));
        json_object_object_add(layer_obj,
                               "role",
                               json_object_new_string(layer.role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE
                                                          ? "base"
                                                          : "overlay"));
        json_object_object_add(layer_obj,
                               "kind",
                               json_object_new_string(RuntimeMaterialTextureLayerKindStableId(layer.kind)));
        json_object_object_add(layer_obj,
                               "blend",
                               json_object_new_string(
                                   RuntimeMaterialTextureLayerBlendModeStableId(layer.blendMode)));
        json_object_object_add(layer_obj, "enabled", json_object_new_boolean(layer.enabled));
        json_object_object_add(layer_obj, "opacity", json_object_new_double(layer.opacity));
        json_object_object_add(layer_obj,
                               "placement",
                               ConfigSaveTexturePlacement(&layer.placement));
        ConfigSaveTextureParams(params, layer.params);
        json_object_object_add(layer_obj, "parameters", params);
        json_object_object_add(layer_obj,
                               "roughnessInfluence",
                               json_object_new_double(layer.roughnessInfluence));
        json_object_object_add(layer_obj,
                               "reflectivityInfluence",
                               json_object_new_double(layer.reflectivityInfluence));
        json_object_object_add(layer_obj,
                               "specularInfluence",
                               json_object_new_double(layer.specularInfluence));
        json_object_object_add(layer_obj,
                               "diffuseInfluence",
                               json_object_new_double(layer.diffuseInfluence));
        json_object_object_add(layer_obj,
                               "transparencyInfluence",
                               json_object_new_double(layer.transparencyInfluence));
        json_object_array_add(layers, layer_obj);
    }
    json_object_object_add(stack_obj, "layers", layers);
    return stack_obj;
}

static struct json_object* SaveMaterialFacePlacementsForObject(int scene_object_index) {
    int count = SceneEditorMaterialFacePlacementOverrideCountForObject(scene_object_index);
    struct json_object* placements = NULL;
    if (count <= 0) return NULL;
    placements = json_object_new_array();
    if (!placements) return NULL;
    for (int i = 0; i < count; ++i) {
        SceneEditorMaterialFacePlacement placement;
        struct json_object* entry = NULL;
        if (!SceneEditorMaterialFacePlacementGetOverrideForObject(scene_object_index,
                                                                  i,
                                                                  &placement)) {
            continue;
        }
        entry = json_object_new_object();
        if (!entry) {
            json_object_put(placements);
            return NULL;
        }
        json_object_object_add(entry, "faceGroupIndex", json_object_new_int(placement.faceGroupIndex));
        json_object_object_add(entry, "textureId", json_object_new_int(placement.textureId));
        json_object_object_add(entry, "offsetU", json_object_new_double(placement.offsetU));
        json_object_object_add(entry, "offsetV", json_object_new_double(placement.offsetV));
        json_object_object_add(entry, "scale", json_object_new_double(placement.scale));
        json_object_object_add(entry, "strength", json_object_new_double(placement.strength));
        json_object_object_add(entry, "rotation", json_object_new_double(placement.rotation));
        {
            RuntimeMaterialTexture3DParams params =
                RuntimeMaterialTexture3DNormalizeParams(placement.params);
            struct json_object* parameters = json_object_new_object();
            if (!parameters) {
                json_object_put(placements);
                return NULL;
            }
            json_object_object_add(parameters, "patternMode", json_object_new_int(params.patternMode));
            json_object_object_add(parameters, "coverage", json_object_new_double(params.coverage));
            json_object_object_add(parameters, "grain", json_object_new_double(params.grain));
            json_object_object_add(parameters, "edgeSoftness", json_object_new_double(params.edgeSoftness));
            json_object_object_add(parameters, "contrast", json_object_new_double(params.contrast));
            json_object_object_add(parameters, "flow", json_object_new_double(params.flow));
            json_object_object_add(parameters, "colorDepth", json_object_new_double(params.colorDepth));
            json_object_object_add(parameters, "surfaceDamage", json_object_new_double(params.surfaceDamage));
            json_object_object_add(parameters, "seed", json_object_new_int(params.seed));
            json_object_object_add(entry, "parameters", parameters);
        }
        json_object_array_add(placements, entry);
    }
    if (json_object_array_length(placements) == 0u) {
        json_object_put(placements);
        return NULL;
    }
    return placements;
}

void SaveSceneConfig(void) {
    if (!config_io_ensure_parent_directory_for_file(SCENE_CONFIG_RUNTIME_FILE)) {
        fprintf(stderr, "Error: Failed to prepare runtime config lane for %s\n", SCENE_CONFIG_RUNTIME_FILE);
        return;
    }
    FILE* file = fopen(SCENE_CONFIG_RUNTIME_FILE, "w");
    if (!file) {
        perror("Error: Failed to open scene config file for writing");
        return;
    }

    struct json_object* config = json_object_new_object();

    // Save Window Size
    struct json_object* window = json_object_new_object();
    json_object_object_add(window, "width", json_object_new_int(sceneSettings.windowWidth));
    json_object_object_add(window, "height", json_object_new_int(sceneSettings.windowHeight));
    json_object_object_add(config, "window", window);

    struct json_object* camera = json_object_new_object();
    json_object_object_add(camera, "x", json_object_new_double(sceneSettings.camera.x));
    json_object_object_add(camera, "y", json_object_new_double(sceneSettings.camera.y));
    json_object_object_add(camera, "zoom", json_object_new_double(sceneSettings.camera.zoom));
    json_object_object_add(camera, "rotation", json_object_new_double(sceneSettings.camera.rotation));
    json_object_object_add(camera, "margin", json_object_new_double(sceneSettings.cameraMargin));
    json_object_object_add(config, "camera", camera);

    // Save number of rays
    json_object_object_add(config, "rays", json_object_new_int(sceneSettings.rays));

    // Save Scene Objects
    struct json_object* objectsArray = json_object_new_array();

    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        struct json_object* jsonObj = json_object_new_object();

        json_object_object_add(jsonObj, "type", json_object_new_string(obj->type));
        json_object_object_add(jsonObj, "x", json_object_new_double(obj->x));
        json_object_object_add(jsonObj, "y", json_object_new_double(obj->y));
        json_object_object_add(jsonObj, "z", json_object_new_double(obj->z));
        json_object_object_add(jsonObj, "scale", json_object_new_double(obj->scale));
        json_object_object_add(jsonObj, "rotation", json_object_new_double(obj->rotation));

        // Save additional object properties
        json_object_object_add(jsonObj, "texture", json_object_new_string(obj->texture));
        json_object_object_add(jsonObj, "color", json_object_new_int(obj->color));
        json_object_object_add(jsonObj, "opacity", json_object_new_double(obj->opacity));
        json_object_object_add(jsonObj, "alpha", json_object_new_double(obj->alpha));
        json_object_object_add(jsonObj, "reflectivity", json_object_new_double(obj->reflectivity));
        json_object_object_add(jsonObj, "roughness", json_object_new_double(obj->roughness));
        json_object_object_add(jsonObj, "emissiveStrength", json_object_new_double(obj->emissiveStrength));
        json_object_object_add(jsonObj, "textureId", json_object_new_int(obj->textureId));
        json_object_object_add(jsonObj, "textureOffsetU", json_object_new_double(obj->textureOffsetU));
        json_object_object_add(jsonObj, "textureOffsetV", json_object_new_double(obj->textureOffsetV));
        json_object_object_add(jsonObj, "textureScale", json_object_new_double(obj->textureScale));
        json_object_object_add(jsonObj, "textureStrength", json_object_new_double(obj->textureStrength));
        json_object_object_add(jsonObj, "texturePatternMode", json_object_new_int(obj->texturePatternMode));
        json_object_object_add(jsonObj, "textureCoverage", json_object_new_double(obj->textureCoverage));
        json_object_object_add(jsonObj, "textureGrain", json_object_new_double(obj->textureGrain));
        json_object_object_add(jsonObj, "textureEdgeSoftness", json_object_new_double(obj->textureEdgeSoftness));
        json_object_object_add(jsonObj, "textureContrast", json_object_new_double(obj->textureContrast));
        json_object_object_add(jsonObj, "textureFlow", json_object_new_double(obj->textureFlow));
        json_object_object_add(jsonObj, "textureColorDepth", json_object_new_double(obj->textureColorDepth));
        json_object_object_add(jsonObj, "textureSurfaceDamage", json_object_new_double(obj->textureSurfaceDamage));
        json_object_object_add(jsonObj, "textureSeed", json_object_new_int(obj->textureSeed));
        {
            struct json_object* materialTextureStack = ConfigSaveMaterialTextureStackForObject(i);
            struct json_object* facePlacements = SaveMaterialFacePlacementsForObject(i);
            if (materialTextureStack) {
                json_object_object_add(jsonObj, "materialTextureStack", materialTextureStack);
            }
            if (facePlacements) {
                json_object_object_add(jsonObj, "materialFacePlacements", facePlacements);
            }
        }
        json_object_object_add(jsonObj, "materialId", json_object_new_int(obj->material_id));
        json_object_object_add(jsonObj, "materialId", json_object_new_int(obj->material_id));

        if (strcmp(obj->type, "circle") == 0) {
            json_object_object_add(jsonObj, "radius", json_object_new_double(obj->radius));
        } else {
            json_object_object_add(jsonObj, "numPoints", json_object_new_int(obj->numPoints));

	    struct json_object* shapePointsArray = json_object_new_array();
            struct json_object* pointsArray = json_object_new_array();
            for (int j = 0; j < obj->numPoints; j++) {
                struct json_object* point = json_object_new_object();
                json_object_object_add(point, "x", json_object_new_double(obj->baseShapePoints[j][0]));
                json_object_object_add(point, "y", json_object_new_double(obj->baseShapePoints[j][1]));
                json_object_array_add(pointsArray, point);
		
		struct json_object* shapePoint = json_object_new_object();
                json_object_object_add(shapePoint, "x", json_object_new_double(obj->shapePoints[j][0]));
                json_object_object_add(shapePoint, "y", json_object_new_double(obj->shapePoints[j][1]));
                json_object_array_add(shapePointsArray, shapePoint);
            }
            json_object_object_add(jsonObj, "baseShapePoints", pointsArray);

	    
        }

        json_object_array_add(objectsArray, jsonObj);
    }

    json_object_object_add(config, "objects", objectsArray);

    // Save Bézier Paths
    config_scene_save_path_to_json(config, "path", &sceneSettings.bezierPath);
    config_scene_save_path_depth_to_json(config,
                                         "pathDepth",
                                         &sceneSettings.bezierPath3D,
                                         &sceneSettings.bezierPath);
    config_scene_save_path_to_json(config, "cameraPath", &sceneSettings.cameraPath);
    config_scene_save_camera_path_depth_to_json(config,
                                                "cameraPathDepth",
                                                &sceneSettings.cameraPath3D,
                                                &sceneSettings.cameraPath);
    json_object_object_add(config, "cameraZ", json_object_new_double(sceneSettings.cameraZ));

    // Write JSON Data to File
    fprintf(file, "%s", json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY));
    fclose(file);
    json_object_put(config);

    printf("Scene configuration saved successfully.\n");
}
void LoadWindowConfig(struct json_object* config) {
    printf("DEBUG: Loading Window Configuration...\n");

    struct json_object *windowConfig;
    if (json_object_object_get_ex(config, "window", &windowConfig)) {
        sceneSettings.windowWidth = json_object_get_int(json_object_object_get(windowConfig, "width"));
        sceneSettings.windowHeight = json_object_get_int(json_object_object_get(windowConfig, "height"));
        printf("INFO: Window Size Loaded - Width: %d, Height: %d\n", 
               sceneSettings.windowWidth, sceneSettings.windowHeight);
    } else {
        printf("ERROR: Window configuration missing in JSON.\n");
    }
}

static void LoadCameraConfig(struct json_object* config) {
    struct json_object *cameraObj;
    double defaultX = sceneSettings.windowWidth * 0.5;
    double defaultY = sceneSettings.windowHeight * 0.5;
    double defaultZoom = 1.0;
    double maxMargin = fmin(sceneSettings.windowWidth, sceneSettings.windowHeight) * 0.45;
    if (maxMargin < 1.0) maxMargin = 1.0;
    double defaultMargin = fmin(sceneSettings.cameraMargin, maxMargin);

    if (json_object_object_get_ex(config, "camera", &cameraObj)) {
        struct json_object *xObj, *yObj, *zoomObj;
        if (json_object_object_get_ex(cameraObj, "x", &xObj)) {
            sceneSettings.camera.x = json_object_get_double(xObj);
        } else {
            sceneSettings.camera.x = defaultX;
        }

        if (json_object_object_get_ex(cameraObj, "y", &yObj)) {
            sceneSettings.camera.y = json_object_get_double(yObj);
        } else {
            sceneSettings.camera.y = defaultY;
        }

        if (json_object_object_get_ex(cameraObj, "zoom", &zoomObj)) {
            double zoom = json_object_get_double(zoomObj);
            sceneSettings.camera.zoom = (zoom > 0.0) ? zoom : defaultZoom;
        } else {
            sceneSettings.camera.zoom = defaultZoom;
        }
        struct json_object *rotObj;
        if (json_object_object_get_ex(cameraObj, "rotation", &rotObj)) {
            sceneSettings.camera.rotation = json_object_get_double(rotObj);
        } else {
            sceneSettings.camera.rotation = 0.0;
        }

        struct json_object *marginObj;
        if (json_object_object_get_ex(cameraObj, "margin", &marginObj)) {
            sceneSettings.cameraMargin = json_object_get_double(marginObj);
        } else {
            sceneSettings.cameraMargin = defaultMargin;
        }
    } else {
        sceneSettings.camera.x = defaultX;
        sceneSettings.camera.y = defaultY;
        sceneSettings.camera.zoom = defaultZoom;
        sceneSettings.cameraMargin = defaultMargin;
    }

    if (sceneSettings.cameraMargin < 0.0) sceneSettings.cameraMargin = 0.0;
    double maxAllowed = fmin(sceneSettings.windowWidth, sceneSettings.windowHeight) * 0.45;
    if (maxAllowed < 1.0) maxAllowed = 1.0;
    if (sceneSettings.cameraMargin > maxAllowed) sceneSettings.cameraMargin = maxAllowed;
}

void LoadObjectProperties(struct json_object* obj, SceneObject* sceneObject) {
    struct json_object *texture, *color, *opacity, *alpha, *transparency, *reflectivity, *roughness,
        *emissiveStrength, *textureId, *textureOffsetU, *textureOffsetV, *textureScale,
        *textureStrength, *texturePatternMode, *textureCoverage, *textureGrain, *textureEdgeSoftness,
        *textureContrast, *textureFlow, *textureColorDepth, *textureSurfaceDamage, *textureSeed,
        *materialId;

    // Load texture path
    if (json_object_object_get_ex(obj, "texture", &texture)) {
        strncpy(sceneObject->texture, json_object_get_string(texture), sizeof(sceneObject->texture) - 1);
        sceneObject->texture[sizeof(sceneObject->texture) - 1] = '\0';
    } else {
        sceneObject->texture[0] = '\0'; // Default: No texture
    }

    // Load color (if available)
    if (json_object_object_get_ex(obj, "color", &color)) {
        sceneObject->color = json_object_get_int(color);
    } else {
        sceneObject->color = 0xFFFFFF; // Default: White color
    }

    // Load opacity (if available)
    if (json_object_object_get_ex(obj, "opacity", &opacity)) {
        sceneObject->opacity = json_object_get_double(opacity);
    } else {
        sceneObject->opacity = 1.0; // Default: Fully opaque
    }

    if (json_object_object_get_ex(obj, "alpha", &alpha)) {
        sceneObject->alpha = json_object_get_double(alpha);
    } else if (json_object_object_get_ex(obj, "transparency", &transparency)) {
        sceneObject->alpha = json_object_get_double(transparency);
    } else {
        sceneObject->alpha = 1.0;
    }

    if (json_object_object_get_ex(obj, "reflectivity", &reflectivity)) {
        sceneObject->reflectivity = json_object_get_double(reflectivity);
    } else {
        sceneObject->reflectivity = 0.35;
    }

    if (json_object_object_get_ex(obj, "roughness", &roughness)) {
        sceneObject->roughness = json_object_get_double(roughness);
    } else {
        sceneObject->roughness = 0.65;
    }

    if (json_object_object_get_ex(obj, "emissiveStrength", &emissiveStrength)) {
        sceneObject->emissiveStrength = json_object_get_double(emissiveStrength);
    } else {
        sceneObject->emissiveStrength = 0.0;
    }

    if (json_object_object_get_ex(obj, "textureId", &textureId)) {
        sceneObject->textureId = json_object_get_int(textureId);
    } else {
        sceneObject->textureId = 0;
    }

    if (json_object_object_get_ex(obj, "textureOffsetU", &textureOffsetU)) {
        sceneObject->textureOffsetU = json_object_get_double(textureOffsetU);
    } else {
        sceneObject->textureOffsetU = 0.0;
    }

    if (json_object_object_get_ex(obj, "textureOffsetV", &textureOffsetV)) {
        sceneObject->textureOffsetV = json_object_get_double(textureOffsetV);
    } else {
        sceneObject->textureOffsetV = 0.0;
    }

    if (json_object_object_get_ex(obj, "textureScale", &textureScale)) {
        sceneObject->textureScale = json_object_get_double(textureScale);
    } else {
        sceneObject->textureScale = 1.0;
    }

    if (json_object_object_get_ex(obj, "textureStrength", &textureStrength)) {
        sceneObject->textureStrength = json_object_get_double(textureStrength);
    } else {
        sceneObject->textureStrength = 0.0;
    }

    if (json_object_object_get_ex(obj, "texturePatternMode", &texturePatternMode)) {
        sceneObject->texturePatternMode = json_object_get_int(texturePatternMode);
    } else {
        sceneObject->texturePatternMode = 0;
    }

    if (json_object_object_get_ex(obj, "textureCoverage", &textureCoverage)) {
        sceneObject->textureCoverage = json_object_get_double(textureCoverage);
    } else {
        sceneObject->textureCoverage = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureGrain", &textureGrain)) {
        sceneObject->textureGrain = json_object_get_double(textureGrain);
    } else {
        sceneObject->textureGrain = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureEdgeSoftness", &textureEdgeSoftness)) {
        sceneObject->textureEdgeSoftness = json_object_get_double(textureEdgeSoftness);
    } else {
        sceneObject->textureEdgeSoftness = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureContrast", &textureContrast)) {
        sceneObject->textureContrast = json_object_get_double(textureContrast);
    } else {
        sceneObject->textureContrast = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureFlow", &textureFlow)) {
        sceneObject->textureFlow = json_object_get_double(textureFlow);
    } else {
        sceneObject->textureFlow = 0.0;
    }

    if (json_object_object_get_ex(obj, "textureColorDepth", &textureColorDepth)) {
        sceneObject->textureColorDepth = json_object_get_double(textureColorDepth);
    } else {
        sceneObject->textureColorDepth = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureSurfaceDamage", &textureSurfaceDamage)) {
        sceneObject->textureSurfaceDamage = json_object_get_double(textureSurfaceDamage);
    } else {
        sceneObject->textureSurfaceDamage = 0.5;
    }

    if (json_object_object_get_ex(obj, "textureSeed", &textureSeed)) {
        sceneObject->textureSeed = json_object_get_int(textureSeed);
    } else {
        sceneObject->textureSeed = 0;
    }
    {
        RuntimeMaterialTexture3DParams params =
            RuntimeMaterialTexture3DParamsFromObject(sceneObject);
        sceneObject->texturePatternMode = params.patternMode;
        sceneObject->textureCoverage = params.coverage;
        sceneObject->textureGrain = params.grain;
        sceneObject->textureEdgeSoftness = params.edgeSoftness;
        sceneObject->textureContrast = params.contrast;
        sceneObject->textureFlow = params.flow;
        sceneObject->textureColorDepth = params.colorDepth;
        sceneObject->textureSurfaceDamage = params.surfaceDamage;
        sceneObject->textureSeed = params.seed;
    }

    if (json_object_object_get_ex(obj, "materialId", &materialId)) {
        sceneObject->material_id = json_object_get_int(materialId);
    } else {
        sceneObject->material_id = MaterialManagerDefaultId();
    }
}

static bool ConfigJsonGetDouble(struct json_object* obj, const char* key, double* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool ConfigJsonGetInt(struct json_object* obj, const char* key, int* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_int) &&
        !json_object_is_type(value, json_type_double)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static void ConfigLoadTextureParams(struct json_object* obj,
                                    RuntimeMaterialTexture3DParams* params) {
    struct json_object* parameters = NULL;
    if (!obj || !params) return;
    if (json_object_object_get_ex(obj, "parameters", &parameters) &&
        json_object_is_type(parameters, json_type_object)) {
        ConfigJsonGetInt(parameters, "patternMode", &params->patternMode);
        ConfigJsonGetInt(parameters, "pattern_mode", &params->patternMode);
        ConfigJsonGetDouble(parameters, "coverage", &params->coverage);
        ConfigJsonGetDouble(parameters, "grain", &params->grain);
        ConfigJsonGetDouble(parameters, "edgeSoftness", &params->edgeSoftness);
        ConfigJsonGetDouble(parameters, "edge_softness", &params->edgeSoftness);
        ConfigJsonGetDouble(parameters, "contrast", &params->contrast);
        ConfigJsonGetDouble(parameters, "flow", &params->flow);
        ConfigJsonGetDouble(parameters, "colorDepth", &params->colorDepth);
        ConfigJsonGetDouble(parameters, "color_depth", &params->colorDepth);
        ConfigJsonGetDouble(parameters, "surfaceDamage", &params->surfaceDamage);
        ConfigJsonGetDouble(parameters, "surface_damage", &params->surfaceDamage);
        ConfigJsonGetInt(parameters, "seed", &params->seed);
    }
    ConfigJsonGetInt(obj, "texturePatternMode", &params->patternMode);
    ConfigJsonGetDouble(obj, "textureCoverage", &params->coverage);
    ConfigJsonGetDouble(obj, "textureGrain", &params->grain);
    ConfigJsonGetDouble(obj, "textureEdgeSoftness", &params->edgeSoftness);
    ConfigJsonGetDouble(obj, "textureContrast", &params->contrast);
    ConfigJsonGetDouble(obj, "textureFlow", &params->flow);
    ConfigJsonGetDouble(obj, "textureColorDepth", &params->colorDepth);
    ConfigJsonGetDouble(obj, "textureSurfaceDamage", &params->surfaceDamage);
    ConfigJsonGetInt(obj, "textureSeed", &params->seed);
    *params = RuntimeMaterialTexture3DNormalizeParams(*params);
}

static void ConfigLoadTexturePlacement(struct json_object* obj,
                                       RuntimeMaterialTexture3DPlacement* placement) {
    struct json_object* placement_obj = NULL;
    if (!obj || !placement) return;
    if (json_object_object_get_ex(obj, "placement", &placement_obj) &&
        json_object_is_type(placement_obj, json_type_object)) {
        ConfigJsonGetDouble(placement_obj, "offsetU", &placement->offsetU);
        ConfigJsonGetDouble(placement_obj, "offset_u", &placement->offsetU);
        ConfigJsonGetDouble(placement_obj, "offsetV", &placement->offsetV);
        ConfigJsonGetDouble(placement_obj, "offset_v", &placement->offsetV);
        ConfigJsonGetDouble(placement_obj, "scale", &placement->scale);
        ConfigJsonGetDouble(placement_obj, "strength", &placement->strength);
        ConfigJsonGetDouble(placement_obj, "rotation", &placement->rotation);
    }
    ConfigJsonGetDouble(obj, "offsetU", &placement->offsetU);
    ConfigJsonGetDouble(obj, "offset_u", &placement->offsetU);
    ConfigJsonGetDouble(obj, "offsetV", &placement->offsetV);
    ConfigJsonGetDouble(obj, "offset_v", &placement->offsetV);
    ConfigJsonGetDouble(obj, "scale", &placement->scale);
    ConfigJsonGetDouble(obj, "strength", &placement->strength);
    ConfigJsonGetDouble(obj, "rotation", &placement->rotation);
}

static bool ConfigLoadMaterialTextureStack(struct json_object* obj, int scene_object_index) {
    struct json_object* stack_obj = NULL;
    struct json_object* layers = NULL;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    size_t count = 0u;
    if (!obj || scene_object_index < 0) return false;
    if (!json_object_object_get_ex(obj, "materialTextureStack", &stack_obj)) {
        json_object_object_get_ex(obj, "material_texture_stack", &stack_obj);
    }
    if (!stack_obj ||
        !json_object_is_type(stack_obj, json_type_object) ||
        !json_object_object_get_ex(stack_obj, "layers", &layers) ||
        !json_object_is_type(layers, json_type_array)) {
        SceneEditorMaterialStackClearObjectStack(scene_object_index);
        return false;
    }

    count = json_object_array_length(layers);
    for (size_t i = 0u;
         i < count && stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
         ++i) {
        struct json_object* layer_obj = json_object_array_get_idx(layers, i);
        struct json_object* value = NULL;
        const char* kind_id = NULL;
        RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
        RuntimeMaterialTextureLayer layer;
        if (!layer_obj || !json_object_is_type(layer_obj, json_type_object)) continue;
        if (json_object_object_get_ex(layer_obj, "kind", &value)) {
            kind_id = json_object_get_string(value);
        }
        kind = RuntimeMaterialTextureLayerKindFromStableId(kind_id);
        if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;
        layer = RuntimeMaterialTextureLayerKindIsBase(kind)
                    ? RuntimeMaterialTextureLayerMakeBase(kind)
                    : RuntimeMaterialTextureLayerMakeOverlay(kind);
        if (json_object_object_get_ex(layer_obj, "id", &value)) {
            snprintf(layer.layerId, sizeof(layer.layerId), "%s", json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "name", &value)) {
            snprintf(layer.displayName, sizeof(layer.displayName), "%s", json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "blend", &value)) {
            layer.blendMode = RuntimeMaterialTextureLayerBlendModeFromStableId(json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "enabled", &value)) {
            layer.enabled = json_object_get_boolean(value);
        }
        ConfigJsonGetDouble(layer_obj, "opacity", &layer.opacity);
        ConfigLoadTexturePlacement(layer_obj, &layer.placement);
        ConfigLoadTextureParams(layer_obj, &layer.params);
        layer.placement.params = layer.params;
        ConfigJsonGetDouble(layer_obj, "roughnessInfluence", &layer.roughnessInfluence);
        ConfigJsonGetDouble(layer_obj, "roughness_influence", &layer.roughnessInfluence);
        ConfigJsonGetDouble(layer_obj, "reflectivityInfluence", &layer.reflectivityInfluence);
        ConfigJsonGetDouble(layer_obj, "reflectivity_influence", &layer.reflectivityInfluence);
        ConfigJsonGetDouble(layer_obj, "specularInfluence", &layer.specularInfluence);
        ConfigJsonGetDouble(layer_obj, "specular_influence", &layer.specularInfluence);
        ConfigJsonGetDouble(layer_obj, "diffuseInfluence", &layer.diffuseInfluence);
        ConfigJsonGetDouble(layer_obj, "diffuse_influence", &layer.diffuseInfluence);
        ConfigJsonGetDouble(layer_obj, "transparencyInfluence", &layer.transparencyInfluence);
        ConfigJsonGetDouble(layer_obj, "transparency_influence", &layer.transparencyInfluence);
        stack.layers[stack.layerCount++] = RuntimeMaterialTextureLayerNormalize(layer);
    }
    if (stack.layerCount <= 0) {
        SceneEditorMaterialStackClearObjectStack(scene_object_index);
        return false;
    }
    return SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack);
}

static void LoadMaterialFacePlacements(struct json_object* obj, int scene_object_index) {
    struct json_object* placements = NULL;
    size_t count = 0u;
    if (!obj || scene_object_index < 0) return;
    if (!json_object_object_get_ex(obj, "materialFacePlacements", &placements) ||
        !json_object_is_type(placements, json_type_array)) {
        return;
    }
    count = json_object_array_length(placements);
    for (size_t i = 0u; i < count; ++i) {
        struct json_object* entry = json_object_array_get_idx(placements, i);
        struct json_object* face_group = NULL;
        SceneEditorMaterialFacePlacement placement;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "faceGroupIndex", &face_group) ||
            !json_object_is_type(face_group, json_type_int)) {
            continue;
        }
        memset(&placement, 0, sizeof(placement));
        placement.hasOverride = true;
        placement.sceneObjectIndex = scene_object_index;
        placement.faceGroupIndex = json_object_get_int(face_group);
        placement.textureId = sceneSettings.sceneObjects[scene_object_index].textureId;
        placement.offsetU = sceneSettings.sceneObjects[scene_object_index].textureOffsetU;
        placement.offsetV = sceneSettings.sceneObjects[scene_object_index].textureOffsetV;
        placement.scale = sceneSettings.sceneObjects[scene_object_index].textureScale;
        placement.strength = sceneSettings.sceneObjects[scene_object_index].textureStrength;
        placement.params = RuntimeMaterialTexture3DParamsFromObject(
            &sceneSettings.sceneObjects[scene_object_index]);
        ConfigJsonGetInt(entry, "textureId", &placement.textureId);
        ConfigJsonGetDouble(entry, "offsetU", &placement.offsetU);
        ConfigJsonGetDouble(entry, "offsetV", &placement.offsetV);
        ConfigJsonGetDouble(entry, "scale", &placement.scale);
        ConfigJsonGetDouble(entry, "strength", &placement.strength);
        ConfigJsonGetDouble(entry, "rotation", &placement.rotation);
        ConfigLoadTextureParams(entry, &placement.params);
        SceneEditorMaterialFacePlacementSetOverride(&placement);
    }
}

            
void LoadSceneObjects(struct json_object* config) {
    printf("DEBUG: Loading Scene Objects...\n");
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();

    struct json_object *objects;
    if (json_object_object_get_ex(config, "objects", &objects)) {
        sceneSettings.objectCount = json_object_array_length(objects);
        if (sceneSettings.objectCount > MAX_OBJECTS) sceneSettings.objectCount = MAX_OBJECTS; // Safety limit

        for (int i = 0; i < sceneSettings.objectCount; i++) {
            struct json_object* obj = json_object_array_get_idx(objects, i);
            SceneObject* sceneObj = &sceneSettings.sceneObjects[i];

            LoadObjectProperties(obj, sceneObj);
            ConfigLoadMaterialTextureStack(obj, i);
            LoadMaterialFacePlacements(obj, i);

            struct json_object *type, *x, *y, *z, *scale, *rotation, *radius, *numPoints, *baseShapePoints, 
*shapePoints;

            if (!json_object_object_get_ex(obj, "type", &type) ||
                !json_object_object_get_ex(obj, "x", &x) ||
                !json_object_object_get_ex(obj, "y", &y)) {
                printf("Warning: Object missing required fields. Skipping.\n");
                continue;
            }

            strcpy(sceneObj->type, json_object_get_string(type));
            sceneObj->x = json_object_get_double(x);
            sceneObj->y = json_object_get_double(y);
            sceneObj->z = 0.0;
            sceneObj->scale = 1.0;
            sceneObj->rotation = 0.0;
            sceneObj->dirty = true;

            if (json_object_object_get_ex(obj, "z", &z)) {
                sceneObj->z = json_object_get_double(z);
            }

            if (json_object_object_get_ex(obj, "scale", &scale)) {
                sceneObj->scale = json_object_get_double(scale);
            }
            if (json_object_object_get_ex(obj, "rotation", &rotation)) {
                sceneObj->rotation = json_object_get_double(rotation);
            }

            if (strcmp(sceneObj->type, "circle") == 0) {
                if (json_object_object_get_ex(obj, "radius", &radius)) {
                    sceneObj->radius = json_object_get_double(radius);
                }
                sceneObj->numPoints = 0;
            } else {
                if (json_object_object_get_ex(obj, "numPoints", &numPoints)) {
                    sceneObj->numPoints = json_object_get_int(numPoints);
                }

                if (json_object_object_get_ex(obj, "baseShapePoints", &baseShapePoints) &&
                    json_object_is_type(baseShapePoints, json_type_array)) {

                    int arrayLen = json_object_array_length(baseShapePoints);
                    sceneObj->numPoints = arrayLen > MAX_POINTS ? MAX_POINTS : arrayLen;

                    for (int j = 0; j < sceneObj->numPoints; j++) {
                        struct json_object* point = json_object_array_get_idx(baseShapePoints, j);
                        struct json_object *px, *py;

                        if (json_object_object_get_ex(point, "x", &px) &&
                            json_object_object_get_ex(point, "y", &py)) {
                            sceneObj->baseShapePoints[j][0] = json_object_get_double(px);
                            sceneObj->baseShapePoints[j][1] = json_object_get_double(py);
                        }
                    }
                }

                if (json_object_object_get_ex(obj, "shapePoints", &shapePoints) &&
                    json_object_is_type(shapePoints, json_type_array)) {

                    int arrayLen = json_object_array_length(shapePoints);
                    for (int j = 0; j < arrayLen && j < sceneObj->numPoints; j++) {
                        struct json_object* point = json_object_array_get_idx(shapePoints, j);
                        struct json_object *px, *py;

                        if (json_object_object_get_ex(point, "x", &px) &&
                            json_object_object_get_ex(point, "y", &py)) {
                            sceneObj->shapePoints[j][0] = json_object_get_double(px);
                            sceneObj->shapePoints[j][1] = json_object_get_double(py);
                        }
                    }
                } else {
                    // If shapePoints are missing, recalculate them from baseShapePoints
                    for (int j = 0; j < sceneObj->numPoints; j++) {
                        sceneObj->shapePoints[j][0] = sceneObj->baseShapePoints[j][0] * sceneObj->scale;
                        sceneObj->shapePoints[j][1] = sceneObj->baseShapePoints[j][1] * sceneObj->scale;
                    }
                }

                SetPolygonRadius(sceneObj);
            }

            MarkObjectDirty(sceneObj);
        }

        printf("INFO: Loaded %d Scene Objects\n", sceneSettings.objectCount);
    } else {
        printf("ERROR: Scene objects missing in JSON.\n");
    }
}



void LoadSceneConfig(void) {
    const char *loaded_path = NULL;
    FILE *file = config_io_open_read_with_fallback(SCENE_CONFIG_RUNTIME_FILE,
                                                   SCENE_CONFIG_DEFAULT_FILE,
                                                   SCENE_CONFIG_LEGACY_FILE,
                                                   &loaded_path);
    if (!file) {
        printf("INFO: Scene config file not found (tried %s, %s, %s); using defaults/runtime overrides.\n",
               SCENE_CONFIG_RUNTIME_FILE,
               SCENE_CONFIG_DEFAULT_FILE,
               SCENE_CONFIG_LEGACY_FILE);
        config_scene_ensure_camera_path_default(&sceneSettings);
        return;
    }
    if (loaded_path) {
        printf("INFO: Loaded scene config from: %s\n", loaded_path);
    }
     
    struct json_object *config = config_io_parse_json_file(file, "scene config", false);
    if (!config) {
        config_scene_ensure_camera_path_default(&sceneSettings);
        return;
    }
    printf("In load scene config hold method for  anaimation.c\n");
    LoadWindowConfig(config);
    LoadCameraConfig(config);
    LoadSceneObjects(config);

    bool lightPathLoaded = config_scene_load_path_from_json(config, "path", &sceneSettings.bezierPath);
    if (!lightPathLoaded || sceneSettings.bezierPath.numPoints < 2) {
        printf("ERROR: Bézier path missing or invalid in scene_config.json.\n");
        sceneSettings.bezierPath.numPoints = 0;
        CameraPath3D_Reset(&sceneSettings.bezierPath3D);
    } else {
        printf("INFO: Loaded Bézier Path with %d points and %d segments.\n",
               sceneSettings.bezierPath.numPoints, sceneSettings.bezierPath.numPoints - 1);
        if (!config_scene_load_path_depth_from_json(config,
                                                    "pathDepth",
                                                    &sceneSettings.bezierPath3D,
                                                    &sceneSettings.bezierPath)) {
            CameraPath3D_Reset(&sceneSettings.bezierPath3D);
            for (int i = 0; i < sceneSettings.bezierPath.numPoints && i < MAX_BEZIER_POINTS; ++i) {
                sceneSettings.bezierPath3D.point_z[i] = animSettings.lightHeight;
            }
        }
    }

    bool cameraPathLoaded = config_scene_load_camera_path_from_json(config,
                                                                    "cameraPath",
                                                                    &sceneSettings.cameraPath);
    if (cameraPathLoaded) {
        printf("INFO: Loaded Camera Path with %d point(s).\n", sceneSettings.cameraPath.numPoints);
    } else {
        printf("INFO: Camera path missing in config; leaving authored camera path empty.\n");
    }
    {
        struct json_object* camera_z_obj = NULL;
        if (json_object_object_get_ex(config, "cameraZ", &camera_z_obj)) {
            sceneSettings.cameraZ = json_object_get_double(camera_z_obj);
        }
    }
    if (!config_scene_load_camera_path_depth_from_json(config,
                                                       "cameraPathDepth",
                                                       &sceneSettings.cameraPath3D,
                                                       &sceneSettings.cameraPath)) {
        int i = 0;
        CameraPath3D_Reset(&sceneSettings.cameraPath3D);
        for (i = 0; i < sceneSettings.cameraPath.numPoints && i < MAX_BEZIER_POINTS; ++i) {
            sceneSettings.cameraPath3D.point_z[i] = sceneSettings.cameraZ;
        }
    }
    config_scene_ensure_camera_path_default(&sceneSettings);

    struct json_object *raysObj;
    if (json_object_object_get_ex(config, "rays", &raysObj)) {
        sceneSettings.rays = json_object_get_int(raysObj);
        printf("Loaded Rays Value: %d\n", sceneSettings.rays);
    } else {
        printf("WARNING: 'rays' key missing in config, using default value.\n");
        sceneSettings.rays = 2000;  //  Default value if not found
    }
    
    // Clamp material ids to valid range
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        if (sceneSettings.sceneObjects[i].material_id < 0 ||
            sceneSettings.sceneObjects[i].material_id >= MaterialManagerCount()) {
            sceneSettings.sceneObjects[i].material_id = MaterialManagerDefaultId();
        }
    }

    json_object_put(config);
    config_scene_ensure_camera_path_default(&sceneSettings);
}
