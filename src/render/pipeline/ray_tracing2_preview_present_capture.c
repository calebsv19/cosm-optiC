#include "render/pipeline/ray_tracing2_preview_present_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RayTracing2Native3DPresentationCapture* g_native3d_presentation_capture = NULL;
static RayTracing2Native3DPresentationCapture g_native3d_env_presentation_capture;

void RayTracing2PreviewPresent_ResetNative3DPresentationCapture(
    RayTracing2Native3DPresentationCapture* capture) {
    if (!capture) {
        return;
    }
    memset(capture, 0, sizeof(*capture));
}

void RayTracing2PreviewPresent_SetNative3DPresentationCapture(
    RayTracing2Native3DPresentationCapture* capture) {
    g_native3d_presentation_capture = capture;
}

const RayTracing2Native3DPresentationCapture*
RayTracing2PreviewPresent_GetNative3DPresentationCapture(void) {
    return g_native3d_presentation_capture;
}

static const char* Native3DPresentationEventKindLabel(
    RayTracing2Native3DPresentationEventKind kind) {
    switch (kind) {
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_SEED:
            return "history_seed";
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_DIRTY_PROGRESS:
            return "dirty_progress";
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_RESOLVE:
            return "final_resolve";
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_PRESENT:
            return "final_present";
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_PROMOTE:
            return "history_promote";
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_NONE:
        default:
            return "none";
    }
}

static const char* Native3DPresentationCapturePath(void) {
    const char* path = getenv("RAY_TRACING_NATIVE3D_PRESENT_CAPTURE_PATH");
    return (path && path[0]) ? path : NULL;
}

bool BeginNative3DEnvPresentationCapture(void) {
    if (g_native3d_presentation_capture || !Native3DPresentationCapturePath()) {
        return false;
    }
    RayTracing2PreviewPresent_ResetNative3DPresentationCapture(
        &g_native3d_env_presentation_capture);
    RayTracing2PreviewPresent_SetNative3DPresentationCapture(
        &g_native3d_env_presentation_capture);
    return true;
}

void WriteNative3DEnvPresentationCapture(bool enabled,
                                         bool render_ok,
                                         const RuntimeNative3DRenderStats* stats) {
    const char* path = Native3DPresentationCapturePath();
    const RayTracing2Native3DPresentationCapture* capture =
        enabled ? &g_native3d_env_presentation_capture : NULL;
    FILE* f = NULL;

    if (!enabled) {
        return;
    }
    RayTracing2PreviewPresent_SetNative3DPresentationCapture(NULL);
    if (!path || !capture) {
        return;
    }

    f = fopen(path, "w");
    if (!f) {
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema_version\": \"ray_tracing_native3d_present_capture_v1\",\n");
    fprintf(f, "  \"render_ok\": %s,\n", render_ok ? "true" : "false");
    fprintf(f, "  \"event_count\": %d,\n", capture->eventCount);
    fprintf(f, "  \"dropped_event_count\": %d,\n", capture->droppedEventCount);
    fprintf(f, "  \"history_seed_count\": %d,\n", capture->historySeedCount);
    fprintf(f, "  \"dirty_progress_count\": %d,\n", capture->dirtyProgressCount);
    fprintf(f, "  \"final_resolve_count\": %d,\n", capture->finalResolveCount);
    fprintf(f, "  \"final_present_count\": %d,\n", capture->finalPresentCount);
    fprintf(f, "  \"history_promote_count\": %d,\n", capture->historyPromoteCount);
    fprintf(f, "  \"renderer_present_count\": %d,\n", capture->rendererPresentCount);
    fprintf(f, "  \"dirty_after_final_resolve_count\": %d,\n",
            capture->dirtyAfterFinalResolveCount);
    fprintf(f, "  \"final_resolve_before_dirty_count\": %d,\n",
            capture->finalResolveBeforeDirtyCount);
    if (stats) {
        fprintf(f, "  \"stats\": {\n");
        fprintf(f, "    \"temporal_progress_dirty_tile_batches\": %d,\n",
                stats->temporalProgressDirtyBatchCount);
        fprintf(f, "    \"temporal_progress_dirty_tiles\": %d,\n",
                stats->temporalProgressDirtyTileCount);
        fprintf(f, "    \"temporal_dirty_preview_presents\": %d,\n",
                stats->temporalDirtyPreviewPresentCount);
        fprintf(f, "    \"temporal_host_full_resolves\": %d,\n",
                stats->temporalHostFullResolveCount);
        fprintf(f, "    \"temporal_final_preview_presents\": %d,\n",
                stats->temporalFinalPreviewPresentCount);
        fprintf(f, "    \"temporal_history_promotes\": %d,\n",
                stats->temporalHistoryPromoteCount);
        fprintf(f, "    \"temporal_dirty_preview_host_pixels\": %llu,\n",
                (unsigned long long)stats->temporalDirtyPreviewHostPixels);
        fprintf(f, "    \"temporal_dirty_preview_host_bytes\": %llu,\n",
                (unsigned long long)stats->temporalDirtyPreviewHostBytes);
        fprintf(f, "    \"temporal_final_resolve_host_pixels\": %llu,\n",
                (unsigned long long)stats->temporalFinalResolveHostPixels);
        fprintf(f, "    \"temporal_final_resolve_host_bytes\": %llu,\n",
                (unsigned long long)stats->temporalFinalResolveHostBytes);
        fprintf(f, "    \"temporal_history_seed_host_bytes\": %llu,\n",
                (unsigned long long)stats->temporalHistorySeedHostBytes);
        fprintf(f, "    \"temporal_history_promote_host_bytes\": %llu,\n",
                (unsigned long long)stats->temporalHistoryPromoteHostBytes);
        fprintf(f, "    \"temporal_final_preview_present_host_bytes\": %llu\n",
                (unsigned long long)stats->temporalFinalPreviewPresentHostBytes);
        fprintf(f, "  },\n");
    }
    fprintf(f, "  \"events\": [\n");
    for (int i = 0; i < capture->eventCount; ++i) {
        const RayTracing2Native3DPresentationEvent* event = &capture->events[i];
        fprintf(f, "    {");
        fprintf(f, "\"sequence\": %d, ", event->sequence);
        fprintf(f, "\"kind\": \"%s\", ",
                Native3DPresentationEventKindLabel(event->kind));
        fprintf(f, "\"renderer_available\": %s, ",
                event->rendererAvailable ? "true" : "false");
        fprintf(f, "\"render_width\": %d, ", event->renderWidth);
        fprintf(f, "\"render_height\": %d, ", event->renderHeight);
        fprintf(f, "\"host_width\": %d, ", event->hostWidth);
        fprintf(f, "\"host_height\": %d, ", event->hostHeight);
        fprintf(f, "\"dirty_tile_count\": %d, ", event->dirtyTileCount);
        fprintf(f,
                "\"progress\": {\"started_subpasses\": %d, "
                "\"completed_subpasses\": %d, \"total_subpasses\": %d, "
                "\"completed_tiles_in_subpass\": %d, "
                "\"total_tiles_in_subpass\": %d}, ",
                event->startedSubpasses,
                event->completedSubpasses,
                event->totalSubpasses,
                event->completedTilesInSubpass,
                event->totalTilesInSubpass);
        fprintf(f,
                "\"dirty_tile_bounds\": {\"valid\": %s, \"min_x\": %d, "
                "\"min_y\": %d, \"max_x\": %d, \"max_y\": %d}, ",
                event->dirtyTileBoundsValid ? "true" : "false",
                event->dirtyTileMinX,
                event->dirtyTileMinY,
                event->dirtyTileMaxX,
                event->dirtyTileMaxY);
        fprintf(f, "\"rect\": {\"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d}, ",
                event->x,
                event->y,
                event->width,
                event->height);
        fprintf(f, "\"reset_dirty_preview\": %s, ",
                event->resetDirtyPreview ? "true" : "false");
        fprintf(f, "\"success\": %s}", event->success ? "true" : "false");
        fprintf(f, "%s\n", (i + 1 < capture->eventCount) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

void RecordNative3DPresentationEvent(
    RayTracing2Native3DPresentationEventKind kind,
    bool renderer_available,
    int render_width,
    int render_height,
    int host_width,
    int host_height,
    int dirty_tile_count,
    const RuntimeNative3DTileSchedulerProgress* progress,
    const SDL_Rect* rect,
    bool reset_dirty_preview,
    bool success) {
    RayTracing2Native3DPresentationCapture* capture = g_native3d_presentation_capture;
    RayTracing2Native3DPresentationEvent* event = NULL;

    if (!capture) {
        return;
    }
    if (capture->eventCount >= RAY_TRACING2_NATIVE3D_PRESENTATION_CAPTURE_MAX_EVENTS) {
        capture->droppedEventCount += 1;
        return;
    }

    event = &capture->events[capture->eventCount++];
    memset(event, 0, sizeof(*event));
    event->kind = kind;
    event->sequence = capture->nextSequence++;
    event->rendererAvailable = renderer_available ? 1 : 0;
    event->renderWidth = render_width;
    event->renderHeight = render_height;
    event->hostWidth = host_width;
    event->hostHeight = host_height;
    event->dirtyTileCount = dirty_tile_count;
    if (progress) {
        event->startedSubpasses = progress->startedSubpasses;
        event->completedSubpasses = progress->completedSubpasses;
        event->totalSubpasses = progress->totalSubpasses;
        event->completedTilesInSubpass = (int)progress->completedTilesInSubpass;
        event->totalTilesInSubpass = (int)progress->totalTilesInSubpass;
        if (progress->dirtyTiles && progress->dirtyTileCount > 0u) {
            const IntegratorTile* first = &progress->dirtyTiles[0];
            int min_x = first->originX;
            int min_y = first->originY;
            int max_x = first->originX + first->width;
            int max_y = first->originY + first->height;
            for (size_t i = 1u; i < progress->dirtyTileCount; ++i) {
                const IntegratorTile* tile = &progress->dirtyTiles[i];
                const int tile_max_x = tile->originX + tile->width;
                const int tile_max_y = tile->originY + tile->height;
                if (tile->originX < min_x) {
                    min_x = tile->originX;
                }
                if (tile->originY < min_y) {
                    min_y = tile->originY;
                }
                if (tile_max_x > max_x) {
                    max_x = tile_max_x;
                }
                if (tile_max_y > max_y) {
                    max_y = tile_max_y;
                }
            }
            event->dirtyTileBoundsValid = 1;
            event->dirtyTileMinX = min_x;
            event->dirtyTileMinY = min_y;
            event->dirtyTileMaxX = max_x;
            event->dirtyTileMaxY = max_y;
        }
    }
    event->resetDirtyPreview = reset_dirty_preview ? 1 : 0;
    event->success = success ? 1 : 0;
    if (rect) {
        event->x = rect->x;
        event->y = rect->y;
        event->width = rect->w;
        event->height = rect->h;
    } else if (host_width > 0 && host_height > 0) {
        event->x = 0;
        event->y = 0;
        event->width = host_width;
        event->height = host_height;
    }

    switch (kind) {
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_SEED:
            capture->historySeedCount += 1;
            break;
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_DIRTY_PROGRESS:
            capture->dirtyProgressCount += 1;
            capture->lastDirtyTileCount = dirty_tile_count;
            if (rect) {
                capture->lastDirtyHostRect = *rect;
            }
            if (capture->finalResolveCount > 0) {
                capture->dirtyAfterFinalResolveCount += 1;
            }
            break;
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_RESOLVE:
            if (capture->dirtyProgressCount == 0) {
                capture->finalResolveBeforeDirtyCount += 1;
            }
            capture->finalResolveCount += 1;
            capture->finalHostRect.x = 0;
            capture->finalHostRect.y = 0;
            capture->finalHostRect.w = host_width;
            capture->finalHostRect.h = host_height;
            break;
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_PRESENT:
            capture->finalPresentCount += 1;
            if (renderer_available) {
                capture->rendererPresentCount += 1;
            }
            break;
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_PROMOTE:
            capture->historyPromoteCount += 1;
            break;
        case RAY_TRACING2_NATIVE3D_PRESENT_EVENT_NONE:
        default:
            break;
    }
}
