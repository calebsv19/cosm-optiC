#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_temporal_checkpoint.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app/ray_tracing_checkpoint_transaction.h"
#include "app/ray_tracing_durable_io.h"
#include "import/runtime_mesh_asset_loader.h"

#define RTCK_MAGIC "RTCKPT1"
#define RTCK_MAX_TILES 1048576u

#pragma pack(push, 1)
typedef struct RayTracingCheckpointFileHeaderV1 {
    char magic[8];
    uint32_t schemaVersion;
    uint32_t headerSize;
    uint32_t tileHeaderSize;
    uint32_t adaptivePixelSize;
    uint32_t adaptiveSummarySize;
    int32_t frameIndex;
    int32_t width;
    int32_t height;
    int32_t tileSize;
    int32_t temporalFrames;
    int32_t integratorId;
    int32_t completedSubpasses;
    uint32_t tileCount;
    char requestSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char sceneSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char assetSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char runtimeSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char rendererSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char samplingSha256[RAY_TRACING_SHA256_HEX_SIZE];
} RayTracingCheckpointFileHeaderV1;

typedef struct RayTracingCheckpointFileHeader {
    RayTracingCheckpointFileHeaderV1 base;
    uint64_t generation;
    int32_t activeSubpass;
    uint32_t completedTilesInSubpass;
    uint32_t totalTilesInSubpass;
    uint64_t previousWriteNanoseconds;
} RayTracingCheckpointFileHeader;

typedef struct RayTracingCheckpointTileHeader {
    int32_t startX;
    int32_t startY;
    int32_t endX;
    int32_t endY;
    int32_t committedSubpasses;
    int32_t accumulationCompletedSubpasses;
    int32_t useAdaptiveSampling;
    int32_t maskTileSize;
    int32_t maskTilesX;
    int32_t maskTilesY;
    int32_t maskMinSubpassesBeforePrune;
    int32_t maskActivePixelCount;
    int32_t maskActiveTileCount;
    int32_t maskInactiveTileCount;
    int32_t maskConservativeEligiblePixelCount;
    int32_t maskConservativeBaseActivePixelCount;
    int32_t maskConservativePaddingHoldPixelCount;
    int32_t maskConservativePaddingHighSeedPixelCount;
    int32_t maskConservativePaddingMediumSeedPixelCount;
    int32_t maskConservativeRegionCounts[RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT];
    uint64_t pixelCount;
    uint64_t adaptiveTileCount;
} RayTracingCheckpointTileHeader;
#pragma pack(pop)

typedef struct RayTracingCheckpointGenerationCandidate {
    uint64_t generation;
    char path[PATH_MAX];
} RayTracingCheckpointGenerationCandidate;

static void set_diag(RayTracingTemporalCheckpointSession* session,
                     const char* message) {
    if (!session) return;
    snprintf(session->diagnostics,
             sizeof(session->diagnostics),
             "%s",
             message ? message : "");
}

static bool host_format_supported(void) {
    const uint16_t endian = 1u;
    return sizeof(float) == 4u && *((const uint8_t*)&endian) == 1u;
}

static bool regular_file(const char* path) {
    struct stat status;
    return path && path[0] && lstat(path, &status) == 0 &&
           S_ISREG(status.st_mode);
}

static bool same_digest(const char* lhs, const char* rhs) {
    return lhs && rhs && strcmp(lhs, rhs) == 0;
}

static bool write_exact(FILE* file, const void* bytes, size_t size) {
    return size == 0u || (file && bytes && fwrite(bytes, 1u, size, file) == size);
}

static bool read_exact(FILE* file, void* bytes, size_t size) {
    return size == 0u || (file && bytes && fread(bytes, 1u, size, file) == size);
}

static bool tile_pixel_count(const RuntimeNative3DRenderUnit* unit,
                             size_t* out_pixels,
                             size_t* out_radiance) {
    size_t pixels = 0u;
    if (!unit || !out_pixels || !out_radiance || unit->width <= 0 ||
        unit->height <= 0 ||
        (size_t)unit->width > SIZE_MAX / (size_t)unit->height) {
        return false;
    }
    pixels = (size_t)unit->width * (size_t)unit->height;
    if (pixels > SIZE_MAX / RUNTIME_NATIVE_3D_RADIANCE_CHANNELS) return false;
    *out_pixels = pixels;
    *out_radiance = pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    return true;
}

void ray_tracing_temporal_checkpoint_session_init(
    RayTracingTemporalCheckpointSession* session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->testExitAfterSubpass = -1;
    set_diag(session, "disabled");
}

static bool digest_assets(const char* const* resolved_asset_paths,
                          size_t resolved_asset_path_count,
                          char out_digest[RAY_TRACING_SHA256_HEX_SIZE]) {
    const RayTracingRuntimeMeshAssetSet* assets =
        ray_tracing_runtime_mesh_assets_last();
    char material[(RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS + 10) *
                  RAY_TRACING_SHA256_HEX_SIZE];
    size_t used = 0u;
    char digest[RAY_TRACING_SHA256_HEX_SIZE];
    memset(material, 0, sizeof(material));
    if (assets) {
        for (int i = 0; i < assets->asset_count; ++i) {
            if (!ray_tracing_sha256_file(assets->assets[i].path, digest) ||
                used + strlen(digest) + 1u >= sizeof(material)) {
                return false;
            }
            used += (size_t)snprintf(material + used,
                                     sizeof(material) - used,
                                     "%s\n",
                                     digest);
        }
    }
    for (size_t i = 0u; i < resolved_asset_path_count; ++i) {
        const char* path = resolved_asset_paths ? resolved_asset_paths[i] : NULL;
        if (!path || !path[0] || !regular_file(path)) continue;
        if (!ray_tracing_sha256_file(path, digest) ||
            used + strlen(digest) + 1u >= sizeof(material)) {
            return false;
        }
        used += (size_t)snprintf(material + used,
                                 sizeof(material) - used,
                                 "%s\n",
                                 digest);
    }
    if (used == 0u) {
        memcpy(material, "ray-tracing-assets:none", 23u);
        used = 23u;
    }
    return ray_tracing_sha256_bytes(material, used, out_digest);
}

bool ray_tracing_temporal_checkpoint_configure_frame(
    RayTracingTemporalCheckpointSession* session,
    const RayTracingAgentRenderRequest* request,
    const char* request_path,
    int frame_index,
    int integrator_id,
    int effective_tile_size,
    const char* const* resolved_asset_paths,
    size_t resolved_asset_path_count) {
    const char* runtime_digest = getenv("RAY_TRACING_WORKER_RUNTIME_SHA256");
    const char* renderer_digest = getenv("RAY_TRACING_RENDERER_BUILD_SHA256");
    char sampling_identity[256];
    RayTracingAgentRenderRequest semantic_request;
    int sampling_size = 0;
    if (!session || !request || !request_path) return false;
    ray_tracing_temporal_checkpoint_session_init(session);
    session->enabled = request->checkpoint_enabled;
    session->resumeRequested = request->checkpoint_resume;
    if (!session->enabled) return true;
    if (snprintf(session->root,
                 sizeof(session->root),
                 "%s",
                 request->checkpoint_root) >= (int)sizeof(session->root) ||
        !runtime_digest || !renderer_digest ||
        !ray_tracing_sha256_is_valid_hex(runtime_digest) ||
        !ray_tracing_sha256_is_valid_hex(renderer_digest) ||
        !ray_tracing_sha256_file(request->runtime_scene_path,
                                 session->identity.sceneSha256) ||
        !digest_assets(resolved_asset_paths,
                       resolved_asset_path_count,
                       session->identity.assetSha256)) {
        set_diag(session, "failed to build checkpoint compatibility identity");
        return false;
    }
    semantic_request = *request;
    if (semantic_request.has_sampling_window) {
        semantic_request.start_frame -= semantic_request.sampling_frame_offset;
        semantic_request.frame_count = semantic_request.sampling_frame_count;
        semantic_request.has_sampling_window = false;
        semantic_request.sampling_frame_offset = 0;
        semantic_request.sampling_frame_count = 1;
    }
    semantic_request.checkpoint_resume = false;
    semantic_request.overwrite = false;
    memset(semantic_request.summary_path, 0, sizeof(semantic_request.summary_path));
    memset(semantic_request.progress_path, 0, sizeof(semantic_request.progress_path));
    if (!ray_tracing_sha256_bytes(&semantic_request,
                                  sizeof(semantic_request),
                                  session->identity.requestSha256)) {
        set_diag(session, "failed to digest normalized render request");
        return false;
    }
    snprintf(session->identity.runtimeSha256,
             sizeof(session->identity.runtimeSha256),
             "%s",
             runtime_digest);
    snprintf(session->identity.rendererSha256,
             sizeof(session->identity.rendererSha256),
             "%s",
             renderer_digest);
    session->identity.frameIndex = frame_index;
    session->identity.width = request->width;
    session->identity.height = request->height;
    session->identity.tileSize = effective_tile_size;
    session->identity.temporalFrames = request->temporal_frames;
    session->identity.integratorId = integrator_id;
    sampling_size = snprintf(sampling_identity,
                             sizeof(sampling_identity),
                             "frame=%d;temporal=%d;integrator=%d;request=%s",
                             frame_index,
                             request->temporal_frames,
                             integrator_id,
                             session->identity.requestSha256);
    if (sampling_size <= 0 || sampling_size >= (int)sizeof(sampling_identity) ||
        !ray_tracing_sha256_bytes(sampling_identity,
                                  (size_t)sampling_size,
                                  session->identity.samplingSha256)) {
        set_diag(session, "failed to build deterministic sampling identity");
        return false;
    }
    {
        const char* test_exit = getenv("RAY_TRACING_TEST_EXIT_AFTER_CHECKPOINT_SUBPASS");
        if (test_exit && test_exit[0]) {
            char* end = NULL;
            long value = strtol(test_exit, &end, 10);
            if (end && *end == '\0' && value > 0 && value <= INT32_MAX) {
                session->testExitAfterSubpass = (int)value;
            }
        }
    }
    set_diag(session, "checkpoint configured");
    return ray_tracing_temporal_checkpoint_identity_validate(&session->identity,
                                                              session->diagnostics,
                                                              sizeof(session->diagnostics));
}

bool ray_tracing_temporal_checkpoint_identity_validate(
    const RayTracingTemporalCheckpointIdentity* identity,
    char* diagnostics,
    size_t diagnostics_size) {
    bool ok = identity && identity->frameIndex >= 0 && identity->width > 0 &&
              identity->height > 0 && identity->tileSize > 0 &&
              identity->temporalFrames > 0 && identity->integratorId >= 0 &&
              ray_tracing_sha256_is_valid_hex(identity->requestSha256) &&
              ray_tracing_sha256_is_valid_hex(identity->sceneSha256) &&
              ray_tracing_sha256_is_valid_hex(identity->assetSha256) &&
              ray_tracing_sha256_is_valid_hex(identity->runtimeSha256) &&
              ray_tracing_sha256_is_valid_hex(identity->rendererSha256) &&
              ray_tracing_sha256_is_valid_hex(identity->samplingSha256);
    if (diagnostics && diagnostics_size > 0u) {
        snprintf(diagnostics, diagnostics_size, "%s", ok ? "ok" : "invalid checkpoint identity");
    }
    return ok;
}

static void fill_header(RayTracingCheckpointFileHeader* header,
                        const RayTracingTemporalCheckpointSession* session,
                        size_t tile_count,
                        int completed_subpasses,
                        int active_subpass,
                        size_t completed_tiles_in_subpass,
                        size_t total_tiles_in_subpass,
                        uint64_t generation) {
    const RayTracingTemporalCheckpointIdentity* id = &session->identity;
    RayTracingCheckpointFileHeaderV1* base = &header->base;
    memset(header, 0, sizeof(*header));
    memcpy(base->magic, RTCK_MAGIC, sizeof(RTCK_MAGIC));
    base->schemaVersion = RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION;
    base->headerSize = (uint32_t)sizeof(*header);
    base->tileHeaderSize = (uint32_t)sizeof(RayTracingCheckpointTileHeader);
    base->adaptivePixelSize = (uint32_t)sizeof(RuntimeNative3DAdaptivePixelState);
    base->adaptiveSummarySize =
        (uint32_t)sizeof(RuntimeNative3DAdaptivePixelStateSummary);
    base->frameIndex = id->frameIndex;
    base->width = id->width;
    base->height = id->height;
    base->tileSize = id->tileSize;
    base->temporalFrames = id->temporalFrames;
    base->integratorId = id->integratorId;
    base->completedSubpasses = completed_subpasses;
    base->tileCount = (uint32_t)tile_count;
    memcpy(base->requestSha256, id->requestSha256, sizeof(base->requestSha256));
    memcpy(base->sceneSha256, id->sceneSha256, sizeof(base->sceneSha256));
    memcpy(base->assetSha256, id->assetSha256, sizeof(base->assetSha256));
    memcpy(base->runtimeSha256, id->runtimeSha256, sizeof(base->runtimeSha256));
    memcpy(base->rendererSha256, id->rendererSha256, sizeof(base->rendererSha256));
    memcpy(base->samplingSha256, id->samplingSha256, sizeof(base->samplingSha256));
    header->generation = generation;
    header->activeSubpass = active_subpass;
    header->completedTilesInSubpass = (uint32_t)completed_tiles_in_subpass;
    header->totalTilesInSubpass = (uint32_t)total_tiles_in_subpass;
    header->previousWriteNanoseconds = session->lastWriteNanoseconds;
}

static bool header_matches(const RayTracingCheckpointFileHeader* header,
                           const RayTracingTemporalCheckpointSession* session,
                           size_t tile_count,
                           int temporal_frames) {
    const RayTracingTemporalCheckpointIdentity* id = &session->identity;
    const RayTracingCheckpointFileHeaderV1* base = header ? &header->base : NULL;
    return base && memcmp(base->magic, RTCK_MAGIC, sizeof(RTCK_MAGIC)) == 0 &&
           base->schemaVersion == RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION &&
           base->headerSize == sizeof(*header) &&
           base->tileHeaderSize == sizeof(RayTracingCheckpointTileHeader) &&
           base->adaptivePixelSize == sizeof(RuntimeNative3DAdaptivePixelState) &&
           base->adaptiveSummarySize ==
               sizeof(RuntimeNative3DAdaptivePixelStateSummary) &&
           base->frameIndex == id->frameIndex && base->width == id->width &&
           base->height == id->height && base->tileSize == id->tileSize &&
           base->temporalFrames == temporal_frames &&
           base->temporalFrames == id->temporalFrames &&
           base->integratorId == id->integratorId &&
           base->completedSubpasses >= 0 &&
           base->completedSubpasses <= temporal_frames &&
           base->tileCount == tile_count && header->generation > 0u &&
           header->activeSubpass < temporal_frames &&
           ((header->activeSubpass == base->completedSubpasses) ||
            (base->completedSubpasses > 0 &&
             header->activeSubpass + 1 == base->completedSubpasses &&
             header->completedTilesInSubpass ==
                 header->totalTilesInSubpass)) &&
           header->completedTilesInSubpass <= header->totalTilesInSubpass &&
           same_digest(base->requestSha256, id->requestSha256) &&
           same_digest(base->sceneSha256, id->sceneSha256) &&
           same_digest(base->assetSha256, id->assetSha256) &&
           same_digest(base->runtimeSha256, id->runtimeSha256) &&
           same_digest(base->rendererSha256, id->rendererSha256) &&
           same_digest(base->samplingSha256, id->samplingSha256);
}

static RuntimeNative3DRenderUnit* find_tile(
    RuntimeNative3DCheckpointTile* tiles,
    size_t tile_count,
    const RayTracingCheckpointTileHeader* header) {
    for (size_t i = 0u; i < tile_count; ++i) {
        RuntimeNative3DRenderUnit* unit = tiles[i].renderUnit;
        if (unit && tiles[i].tile.originX == header->startX &&
            tiles[i].tile.originY == header->startY &&
            tiles[i].tile.originX + tiles[i].tile.width == header->endX &&
            tiles[i].tile.originY + tiles[i].tile.height == header->endY) {
            return unit;
        }
    }
    return NULL;
}

static bool write_tile(FILE* file,
                       const RuntimeNative3DCheckpointTile* tile,
                       int temporal_frames) {
    RayTracingCheckpointTileHeader header;
    RuntimeNative3DRenderUnit* unit = tile ? tile->renderUnit : NULL;
    size_t pixels = 0u;
    size_t radiance = 0u;
    size_t adaptive_tiles = 0u;
    if (!unit || !tile_pixel_count(unit, &pixels, &radiance) ||
        unit->committedSubpasses < 0 ||
        unit->committedSubpasses > temporal_frames ||
        unit->accumulation.completedSubpasses != unit->committedSubpasses) {
        return false;
    }
    if (unit->useAdaptiveSampling) {
        if (!unit->adaptiveMask.stableEmitterMask ||
            !unit->adaptiveMask.activeSampleMask ||
            !unit->adaptiveMask.scratchSampleMask ||
            !unit->adaptiveMask.activeTileMask ||
            !unit->adaptivePixelState.pixels ||
            unit->adaptiveMask.tilesX <= 0 || unit->adaptiveMask.tilesY <= 0) {
            return false;
        }
        adaptive_tiles = (size_t)unit->adaptiveMask.tilesX *
                         (size_t)unit->adaptiveMask.tilesY;
    }
    memset(&header, 0, sizeof(header));
    header.startX = tile->tile.originX;
    header.startY = tile->tile.originY;
    header.endX = tile->tile.originX + tile->tile.width;
    header.endY = tile->tile.originY + tile->tile.height;
    header.committedSubpasses = unit->committedSubpasses;
    header.accumulationCompletedSubpasses = unit->accumulation.completedSubpasses;
    header.useAdaptiveSampling = unit->useAdaptiveSampling ? 1 : 0;
    header.maskTileSize = unit->adaptiveMask.tileSize;
    header.maskTilesX = unit->adaptiveMask.tilesX;
    header.maskTilesY = unit->adaptiveMask.tilesY;
    header.maskMinSubpassesBeforePrune = unit->adaptiveMask.minSubpassesBeforePrune;
    header.maskActivePixelCount = unit->adaptiveMask.activePixelCount;
    header.maskActiveTileCount = unit->adaptiveMask.activeTileCount;
    header.maskInactiveTileCount = unit->adaptiveMask.inactiveTileCount;
    header.maskConservativeEligiblePixelCount =
        unit->adaptiveMask.conservativeEarlyStopEligiblePixelCount;
    header.maskConservativeBaseActivePixelCount =
        unit->adaptiveMask.conservativeEarlyStopBaseActivePixelCount;
    header.maskConservativePaddingHoldPixelCount =
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldPixelCount;
    header.maskConservativePaddingHighSeedPixelCount =
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldHighSeedPixelCount;
    header.maskConservativePaddingMediumSeedPixelCount =
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldMediumSeedPixelCount;
    memcpy(header.maskConservativeRegionCounts,
           unit->adaptiveMask.conservativeEarlyStopPaddingHoldRegionCounts,
           sizeof(header.maskConservativeRegionCounts));
    header.pixelCount = pixels;
    header.adaptiveTileCount = adaptive_tiles;
    return write_exact(file, &header, sizeof(header)) &&
           write_exact(file,
                       unit->accumulation.accumulationBuffer,
                       radiance * sizeof(float)) &&
           write_exact(file,
                       unit->accumulation.activityBuffer,
                       pixels * sizeof(float)) &&
           write_exact(file,
                       unit->accumulation.sampleCountBuffer,
                       pixels * sizeof(uint16_t)) &&
           (!unit->useAdaptiveSampling ||
            (write_exact(file, unit->adaptiveMask.stableEmitterMask, pixels) &&
             write_exact(file, unit->adaptiveMask.activeSampleMask, pixels) &&
             write_exact(file, unit->adaptiveMask.scratchSampleMask, pixels) &&
             write_exact(file, unit->adaptiveMask.activeTileMask, adaptive_tiles) &&
             write_exact(file,
                         unit->adaptivePixelState.pixels,
                         pixels * sizeof(RuntimeNative3DAdaptivePixelState)) &&
             write_exact(file,
                         &unit->adaptivePixelState.summary,
                         sizeof(unit->adaptivePixelState.summary))));
}

static bool read_tile(FILE* file,
                      RuntimeNative3DCheckpointTile* tiles,
                      size_t tile_count,
                      int temporal_frames) {
    RayTracingCheckpointTileHeader header;
    RuntimeNative3DRenderUnit* unit = NULL;
    size_t pixels = 0u;
    size_t radiance = 0u;
    size_t expected_adaptive_tiles = 0u;
    if (!read_exact(file, &header, sizeof(header))) return false;
    unit = find_tile(tiles, tile_count, &header);
    if (!unit || !tile_pixel_count(unit, &pixels, &radiance) ||
        header.pixelCount != pixels ||
        header.committedSubpasses < 0 ||
        header.committedSubpasses > temporal_frames ||
        header.accumulationCompletedSubpasses != header.committedSubpasses ||
        header.useAdaptiveSampling != (unit->useAdaptiveSampling ? 1 : 0)) {
        return false;
    }
    if (!read_exact(file,
                    unit->accumulation.accumulationBuffer,
                    radiance * sizeof(float)) ||
        !read_exact(file,
                    unit->accumulation.activityBuffer,
                    pixels * sizeof(float)) ||
        !read_exact(file,
                    unit->accumulation.sampleCountBuffer,
                    pixels * sizeof(uint16_t))) {
        return false;
    }
    if (unit->useAdaptiveSampling) {
        if (!RuntimeNative3DAdaptiveSamplingMask_Ensure(&unit->adaptiveMask,
                                                        unit->width,
                                                        unit->height) ||
            !RuntimeNative3DAdaptivePixelStateBuffer_Ensure(&unit->adaptivePixelState,
                                                            unit->width,
                                                            unit->height)) {
            return false;
        }
        expected_adaptive_tiles = (size_t)unit->adaptiveMask.tilesX *
                                  (size_t)unit->adaptiveMask.tilesY;
        if (header.maskTileSize != unit->adaptiveMask.tileSize ||
            header.maskTilesX != unit->adaptiveMask.tilesX ||
            header.maskTilesY != unit->adaptiveMask.tilesY ||
            header.adaptiveTileCount != expected_adaptive_tiles ||
            !read_exact(file, unit->adaptiveMask.stableEmitterMask, pixels) ||
            !read_exact(file, unit->adaptiveMask.activeSampleMask, pixels) ||
            !read_exact(file, unit->adaptiveMask.scratchSampleMask, pixels) ||
            !read_exact(file,
                        unit->adaptiveMask.activeTileMask,
                        expected_adaptive_tiles) ||
            !read_exact(file,
                        unit->adaptivePixelState.pixels,
                        pixels * sizeof(RuntimeNative3DAdaptivePixelState)) ||
            !read_exact(file,
                        &unit->adaptivePixelState.summary,
                        sizeof(unit->adaptivePixelState.summary))) {
            return false;
        }
        unit->adaptiveMask.minSubpassesBeforePrune =
            header.maskMinSubpassesBeforePrune;
        unit->adaptiveMask.activePixelCount = header.maskActivePixelCount;
        unit->adaptiveMask.activeTileCount = header.maskActiveTileCount;
        unit->adaptiveMask.inactiveTileCount = header.maskInactiveTileCount;
        unit->adaptiveMask.conservativeEarlyStopEligiblePixelCount =
            header.maskConservativeEligiblePixelCount;
        unit->adaptiveMask.conservativeEarlyStopBaseActivePixelCount =
            header.maskConservativeBaseActivePixelCount;
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldPixelCount =
            header.maskConservativePaddingHoldPixelCount;
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldHighSeedPixelCount =
            header.maskConservativePaddingHighSeedPixelCount;
        unit->adaptiveMask.conservativeEarlyStopPaddingHoldMediumSeedPixelCount =
            header.maskConservativePaddingMediumSeedPixelCount;
        memcpy(unit->adaptiveMask.conservativeEarlyStopPaddingHoldRegionCounts,
               header.maskConservativeRegionCounts,
               sizeof(header.maskConservativeRegionCounts));
    }
    unit->committedSubpasses = header.committedSubpasses;
    unit->accumulation.completedSubpasses =
        header.accumulationCompletedSubpasses;
    unit->featuresPrepared = false;
    return true;
}

static bool load_generation(RayTracingTemporalCheckpointSession* session,
                            const char* path,
                            RuntimeNative3DCheckpointTile* tiles,
                            size_t tile_count,
                            int temporal_frames,
                            int* out_completed_subpasses) {
    RayTracingCheckpointFileHeader header;
    FILE* file = NULL;
    bool ok = false;
    if (!regular_file(path)) return false;
    file = fopen(path, "rb");
    if (!file) return false;
    if (!read_exact(file, &header, sizeof(header)) ||
        !header_matches(&header, session, tile_count, temporal_frames)) {
        goto done;
    }
    for (size_t i = 0u; i < tile_count; ++i) {
        if (!read_tile(file, tiles, tile_count, temporal_frames)) goto done;
    }
    if (fgetc(file) != EOF || ferror(file)) goto done;
    *out_completed_subpasses = header.base.completedSubpasses;
    ok = true;
done:
    fclose(file);
    if (ok) {
        snprintf(session->latestPath, sizeof(session->latestPath), "%s", path);
        (void)ray_tracing_sha256_file(path, session->latestSha256);
        session->nextGeneration = header.generation + 1u;
        session->resumedActiveSubpass = header.activeSubpass;
        session->resumedTilesInSubpass = header.completedTilesInSubpass;
    }
    return ok;
}

static bool generation_header_is_complete(const char* path, int temporal_frames) {
    RayTracingCheckpointFileHeader header;
    FILE* file = NULL;
    bool complete = false;
    if (!regular_file(path)) return false;
    file = fopen(path, "rb");
    if (!file) return false;
    complete = read_exact(file, &header, sizeof(header)) &&
               memcmp(header.base.magic,
                      RTCK_MAGIC,
                      sizeof(header.base.magic)) == 0 &&
               header.base.schemaVersion ==
                   RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION &&
               header.base.temporalFrames == temporal_frames &&
               header.base.completedSubpasses == temporal_frames;
    fclose(file);
    return complete;
}

static int compare_generation_descending(const void* lhs, const void* rhs) {
    const RayTracingCheckpointGenerationCandidate* left =
        (const RayTracingCheckpointGenerationCandidate*)lhs;
    const RayTracingCheckpointGenerationCandidate* right =
        (const RayTracingCheckpointGenerationCandidate*)rhs;
    if (left->generation < right->generation) return 1;
    if (left->generation > right->generation) return -1;
    return 0;
}

static size_t scan_generations(
    const char* root,
    int frame_index,
    RayTracingCheckpointGenerationCandidate* candidates,
    size_t capacity) {
    char directory_path[PATH_MAX];
    DIR* directory = NULL;
    struct dirent* entry = NULL;
    size_t count = 0u;
    if (!root || !candidates || capacity == 0u ||
        snprintf(directory_path,
                 sizeof(directory_path),
                 "%s/frame_%04d",
                 root,
                 frame_index) >= (int)sizeof(directory_path)) {
        return 0u;
    }
    directory = opendir(directory_path);
    if (!directory) return 0u;
    while ((entry = readdir(directory)) != NULL && count < capacity) {
        unsigned long long generation = 0u;
        int consumed = 0;
        if (sscanf(entry->d_name,
                   "generation_%20llu.rtck%n",
                   &generation,
                   &consumed) != 1 ||
            consumed <= 0 || entry->d_name[consumed] != '\0' ||
            generation == 0u ||
            snprintf(candidates[count].path,
                     sizeof(candidates[count].path),
                     "%s/%s",
                     directory_path,
                     entry->d_name) >=
                (int)sizeof(candidates[count].path) ||
            !regular_file(candidates[count].path)) {
            continue;
        }
        candidates[count].generation = (uint64_t)generation;
        count += 1u;
    }
    closedir(directory);
    if (count > 1u) {
        qsort(candidates,
              count,
              sizeof(*candidates),
              compare_generation_descending);
    }
    return count;
}

bool ray_tracing_temporal_checkpoint_restore(
    RuntimeNative3DCheckpointTile* tiles,
    size_t tile_count,
    int temporal_frames,
    int* out_completed_subpasses,
    void* user_data) {
    RayTracingTemporalCheckpointSession* session =
        (RayTracingTemporalCheckpointSession*)user_data;
    RayTracingCheckpointGenerationCandidate candidates[1024];
    size_t candidate_count = 0u;
    bool any_generation_found = false;
    bool complete_generation_found = false;
    if (!session || !out_completed_subpasses || !tiles || tile_count == 0u ||
        tile_count > RTCK_MAX_TILES || !host_format_supported()) {
        return false;
    }
    *out_completed_subpasses = 0;
    if (!session->enabled || !session->resumeRequested) {
        set_diag(session, session->enabled ? "fresh checkpoint run" : "disabled");
        return true;
    }
    candidate_count = scan_generations(session->root,
                                       session->identity.frameIndex,
                                       candidates,
                                       sizeof(candidates) /
                                           sizeof(candidates[0]));
    any_generation_found = candidate_count > 0u;
    for (size_t i = 0u; i < candidate_count; ++i) {
        complete_generation_found =
            complete_generation_found ||
            generation_header_is_complete(candidates[i].path, temporal_frames);
        if (load_generation(session,
                            candidates[i].path,
                            tiles,
                            tile_count,
                            temporal_frames,
                            out_completed_subpasses)) {
            session->resumed = true;
            session->resumedSubpasses = *out_completed_subpasses;
            set_diag(session, "tile-batch checkpoint restored");
            return true;
        }
    }
    if (!any_generation_found) {
        set_diag(session, "resume requested; no checkpoint exists for this frame");
        return true;
    }
    if (complete_generation_found) {
        set_diag(session, "stale completed-frame checkpoint ignored for clean rerender");
        return true;
    }
    set_diag(session, "no compatible checkpoint generation found");
    return false;
}

static bool write_current_pointer(RayTracingTemporalCheckpointSession* session,
                                  uint64_t generation,
                                  int completed_subpasses,
                                  int active_subpass,
                                  size_t completed_tiles_in_subpass,
                                  size_t total_tiles_in_subpass,
                                  const char* generation_sha256,
                                  const RayTracingCheckpointGenerationCandidate*
                                      previous) {
    char pointer_path[PATH_MAX];
    char generation_name[64];
    char previous_digest[RAY_TRACING_SHA256_HEX_SIZE] = "";
    const char* previous_name = "";
    RayTracingDurableOutput output;
    if (snprintf(pointer_path,
                 sizeof(pointer_path),
                 "%s/frame_%04d/current.json",
                 session->root,
                 session->identity.frameIndex) >= (int)sizeof(pointer_path) ||
        snprintf(generation_name,
                 sizeof(generation_name),
                 "generation_%020llu.rtck",
                 (unsigned long long)generation) >=
            (int)sizeof(generation_name) ||
        !ray_tracing_durable_output_begin(&output, pointer_path)) {
        return false;
    }
    if (previous) {
        const char* separator = strrchr(previous->path, '/');
        previous_name = separator ? separator + 1 : previous->path;
        if (!ray_tracing_sha256_file(previous->path, previous_digest)) {
            ray_tracing_durable_output_abort(&output);
            return false;
        }
    }
    fprintf(output.stream,
            "{\n"
            "  \"schema\": \"ray_tracing_temporal_checkpoint_pointer_v2\",\n"
            "  \"checkpoint_schema_version\": %d,\n"
            "  \"frame_index\": %d,\n"
            "  \"generation\": %llu,\n"
            "  \"completed_subpasses\": %d,\n"
            "  \"active_subpass\": %d,\n"
            "  \"completed_tiles_in_subpass\": %zu,\n"
            "  \"total_tiles_in_subpass\": %zu,\n"
            "  \"path\": \"%s\",\n"
            "  \"sha256\": \"%s\",\n"
            "  \"previous_path\": \"%s\",\n"
            "  \"previous_sha256\": \"%s\"\n"
            "}\n",
            RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION,
            session->identity.frameIndex,
            (unsigned long long)generation,
            completed_subpasses,
            active_subpass,
            completed_tiles_in_subpass,
            total_tiles_in_subpass,
            generation_name,
            generation_sha256,
            previous_name,
            previous_digest);
    if (!ray_tracing_durable_output_commit(&output)) {
        ray_tracing_durable_output_abort(&output);
        return false;
    }
    return true;
}

static void retain_two_generations(RayTracingTemporalCheckpointSession* session) {
    RayTracingCheckpointGenerationCandidate candidates[1024];
    size_t count = scan_generations(session->root,
                                    session->identity.frameIndex,
                                    candidates,
                                    sizeof(candidates) / sizeof(candidates[0]));
    for (size_t i = 2u; i < count; ++i) {
        (void)unlink(candidates[i].path);
    }
}

bool ray_tracing_temporal_checkpoint_commit(
    const RuntimeNative3DCheckpointTile* tiles,
    size_t tile_count,
    int completed_subpasses,
    int active_subpass,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    int temporal_frames,
    void* user_data) {
    RayTracingTemporalCheckpointSession* session =
        (RayTracingTemporalCheckpointSession*)user_data;
    RayTracingCheckpointFileHeader header;
    RayTracingCheckpointTransaction transaction;
    RayTracingCheckpointGenerationCandidate candidates[1024];
    size_t candidate_count = 0u;
    char path[PATH_MAX];
    char digest[RAY_TRACING_SHA256_HEX_SIZE];
    struct timespec started = {0};
    struct timespec finished = {0};
    uint64_t elapsed_ns = 0u;
    uint64_t generation = 0u;
    if (!session || !session->enabled || !tiles || tile_count == 0u ||
        tile_count > RTCK_MAX_TILES || completed_subpasses < 0 ||
        completed_subpasses > temporal_frames || active_subpass < 0 ||
        active_subpass >= temporal_frames ||
        completed_tiles_in_subpass > total_tiles_in_subpass ||
        !host_format_supported() ||
        !ray_tracing_temporal_checkpoint_identity_validate(&session->identity,
                                                            NULL,
                                                            0u)) {
        set_diag(session, "checkpoint generation begin failed");
        return false;
    }
    candidate_count = scan_generations(session->root,
                                       session->identity.frameIndex,
                                       candidates,
                                       sizeof(candidates) /
                                           sizeof(candidates[0]));
    generation = session->nextGeneration;
    if (generation == 0u) {
        generation = candidate_count > 0u ? candidates[0].generation + 1u : 1u;
    }
    if (snprintf(path,
                 sizeof(path),
                 "%s/frame_%04d/generation_%020llu.rtck",
                 session->root,
                 session->identity.frameIndex,
                 (unsigned long long)generation) >= (int)sizeof(path) ||
        clock_gettime(CLOCK_MONOTONIC, &started) != 0 ||
        !ray_tracing_checkpoint_transaction_begin(&transaction,
                                                  path,
                                                  generation)) {
        set_diag(session, "checkpoint generation begin failed");
        return false;
    }
    fill_header(&header,
                session,
                tile_count,
                completed_subpasses,
                active_subpass,
                completed_tiles_in_subpass,
                total_tiles_in_subpass,
                generation);
    if (!write_exact(transaction.stream, &header, sizeof(header))) {
        ray_tracing_checkpoint_transaction_abort(&transaction);
        set_diag(session, "checkpoint header write failed");
        return false;
    }
    ray_tracing_checkpoint_transaction_reached_temporary_write(&transaction);
    for (size_t i = 0u; i < tile_count; ++i) {
        if (!write_tile(transaction.stream, &tiles[i], temporal_frames)) {
            ray_tracing_checkpoint_transaction_abort(&transaction);
            set_diag(session, "checkpoint tile write failed");
            return false;
        }
    }
    if (!ray_tracing_checkpoint_transaction_commit(&transaction) ||
        !ray_tracing_sha256_file(path, digest)) {
        set_diag(session, "checkpoint generation commit failed");
        return false;
    }
    candidate_count = scan_generations(session->root,
                                       session->identity.frameIndex,
                                       candidates,
                                       sizeof(candidates) /
                                           sizeof(candidates[0]));
    if (!write_current_pointer(session,
                               generation,
                               completed_subpasses,
                               active_subpass,
                               completed_tiles_in_subpass,
                               total_tiles_in_subpass,
                               digest,
                               candidate_count > 1u ? &candidates[1] : NULL)) {
        set_diag(session, "checkpoint pointer commit failed");
        return false;
    }
    retain_two_generations(session);
    if (clock_gettime(CLOCK_MONOTONIC, &finished) == 0) {
        int64_t elapsed_signed =
            (int64_t)(finished.tv_sec - started.tv_sec) * INT64_C(1000000000) +
            (int64_t)(finished.tv_nsec - started.tv_nsec);
        if (elapsed_signed > 0) elapsed_ns = (uint64_t)elapsed_signed;
    }
    snprintf(session->latestPath, sizeof(session->latestPath), "%s", path);
    snprintf(session->latestSha256, sizeof(session->latestSha256), "%s", digest);
    session->checkpointsWritten += 1;
    if (completed_tiles_in_subpass < total_tiles_in_subpass) {
        session->tileBatchCheckpointsWritten += 1;
    }
    session->lastWriteNanoseconds = elapsed_ns;
    session->totalWriteNanoseconds += elapsed_ns;
    if (elapsed_ns > session->maximumWriteNanoseconds) {
        session->maximumWriteNanoseconds = elapsed_ns;
    }
    session->nextGeneration = generation + 1u;
    set_diag(session, "tile-batch checkpoint committed");
    if (session->testExitAfterSubpass == completed_subpasses &&
        completed_tiles_in_subpass == total_tiles_in_subpass) {
        _exit(86);
    }
    return true;
}
