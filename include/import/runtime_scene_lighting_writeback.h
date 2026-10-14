#ifndef RUNTIME_SCENE_LIGHTING_WRITEBACK_H
#define RUNTIME_SCENE_LIGHTING_WRITEBACK_H
#include <json-c/json.h>
/* Preserve producer properties the legacy runtime overlay cannot reconstruct. */
void RuntimeSceneLightingPreserveOverlay(json_object* scene,json_object* overlay);
#endif
