#include <string.h>
#include <stdlib.h>
#include "render/runtime_incident_light_3d.h"
#include "render/runtime_visibility_3d.h"
#include "render/runtime_native_3d_render_internal_host.h"

#include "render/runtime_caustic_volume_cache_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_volume_3d.h"
#include "render/runtime_volume_3d_debug.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"
#include "test_runtime_volume_3d.h"
#include "test_support.h"

static int test_runtime_volume_3d_defaults(void) {
    RuntimeVolumeAttachment3D attachment;

    RuntimeVolumeAttachment3D_Init(&attachment);

    assert_true("runtime_volume_3d_defaults_source_none",
                attachment.sourceKind == RUNTIME_VOLUME_3D_SOURCE_NONE);
    assert_true("runtime_volume_3d_defaults_disabled", !attachment.enabled);
    assert_true("runtime_volume_3d_defaults_affects_lighting",
                attachment.affectsLighting);
    assert_true("runtime_volume_3d_defaults_debug_off",
                !attachment.debugOverlayEnabled);
    assert_true("runtime_volume_3d_defaults_has_no_data", !attachment.hasData);
    assert_true("runtime_volume_3d_defaults_layout_invalid",
                !RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));
    assert_true("runtime_volume_3d_defaults_scene_up_z",
                attachment.grid.sceneUp.z == 1.0);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_configures_truthful_layout(void) {
    RuntimeVolumeGrid3D grid;
    bool ok = false;

    RuntimeVolumeGrid3D_Reset(&grid);
    ok = RuntimeVolumeGrid3D_Configure(&grid,
                                       1u,
                                       10u,
                                       20u,
                                       30u,
                                       1.25,
                                       42u,
                                       0.016,
                                       vec3(1.5, -2.0, 3.25),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       77u);
    assert_true("runtime_volume_3d_layout_configure_ok", ok);
    assert_true("runtime_volume_3d_layout_valid",
                RuntimeVolumeGrid3D_IsConfigured(&grid));
    assert_true("runtime_volume_3d_layout_dims_w", grid.gridW == 10u);
    assert_true("runtime_volume_3d_layout_dims_h", grid.gridH == 20u);
    assert_true("runtime_volume_3d_layout_dims_d", grid.gridD == 30u);
    assert_true("runtime_volume_3d_layout_cell_count", grid.cellCount == 6000u);
    assert_close("runtime_volume_3d_layout_origin_x", grid.origin.x, 1.5, 1e-9);
    assert_close("runtime_volume_3d_layout_origin_y", grid.origin.y, -2.0, 1e-9);
    assert_close("runtime_volume_3d_layout_origin_z", grid.origin.z, 3.25, 1e-9);
    assert_close("runtime_volume_3d_layout_bounds_max_x", grid.boundsMax.x, 6.5, 1e-9);
    assert_close("runtime_volume_3d_layout_bounds_max_y", grid.boundsMax.y, 8.0, 1e-9);
    assert_close("runtime_volume_3d_layout_bounds_max_z", grid.boundsMax.z, 18.25, 1e-9);
    assert_true("runtime_volume_3d_layout_frame_index", grid.frameIndex == 42u);
    assert_true("runtime_volume_3d_layout_crc", grid.solidMaskCrc32 == 77u);

    return 0;
}

static int test_runtime_volume_3d_allocates_owned_channels(void) {
    RuntimeVolumeAttachment3D attachment;
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY |
        RUNTIME_VOLUME_3D_CHANNEL_VELOCITY |
        RUNTIME_VOLUME_3D_CHANNEL_PRESSURE |
        RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = RuntimeVolumeGrid3D_Configure(&attachment.grid,
                                       1u,
                                       4u,
                                       3u,
                                       2u,
                                       0.0,
                                       7u,
                                       0.033,
                                       vec3(-1.0, 2.0, 0.5),
                                       0.25,
                                       vec3(0.0, 0.0, 0.0),
                                       19u);
    assert_true("runtime_volume_3d_channels_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&attachment, channel_mask);
    assert_true("runtime_volume_3d_channels_alloc_ok", ok);
    assert_true("runtime_volume_3d_channels_has_data", attachment.hasData);
    assert_true("runtime_volume_3d_channels_owned", attachment.ownsChannelBuffers);
    assert_true("runtime_volume_3d_channels_has_density",
                RuntimeVolumeAttachment3D_HasChannel(&attachment,
                                                     RUNTIME_VOLUME_3D_CHANNEL_DENSITY));
    assert_true("runtime_volume_3d_channels_has_velocity",
                RuntimeVolumeAttachment3D_HasChannel(&attachment,
                                                     RUNTIME_VOLUME_3D_CHANNEL_VELOCITY));
    assert_true("runtime_volume_3d_channels_has_pressure",
                RuntimeVolumeAttachment3D_HasChannel(&attachment,
                                                     RUNTIME_VOLUME_3D_CHANNEL_PRESSURE));
    assert_true("runtime_volume_3d_channels_has_solid",
                RuntimeVolumeAttachment3D_HasChannel(&attachment,
                                                     RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK));
    assert_true("runtime_volume_3d_channels_density_ptr", attachment.channels.density != NULL);
    assert_true("runtime_volume_3d_channels_velocity_z_ptr", attachment.channels.velocityZ != NULL);
    assert_true("runtime_volume_3d_channels_pressure_ptr", attachment.channels.pressure != NULL);
    assert_true("runtime_volume_3d_channels_solid_ptr", attachment.channels.solidMask != NULL);
    assert_true("runtime_volume_3d_channels_zero_init_density",
                attachment.channels.density[0] == 0.0f);
    assert_true("runtime_volume_3d_channels_zero_init_solid",
                attachment.channels.solidMask[0] == 0u);

    attachment.channels.density[0] = 3.5f;
    attachment.channels.solidMask[0] = 1u;
    RuntimeVolumeAttachment3D_ClearOwnedChannels(&attachment);
    assert_true("runtime_volume_3d_channels_clear_has_no_data", !attachment.hasData);
    assert_true("runtime_volume_3d_channels_clear_density_null", attachment.channels.density == NULL);
    assert_true("runtime_volume_3d_channels_clear_layout_still_valid",
                RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_rejects_channels_without_layout(void) {
    RuntimeVolumeAttachment3D attachment;
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &attachment, RUNTIME_VOLUME_3D_CHANNEL_DENSITY);
    assert_true("runtime_volume_3d_channels_reject_without_layout", !ok);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_debug_summary_defaults_without_layout(void) {
    RuntimeVolumeAttachment3D attachment;
    RuntimeVolumeDebugSummary3D summary;
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = RuntimeVolumeDebugSummary3D_Build(&attachment, &summary);
    assert_true("runtime_volume_3d_debug_defaults_build_ok", ok);
    assert_true("runtime_volume_3d_debug_defaults_source_none",
                summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_NONE);
    assert_true("runtime_volume_3d_debug_defaults_layout_invalid",
                !summary.layoutValid);
    assert_true("runtime_volume_3d_debug_defaults_no_density_range",
                !summary.hasDensityRange);
    assert_true("runtime_volume_3d_debug_defaults_no_channels",
                summary.channelMask == 0u);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_debug_summary_reports_density_range(void) {
    RuntimeVolumeAttachment3D attachment;
    RuntimeVolumeDebugSummary3D summary;
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY |
        RUNTIME_VOLUME_3D_CHANNEL_VELOCITY |
        RUNTIME_VOLUME_3D_CHANNEL_PRESSURE;
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    attachment.enabled = true;
    attachment.affectsLighting = false;
    attachment.debugOverlayEnabled = true;
    attachment.sourceKind = RUNTIME_VOLUME_3D_SOURCE_PACK;
    ok = RuntimeVolumeGrid3D_Configure(&attachment.grid,
                                       1u,
                                       2u,
                                       2u,
                                       2u,
                                       3.0,
                                       11u,
                                       0.02,
                                       vec3(-1.0, 1.0, 2.0),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       5u);
    assert_true("runtime_volume_3d_debug_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&attachment, channel_mask);
    assert_true("runtime_volume_3d_debug_alloc_ok", ok);
    attachment.channels.density[0] = 0.0f;
    attachment.channels.density[1] = 0.5f;
    attachment.channels.density[2] = 1.25f;
    attachment.channels.density[3] = 0.25f;
    attachment.channels.density[4] = 0.0f;
    attachment.channels.density[5] = 2.0f;
    attachment.channels.density[6] = 1.5f;
    attachment.channels.density[7] = 0.75f;

    ok = RuntimeVolumeDebugSummary3D_Build(&attachment, &summary);
    assert_true("runtime_volume_3d_debug_build_ok", ok);
    assert_true("runtime_volume_3d_debug_layout_valid", summary.layoutValid);
    assert_true("runtime_volume_3d_debug_has_density", summary.hasDensity);
    assert_true("runtime_volume_3d_debug_has_velocity", summary.hasVelocity);
    assert_true("runtime_volume_3d_debug_has_pressure", summary.hasPressure);
    assert_true("runtime_volume_3d_debug_no_solid", !summary.hasSolidMask);
    assert_true("runtime_volume_3d_debug_has_density_range", summary.hasDensityRange);
    assert_true("runtime_volume_3d_debug_source_pack",
                summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_PACK);
    assert_true("runtime_volume_3d_debug_affects_lighting_false",
                !summary.affectsLighting);
    assert_true("runtime_volume_3d_debug_overlay_true",
                summary.debugOverlayEnabled);
    assert_true("runtime_volume_3d_debug_dims_d", summary.gridD == 2u);
    assert_true("runtime_volume_3d_debug_cell_count", summary.cellCount == 8u);
    assert_true("runtime_volume_3d_debug_nonzero_density_count",
                summary.densityNonZeroCellCount == 6u);
    assert_close("runtime_volume_3d_debug_bounds_max_x",
                 summary.boundsMax.x, 0.0, 1e-9);
    assert_close("runtime_volume_3d_debug_bounds_max_z",
                 summary.boundsMax.z, 3.0, 1e-9);
    assert_close("runtime_volume_3d_debug_density_min",
                 summary.densityMin, 0.0, 1e-9);
    assert_close("runtime_volume_3d_debug_density_max",
                 summary.densityMax, 2.0, 1e-9);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_transmittance_is_unit_without_active_density(void) {
    RuntimeVolumeAttachment3D attachment;
    RuntimeVisibility3DTransmittance transmittance;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    ok = RuntimeVolumeGrid3D_Configure(&attachment.grid,
                                       1u,
                                       2u,
                                       2u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -0.5, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_integrate_unit_layout_ok", ok);
    ray.origin = vec3(0.0, 0.0, 1.0);
    ray.direction = vec3(0.0, 0.0, -1.0);

    transmittance = RuntimeVolume3D_TransmittanceAlongRayRGB(&attachment, &ray, 0.0, 2.0);
    assert_close("runtime_volume_3d_integrate_unit_luma", transmittance.luma, 1.0, 1e-9);
    assert_close("runtime_volume_3d_integrate_unit_r", transmittance.r, 1.0, 1e-9);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_transmittance_drops_through_dense_segment(void) {
    RuntimeVolumeAttachment3D attachment;
    RuntimeVisibility3DTransmittance transmittance;
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    attachment.enabled = true;
    attachment.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&attachment.grid,
                                       1u,
                                       2u,
                                       8u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.0, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_integrate_dense_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&attachment, channel_mask);
    assert_true("runtime_volume_3d_integrate_dense_alloc_ok", ok);
    for (uint64_t i = 0; i < attachment.grid.cellCount; ++i) {
        attachment.channels.density[i] = 1.0f;
        attachment.channels.solidMask[i] = 0u;
    }
    ray.origin = vec3(0.0, 0.0, 0.0);
    ray.direction = vec3(0.0, -1.0, 0.0);

    transmittance = RuntimeVolume3D_TransmittanceAlongRayRGB(&attachment, &ray, 0.0, 5.0);
    assert_true("runtime_volume_3d_integrate_dense_darker", transmittance.luma < 0.05);
    assert_close("runtime_volume_3d_integrate_dense_rgb_match",
                 transmittance.r,
                 transmittance.g,
                 1e-9);

    RuntimeVolumeAttachment3D_Free(&attachment);
    return 0;
}

static int test_runtime_volume_3d_single_scatter_accumulates_lit_density(void) {
    RuntimeScene3D scene;
    RuntimeVolume3DScatterResult scatter = {0};
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(1.5, -4.0, 0.5);
    scene.light.radius = 0.25;
    scene.light.intensity = 14.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       8u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.0, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_scatter_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene.volume, channel_mask);
    assert_true("runtime_volume_3d_scatter_alloc_ok", ok);
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 1.0f;
        scene.volume.channels.solidMask[i] = 0u;
    }
    ray.origin = vec3(0.0, 0.0, 0.0);
    ray.direction = vec3(0.0, -1.0, 0.0);

    scatter = RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene, &ray, 0.0, 5.0, NULL);
    assert_true("runtime_volume_3d_scatter_active", scatter.active);
    assert_true("runtime_volume_3d_scatter_samples_positive", scatter.sampleCount > 0);
    assert_true("runtime_volume_3d_scatter_radiance_positive", scatter.radiance > 0.0);
    assert_close("runtime_volume_3d_scatter_rgb_match", scatter.radianceR, scatter.radianceG, 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_volume_3d_single_scatter_prefers_forward_light_path(void) {
    RuntimeScene3D scene;
    RuntimeVolume3DScatterResult forward_scatter = {0};
    RuntimeVolume3DScatterResult lateral_scatter = {0};
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D forward_ray = {0};
    Ray3D lateral_ray = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -4.0, 0.0);
    scene.light.radius = 0.25;
    scene.light.intensity = 14.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       6u,
                                       10u,
                                       6u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-1.5, -4.5, -1.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_scatter_forward_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene.volume, channel_mask);
    assert_true("runtime_volume_3d_scatter_forward_alloc_ok", ok);
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 0.6f;
        scene.volume.channels.solidMask[i] = 0u;
    }

    forward_ray.origin = vec3(0.0, 0.0, 0.0);
    forward_ray.direction = vec3(0.0, -1.0, 0.0);
    lateral_ray.origin = vec3(0.0, 0.0, 0.0);
    lateral_ray.direction = vec3(0.0, 0.0, 1.0);

    forward_scatter =
        RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene, &forward_ray, 0.0, 6.0, NULL);
    lateral_scatter =
        RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene, &lateral_ray, 0.0, 6.0, NULL);

    assert_true("runtime_volume_3d_scatter_forward_active", forward_scatter.active);
    assert_true("runtime_volume_3d_scatter_lateral_active", lateral_scatter.active);
    assert_true("runtime_volume_3d_scatter_forward_stronger",
                forward_scatter.radiance > lateral_scatter.radiance * 2.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_volume_3d_caustic_cache_null_matches_existing_scatter(void) {
    RuntimeScene3D scene;
    RuntimeVolume3DScatterResult existing = {0};
    RuntimeVolume3DScatterResult with_null_cache = {0};
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(1.5, -4.0, 0.5);
    scene.light.radius = 0.25;
    scene.light.intensity = 14.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       8u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.0, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_caustic_null_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene.volume, channel_mask);
    assert_true("runtime_volume_3d_caustic_null_alloc_ok", ok);
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 1.0f;
        scene.volume.channels.solidMask[i] = 0u;
    }
    ray.origin = vec3(0.0, 0.0, 0.0);
    ray.direction = vec3(0.0, -1.0, 0.0);

    existing = RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene, &ray, 0.0, 5.0, NULL);
    with_null_cache = RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        &scene, &ray, 0.0, 5.0, NULL, NULL);
    assert_true("runtime_volume_3d_caustic_null_active_match",
                existing.active == with_null_cache.active);
    assert_true("runtime_volume_3d_caustic_null_samples_match",
                existing.sampleCount == with_null_cache.sampleCount);
    assert_close("runtime_volume_3d_caustic_null_r_match",
                 existing.radianceR,
                 with_null_cache.radianceR,
                 1e-12);
    assert_close("runtime_volume_3d_caustic_null_g_match",
                 existing.radianceG,
                 with_null_cache.radianceG,
                 1e-12);
    assert_close("runtime_volume_3d_caustic_null_b_match",
                 existing.radianceB,
                 with_null_cache.radianceB,
                 1e-12);
    assert_true("runtime_volume_3d_caustic_null_no_caustic_samples",
                with_null_cache.causticSampleCount == 0);
    assert_close("runtime_volume_3d_caustic_null_no_caustic_radiance",
                 with_null_cache.causticRadiance,
                 0.0,
                 1e-12);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_volume_3d_caustic_cache_lifts_density_without_direct_light(void) {
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;
    RuntimeVolume3DScatterResult scatter = {0};
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeCausticVolumeCache3D_Init(&cache);
    scene.hasLight = false;
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       8u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.0, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_caustic_cache_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene.volume, channel_mask);
    assert_true("runtime_volume_3d_caustic_cache_alloc_ok", ok);
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 1.0f;
        scene.volume.channels.solidMask[i] = 0u;
    }
    assert_true("runtime_volume_3d_caustic_cache_allocate",
                RuntimeCausticVolumeCache3D_AllocateFromVolume(&cache, &scene.volume));
    assert_true("runtime_volume_3d_caustic_cache_deposit",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(0.0, -2.0, 0.0), 40.0, 10.0, 4.0));

    ray.origin = vec3(0.0, 0.0, 0.0);
    ray.direction = vec3(0.0, -1.0, 0.0);
    scatter = RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        &scene, &ray, 0.0, 5.0, NULL, &cache);
    assert_true("runtime_volume_3d_caustic_cache_scatter_active", scatter.active);
    assert_true("runtime_volume_3d_caustic_cache_samples",
                scatter.causticSampleCount > 0);
    assert_true("runtime_volume_3d_caustic_cache_contributing",
                scatter.causticContributingSampleCount > 0);
    assert_true("runtime_volume_3d_caustic_cache_radiance",
                scatter.causticRadiance > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_sampled_radiance_sum",
                scatter.causticSampledCacheRadianceSum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_sampled_radiance_max",
                scatter.causticSampledCacheRadianceMax > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_density_avg_source",
                scatter.causticSampledRawDensitySum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_density_avg_remap",
                scatter.causticSampledDensitySum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_probability",
                scatter.causticScatterProbabilitySum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_transmittance",
                scatter.causticCameraTransmittanceSum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_term",
                scatter.causticVisibilityTermSum > 0.0);
    assert_true("runtime_volume_3d_caustic_cache_term_not_isotropic_phase_attenuated",
                scatter.causticVisibilityTermSum >
                    scatter.causticScatterProbabilitySum * 0.05);
    assert_close("runtime_volume_3d_caustic_cache_direct_r",
                 scatter.radianceR,
                 scatter.causticRadianceR,
                 1e-12);
    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_volume_3d_caustic_cache_sampled_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED);
    assert_true("runtime_volume_3d_caustic_cache_lookup_count",
                diagnostics.sampleLookupCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_volume_3d_caustic_gain_is_separate_from_direct_gain(void) {
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeVolume3DScatterResult base = {0};
    RuntimeVolume3DScatterResult direct_boosted = {0};
    RuntimeVolume3DScatterResult caustic_boosted = {0};
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Ray3D ray = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeCausticVolumeCache3D_Init(&cache);
    RuntimeVolume3DScatter_ResetTuning();
    scene.hasLight = true;
    scene.light.position = vec3(1.5, -4.0, 0.5);
    scene.light.radius = 0.25;
    scene.light.intensity = 14.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       8u,
                                       2u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.0, -0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_volume_3d_caustic_gain_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene.volume, channel_mask);
    assert_true("runtime_volume_3d_caustic_gain_alloc_ok", ok);
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 1.0f;
        scene.volume.channels.solidMask[i] = 0u;
    }
    assert_true("runtime_volume_3d_caustic_gain_cache_allocate",
                RuntimeCausticVolumeCache3D_AllocateFromVolume(&cache, &scene.volume));
    assert_true("runtime_volume_3d_caustic_gain_cache_deposit",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(0.0, -2.0, 0.0), 40.0, 10.0, 4.0));

    ray.origin = vec3(0.0, 0.0, 0.0);
    ray.direction = vec3(0.0, -1.0, 0.0);

    base = RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        &scene, &ray, 0.0, 5.0, NULL, &cache);
    assert_true("runtime_volume_3d_caustic_gain_base_direct",
                base.directRadiance > 0.0);
    assert_true("runtime_volume_3d_caustic_gain_base_caustic",
                base.causticRadiance > 0.0);

    RuntimeVolume3DScatter_SetStrengthGain(4.0);
    direct_boosted = RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        &scene, &ray, 0.0, 5.0, NULL, &cache);
    assert_true("runtime_volume_3d_caustic_gain_direct_boosted",
                direct_boosted.directRadiance > base.directRadiance * 3.5);
    assert_close("runtime_volume_3d_caustic_gain_direct_keeps_caustic",
                 direct_boosted.causticRadiance,
                 base.causticRadiance,
                 1e-12);

    RuntimeVolume3DScatter_ResetTuning();
    RuntimeVolume3DScatter_SetCausticStrengthGain(4.0);
    caustic_boosted = RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        &scene, &ray, 0.0, 5.0, NULL, &cache);
    assert_close("runtime_volume_3d_caustic_gain_caustic_keeps_direct",
                 caustic_boosted.directRadiance,
                 base.directRadiance,
                 1e-12);
    assert_true("runtime_volume_3d_caustic_gain_caustic_boosted",
                caustic_boosted.causticRadiance > base.causticRadiance * 3.5);

    RuntimeVolume3DScatter_ResetTuning();
    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    return 0;
}


static int test_runtime_volume_3d_registered_lights(void) {
    RuntimeScene3D scene;
    RuntimeLightSource3D light;
    Ray3D ray = {0};
    RuntimeScene3D_Init(&scene);
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    assert_true("registry_volume_config", RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
        1, 2, 8, 2, 0, 0, .02, vec3(-.5,-4,-.5), .5, vec3(0,0,1), 0));
    assert_true("registry_volume_allocate", RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &scene.volume, RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK));
    for (uint64_t q=0; q<scene.volume.grid.cellCount; ++q) scene.volume.channels.density[q]=1;
    ray.origin=vec3(0,0,0); ray.direction=vec3(0,-1,0);
    RuntimeLightSource3D_Init(&light);
    light.kind=RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.origin=RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.enabled=true; light.radius=0; light.position=vec3(1.5,-4,.5);
    light.color=vec3(1,0,0); light.intensity=14; light.falloffDistance=10;
    light.falloffMode=FORWARD_FALLOFF_MODE_LINEAR;
    assert_true("registry_append_red", RuntimeLightSet3D_Append(&scene.lightSet,&light,NULL));
    RuntimeVolume3DScatterResult red=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene,&ray,0,5,NULL);
    assert_true("registry_works_without_compat_light", red.radianceR>0 && red.radianceG==0 && red.radianceB==0);
    assert_true("registry_diagnostics", red.registrySourceEvaluations>0 && red.registryContributingSamples>0 && red.registryVisibilityRays>0 && red.compatibilityFallbackSamples==0);
    scene.hasLight=true; scene.light.position=light.position; scene.light.intensity=100000;
    RuntimeVolume3DScatterResult no_duplicate=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene,&ray,0,5,NULL);
    assert_close("registry_no_compat_duplicate", no_duplicate.radianceR,red.radianceR,1e-12);
    light.color=vec3(0,1,0);
    assert_true("registry_append_green", RuntimeLightSet3D_Append(&scene.lightSet,&light,NULL));
    RuntimeVolume3DScatterResult sum=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene,&ray,0,5,NULL);
    assert_close("registry_additive_red",sum.radianceR,red.radianceR,1e-12);
    assert_close("registry_additive_green",sum.radianceG,red.radianceR,1e-12);
    scene.lightSet.lights[0].enabled=false; scene.lightSet.lights[1].enabled=false;
    RuntimeVolume3DScatterResult disabled=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene,&ray,0,5,NULL);
    assert_true("registry_disabled_does_not_fall_back",disabled.radiance==0 && disabled.compatibilityFallbackSamples==0);
    scene.lightSet.lights[0].enabled=true;
    scene.lightSet.lights[0].kind=RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE;
    RuntimeVolume3DScatterResult unsupported=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&scene,&ray,0,5,NULL);
    assert_true("registry_mesh_explicitly_unqualified",unsupported.registryUnsupportedSources>0 && unsupported.radiance==0);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_volume_3d_prepared_incident_parity(void) {
    RuntimeLightSource3D source;
    RuntimeLightSource3D_Init(&source);
    source.enabled=true; source.position=vec3(.2,-2,.4);
    source.color=vec3(1,.3,.1);source.intensity=3;source.falloffDistance=.7;
    source.width=.4;source.height=.6;source.axisU=vec3(2,0,0);source.axisV=vec3(0,0,3);
    source.normal=vec3(0,2,0);source.radius=.2;
    source.origin=RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER;
    source.sourceSceneObjectIndex=3;source.sourceTriangleIndex=-1;
    for (int kind=0;kind<=4;++kind) for (int profile=0;profile<=2;++profile)
    for (int radiometry=0;radiometry<=1;++radiometry) for (int jitter=0;jitter<3;++jitter) {
        source.kind=(RuntimeLightSource3DKind)kind;
        source.emissionProfile=(RuntimeLightSource3DEmissionProfile)profile;
        source.radiometryMode=(RuntimeLightRadiometryMode3D)radiometry;source.radiance=4;
        RuntimeIncidentLight3DPreparedSource prepared;
        RuntimeIncidentLight3D_Prepare(&source,&prepared);
        RuntimeNative3DSamplingContext sampling={0};sampling.temporalSubpassCount=jitter==2?4:1;
        sampling.temporalSubpassIndex=jitter==2?2:0;sampling.sampleSequence=7;
        for (int side=-1;side<=1;++side) {
            Vec3 receiver=vec3(.3,side*3,.1);
            RuntimeIncidentLight3DSample samples[4];bool valid[4]={0};
            RuntimeIncidentLight3D_SamplePrepared(&prepared,receiver,jitter?&sampling:NULL,samples,valid);
            assert_true("prepared_count_parity",prepared.count==RuntimeIncidentLight3D_SampleCount(&source));
            for(int q=0;q<prepared.count;++q) {
                RuntimeIncidentLight3DSample reference;
                bool expected=RuntimeIncidentLight3D_Sample(&source,receiver,q,jitter?&sampling:NULL,&reference);
                assert_true("prepared_valid_parity",valid[q]==expected);
                if(!expected)continue;
                assert_close("prepared_position_x",samples[q].position.x,reference.position.x,0);
                assert_close("prepared_position_y",samples[q].position.y,reference.position.y,0);
                assert_close("prepared_position_z",samples[q].position.z,reference.position.z,0);
                assert_close("prepared_weight_r",samples[q].rgb_weight.x,reference.rgb_weight.x,0);
                assert_close("prepared_weight_g",samples[q].rgb_weight.y,reference.rgb_weight.y,0);
                assert_close("prepared_weight_b",samples[q].rgb_weight.z,reference.rgb_weight.z,0);
                assert_true("prepared_endpoint_parity",samples[q].target_object_index==reference.target_object_index &&
                    samples[q].target_triangle_index==reference.target_triangle_index);
            }
        }
    }
    source.enabled=false;
    RuntimeIncidentLight3DPreparedSource refreshed;RuntimeIncidentLight3D_Prepare(&source,&refreshed);
    assert_true("prepared_disabled_refresh",refreshed.count==0);
    return 0;
}

static int test_runtime_volume_3d_incident_and_visibility(void) {
    RuntimeLightSource3D light; RuntimeIncidentLight3DSample sample;
    RuntimeLightSource3D_Init(&light);
    light.enabled=true; light.kind=RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;
    light.origin=RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER;
    light.sourceSceneObjectIndex=5; light.sourceTriangleIndex=7;
    light.position=vec3(0,-2,0); light.axisU=vec3(1,0,0); light.axisV=vec3(0,0,1);
    light.normal=vec3(0,1,0); light.width=.4;light.height=.8;
    light.color=vec3(1,.5,.25);light.intensity=10;light.falloffDistance=10;
    light.emissionProfile=RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
    Vec3 centroid=vec3(0,0,0);
    for(int q=0;q<4;++q) {
        assert_true("incident_rect_front",RuntimeIncidentLight3D_Sample(&light,vec3(0,0,0),q,NULL,&sample));
        centroid=vec3_add(centroid,sample.position);
        assert_close("incident_rgb_ratio",sample.rgb_weight.x*.5,sample.rgb_weight.y,1e-12);
        assert_true("incident_target_identity",sample.target_object_index==5 && sample.target_triangle_index==7);
    }
    assert_close("incident_rect_centroid_x",centroid.x/4,0,1e-12);
    assert_close("incident_rect_centroid_z",centroid.z/4,0,1e-12);
    light.normal=vec3(0,-1,0);
    assert_true("incident_rect_back_dark",!RuntimeIncidentLight3D_Sample(&light,vec3(0,0,0),0,NULL,&sample));
    light.normal=vec3(0,1,0);light.radiometryMode=RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE;light.radiance=2;
    assert_true("incident_physical_rect",RuntimeIncidentLight3D_Sample(&light,vec3(0,0,0),0,NULL,&sample));
    double weight=sample.rgb_weight.x;light.radiance=4;
    assert_true("incident_physical_rect_double",RuntimeIncidentLight3D_Sample(&light,vec3(0,0,0),0,NULL,&sample));
    assert_close("incident_radiance_linear",sample.rgb_weight.x,2*weight,1e-12);
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);
    RuntimeRay3D_SetTraceRoute(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    RuntimeVisibility3DTransmittance visible=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,vec3(0,0,0),vec3(0,-2,0),5,0);
    assert_close("volume_point_empty_visible",visible.r,1,1e-12);
    scene.triangleMesh.triangles=calloc(1,sizeof(RuntimeTriangle3D));scene.triangleMesh.triangleCount=1;scene.triangleMesh.triangleCapacity=1;
    RuntimeTriangle3D* t=scene.triangleMesh.triangles;t->p0=vec3(-2,-1,-2);t->p1=vec3(2,-1,-2);t->p2=vec3(0,-1,2);
    t->normal=vec3(0,1,0);t->twoSided=true;t->sceneObjectIndex=8;t->primitiveIndex=-1;
    RuntimeVisibility3DTransmittance blocked=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,vec3(0,0,0),vec3(0,-2,0),5,0);
    assert_true("volume_point_other_object_blocks",blocked.r==0 && blocked.g==0 && blocked.b==0);
    t->sceneObjectIndex=5;
    RuntimeVisibility3DTransmittance target=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,vec3(0,0,0),vec3(0,-2,0),5,0);
    assert_close("volume_point_emitter_identity_not_self_blocked",target.r,1,1e-12);
    scene.volume.enabled = true;
    assert_true("visibility_active_volume_config", RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
        1, 2, 8, 2, 0, 0, .02, vec3(-.5,-4,-.5), .5, vec3(0,0,1), 0));
    assert_true("visibility_active_volume_allocate", RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &scene.volume, RUNTIME_VOLUME_3D_CHANNEL_DENSITY));
    for (uint64_t q=0; q<scene.volume.grid.cellCount; ++q) scene.volume.channels.density[q]=.25f;
    Ray3D connection = {.origin=vec3(0,0,0), .direction=vec3(0,-1,0)};
    RuntimeVisibility3DTransmittance extinction = RuntimeVolume3D_TransmittanceAlongRayRGB(
        &scene.volume, &connection, 1e-4, 2);
    target=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,connection.origin,vec3(0,-2,0),5,0);
    assert_true("visibility_endpoint_retains_extinction", target.r>0 && target.r<1);
    assert_close("visibility_endpoint_exact_extinction",target.r,extinction.r,1e-12);
    t->sceneObjectIndex=8;
    blocked=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,connection.origin,vec3(0,-2,0),5,0);
    assert_true("visibility_opaque_with_volume_blocks",blocked.r==0);
    t->sceneObjectIndex=5;
    for (uint64_t q=0; q<scene.volume.grid.cellCount; ++q) scene.volume.channels.density[q]=.5f;
    scene.volume.grid.frameIndex=1;
    target=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,connection.origin,vec3(0,-2,0),5,0);
    assert_true("visibility_new_density_frame_observed",target.r<extinction.r);
    scene.volume.enabled=false;
    target=RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB(&scene,connection.origin,vec3(0,-2,0),5,0);
    assert_close("visibility_disabled_volume_observed",target.r,1,1e-12);
    RuntimeScene3D_Free(&scene);RuntimeRay3D_ResetTraceRouteForTests();return 0;
}

int run_test_runtime_volume_3d_tests(void) {
    int before = test_support_failures();

    RuntimeVolume3DMaterial_ResetTuning();
    RuntimeVolume3DMaterial_SetDensityScale(2);
    RuntimeVolume3DMaterial_SetDensityGamma(0.5);
    const double original_density=RuntimeVolume3DMaterial_RemapDensity(8);
    RuntimeVolume3DMaterial_SetOpacity(0.25);
    assert_true("opacity_scales_after_gamma",fabs(RuntimeVolume3DMaterial_RemapDensity(8)-original_density*0.25)<1e-12);
    RuntimeVolume3DMaterialTuning retained=RuntimeVolume3DMaterial_GetTuning();
    RuntimeVolume3DMaterial_SetOpacity(0);
    assert_true("fully_transparent_density_zero",RuntimeVolume3DMaterial_RemapDensity(8)==0 && RuntimeVolume3DMaterial_ExtinctionDensity(8)==0);
    RuntimeVolume3DMaterial_RestoreTuning(retained);
    assert_true("opacity_tuning_restored",RuntimeVolume3DMaterial_GetTuning().opacity==0.25);
    RuntimeVolume3DMaterial_ResetTuning();

    RuntimeNative3DRenderStats total={0},part={0};
    total.volumeScatterDirectSampleCount=UINT64_C(3000000001);
    part.volumeScatterDirectSampleCount=UINT64_C(3000000000);
    total.volumeRegistryVisibilityRays=UINT64_C(6000000001);
    RuntimeNative3DRenderStats_Accumulate(&total,&part);
    assert_true("volume_counter_above_int_max",total.volumeScatterDirectSampleCount==UINT64_C(6000000001));
    runtime_native_3d_render_stats_normalize_temporal(&total,2);
    assert_true("volume_counter_temporal_rounding_wide",total.volumeScatterDirectSampleCount==UINT64_C(3000000001));
    assert_true("volume_raw_work_not_normalized",total.volumeRegistryVisibilityRays==UINT64_C(6000000001));
    test_runtime_volume_3d_registered_lights();
    test_runtime_volume_3d_incident_and_visibility();
    test_runtime_volume_3d_prepared_incident_parity();
    test_runtime_volume_3d_defaults();
    test_runtime_volume_3d_configures_truthful_layout();
    test_runtime_volume_3d_allocates_owned_channels();
    test_runtime_volume_3d_rejects_channels_without_layout();
    test_runtime_volume_3d_debug_summary_defaults_without_layout();
    test_runtime_volume_3d_debug_summary_reports_density_range();
    test_runtime_volume_3d_transmittance_is_unit_without_active_density();
    test_runtime_volume_3d_transmittance_drops_through_dense_segment();
    test_runtime_volume_3d_single_scatter_accumulates_lit_density();
    test_runtime_volume_3d_single_scatter_prefers_forward_light_path();
    test_runtime_volume_3d_caustic_cache_null_matches_existing_scatter();
    test_runtime_volume_3d_caustic_cache_lifts_density_without_direct_light();
    test_runtime_volume_3d_caustic_gain_is_separate_from_direct_gain();
    return test_support_failures() - before;
}
