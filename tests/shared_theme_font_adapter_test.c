#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
    fprintf(stderr, "shared_theme_font_adapter_test: %s\n", msg);
    return 1;
}

int main(void) {
    RayTracingThemePalette palette = {0};
    char path[256] = {0};
    int point_size = 0;
    size_t i = 0;
    const char* theme_presets[] = {
        "studio_blue",
        "harbor_blue",
        "midnight_contrast",
        "soft_light",
        "standard_grey",
        "greyscale"
    };

    unsetenv("RAY_TRACING_USE_SHARED_THEME_FONT");
    unsetenv("RAY_TRACING_USE_SHARED_THEME");
    unsetenv("RAY_TRACING_USE_SHARED_FONT");
    unsetenv("RAY_TRACING_THEME_PRESET");
    unsetenv("RAY_TRACING_FONT_PRESET");

    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        return fail("theme should be enabled by default");
    }
    if (!ray_tracing_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be enabled by default");
    }
    if (strstr(path, "Lato-Regular.ttf") == NULL) {
        return fail("default shared font should resolve to the IDE/Lato baseline");
    }
    if (point_size != 11) {
        return fail("default shared font should resolve to the BASIC IDE tier size");
    }

    setenv("RAY_TRACING_USE_SHARED_THEME_FONT", "1", 1);
    setenv("RAY_TRACING_THEME_PRESET", "standard_grey", 1);
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        return fail("theme should resolve when shared toggle is enabled");
    }

    setenv("RAY_TRACING_USE_SHARED_FONT", "0", 1);
    if (ray_tracing_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be disabled via per-domain toggle");
    }

    setenv("RAY_TRACING_USE_SHARED_FONT", "1", 1);
    setenv("RAY_TRACING_FONT_PRESET", "studio_blue", 1);
    if (!ray_tracing_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should resolve when enabled");
    }
    if (path[0] == '\0' || point_size <= 0) {
        return fail("font resolution returned invalid path or point size");
    }

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("RAY_TRACING_THEME_PRESET", theme_presets[i], 1);
        if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
            return fail("theme preset matrix should resolve");
        }
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
