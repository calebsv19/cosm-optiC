#include "config/config_manager.h"
#include "config/config_file_io.h"
#include "config/config_scene_path_io.h"
#include "config/core/config_runtime_paths.h"
#include "app/data_paths.h"
#include "render/ray_tracing_integrator_catalog.h"
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
    .rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT,
    .secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT,
    .transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT,
    .temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT,
    .renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT,
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
        *emissiveStrength, *textureId, *materialId;

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
        sceneObject->emissiveStrength = 1.0;
    }

    if (json_object_object_get_ex(obj, "textureId", &textureId)) {
        sceneObject->textureId = json_object_get_int(textureId);
    } else {
        sceneObject->textureId = 0;
    }

    if (json_object_object_get_ex(obj, "materialId", &materialId)) {
        sceneObject->material_id = json_object_get_int(materialId);
    } else {
        sceneObject->material_id = MaterialManagerDefaultId();
    }
}

            
void LoadSceneObjects(struct json_object* config) {
    printf("DEBUG: Loading Scene Objects...\n");

    struct json_object *objects;
    if (json_object_object_get_ex(config, "objects", &objects)) {
        sceneSettings.objectCount = json_object_array_length(objects);
        if (sceneSettings.objectCount > MAX_OBJECTS) sceneSettings.objectCount = MAX_OBJECTS; // Safety limit

        for (int i = 0; i < sceneSettings.objectCount; i++) {
            struct json_object* obj = json_object_array_get_idx(objects, i);
            SceneObject* sceneObj = &sceneSettings.sceneObjects[i];

            LoadObjectProperties(obj, sceneObj);

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
        printf("ERROR: Failed to open scene config file (tried %s, %s, %s)\n",
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
