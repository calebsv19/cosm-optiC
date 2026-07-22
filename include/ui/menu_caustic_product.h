#ifndef UI_MENU_CAUSTIC_PRODUCT_H
#define UI_MENU_CAUSTIC_PRODUCT_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_settings_3d.h"

typedef struct {
    RuntimeCausticProductMode3D productMode;
    RuntimeCausticPhotonIntegrationSettings3D photonSettings;
    bool photonPopulationEnabled;
} MenuCausticProductRuntimePlan;

RuntimeCausticProductMode3D menu_caustic_product_mode(
    const RuntimeCausticSettings3D* settings);
const char* menu_caustic_product_label(const RuntimeCausticSettings3D* settings);
RuntimeCausticProductMode3D menu_caustic_product_next_mode(
    const RuntimeCausticSettings3D* settings);
void menu_caustic_product_select(RuntimeCausticSettings3D* settings,
                                 RuntimeCausticProductMode3D product_mode);
void menu_caustic_product_build_runtime_plan(
    const RuntimeCausticSettings3D* settings,
    MenuCausticProductRuntimePlan* out_plan);
void menu_caustic_product_apply_runtime(const RuntimeCausticSettings3D* settings);

#endif
