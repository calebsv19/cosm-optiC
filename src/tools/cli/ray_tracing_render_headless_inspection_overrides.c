#include "tools/ray_tracing_render_headless_internal.h"

#include "app/animation.h"
#include "render/runtime_caustic_bootstrap_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_caustic_transport_debug_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"

void ray_tracing_headless_apply_inspection_overrides(
    const RayTracingAgentRenderRequest *request) {
    if (!request) return;

    RuntimeNative3DRender_ResetInspectionCameraOverrides();
    RuntimeNative3DRender_ResetCausticPhotonMapStore();
    RuntimeNative3DRender_SetCausticPhotonRenderPrepPopulation(
        request->caustic_photon_render_prep_population_enabled,
        &request->caustic_photon_integration_settings);
    RuntimeVolume3DScatter_ResetTuning();
    RuntimeVolume3DMaterial_ResetTuning();
    RuntimeRay3D_SetTraceRoute(request->trace_route);
    if (request->has_camera_zoom_override) {
        sceneSettings.camera.zoom = request->camera_zoom_override;
    }
    if (request->has_camera_position_override) {
        RuntimeNative3DRender_SetInspectionCameraPosition(
            vec3(request->camera_position_x,
                 request->camera_position_y,
                 request->camera_position_z));
    }
    if (request->has_camera_look_at_override) {
        RuntimeNative3DRender_SetInspectionCameraLookAt(
            vec3(request->camera_look_at_x,
                 request->camera_look_at_y,
                 request->camera_look_at_z));
    }
    if (request->has_environment_brightness_override) {
        animSettings.environmentBrightness = request->environment_brightness_override;
    }
    if (request->has_ambient_strength_override) {
        animSettings.environmentBrightness = request->ambient_strength_override * 255.0;
    }
    if (request->has_environment_light_mode_override) {
        animSettings.environmentLightMode =
            animation_config_environment_light_mode_clamp(
                request->environment_light_mode_override);
    }
    if (request->has_environment_preset_override) {
        animSettings.environmentBackgroundLightingAuthored = true;
        animSettings.environmentPreset =
            animation_config_environment_preset_clamp(request->environment_preset_override);
    }
    if (request->has_background_brightness_override) {
        animSettings.environmentBackgroundLightingAuthored = true;
        animSettings.environmentBackgroundBrightnessAuto = false;
        animSettings.environmentBackgroundBrightness = request->background_brightness_override;
    }
    if (request->has_background_color_override) {
        animSettings.environmentBackgroundLightingAuthored = true;
        animSettings.environmentBackgroundColorR = request->background_color_r;
        animSettings.environmentBackgroundColorG = request->background_color_g;
        animSettings.environmentBackgroundColorB = request->background_color_b;
    }
    if (request->has_top_fill_strength_override) {
        animSettings.topFillStrength = request->top_fill_strength_override;
    }
    if (request->has_light_intensity_override) {
        animSettings.lightIntensity = request->light_intensity_override;
    }
    if (request->has_light_radius_override) {
        animSettings.lightRadius = request->light_radius_override;
    }
    if (request->has_forward_decay_override) {
        animSettings.forwardDecay = request->forward_decay_override;
    }
    if (request->has_volume_scatter_gain_override) {
        RuntimeVolume3DScatter_SetStrengthGain(request->volume_scatter_gain_override);
    }
    if (request->has_caustic_volume_scatter_gain_override) {
        RuntimeVolume3DScatter_SetCausticStrengthGain(
            request->caustic_volume_scatter_gain_override);
    }
    if (request->has_volume_density_scale_override) {
        RuntimeVolume3DMaterial_SetDensityScale(request->volume_density_scale_override);
    }
    if (request->has_volume_density_gamma_override) {
        RuntimeVolume3DMaterial_SetDensityGamma(request->volume_density_gamma_override);
    }
    if (request->has_volume_absorption_gain_override) {
        RuntimeVolume3DMaterial_SetAbsorptionGain(request->volume_absorption_gain_override);
    }
    if (request->has_volume_opacity_clamp_override) {
        RuntimeVolume3DMaterial_SetOpacityClamp(request->volume_opacity_clamp_override);
    }
    if (request->has_volume_step_scale_override) {
        RuntimeVolume3DScatter_SetStepScale(request->volume_step_scale_override);
    }
    if (request->has_secondary_diffuse_samples_3d_override) {
        animSettings.secondaryDiffuseSamples3D = request->secondary_diffuse_samples_3d_override;
    }
    if (request->has_transmission_samples_3d_override) {
        animSettings.transmissionSamples3D = request->transmission_samples_3d_override;
    }
    RuntimeDisneyV2_3D_SetCausticMode(
        request->integrator_3d == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2
            ? request->caustic_mode
            : RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF,
        request->has_caustic_sidecar_strength_override ? request->caustic_sidecar_strength
                                                       : 1.0);
    RuntimeCausticBootstrap3D_SetRequestState(&request->caustic_settings);
    RuntimeCausticTransport3D_SetRequestState(&request->caustic_settings);
    RuntimeCausticTransportDebug3D_SetOutputRoot(request->output_root);
    if (request->has_volume_tint_override) {
        RuntimeVolume3DScatter_SetTint(request->volume_tint_r,
                                       request->volume_tint_g,
                                       request->volume_tint_b);
    }
    if (request->has_volume_albedo_override) {
        RuntimeVolume3DScatter_SetTint(request->volume_albedo_r,
                                       request->volume_albedo_g,
                                       request->volume_albedo_b);
    }
}
