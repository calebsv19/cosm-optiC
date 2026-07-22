#include "ui/menu_caustic_product.h"

#include <string.h>

#include "render/runtime_native_3d_render.h"

RuntimeCausticProductMode3D menu_caustic_product_mode(
    const RuntimeCausticSettings3D* settings) {
    if (!settings || settings->mode == RUNTIME_CAUSTIC_MODE_OFF) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_OFF;
    }
    if (settings->mode == RUNTIME_CAUSTIC_MODE_ANALYTIC) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC;
    }
    if (settings->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        settings->transportEngine == RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP;
    }
    return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT;
}

const char* menu_caustic_product_label(const RuntimeCausticSettings3D* settings) {
    switch (menu_caustic_product_mode(settings)) {
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC:
            return "Reference Analytic";
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT:
            return "Reference Transport";
        case RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP:
            return "Photon Map (Experimental)";
        case RUNTIME_CAUSTIC_PRODUCT_MODE_OFF:
        default:
            return "Off";
    }
}

RuntimeCausticProductMode3D menu_caustic_product_next_mode(
    const RuntimeCausticSettings3D* settings) {
    switch (menu_caustic_product_mode(settings)) {
        case RUNTIME_CAUSTIC_PRODUCT_MODE_OFF:
            return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC:
            return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT:
            return RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP:
        default:
            return RUNTIME_CAUSTIC_PRODUCT_MODE_OFF;
    }
}

void menu_caustic_product_select(RuntimeCausticSettings3D* settings,
                                 RuntimeCausticProductMode3D product_mode) {
    if (!settings) return;
    switch (product_mode) {
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC:
            settings->mode = RUNTIME_CAUSTIC_MODE_ANALYTIC;
            settings->transportEngine =
                RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
            settings->surfaceCacheEnabled = false;
            settings->volumeCacheEnabled = false;
            break;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT:
            settings->mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
            settings->transportEngine =
                RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
            if (!settings->surfaceCacheEnabled && !settings->volumeCacheEnabled) {
                settings->surfaceCacheEnabled = true;
            }
            break;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP:
            settings->mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
            settings->transportEngine = RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP;
            if (!settings->surfaceCacheEnabled && !settings->volumeCacheEnabled) {
                settings->surfaceCacheEnabled = true;
            }
            break;
        case RUNTIME_CAUSTIC_PRODUCT_MODE_OFF:
        default:
            settings->mode = RUNTIME_CAUSTIC_MODE_OFF;
            settings->transportEngine =
                RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
            settings->surfaceCacheEnabled = false;
            settings->volumeCacheEnabled = false;
            break;
    }
}

void menu_caustic_product_build_runtime_plan(
    const RuntimeCausticSettings3D* settings,
    MenuCausticProductRuntimePlan* out_plan) {
    RuntimeCausticPhotonIntegrationSettings3D* photon;
    if (!out_plan) return;
    memset(out_plan, 0, sizeof(*out_plan));
    photon = &out_plan->photonSettings;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(photon);
    out_plan->productMode = menu_caustic_product_mode(settings);
    photon->productMode = out_plan->productMode;
    if (!settings || out_plan->productMode != RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP) {
        return;
    }
    photon->surfaceQueryEnabled = settings->surfaceCacheEnabled;
    photon->volumeQueryEnabled = settings->volumeCacheEnabled;
    photon->renderContributionEnabled = true;
    if (settings->sampleBudget > 0) photon->sampleBudget = settings->sampleBudget;
    if (settings->maxPathDepth > 0) photon->maxPathDepth = settings->maxPathDepth;
    photon->surfaceRadianceScale = settings->surfaceRadianceScale;
    RuntimeCausticPhotonIntegration3D_NormalizeSettings(photon);
    out_plan->photonPopulationEnabled =
        photon->surfaceQueryEnabled || photon->volumeQueryEnabled;
}

void menu_caustic_product_apply_runtime(const RuntimeCausticSettings3D* settings) {
    MenuCausticProductRuntimePlan plan;
    menu_caustic_product_build_runtime_plan(settings, &plan);
    RuntimeNative3DRender_SetCausticPhotonRenderPrepPopulation(
        plan.photonPopulationEnabled, &plan.photonSettings);
}
