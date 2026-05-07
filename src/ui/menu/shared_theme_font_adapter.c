#include "ui/shared_theme_font_adapter.h"

#include "core_font.h"
#include "core_io.h"
#include "core_theme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_theme_runtime_initialized = false;
static CoreThemePresetId g_theme_runtime_preset = CORE_THEME_PRESET_DARK_DEFAULT;
static bool g_font_runtime_initialized = false;
static CoreFontPresetId g_font_runtime_preset = CORE_FONT_PRESET_IDE;
static const char* k_theme_persist_path = "theme_preset.txt";
static const char* k_font_persist_path = "data/runtime/font_preset.txt";
static const CoreThemePresetId k_theme_cycle_order[] = {
    CORE_THEME_PRESET_DAW_DEFAULT,
    CORE_THEME_PRESET_MAP_FORGE_DEFAULT,
    CORE_THEME_PRESET_DARK_DEFAULT,
    CORE_THEME_PRESET_LIGHT_DEFAULT,
    CORE_THEME_PRESET_IDE_GRAY,
    CORE_THEME_PRESET_GREYSCALE
};

static bool parse_bool_env(const char* value, bool* out_value) {
    char lowered[16];
    size_t i = 0;
    if (!value || !value[0] || !out_value) {
        return false;
    }
    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = true;
        return true;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static bool is_shared_toggle_enabled(const char* override_var_name) {
    const char* override = getenv(override_var_name);
    bool out_value = false;
    if (parse_bool_env(override, &out_value)) {
        return out_value;
    }
    if (parse_bool_env(getenv("RAY_TRACING_USE_SHARED_THEME_FONT"), &out_value)) {
        return out_value;
    }
    return true;
}

static void trim_trailing_whitespace(char* text) {
    size_t len;
    if (!text) {
        return;
    }
    len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[len - 1] = '\0';
        --len;
    }
}

static SDL_Color theme_color_or_default(const CoreThemePreset* preset,
                                        CoreThemeColorToken token,
                                        SDL_Color fallback) {
    CoreThemeColor raw = {0};
    CoreResult r = core_theme_get_color(preset, token, &raw);
    if (r.code != CORE_OK) {
        return fallback;
    }
    return (SDL_Color){raw.r, raw.g, raw.b, raw.a};
}

static int theme_color_luma(SDL_Color color) {
    return (int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114;
}

static int theme_color_contrast_gap(SDL_Color a, SDL_Color b) {
    int dr = abs((int)a.r - (int)b.r);
    int dg = abs((int)a.g - (int)b.g);
    int db = abs((int)a.b - (int)b.b);
    return dr + dg + db;
}

static Uint8 theme_mix_u8(Uint8 a, Uint8 b, int a_weight, int b_weight) {
    int total = a_weight + b_weight;
    if (total <= 0) return a;
    return (Uint8)(((int)a * a_weight + (int)b * b_weight) / total);
}

static SDL_Color theme_mix_color(SDL_Color a, SDL_Color b, int a_weight, int b_weight) {
    return (SDL_Color){
        theme_mix_u8(a.r, b.r, a_weight, b_weight),
        theme_mix_u8(a.g, b.g, a_weight, b_weight),
        theme_mix_u8(a.b, b.b, a_weight, b_weight),
        theme_mix_u8(a.a, b.a, a_weight, b_weight)
    };
}

static void theme_runtime_init_if_needed(void) {
    const char* preset_name;
    CoreThemePresetId resolved_id;
    if (g_theme_runtime_initialized) {
        return;
    }
    preset_name = getenv("RAY_TRACING_THEME_PRESET");
    if (preset_name && preset_name[0] &&
        core_theme_preset_id_from_name(preset_name, &resolved_id).code == CORE_OK) {
        g_theme_runtime_preset = resolved_id;
    } else {
        g_theme_runtime_preset = CORE_THEME_PRESET_DARK_DEFAULT;
    }
    g_theme_runtime_initialized = true;
}

static void font_runtime_init_if_needed(void) {
    const char* preset_name;
    CoreFontPreset resolved_preset = {0};
    if (g_font_runtime_initialized) {
        return;
    }
    preset_name = getenv("RAY_TRACING_FONT_PRESET");
    if (preset_name && preset_name[0] &&
        core_font_get_preset_by_name(preset_name, &resolved_preset).code == CORE_OK) {
        g_font_runtime_preset = resolved_preset.id;
    } else {
        g_font_runtime_preset = CORE_FONT_PRESET_IDE;
    }
    g_font_runtime_initialized = true;
}

static bool resolve_theme_preset(CoreThemePreset* out_preset) {
    CoreResult r;
    theme_runtime_init_if_needed();
    r = core_theme_get_preset(g_theme_runtime_preset, out_preset);
    if (r.code == CORE_OK) {
        return true;
    }
    r = core_theme_get_preset(CORE_THEME_PRESET_DARK_DEFAULT, out_preset);
    return r.code == CORE_OK;
}

static bool copy_existing_path(char* out_path, size_t out_path_size, const char* candidate) {
    if (!out_path || out_path_size == 0 || !candidate || !candidate[0]) {
        return false;
    }
    if (core_io_path_exists(candidate)) {
        strncpy(out_path, candidate, out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
        return true;
    }
    return false;
}

static bool resolve_existing_font_path(const CoreFontRoleSpec* role,
                                       char* out_path,
                                       size_t out_path_size) {
    char adjusted[384];
    const char* candidates[2];
    size_t i;
    candidates[0] = role ? role->primary_path : NULL;
    candidates[1] = role ? role->fallback_path : NULL;

    for (i = 0; i < 2; ++i) {
        const char* raw = candidates[i];
        const char* base = NULL;
        if (!raw || !raw[0]) {
            continue;
        }

        if (copy_existing_path(out_path, out_path_size, raw)) {
            return true;
        }

        if (strncmp(raw, "shared/", 7) == 0) {
            snprintf(adjusted, sizeof(adjusted), "../%s", raw);
            if (copy_existing_path(out_path, out_path_size, adjusted)) {
                return true;
            }
        }

        if (strncmp(raw, "include/fonts/Montserrat/", 25) == 0 &&
            strstr(raw, "/static/") == NULL) {
            base = strrchr(raw, '/');
            if (base && base[1]) {
                snprintf(adjusted, sizeof(adjusted), "../line_drawing/include/fonts/Montserrat/static/%s", base + 1);
                if (copy_existing_path(out_path, out_path_size, adjusted)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool ray_tracing_shared_theme_resolve_palette(RayTracingThemePalette* out_palette) {
    CoreThemePreset preset = {0};
    if (!out_palette || !is_shared_toggle_enabled("RAY_TRACING_USE_SHARED_THEME")) {
        return false;
    }
    if (!resolve_theme_preset(&preset)) {
        return false;
    }

    out_palette->background_fill =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_0, (SDL_Color){0, 0, 0, 255});
    out_palette->panel_fill =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_1, (SDL_Color){28, 28, 30, 230});
    out_palette->panel_border =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_2, (SDL_Color){80, 80, 90, 255});
    out_palette->button_fill =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_2, (SDL_Color){100, 100, 100, 255});
    out_palette->button_active_fill =
        theme_color_or_default(&preset, CORE_THEME_COLOR_ACCENT_PRIMARY, (SDL_Color){0, 255, 0, 255});
    out_palette->button_text =
        theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_PRIMARY, (SDL_Color){255, 255, 255, 255});
    out_palette->text_primary =
        theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_PRIMARY, (SDL_Color){230, 230, 230, 255});
    out_palette->text_muted =
        theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_MUTED, (SDL_Color){210, 210, 210, 255});
    out_palette->accent_primary =
        theme_color_or_default(&preset, CORE_THEME_COLOR_ACCENT_PRIMARY, (SDL_Color){120, 200, 160, 255});
    return true;
}

SDL_Color ray_tracing_theme_resolve_button_active_fill(RayTracingThemePalette palette) {
    SDL_Color fill = palette.button_active_fill;
    SDL_Color preferred_text = palette.button_text;
    SDL_Color anchor = palette.button_fill;

    if (theme_color_contrast_gap(fill, preferred_text) >= 110) {
        return fill;
    }
    if (theme_color_luma(preferred_text) >= 150) {
        return theme_mix_color(fill, anchor, 1, 2);
    }
    return theme_mix_color(fill, (SDL_Color){240, 243, 247, fill.a}, 1, 2);
}

SDL_Color ray_tracing_theme_choose_button_text(SDL_Color fill, RayTracingThemePalette palette) {
    SDL_Color preferred_text = palette.button_text;
    if (theme_color_contrast_gap(fill, preferred_text) >= 110) {
        return preferred_text;
    }
    if (theme_color_luma(fill) >= 150) {
        return (SDL_Color){24, 28, 34, preferred_text.a ? preferred_text.a : 255};
    }
    return (SDL_Color){245, 247, 250, preferred_text.a ? preferred_text.a : 255};
}

bool ray_tracing_shared_theme_set_preset(const char* preset_name) {
    CoreThemePresetId id;
    if (!preset_name || !preset_name[0]) {
        return false;
    }
    if (core_theme_preset_id_from_name(preset_name, &id).code != CORE_OK) {
        return false;
    }
    g_theme_runtime_preset = id;
    g_theme_runtime_initialized = true;
    return true;
}

bool ray_tracing_shared_theme_current_preset(char* out_name, size_t out_name_size) {
    const char* name;
    if (!out_name || out_name_size == 0) {
        return false;
    }
    theme_runtime_init_if_needed();
    name = core_theme_preset_name(g_theme_runtime_preset);
    if (!name || !name[0]) {
        return false;
    }
    strncpy(out_name, name, out_name_size - 1);
    out_name[out_name_size - 1] = '\0';
    return true;
}

bool ray_tracing_shared_theme_cycle_next(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

bool ray_tracing_shared_theme_cycle_prev(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + n - 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

bool ray_tracing_shared_theme_load_persisted(void) {
    CoreBuffer file_data = {0};
    CoreResult read_result;
    char preset_name[128];
    size_t copy_len = 0;
    if (!core_io_path_exists(k_theme_persist_path)) {
        return false;
    }
    read_result = core_io_read_all(k_theme_persist_path, &file_data);
    if (read_result.code != CORE_OK || !file_data.data) {
        return false;
    }
    copy_len = file_data.size;
    if (copy_len >= sizeof(preset_name)) {
        copy_len = sizeof(preset_name) - 1;
    }
    if (copy_len == 0u) {
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(preset_name, file_data.data, copy_len);
    preset_name[copy_len] = '\0';
    core_io_buffer_free(&file_data);
    trim_trailing_whitespace(preset_name);
    if (!preset_name[0]) {
        return false;
    }
    return ray_tracing_shared_theme_set_preset(preset_name);
}

bool ray_tracing_shared_theme_save_persisted(void) {
    char preset_name[128];
    char payload[144];
    int written;
    if (!ray_tracing_shared_theme_current_preset(preset_name, sizeof(preset_name))) {
        return false;
    }
    written = snprintf(payload, sizeof(payload), "%s\n", preset_name);
    if (written <= 0 || (size_t)written >= sizeof(payload)) {
        return false;
    }
    return core_io_write_all(k_theme_persist_path, payload, (size_t)written).code == CORE_OK;
}

bool ray_tracing_shared_font_load_persisted(void) {
    CoreBuffer file_data = {0};
    CoreResult read_result;
    char preset_name[128];
    size_t copy_len = 0;
    if (!core_io_path_exists(k_font_persist_path)) {
        return false;
    }
    read_result = core_io_read_all(k_font_persist_path, &file_data);
    if (read_result.code != CORE_OK || !file_data.data) {
        return false;
    }
    copy_len = file_data.size;
    if (copy_len >= sizeof(preset_name)) {
        copy_len = sizeof(preset_name) - 1;
    }
    if (copy_len == 0u) {
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(preset_name, file_data.data, copy_len);
    preset_name[copy_len] = '\0';
    core_io_buffer_free(&file_data);
    trim_trailing_whitespace(preset_name);
    if (!preset_name[0]) {
        return false;
    }
    return ray_tracing_shared_font_set_preset(preset_name);
}

bool ray_tracing_shared_font_resolve_ui_regular(char* out_path, size_t out_path_size, int* out_point_size) {
    CoreFontPreset preset = {0};
    CoreFontRoleSpec role = {0};
    int tier_size = 0;
    CoreResult r;

    if (!out_path || out_path_size == 0 || !out_point_size ||
        !is_shared_toggle_enabled("RAY_TRACING_USE_SHARED_FONT")) {
        return false;
    }

    font_runtime_init_if_needed();
    r = core_font_get_preset(g_font_runtime_preset, &preset);
    if (r.code != CORE_OK) {
        r = core_font_get_preset(CORE_FONT_PRESET_IDE, &preset);
        if (r.code != CORE_OK) {
            return false;
        }
    }

    r = core_font_resolve_role(&preset, CORE_FONT_ROLE_UI_REGULAR, &role);
    if (r.code != CORE_OK) {
        return false;
    }

    if (!resolve_existing_font_path(&role, out_path, out_path_size) &&
        !copy_existing_path(out_path, out_path_size, "config/default.ttf")) {
        return false;
    }
    r = core_font_point_size_for_tier(&role, CORE_FONT_TEXT_SIZE_BASIC, &tier_size);
    if (r.code == CORE_OK && tier_size > 0) {
        *out_point_size = tier_size;
    } else {
        *out_point_size = role.point_size > 0 ? role.point_size : 24;
    }
    return true;
}

bool ray_tracing_shared_font_set_preset(const char* preset_name) {
    CoreFontPreset preset = {0};
    if (!preset_name || !preset_name[0]) {
        return false;
    }
    if (core_font_get_preset_by_name(preset_name, &preset).code != CORE_OK) {
        return false;
    }
    g_font_runtime_preset = preset.id;
    g_font_runtime_initialized = true;
    return true;
}

bool ray_tracing_shared_font_current_preset(char* out_name, size_t out_name_size) {
    const char* name;
    if (!out_name || out_name_size == 0) {
        return false;
    }
    font_runtime_init_if_needed();
    name = core_font_preset_name(g_font_runtime_preset);
    if (!name || !name[0]) {
        return false;
    }
    strncpy(out_name, name, out_name_size - 1);
    out_name[out_name_size - 1] = '\0';
    return true;
}

bool ray_tracing_shared_font_save_persisted(void) {
    char preset_name[128];
    char payload[144];
    int written;
    if (!ray_tracing_shared_font_current_preset(preset_name, sizeof(preset_name))) {
        return false;
    }
    written = snprintf(payload, sizeof(payload), "%s\n", preset_name);
    if (written <= 0 || (size_t)written >= sizeof(payload)) {
        return false;
    }
    return core_io_write_all(k_font_persist_path, payload, (size_t)written).code == CORE_OK;
}
