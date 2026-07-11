#ifndef RENDER_RUNTIME_NATIVE_3D_PREPARED_SCENE_CACHE_INTERNAL_H
#define RENDER_RUNTIME_NATIVE_3D_PREPARED_SCENE_CACHE_INTERNAL_H

#include <stdbool.h>
#include <time.h>

#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_scene_3d.h"

double runtime_native_3d_prepare_elapsed_ms_since(const struct timespec* start_time);
RuntimeNative3DPreparedSceneCacheStats*
runtime_native_3d_prepared_scene_dataflow_stats(void);
bool runtime_native_3d_prepared_scene_build_or_copy_for_frame(
    RuntimeScene3D* scene,
    double normalized_t);
bool runtime_native_3d_prepare_bind_scene_for_frame(const RuntimeScene3D* scene,
                                                    bool final_frame_bind);

#endif
