#include "import/runtime_curve_asset_loader.h"
#include "render/runtime_scene_3d_builder.h"

/*
 * Standalone material/mesh-builder tests do not ingest runtime scene files.
 * Keep their deliberately narrow link surfaces while satisfying the scene
 * builder's optional serialized-curve hook.
 */
const RayTracingRuntimeCurveAssetSet *
ray_tracing_runtime_curve_assets_last(void) {
    return NULL;
}

bool RuntimeScene3DBuilder_AppendCurveAssetSet(
    RuntimeScene3D *scene,
    const RayTracingRuntimeCurveAssetSet *curve_assets) {
    (void)scene;
    return curve_assets == NULL || curve_assets->instance_count == 0;
}
