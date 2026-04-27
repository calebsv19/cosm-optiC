#include "render/ray_tracing_integrator_catalog.h"

#include <string.h>

static int clamp_int(int value, int min_v, int max_v) {
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

static const char* label_2d(int value) {
    switch (RayTracingIntegratorCatalog_Clamp2D(value)) {
        case RAY_TRACING_2D_INTEGRATOR_HYBRID:
            return "Integrator: Hybrid";
        case RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT:
            return "Integrator: Direct Light";
        case RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT:
        default:
            return "Integrator: Forward Light";
    }
}

static const char* label_3d_button(int value) {
    switch (RayTracingIntegratorCatalog_Clamp3DToShipped(value)) {
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return "Integrator: 3D Emission / Transparency";
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return "Integrator: 3D Material";
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return "Integrator: 3D Diffuse Bounce";
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        default:
            return "Integrator: 3D Direct Light";
    }
}

static const char* label_3d_status(int value) {
    switch (RayTracingIntegratorCatalog_Clamp3DToShipped(value)) {
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return "integrator: 3D Emission / Transparency";
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return "integrator: 3D Material";
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return "integrator: 3D Diffuse Bounce";
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        default:
            return "integrator: 3D Direct Light";
    }
}

int RayTracingIntegratorCatalog_Clamp2D(int value) {
    return clamp_int(value,
                     RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT,
                     RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
}

int RayTracingIntegratorCatalog_Clamp3D(int value) {
    return clamp_int(value,
                     RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                     RAY_TRACING_3D_INTEGRATOR_DISNEY);
}

int RayTracingIntegratorCatalog_Default3D(void) {
    return RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
}

bool RayTracingIntegratorCatalog_Is3DShipped(int value) {
    switch (RayTracingIntegratorCatalog_Clamp3D(value)) {
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return true;
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
        default:
            return false;
    }
}

int RayTracingIntegratorCatalog_Clamp3DToShipped(int value) {
    switch (RayTracingIntegratorCatalog_Clamp3D(value)) {
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return RAY_TRACING_3D_INTEGRATOR_MATERIAL;
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        default:
            return RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    }
}

void RayTracingIntegratorCatalog_NormalizeAnimationConfig(AnimationConfig* cfg) {
    if (!cfg) return;
    cfg->integratorMode = RayTracingIntegratorCatalog_Clamp2D(cfg->integratorMode);
    cfg->integratorMode3D = RayTracingIntegratorCatalog_Clamp3D(cfg->integratorMode3D);
}

RayTracingIntegratorMenuState RayTracingIntegratorCatalog_BuildMenuState(
    const AnimationConfig* cfg) {
    RayTracingIntegratorMenuState state;
    SpaceMode space_mode = SPACE_MODE_2D;

    memset(&state, 0, sizeof(state));
    state.active2D = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    state.active3D = RayTracingIntegratorCatalog_Default3D();
    state.buttonLabel = label_2d(state.active2D);
    state.visibleCount = 3;

    if (!cfg) return state;

    state.raw2D = cfg->integratorMode;
    state.raw3D = cfg->integratorMode3D;
    state.active2D = RayTracingIntegratorCatalog_Clamp2D(cfg->integratorMode);
    state.active3D = RayTracingIntegratorCatalog_Clamp3DToShipped(cfg->integratorMode3D);
    space_mode = animation_config_space_mode_clamp(cfg->spaceMode);
    state.uses3DCatalog = (space_mode == SPACE_MODE_3D);
    if (state.uses3DCatalog) {
        state.buttonLabel = label_3d_button(state.active3D);
        state.visibleCount = 4;
        state.showPathToggles = false;
    } else {
        state.buttonLabel = label_2d(state.active2D);
        state.visibleCount = 3;
        state.showPathToggles = (state.active2D == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    }
    return state;
}

void RayTracingIntegratorCatalog_CycleActiveSelection(AnimationConfig* cfg) {
    int next = 0;
    RayTracingIntegratorMenuState state;
    if (!cfg) return;

    RayTracingIntegratorCatalog_NormalizeAnimationConfig(cfg);
    state = RayTracingIntegratorCatalog_BuildMenuState(cfg);
    if (state.uses3DCatalog) {
        switch (state.active3D) {
            case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
                cfg->integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
                break;
            case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
                cfg->integratorMode3D = RAY_TRACING_3D_INTEGRATOR_MATERIAL;
                break;
            case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
                cfg->integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
                break;
            case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            default:
                cfg->integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
                break;
        }
        return;
    }

    next = (state.active2D + 1) % 3;
    cfg->integratorMode = next;
}

RayTracingResolvedIntegratorState RayTracingIntegratorCatalog_ResolveRuntime(
    const AnimationConfig* cfg,
    SpaceMode requested_mode,
    bool native_route_active,
    bool compat_route_active,
    bool use_tiles_requested,
    bool tile_preview_requested) {
    RayTracingResolvedIntegratorState state;
    int raw_2d = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    int raw_3d = RayTracingIntegratorCatalog_Default3D();
    bool invalid_2d = false;
    bool invalid_3d = false;
    bool use_3d_catalog = false;

    memset(&state, 0, sizeof(state));
    if (cfg) {
        raw_2d = cfg->integratorMode;
        raw_3d = cfg->integratorMode3D;
    }

    state.raw2D = raw_2d;
    state.raw3D = raw_3d;
    state.normalized2D = RayTracingIntegratorCatalog_Clamp2D(raw_2d);
    state.normalized3D = RayTracingIntegratorCatalog_Clamp3D(raw_3d);
    invalid_2d = (raw_2d != state.normalized2D);
    invalid_3d = (raw_3d != state.normalized3D);

    requested_mode = animation_config_space_mode_clamp(requested_mode);
    use_3d_catalog = native_route_active || compat_route_active || requested_mode == SPACE_MODE_3D;
    state.uses3DCatalog = use_3d_catalog;

    if (!use_3d_catalog) {
        state.activeLegacy2DMode = state.normalized2D;
        state.active3DMode = RayTracingIntegratorCatalog_Default3D();
        state.showPathToggles =
            (state.activeLegacy2DMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
        state.buildIrradianceCache = state.showPathToggles;
        state.tilePreviewEnabled =
            use_tiles_requested && tile_preview_requested && state.showPathToggles;
        if (invalid_2d) {
            state.fallbackReason = RAY_TRACING_INTEGRATOR_FALLBACK_2D_INVALID;
        }
        return state;
    }

    state.activeLegacy2DMode = RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT;
    if (compat_route_active) {
        state.active3DMode = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    } else {
        state.active3DMode = RayTracingIntegratorCatalog_Clamp3DToShipped(state.normalized3D);
    }
    state.showPathToggles = false;
    state.buildIrradianceCache = false;
    state.tilePreviewEnabled =
        native_route_active && use_tiles_requested && tile_preview_requested;

    if (compat_route_active) {
        state.fallbackReason = RAY_TRACING_INTEGRATOR_FALLBACK_3D_COMPAT_DIRECT_LIGHT;
    } else if (invalid_3d) {
        state.fallbackReason = RAY_TRACING_INTEGRATOR_FALLBACK_3D_INVALID;
    } else if (!RayTracingIntegratorCatalog_Is3DShipped(state.normalized3D)) {
        state.fallbackReason = RAY_TRACING_INTEGRATOR_FALLBACK_3D_UNSHIPPED;
    }

    return state;
}

const char* RayTracingIntegratorCatalog_FallbackReasonLabel(
    RayTracingIntegratorFallbackReason reason) {
    switch (reason) {
        case RAY_TRACING_INTEGRATOR_FALLBACK_2D_INVALID:
            return "integrator clamp: 2D Forward Light";
        case RAY_TRACING_INTEGRATOR_FALLBACK_3D_INVALID:
            return "integrator clamp: 3D Direct Light";
        case RAY_TRACING_INTEGRATOR_FALLBACK_3D_UNSHIPPED:
            return "integrator clamp: reserved 3D -> Direct Light";
        case RAY_TRACING_INTEGRATOR_FALLBACK_3D_COMPAT_DIRECT_LIGHT:
            return "integrator fallback: compat 3D Direct Light";
        case RAY_TRACING_INTEGRATOR_FALLBACK_NONE:
        default:
            return "";
    }
}

const char* RayTracingIntegratorCatalog_3DStatusLabel(int value) {
    return label_3d_status(value);
}
