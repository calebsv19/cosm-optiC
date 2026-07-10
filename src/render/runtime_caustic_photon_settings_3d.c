#include "render/runtime_caustic_photon_settings_3d.h"

#include <string.h>

RuntimeCausticTransportEngine3D RuntimeCausticTransportEngine3D_FromLabel(
    const char* label) {
    if (!label || !label[0] ||
        strcmp(label, "exploratory_lens_transport") == 0 ||
        strcmp(label, "lens_transport") == 0 ||
        strcmp(label, "legacy_lens_transport") == 0 ||
        strcmp(label, "reference_lens_transport") == 0 ||
        strcmp(label, "default") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
    }
    if (strcmp(label, "photon_map") == 0 ||
        strcmp(label, "production_photon_map") == 0 ||
        strcmp(label, "photon_mapper") == 0 ||
        strcmp(label, "beam_photon_map") == 0) {
        return RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP;
    }
    return RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT;
}

const char* RuntimeCausticTransportEngine3D_Label(
    RuntimeCausticTransportEngine3D engine) {
    switch (engine) {
        case RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT:
            return "exploratory_lens_transport";
        case RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP:
            return "photon_map";
        default:
            return "unknown";
    }
}
