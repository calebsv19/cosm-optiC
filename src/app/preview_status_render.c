#include "app/preview_status_render.h"
#include "app/preview_shared_controls.h"
#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "ui/shared_theme_font_adapter.h"
#include <stdio.h>
void PreviewStatusRender(SDL_Renderer *renderer, int width, int panel_y, bool inspection_selected,
                         const PreviewModeRouteDecision *decision,
                         const RayEvaluatedSceneServiceResult *evaluation,
                         PreviewRetainedSceneQuality quality) {
    if (!renderer || !decision) return;
    RayTracingThemePalette palette = {0};
    (void)ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Rect close = PreviewSharedCloseRect(width);
    scene_editor_shared_label(renderer, (SDL_Rect){12,12,close.x-24,34},
                              decision->branchLabel,palette.text_primary,13,0,0);
    char quality_line[128], mesh_line[128];
    snprintf(quality_line,sizeof(quality_line),"Preview quality: %s (Q cycles; wireframe is always retained)",
             PreviewRetainedSceneQualityLabel(quality));
    snprintf(mesh_line,sizeof(mesh_line),"Mesh presentation: %d objects, %d AABB fallback",
             SceneEditorMeshPreviewStoreInstanceCount(),SceneEditorMeshPreviewStoreBoundsFallbackInstanceCount());
    const char *rows[] = {decision->statusLine,
        evaluation ? evaluation->status_line : "", quality_line,mesh_line};
    unsigned count = inspection_selected && panel_y < 300 ? 2 : 4;
    int bottom = inspection_selected ? panel_y - 120 : panel_y;
    int row_height = (bottom - 60) / (int)count;
    if (row_height > 28) row_height = 28;
    if (row_height <= 0) return;
    for (unsigned i=0;i<count;++i)
        scene_editor_shared_label(renderer,(SDL_Rect){12,52+(int)i*row_height,width-24,row_height},
                                  rows[i],palette.text_muted,13,0,0);
}
