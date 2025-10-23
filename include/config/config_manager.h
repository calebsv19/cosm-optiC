#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>
#include <json-c/json.h>
#include "path/path_system.h"  // Ensure Bézier path handling is included
#include "scene/object_manager.h"

#define MAX_SHAPE_POINTS 10  // Supports up to 10 points for custom shapes

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
    int editorMode;
} AnimationConfig;


// **Scene Config Struct**
typedef struct {
    int windowWidth;
    int windowHeight;
    SceneObject sceneObjects[10];  // Stores up to 10 objects
    int objectCount;
    Path bezierPath;  // Stores the Bézier path
    int rays;
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
