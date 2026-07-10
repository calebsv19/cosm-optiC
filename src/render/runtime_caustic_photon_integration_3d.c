#include "render/runtime_caustic_photon_integration_3d.h"

#include <string.h>

enum {
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_DEFAULT_SAMPLE_BUDGET = 256,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET = 4096,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_DEFAULT_MAX_DEPTH = 4,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_DEPTH = 16
};

static int photon_integration_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_integration_clamp_double(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 photon_integration_scaled(Vec3 value, double scale) {
    return vec3_scale(value, scale);
}

void RuntimeCausticPhotonIntegration3D_DefaultSettings(
    RuntimeCausticPhotonIntegrationSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE;
    settings->surfaceQueryEnabled = true;
    settings->volumeQueryEnabled = false;
    settings->renderContributionEnabled = false;
    settings->sampleBudget = RUNTIME_CAUSTIC_PHOTON_INTEGRATION_DEFAULT_SAMPLE_BUDGET;
    settings->maxPathDepth = RUNTIME_CAUSTIC_PHOTON_INTEGRATION_DEFAULT_MAX_DEPTH;
    settings->surfaceRadianceScale = 1.0;
    settings->surfaceQueryRadius = 0.10;
    settings->volumeQueryRadius = 0.10;
}

void RuntimeCausticPhotonIntegration3D_DefaultQuery(
    RuntimeCausticPhotonIntegrationQuery3D* query) {
    if (!query) return;
    memset(query, 0, sizeof(*query));
    RuntimeCausticPhotonMap3D_DefaultQuery(&query->surface);
    RuntimeCausticBeamMap3D_DefaultQuery(&query->volume);
    query->querySurface = true;
    query->queryVolume = true;
}

RuntimeCausticProductMode3D RuntimeCausticProductMode3D_FromLabel(
    const char* label) {
    if (!label || !label[0] || strcmp(label, "reference") == 0 ||
        strcmp(label, "legacy") == 0 || strcmp(label, "exploratory") == 0) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE;
    }
    if (strcmp(label, "off") == 0 || strcmp(label, "none") == 0 ||
        strcmp(label, "disabled") == 0) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_OFF;
    }
    if (strcmp(label, "production") == 0 || strcmp(label, "photon_map") == 0 ||
        strcmp(label, "photon") == 0) {
        return RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    }
    return RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE;
}

const char* RuntimeCausticProductMode3D_Label(RuntimeCausticProductMode3D mode) {
    switch (mode) {
        case RUNTIME_CAUSTIC_PRODUCT_MODE_OFF:
            return "off";
        case RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE:
            return "reference";
        case RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION:
            return "production";
        default:
            return "unknown";
    }
}

const char* RuntimeCausticPhotonIntegrationRoute3D_Label(
    RuntimeCausticPhotonIntegrationRoute3D route) {
    switch (route) {
        case RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_NONE:
            return "none";
        case RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE:
            return "exploratory_reference";
        case RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY:
            return "photon_query_ready";
        default:
            return "unknown";
    }
}

RuntimeCausticPhotonIntegrationRoute3D
RuntimeCausticPhotonIntegration3D_RouteForSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* settings) {
    RuntimeCausticPhotonIntegrationSettings3D defaults;
    const RuntimeCausticPhotonIntegrationSettings3D* active = settings;

    if (!active) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    if (active->productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_OFF) {
        return RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_NONE;
    }
    if (active->productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION) {
        return RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY;
    }
    return RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE;
}

void RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* integration,
    RuntimeCausticSettings3D* caustic) {
    RuntimeCausticPhotonIntegrationSettings3D defaults;
    const RuntimeCausticPhotonIntegrationSettings3D* active = integration;
    int sample_budget;
    int max_depth;

    if (!caustic) return;
    if (!active) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    sample_budget = photon_integration_clamp_int(
        active->sampleBudget,
        0,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET);
    max_depth = photon_integration_clamp_int(
        active->maxPathDepth,
        0,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_DEPTH);

    if (active->productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_OFF) {
        caustic->mode = RUNTIME_CAUSTIC_MODE_OFF;
        caustic->volumeCacheEnabled = false;
        caustic->surfaceCacheEnabled = false;
        caustic->transportEngine =
            RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
        return;
    }

    if (active->productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION) {
        caustic->mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
        caustic->transportEngine = RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP;
        caustic->surfaceCacheEnabled = active->surfaceQueryEnabled;
        caustic->volumeCacheEnabled = active->volumeQueryEnabled;
        caustic->sampleBudget = sample_budget;
        caustic->maxPathDepth = max_depth;
        caustic->surfaceRadianceScale =
            photon_integration_clamp_double(active->surfaceRadianceScale, 0.0, 1.0e6);
        return;
    }

    caustic->mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    caustic->transportEngine =
        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
    caustic->surfaceCacheEnabled = active->surfaceQueryEnabled;
    caustic->volumeCacheEnabled = active->volumeQueryEnabled;
    caustic->sampleBudget = sample_budget;
    caustic->maxPathDepth = max_depth;
}

bool RuntimeCausticPhotonIntegration3D_Query(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonIntegrationResult3D* out_result) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonIntegrationQuery3D default_query;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    const RuntimeCausticPhotonIntegrationQuery3D* active_query = query;
    RuntimeCausticPhotonIntegrationResult3D result;
    RuntimeCausticPhotonMapQueryResult3D surface_result;
    RuntimeCausticBeamMapQueryResult3D volume_result;

    memset(&result, 0, sizeof(result));
    if (out_result) *out_result = result;
    if (!out_result) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (!active_query) {
        RuntimeCausticPhotonIntegration3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }

    result.productMode = active_settings->productMode;
    result.route = RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings);
    result.surfaceQueryEnabled = active_settings->surfaceQueryEnabled;
    result.volumeQueryEnabled = active_settings->volumeQueryEnabled;
    result.renderContributionEnabled = active_settings->renderContributionEnabled;
    result.renderContributionSuppressed = !active_settings->renderContributionEnabled;

    if (result.route != RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        *out_result = result;
        return false;
    }

    if (active_settings->surfaceQueryEnabled && active_query->querySurface &&
        surface_map) {
        memset(&surface_result, 0, sizeof(surface_result));
        result.surfaceHit = RuntimeCausticPhotonMap3D_Query(
            surface_map,
            &active_query->surface,
            &surface_result);
        result.surfaceFlux = photon_integration_scaled(
            surface_result.flux,
            photon_integration_clamp_double(active_settings->surfaceRadianceScale,
                                            0.0,
                                            1.0e6));
        result.surfaceCandidateCount = surface_result.candidateCount;
        result.surfaceContributingCount = surface_result.contributingCount;
    }

    if (active_settings->volumeQueryEnabled && active_query->queryVolume && beam_map) {
        memset(&volume_result, 0, sizeof(volume_result));
        result.volumeHit = RuntimeCausticBeamMap3D_Query(
            beam_map,
            &active_query->volume,
            &volume_result);
        result.volumeFlux = volume_result.flux;
        result.volumeCandidateCount = volume_result.candidateCount;
        result.volumeContributingCount = volume_result.contributingCount;
    }

    result.combinedFlux = vec3_add(result.surfaceFlux, result.volumeFlux);
    *out_result = result;
    return result.surfaceHit || result.volumeHit;
}

bool RuntimeCausticPhotonIntegration3D_BuildContribution(
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    const RuntimeCausticPhotonIntegrationResult3D* query_result,
    RuntimeCausticPhotonContribution3D* out_contribution) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonIntegrationQuery3D default_query;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    const RuntimeCausticPhotonIntegrationQuery3D* active_query = query;
    RuntimeCausticPhotonContribution3D contribution;

    memset(&contribution, 0, sizeof(contribution));
    if (out_contribution) *out_contribution = contribution;
    if (!out_contribution) return false;
    if (!query_result) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (!active_query) {
        RuntimeCausticPhotonIntegration3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }

    contribution.suppressed = !active_settings->renderContributionEnabled;
    if (!active_settings->renderContributionEnabled ||
        query_result->route !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        *out_contribution = contribution;
        return false;
    }

    if (query_result->surfaceHit && active_settings->surfaceQueryEnabled &&
        active_query->querySurface) {
        contribution.hasSurfaceContribution = true;
        contribution.surfacePosition = active_query->surface.position;
        contribution.surfaceNormal = vec3_normalize(active_query->surface.normal);
        contribution.surfaceRadius =
            photon_integration_clamp_double(active_query->surface.radius, 0.005, 2.0);
        contribution.surfaceRadiance = query_result->surfaceFlux;
        contribution.surfaceSceneObjectIndex = active_query->surface.sceneObjectIndex;
        contribution.surfacePrimitiveIndex = active_query->surface.primitiveIndex;
        contribution.surfaceTriangleIndex = active_query->surface.triangleIndex;
        contribution.surfaceContributingCount =
            query_result->surfaceContributingCount;
    }

    if (query_result->volumeHit && active_settings->volumeQueryEnabled &&
        active_query->queryVolume) {
        contribution.hasVolumeContribution = true;
        contribution.volumePosition = active_query->volume.position;
        contribution.volumeDirection = vec3_normalize(active_query->volume.direction);
        contribution.volumeRadius =
            photon_integration_clamp_double(active_query->volume.radius, 0.005, 4.0);
        contribution.volumeRadiance = query_result->volumeFlux;
        contribution.volumeContributingCount = query_result->volumeContributingCount;
    }

    contribution.combinedRadiance =
        vec3_add(contribution.surfaceRadiance, contribution.volumeRadiance);
    contribution.eligible = contribution.hasSurfaceContribution ||
                            contribution.hasVolumeContribution;
    *out_contribution = contribution;
    return contribution.eligible;
}

bool RuntimeCausticPhotonIntegration3D_DepositContributionToCaches(
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticVolumeCache3D* volume_cache,
    const RuntimeCausticPhotonContribution3D* contribution,
    RuntimeCausticPhotonContributionDepositResult3D* out_result) {
    RuntimeCausticPhotonContributionDepositResult3D result;

    memset(&result, 0, sizeof(result));
    if (out_result) *out_result = result;
    if (!out_result || !contribution || !contribution->eligible ||
        contribution->suppressed) {
        return false;
    }

    result.attempted = true;
    if (contribution->hasSurfaceContribution) {
        HitInfo3D hit;
        HitInfo3D_Reset(&hit);
        hit.position = contribution->surfacePosition;
        hit.normal = contribution->surfaceNormal;
        hit.sceneObjectIndex = contribution->surfaceSceneObjectIndex;
        hit.primitiveIndex = contribution->surfacePrimitiveIndex;
        hit.triangleIndex = contribution->surfaceTriangleIndex;
        result.surfaceAttempted = true;
        result.surfaceDeposited = RuntimeCausticSurfaceCache3D_DepositAtHit(
            surface_cache,
            &hit,
            contribution->surfaceRadius,
            contribution->surfaceRadiance.x,
            contribution->surfaceRadiance.y,
            contribution->surfaceRadiance.z);
    }

    if (contribution->hasVolumeContribution) {
        result.volumeAttempted = true;
        result.volumeDeposited =
            RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
                volume_cache,
                contribution->volumePosition,
                contribution->volumeDirection,
                contribution->volumeRadius,
                contribution->volumeRadius,
                contribution->volumeRadiance.x,
                contribution->volumeRadiance.y,
                contribution->volumeRadiance.z);
    }

    *out_result = result;
    return result.surfaceDeposited || result.volumeDeposited;
}
