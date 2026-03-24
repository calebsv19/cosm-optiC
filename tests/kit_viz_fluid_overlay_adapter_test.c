#include "kit_viz_fluid_overlay_adapter_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kit_viz.h"
#include "render/kit_viz_fluid_overlay_adapter.h"

static int fail(const char *name) {
    printf("FAIL %-32s\n", name);
    return 1;
}

int run_kit_viz_fluid_overlay_adapter_tests(void) {
    int failures = 0;

    float density[4] = { 0.0f, 0.5f, 1.0f, 2.0f };
    FluidFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.w = 2;
    frame.h = 2;
    frame.density = density;

    KitVizFluidOverlayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.alpha = 123u;
    cfg.auto_range = true;
    cfg.colormap = KIT_VIZ_COLORMAP_HEAT;

    uint8_t rgba[16];
    memset(rgba, 0, sizeof(rgba));
    if (!kit_viz_fluid_overlay_build_density_rgba(&frame, &cfg, rgba, sizeof(rgba))) {
        failures += fail("kit_viz_overlay_build_ok");
    } else {
        for (int i = 0; i < 4; ++i) {
            if (rgba[i * 4 + 3] != 123u) {
                failures += fail("kit_viz_overlay_alpha");
                break;
            }
        }
    }

    uint8_t tiny[8];
    if (kit_viz_fluid_overlay_build_density_rgba(&frame, &cfg, tiny, sizeof(tiny))) {
        failures += fail("kit_viz_overlay_small_buffer");
    }

    if (kit_viz_fluid_overlay_build_density_rgba(NULL, &cfg, rgba, sizeof(rgba))) {
        failures += fail("kit_viz_overlay_null_frame");
    }
    if (kit_viz_fluid_overlay_build_density_rgba(&frame, NULL, rgba, sizeof(rgba))) {
        failures += fail("kit_viz_overlay_null_cfg");
    }

    return failures;
}
