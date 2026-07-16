#include "editor/scene_editor_mesh_preview_shading.h"

#include <math.h>

static Uint8 scene_editor_mesh_preview_shade_channel(double value) {
    if (!isfinite(value) || value <= 0.0) return 0u;
    if (value >= 255.0) return 255u;
    return (Uint8)lround(value);
}

double SceneEditorMeshPreviewShadeFactor(SceneEditorMeshPreviewShadeNormal normal) {
    static const SceneEditorMeshPreviewShadeNormal light = {
        0.3836486121626103,
        -0.4240337281797272,
        -0.8204351898687576
    };
    const double length = sqrt(normal.x * normal.x +
                               normal.y * normal.y +
                               normal.z * normal.z);
    double facing = 0.0;
    if (!(length > 1e-12) || !isfinite(length)) return 0.36;
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    facing = fabs(normal.x * light.x + normal.y * light.y + normal.z * light.z);
    if (!isfinite(facing)) return 0.36;
    if (facing > 1.0) facing = 1.0;
    return 0.36 + 0.64 * facing;
}

SDL_Color SceneEditorMeshPreviewShadeColor(SDL_Color base,
                                           SceneEditorMeshPreviewShadeNormal normal) {
    const double factor = SceneEditorMeshPreviewShadeFactor(normal);
    return (SDL_Color){scene_editor_mesh_preview_shade_channel((double)base.r * factor),
                       scene_editor_mesh_preview_shade_channel((double)base.g * factor),
                       scene_editor_mesh_preview_shade_channel((double)base.b * factor),
                       base.a};
}
