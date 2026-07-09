#include "render/runtime_caustic_settings_3d.h"

#include <string.h>

void RuntimeCausticSettings3D_Default(RuntimeCausticSettings3D* settings) {
    if (!settings) return;
    settings->mode = RUNTIME_CAUSTIC_MODE_ANALYTIC;
    settings->volumeCacheEnabled = false;
    settings->surfaceCacheEnabled = false;
    settings->sampleBudget = 0;
    settings->maxPathDepth = 0;
    settings->emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS;
    settings->surfaceRadianceScale = 1.0;
    settings->surfaceFootprintScale = 1.0;
    settings->surfaceReceiverFallbackEnabled = true;
    settings->cacheGridMode = RUNTIME_CAUSTIC_CACHE_GRID_VF3D_ALIGNED;
    settings->debugSummaryEnabled = false;
    settings->debugExportEnabled = false;
    settings->hasTraversalProfileOverride = false;
    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&settings->traversalProfileOverride);
}

RuntimeCausticMode3D RuntimeCausticMode3D_FromLabel(const char* label) {
    if (!label || !label[0] || strcmp(label, "analytic") == 0 ||
        strcmp(label, "sidecar") == 0 || strcmp(label, "on") == 0) {
        return RUNTIME_CAUSTIC_MODE_ANALYTIC;
    }
    if (strcmp(label, "off") == 0 || strcmp(label, "none") == 0) {
        return RUNTIME_CAUSTIC_MODE_OFF;
    }
    if (strcmp(label, "spatial_cache") == 0 || strcmp(label, "spatial") == 0 ||
        strcmp(label, "cache") == 0 || strcmp(label, "volume_cache") == 0) {
        return RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE;
    }
    if (strcmp(label, "transport") == 0 || strcmp(label, "physical") == 0) {
        return RUNTIME_CAUSTIC_MODE_TRANSPORT;
    }
    return RUNTIME_CAUSTIC_MODE_ANALYTIC;
}

RuntimeCausticTransportEmissionPolicy3D
RuntimeCausticTransportEmissionPolicy3D_FromLabel(const char* label) {
    if (!label || !label[0] || strcmp(label, "triangle_targets") == 0 ||
        strcmp(label, "triangle") == 0 || strcmp(label, "mesh_triangle_targets") == 0 ||
        strcmp(label, "default") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS;
    }
    if (strcmp(label, "analytic_sphere_lens") == 0 ||
        strcmp(label, "sphere_lens") == 0 ||
        strcmp(label, "sphere") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS;
    }
    if (strcmp(label, "analytic_cylinder_lens") == 0 ||
        strcmp(label, "cylinder_lens") == 0 ||
        strcmp(label, "cylinder") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS;
    }
    if (strcmp(label, "analytic_cylinder_lens_focused") == 0 ||
        strcmp(label, "focused_cylinder_lens") == 0 ||
        strcmp(label, "cylinder_lens_focused") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED;
    }
    if (strcmp(label, "analytic_prism_lens") == 0 ||
        strcmp(label, "prism_lens") == 0 ||
        strcmp(label, "prism") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS;
    }
    if (strcmp(label, "analytic_bowl_lens") == 0 ||
        strcmp(label, "bowl_lens") == 0 ||
        strcmp(label, "concave_lens") == 0 ||
        strcmp(label, "water_bowl_lens") == 0 ||
        strcmp(label, "bowl") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS;
    }
    if (strcmp(label, "mesh_dielectric_lens") == 0 ||
        strcmp(label, "mesh_dielectric") == 0 ||
        strcmp(label, "dielectric_mesh") == 0 ||
        strcmp(label, "mesh_lens") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_MESH_DIELECTRIC_LENS;
    }
    return RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS;
}

const char* RuntimeCausticMode3D_Label(RuntimeCausticMode3D mode) {
    switch (mode) {
        case RUNTIME_CAUSTIC_MODE_OFF:
            return "off";
        case RUNTIME_CAUSTIC_MODE_ANALYTIC:
            return "analytic";
        case RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE:
            return "spatial_cache";
        case RUNTIME_CAUSTIC_MODE_TRANSPORT:
            return "transport";
        default:
            return "unknown";
    }
}

const char* RuntimeCausticTransportEmissionPolicy3D_Label(
    RuntimeCausticTransportEmissionPolicy3D policy) {
    switch (policy) {
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS:
            return "triangle_targets";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS:
            return "analytic_sphere_lens";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS:
            return "analytic_cylinder_lens";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED:
            return "analytic_cylinder_lens_focused";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS:
            return "analytic_prism_lens";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS:
            return "analytic_bowl_lens";
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_MESH_DIELECTRIC_LENS:
            return "mesh_dielectric_lens";
        default:
            return "unknown";
    }
}

const char* RuntimeCausticCacheGridMode3D_Label(RuntimeCausticCacheGridMode3D mode) {
    switch (mode) {
        case RUNTIME_CAUSTIC_CACHE_GRID_NONE:
            return "none";
        case RUNTIME_CAUSTIC_CACHE_GRID_VF3D_ALIGNED:
            return "vf3d_aligned";
        default:
            return "unknown";
    }
}

const char* RuntimeCausticCacheState3D_Label(RuntimeCausticCacheState3D state) {
    switch (state) {
        case RUNTIME_CAUSTIC_CACHE_STATE_NONE:
            return "none";
        case RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED:
            return "requested_not_allocated";
        case RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY:
            return "allocated_empty";
        case RUNTIME_CAUSTIC_CACHE_STATE_POPULATED:
            return "populated";
        case RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED:
            return "sampled";
        default:
            return "unknown";
    }
}

RuntimeCausticReadback3D RuntimeCausticSettings3D_Phase0Readback(
    const RuntimeCausticSettings3D* settings,
    bool analytic_sidecar_requested) {
    RuntimeCausticReadback3D readback = {0};
    RuntimeCausticSettings3D defaults;
    const RuntimeCausticSettings3D* src = settings;

    if (!src) {
        RuntimeCausticSettings3D_Default(&defaults);
        src = &defaults;
    }

    readback.mode = src->mode;
    readback.analyticSidecarRequested = analytic_sidecar_requested;
    readback.volumeCacheRequested =
        (src->mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE ||
         src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT) &&
        src->volumeCacheEnabled;
    readback.surfaceCacheRequested =
        (src->mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE ||
         src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT) &&
        src->surfaceCacheEnabled;
    readback.volumeCacheState = readback.volumeCacheRequested
                                    ? RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED
                                    : RUNTIME_CAUSTIC_CACHE_STATE_NONE;
    readback.surfaceCacheState = readback.surfaceCacheRequested
                                     ? RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED
                                     : RUNTIME_CAUSTIC_CACHE_STATE_NONE;
    readback.emissionPolicy = src->emissionPolicy;
    readback.pathEmissionActive =
        src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        (src->volumeCacheEnabled || src->surfaceCacheEnabled);
    readback.transportReserved =
        src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        !src->volumeCacheEnabled &&
        !src->surfaceCacheEnabled;
    return readback;
}
