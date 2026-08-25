#include "app/ray_tracing_build_identity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ray_tracing_build_identity_format_window_title(char* output,
                                                    size_t output_size,
                                                    const char* base_title) {
    const char* profile = getenv("RAY_TRACING_PACKAGE_PROFILE");
    const char* label = getenv("RAY_TRACING_BUILD_LABEL");

    if (!output || output_size == 0) return;
    if (!base_title) base_title = "optiC";
    if (!profile || !profile[0] || strcmp(profile, "standard") == 0 ||
        !label || !label[0]) {
        snprintf(output, output_size, "%s", base_title);
    } else {
        snprintf(output, output_size, "%s - %s", base_title, label);
    }
    output[output_size - 1] = '\0';
}
