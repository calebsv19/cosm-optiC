#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum SceneEditorMaterialPerfStage {
    SCENE_MATERIAL_PERF_GEOMETRY, SCENE_MATERIAL_PERF_SIGNATURE,
    SCENE_MATERIAL_PERF_BUFFER, SCENE_MATERIAL_PERF_RASTER_SHADE,
    SCENE_MATERIAL_PERF_OUTLINE, SCENE_MATERIAL_PERF_UPLOAD,
    SCENE_MATERIAL_PERF_STAGE_COUNT
} SceneEditorMaterialPerfStage;
typedef struct SceneEditorMaterialPerfSample {
    uint64_t ns[SCENE_MATERIAL_PERF_STAGE_COUNT];
    size_t submitted_triangles, rendered_triangles;
    int width, height, instances;
    bool rasterized, interactive;
} SceneEditorMaterialPerfSample;
/* Opt-in measurement only. Default disabled; settled override is test-only. */
void SceneEditorMaterialPerfEnable(bool enabled, bool force_settled);
bool SceneEditorMaterialPerfForceSettled(void);
uint64_t SceneEditorMaterialPerfNow(void);
void SceneEditorMaterialPerfAdd(SceneEditorMaterialPerfStage stage, uint64_t start);
void SceneEditorMaterialPerfBeginSample(void);
void SceneEditorMaterialPerfFrame(bool rasterized,bool interactive,int width,int height,
                                  size_t submitted,size_t rendered,int instances);
SceneEditorMaterialPerfSample SceneEditorMaterialPerfRead(void);

void SceneEditorMaterialPerfPixels(const unsigned char *pixels,size_t size);
uint64_t SceneEditorMaterialPerfPixelHash(void);
