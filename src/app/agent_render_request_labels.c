#include "app/agent_render_request_internal.h"

const char *ray_tracing_agent_render_request_volume_kind_label(int kind) {
    switch (animation_config_volume_source_kind_clamp(kind)) {
        case VOLUME_SOURCE_MANIFEST:
            return "manifest";
        case VOLUME_SOURCE_RAW_VF3D:
            return "raw_vf3d";
        case VOLUME_SOURCE_PACK:
            return "pack";
        case VOLUME_SOURCE_NONE:
        default:
            return "none";
    }
}

const char *ray_tracing_agent_render_request_integrator_label(RayTracing3DIntegratorId id) {
    switch (RayTracingIntegratorCatalog_Clamp3DToShipped((int)id)) {
        case RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:
            return "disney_v2";
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return "diffuse_bounce";
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return "material";
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return "emission_transparency";
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
            return "disney";
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        default:
            return "direct_light";
    }
}

const char *ray_tracing_agent_render_request_inspection_preset_label(int preset) {
    switch (preset) {
        case RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW:
            return "glass_review";
        case RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW:
            return "glass_preview";
        case RAY_TRACING_AGENT_RENDER_PRESET_NONE:
        default:
            return "none";
    }
}
