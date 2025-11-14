#include "config/config_manager.h"
#include "scene/object_manager.h"
#include <stdio.h>       // For file handling (fopen, fprintf, fclose, perror)
#include <stdlib.h>      // For memory allocation (malloc, free)
#include <string.h>      // For string functions (strncpy)
#include <json-c/json.h> // For JSON handling
#include <math.h>

#define SCENE_CONFIG_FILE "Configs/scene_config.json"
#define ANIMATION_CONFIG_FILE "Configs/animation_config.json"


typedef enum {
    LONG_RAYS = 0,         // Original ray casting mode
    REALISTIC_LIGHT = 1    // Inverse-square law lighting
} LightMode;

static double ClampDoubleValue(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

// **Global config instances**
AnimationConfig animSettings = {
    .interactiveMode = true,
    .deepRenderMode = false,
    .bounceMode = false,
    .autoMP4 = false,
    .bounceLimit = 10,
    .frameLimit = 50,
    .framesForTravel = 40,
    .maxLoopCount = 1,
    .fps = 30,
    .frameDuration = 1.0 / 30.0,
    .frameDir = "Animations/default",
    .loopMode = "Normal",
    .lightMode = 0,
    .blurMode = 0,
    .lightDiffusionEnabled = true,
    .lightDiffusionRadius = 4,
    .lightDiffusionStrength = 0.65,
    .useTiledRenderer = false,
    .tileSize = 16,
    .rouletteThreshold = 0.01,
    .integratorMode = 0,
    .pathSamplesPerPixel = 4,
    .pathMaxDepth = 4,
    .pathDirectLighting = true,
    .pathRussianRoulette = true,
    .pathEnableMIS = true,
    .environmentBrightness = 0.0,
    .pathSeed = 1,
    .editorMode = 0,
    .cacheContributionWeight = 1.0,
    .bsdfModel = 1,
    .lightIntensity = 5.0,
    .forwardDecay = 0.0,
    .forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC,
    .renderQuality = RENDER_QUALITY_MEDIUM
};

SceneConfig sceneSettings = {
    .windowWidth = 1200,
    .windowHeight = 800,
    .objectCount = 0,  // No objects initially
    .bezierPath = { .numPoints = 0, .mode = BEZIER_CUBIC },
    .rays = 2000,
    .camera = { .x = 0.0, .y = 0.0, .zoom = 1.0 },
    .cameraMargin = 80.0
};

static double DefaultForwardFalloffDistance(void) {
    double w = (sceneSettings.windowWidth > 0) ? sceneSettings.windowWidth : 1200.0;
    double h = (sceneSettings.windowHeight > 0) ? sceneSettings.windowHeight : 800.0;
    return hypot(w, h);
}

void SaveAllSettings(void) {
	SaveSceneConfig();
	SaveAnimationConfig();
}

void LoadAllSettings(void) {
        LoadSceneConfig();
        LoadAnimationConfig();
}

void SaveSceneConfig(void) {
    FILE* file = fopen(SCENE_CONFIG_FILE, "w");
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
        json_object_object_add(jsonObj, "scale", json_object_new_double(obj->scale));
        json_object_object_add(jsonObj, "rotation", json_object_new_double(obj->rotation));

        // Save additional object properties
        json_object_object_add(jsonObj, "texture", json_object_new_string(obj->texture));
        json_object_object_add(jsonObj, "color", json_object_new_int(obj->color));
        json_object_object_add(jsonObj, "opacity", json_object_new_double(obj->opacity));
        json_object_object_add(jsonObj, "reflectivity", json_object_new_double(obj->reflectivity));
        json_object_object_add(jsonObj, "roughness", json_object_new_double(obj->roughness));
        json_object_object_add(jsonObj, "textureId", json_object_new_int(obj->textureId));

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

    // Save Bézier Path
    struct json_object* path = json_object_new_object();
    struct json_object* pointsArray = json_object_new_array();

    // Save Bézier Path Mode
    const char* modeStr = (sceneSettings.bezierPath.mode == BEZIER_QUADRATIC) ? "BEZIER_QUADRATIC" : 
"BEZIER_CUBIC";
    json_object_object_add(path, "mode", json_object_new_string(modeStr));

    for (int i = 0; i < sceneSettings.bezierPath.numPoints; i++) {
        struct json_object* pointObj = json_object_new_object();
        struct json_object* velocity1Obj = json_object_new_object();
        struct json_object* velocity2Obj = json_object_new_object();

        json_object_object_add(pointObj, "x", json_object_new_int(sceneSettings.bezierPath.points[i].x));
        json_object_object_add(pointObj, "y", json_object_new_int(sceneSettings.bezierPath.points[i].y));

        if (i < sceneSettings.bezierPath.numPoints - 1) {
            json_object_object_add(velocity1Obj, "vx", 
					json_object_new_int(sceneSettings.bezierPath.handles[i][0].vx));
            json_object_object_add(velocity1Obj, "vy", 
					json_object_new_int(sceneSettings.bezierPath.handles[i][0].vy));
            json_object_object_add(pointObj, "velocity1", velocity1Obj);
        }
        if (i > 0) {
            json_object_object_add(velocity2Obj, "vx", 
					json_object_new_int(sceneSettings.bezierPath.handles[i - 1][1].vx));
            json_object_object_add(velocity2Obj, "vy", 
					json_object_new_int(sceneSettings.bezierPath.handles[i - 1][1].vy));
            json_object_object_add(pointObj, "velocity2", velocity2Obj);
        }

        json_object_array_add(pointsArray, pointObj);
    }

    json_object_object_add(path, "points", pointsArray);
    json_object_object_add(config, "path", path);

    // Write JSON Data to File
    fprintf(file, "%s", json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY));
    fclose(file);
    json_object_put(config);

    printf("Scene configuration saved successfully.\n");
}


void SaveAnimationConfig(void) {
    FILE* file = fopen(ANIMATION_CONFIG_FILE, "w");
    if (!file) {
        perror("Failed to open animation config file for writing");
        return;
    }

    struct json_object* config = json_object_new_object();

    json_object_object_add(config, "interactiveMode", json_object_new_boolean(animSettings.interactiveMode));
    json_object_object_add(config, "deepRenderMode", json_object_new_boolean(animSettings.deepRenderMode));
    json_object_object_add(config, "bounceMode", json_object_new_boolean(animSettings.bounceMode));
    json_object_object_add(config, "autoMP4", json_object_new_boolean(animSettings.autoMP4));
    json_object_object_add(config, "bounceLimit", json_object_new_int(animSettings.bounceLimit));
    json_object_object_add(config, "frameLimit", json_object_new_int(animSettings.frameLimit));
    json_object_object_add(config, "framesForTravel", json_object_new_int(animSettings.framesForTravel));
    json_object_object_add(config, "fps", json_object_new_int(animSettings.fps));
    json_object_object_add(config, "frameDuration", json_object_new_double(animSettings.frameDuration));
    json_object_object_add(config, "frameDir", json_object_new_string(animSettings.frameDir));
    json_object_object_add(config, "maxLoopCount", json_object_new_int(animSettings.maxLoopCount));
    json_object_object_add(config, "loopMode", json_object_new_string(animSettings.loopMode));
    json_object_object_add(config, "lightMode", json_object_new_int(animSettings.lightMode));
    json_object_object_add(config, "blurMode", json_object_new_int(animSettings.blurMode));
    json_object_object_add(config, "lightDiffusionEnabled", json_object_new_boolean(animSettings.lightDiffusionEnabled));
    json_object_object_add(config, "lightDiffusionRadius", json_object_new_int(animSettings.lightDiffusionRadius));
    json_object_object_add(config, "lightDiffusionStrength", json_object_new_double(animSettings.lightDiffusionStrength));
    json_object_object_add(config, "editorMode", json_object_new_int(animSettings.editorMode));
    json_object_object_add(config, "useTiledRenderer", json_object_new_boolean(animSettings.useTiledRenderer));
    json_object_object_add(config, "tileSize", json_object_new_int(animSettings.tileSize));
    json_object_object_add(config, "rouletteThreshold", json_object_new_double(animSettings.rouletteThreshold));
    json_object_object_add(config, "integratorMode", json_object_new_int(animSettings.integratorMode));
    json_object_object_add(config, "pathSamplesPerPixel", json_object_new_int(animSettings.pathSamplesPerPixel));
    json_object_object_add(config, "pathMaxDepth", json_object_new_int(animSettings.pathMaxDepth));
    json_object_object_add(config, "pathDirectLighting", json_object_new_boolean(animSettings.pathDirectLighting));
    json_object_object_add(config, "pathRussianRoulette", json_object_new_boolean(animSettings.pathRussianRoulette));
    json_object_object_add(config, "pathEnableMIS", json_object_new_boolean(animSettings.pathEnableMIS));
    json_object_object_add(config, "environmentBrightness", json_object_new_double(animSettings.environmentBrightness));
    json_object_object_add(config, "pathSeed", json_object_new_int(animSettings.pathSeed));
    json_object_object_add(config, "cacheContributionWeight", json_object_new_double(animSettings.cacheContributionWeight));
    json_object_object_add(config, "bsdfModel", json_object_new_int(animSettings.bsdfModel));
    json_object_object_add(config, "lightIntensity", json_object_new_double(animSettings.lightIntensity));
    json_object_object_add(config, "forwardDecay", json_object_new_double(animSettings.forwardDecay));
    json_object_object_add(config, "forwardFalloffMode", json_object_new_int(animSettings.forwardFalloffMode));
    json_object_object_add(config, "renderQuality", json_object_new_int(animSettings.renderQuality));
    fprintf(file, "%s", json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY));
    fclose(file);
    json_object_put(config);

    printf("✅ Animation config saved successfully.\n");
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
    struct json_object *texture, *color, *opacity, *reflectivity, *roughness, *textureId;

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

    if (json_object_object_get_ex(obj, "textureId", &textureId)) {
        sceneObject->textureId = json_object_get_int(textureId);
    } else {
        sceneObject->textureId = 0;
    }
}

            
void LoadVelocityHandle(struct json_object* pointObj, const char* key, Velocity* handle) {
    struct json_object *velocityObj, *vxObj, *vyObj;  
    if (json_object_object_get_ex(pointObj, key, &velocityObj) &&
        json_object_object_get_ex(velocityObj, "vx", &vxObj) &&
        json_object_object_get_ex(velocityObj, "vy", &vyObj)) {
        handle->vx = json_object_get_int(vxObj);
        handle->vy = json_object_get_int(vyObj);
    } else {
        handle->vx = 0;
        handle->vy = 0;
    }
}

void LoadBezierPath(struct json_object* config) {
    printf("DEBUG: Loading Bézier Path from JSON...\n");

    struct json_object *pathData, *pointsArray;
    if (json_object_object_get_ex(config, "path", &pathData) &&
        json_object_object_get_ex(pathData, "points", &pointsArray)) {

        int numPoints = json_object_array_length(pointsArray);
        if (numPoints < 2) {
            printf("ERROR: Bézier path must have at least two points.\n");
            return;
        }

        sceneSettings.bezierPath.numPoints = numPoints;

        for (int i = 0; i < numPoints; i++) {
            struct json_object *pointObj = json_object_array_get_idx(pointsArray, i);
            struct json_object *xObj, *yObj, *velocity1Obj, *vx1Obj, *vy1Obj, *velocity2Obj, *vx2Obj, *vy2Obj;

            if (json_object_object_get_ex(pointObj, "x", &xObj) &&
                json_object_object_get_ex(pointObj, "y", &yObj)) {
                sceneSettings.bezierPath.points[i].x = json_object_get_double(xObj);
                sceneSettings.bezierPath.points[i].y = json_object_get_double(yObj);
            } else {
                printf("ERROR: Missing x or y coordinate for point %d.\n", i);
                return;
            }

            // Load outgoing handle for segment start (i.e., handles[i][0])
            if (i < sceneSettings.bezierPath.numPoints - 1 &&
                json_object_object_get_ex(pointObj, "velocity1", &velocity1Obj) &&
                json_object_object_get_ex(velocity1Obj, "vx", &vx1Obj) &&
                json_object_object_get_ex(velocity1Obj, "vy", &vy1Obj)) {
                sceneSettings.bezierPath.handles[i][0].vx = json_object_get_double(vx1Obj);
                sceneSettings.bezierPath.handles[i][0].vy = json_object_get_double(vy1Obj);
            } else {
                sceneSettings.bezierPath.handles[i][0].vx = 0;
                sceneSettings.bezierPath.handles[i][0].vy = 0;
            }

            // Load incoming handle for segment end (i.e., handles[i][1])
            if (i > 0 &&
                json_object_object_get_ex(pointObj, "velocity2", &velocity2Obj) &&
                json_object_object_get_ex(velocity2Obj, "vx", &vx2Obj) &&
                json_object_object_get_ex(velocity2Obj, "vy", &vy2Obj)) {
                sceneSettings.bezierPath.handles[i - 1][1].vx = json_object_get_double(vx2Obj);
                sceneSettings.bezierPath.handles[i - 1][1].vy = json_object_get_double(vy2Obj);
            } else if (i > 0) { // Default for midpoints if not found
                sceneSettings.bezierPath.handles[i - 1][1].vx = 0;
                sceneSettings.bezierPath.handles[i - 1][1].vy = 0;
            }
        }

        printf("INFO: Loaded Bézier Path with %d points and %d segments.\n",
               sceneSettings.bezierPath.numPoints, sceneSettings.bezierPath.numPoints - 1);
    } else {
        printf("ERROR: Path data missing in scene_config.json.\n");
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

            struct json_object *type, *x, *y, *scale, *rotation, *radius, *numPoints, *baseShapePoints, 
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
            sceneObj->scale = 1.0;
            sceneObj->rotation = 0.0;
            sceneObj->dirty = true;

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
    FILE *file = fopen(SCENE_CONFIG_FILE, "r");
    if (!file) {
        printf("ERROR: Failed to open scene config file: %s\n", SCENE_CONFIG_FILE);
        return;
    }
     
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);
    
    char *buffer = malloc(fsize + 1);
    if (!buffer) {
        printf("ERROR: Memory allocation failed for JSON buffer.\n");
        fclose(file);
        return;
    }
     
    fread(buffer, 1, fsize, file);
    fclose(file);
    buffer[fsize] = '\0';
    
    //    printf("DEBUG: Loaded JSON Data:\n%s\n", buffer);
    
    struct json_object *config = json_tokener_parse(buffer);
    if (!config) {
        printf("ERROR: Failed to parse scene config JSON.\n");
        free(buffer);
        return;
    }
    printf("In load scene config hold method for  anaimation.c\n");
    LoadWindowConfig(config);
    LoadCameraConfig(config);
    LoadSceneObjects(config);
    LoadBezierPath(config);  

    struct json_object *raysObj;
    if (json_object_object_get_ex(config, "rays", &raysObj)) {
        sceneSettings.rays = json_object_get_int(raysObj);
        printf("Loaded Rays Value: %d\n", sceneSettings.rays);
    } else {
        printf("WARNING: 'rays' key missing in config, using default value.\n");
        sceneSettings.rays = 2000;  //  Default value if not found
    }
    
    json_object_put(config);
    free(buffer);
}

void LoadAnimationConfig(void) {
    FILE* file = fopen(ANIMATION_CONFIG_FILE, "r");
    if (!file) {
        printf("️ Failed to open animation config file: %s\n", ANIMATION_CONFIG_FILE);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);
    
    if (fsize <= 0) {
        printf("️ Warning: Animation config file is empty or invalid.\n");
        fclose(file);
        return;
    }

    char* buffer = malloc(fsize + 1);
    if (!buffer) {
        printf(" Error: Memory allocation failed for animation config buffer.\n");
        fclose(file);
        return;
    }

    fread(buffer, 1, fsize, file);
    fclose(file);
    buffer[fsize] = '\0';

    struct json_object* config = json_tokener_parse(buffer);
    if (!config) {
        printf(" Failed to parse animation config JSON.\n");
        free(buffer);
        return;
    }

    struct json_object* temp;
    
    if (json_object_object_get_ex(config, "interactiveMode", &temp))   
        animSettings.interactiveMode = json_object_get_boolean(temp);   
    if (json_object_object_get_ex(config, "deepRenderMode", &temp)) 
        animSettings.deepRenderMode = json_object_get_boolean(temp);    
    if (json_object_object_get_ex(config, "bounceMode", &temp))     
        animSettings.bounceMode = json_object_get_boolean(temp);        
    if (json_object_object_get_ex(config, "autoMP4", &temp))        
        animSettings.autoMP4 = json_object_get_boolean(temp);          
    if (json_object_object_get_ex(config, "bounceLimit", &temp))   
        animSettings.bounceLimit = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "frameLimit", &temp))
        animSettings.frameLimit = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "framesForTravel", &temp))
        animSettings.framesForTravel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "fps", &temp)) {
        animSettings.fps = json_object_get_int(temp);
        animSettings.frameDuration = (animSettings.fps > 0) ? 1.0 / animSettings.fps : 1.0 / 30.0;
    }
    if (json_object_object_get_ex(config, "loopMode", &temp)) {
        strncpy(animSettings.loopMode, json_object_get_string(temp), sizeof(animSettings.loopMode) - 1);
        animSettings.loopMode[sizeof(animSettings.loopMode) - 1] = '\0';
    }
    if (json_object_object_get_ex(config, "maxLoopCount", &temp))
        animSettings.maxLoopCount = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "frameDir", &temp)) {
        const char* dir = json_object_get_string(temp);
        if (dir) {
            strncpy(animSettings.frameDir, dir, sizeof(animSettings.frameDir) - 1);
            animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
        }
    }
    if (json_object_object_get_ex(config, "lightMode", &temp))
        animSettings.lightMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "blurMode", &temp))
        animSettings.blurMode = json_object_get_int(temp); 
    if (json_object_object_get_ex(config, "lightDiffusionEnabled", &temp))
        animSettings.lightDiffusionEnabled = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "lightDiffusionRadius", &temp))
        animSettings.lightDiffusionRadius = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "lightDiffusionStrength", &temp))
        animSettings.lightDiffusionStrength = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "editorMode", &temp))
        animSettings.editorMode = json_object_get_int(temp); 
    if (json_object_object_get_ex(config, "useTiledRenderer", &temp))
        animSettings.useTiledRenderer = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "tileSize", &temp))
        animSettings.tileSize = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "rouletteThreshold", &temp))
        animSettings.rouletteThreshold = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "integratorMode", &temp))
        animSettings.integratorMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "pathSamplesPerPixel", &temp))
        animSettings.pathSamplesPerPixel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "pathMaxDepth", &temp))
        animSettings.pathMaxDepth = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "pathDirectLighting", &temp))
        animSettings.pathDirectLighting = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "pathRussianRoulette", &temp))
        animSettings.pathRussianRoulette = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "pathEnableMIS", &temp))
        animSettings.pathEnableMIS = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "environmentBrightness", &temp))
        animSettings.environmentBrightness = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "pathSeed", &temp))
        animSettings.pathSeed = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "cacheContributionWeight", &temp))
        animSettings.cacheContributionWeight = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "bsdfModel", &temp))
        animSettings.bsdfModel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "lightIntensity", &temp))
        animSettings.lightIntensity = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "forwardDecay", &temp))
        animSettings.forwardDecay = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "forwardFalloffMode", &temp))
        animSettings.forwardFalloffMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "renderQuality", &temp))
        animSettings.renderQuality = (RenderQuality)json_object_get_int(temp);

    animSettings.cacheContributionWeight = ClampDoubleValue(animSettings.cacheContributionWeight, 0.0, 1.0);
    animSettings.bsdfModel = (animSettings.bsdfModel != 0) ? 1 : 0;
    animSettings.lightIntensity = ClampDoubleValue(animSettings.lightIntensity, 0.0, 20.0);
    if (animSettings.forwardDecay <= 1.0) {
        double legacyDrop = 1.0 - ClampDoubleValue(animSettings.forwardDecay, 0.0, 0.999999);
        double scaleFactor = 1.0 / fmax(legacyDrop, 1e-6);
        animSettings.forwardDecay = DefaultForwardFalloffDistance() * scaleFactor;
    }
    if (animSettings.forwardDecay <= 0.0) {
        animSettings.forwardDecay = DefaultForwardFalloffDistance();
    }
    animSettings.forwardDecay = ClampDoubleValue(animSettings.forwardDecay, 50.0, 100000.0);

    if (animSettings.forwardFalloffMode < FORWARD_FALLOFF_MODE_QUADRATIC || animSettings.forwardFalloffMode > FORWARD_FALLOFF_MODE_NONE) {
        animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    }
    if (animSettings.renderQuality < RENDER_QUALITY_LOW || animSettings.renderQuality > RENDER_QUALITY_HIGH) {
        animSettings.renderQuality = RENDER_QUALITY_MEDIUM;
    }
	
    printf(" Loaded animation config successfully.\n");
    json_object_put(config);
    free(buffer);
}
