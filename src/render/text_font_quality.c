#include "render/text_font_quality.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int text_hinting_mono_small_enabled(void) {
    const char* value = getenv("RAY_TRACING_TEXT_HINTING_MONO_SMALL");
    char lowered[16];
    size_t i = 0;

    if (!value || !value[0]) {
        return 0;
    }
    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    return strcmp(lowered, "1") == 0 ||
           strcmp(lowered, "true") == 0 ||
           strcmp(lowered, "yes") == 0 ||
           strcmp(lowered, "on") == 0;
}

void ray_tracing_text_apply_font_quality(TTF_Font* font) {
    int font_height = 0;
    if (!font) {
        return;
    }

    font_height = TTF_FontHeight(font);
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    if (font_height > 0 &&
        font_height <= 14 &&
        text_hinting_mono_small_enabled()) {
        TTF_SetFontHinting(font, TTF_HINTING_MONO);
    }
    TTF_SetFontKerning(font, 1);
}
