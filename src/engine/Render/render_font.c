#include "engine/Render/render_font.h"

#include "render/font_runtime.h"

void invalidateActiveFontHandle(void) {
    ray_tracing_font_runtime_invalidate_active_font();
}

bool fontSystemReady(void) {
    return ray_tracing_font_runtime_is_ready();
}

bool initFontSystem(void) {
    return ray_tracing_font_runtime_init() &&
           ray_tracing_font_runtime_activate_default_font();
}

void shutdownFontSystem(void) {
    ray_tracing_font_runtime_shutdown();
}

bool loadFontByID(FontID id) {
    (void)id;
    return ray_tracing_font_runtime_activate_default_font();
}

TTF_Font* getActiveFont(void) {
    return ray_tracing_font_runtime_get_active_font();
}

bool refreshActiveFontFromAnimationConfig(void) {
    return ray_tracing_font_runtime_refresh_active_font();
}

int getActiveFontPointSize(void) {
    return ray_tracing_font_runtime_active_point_size();
}
