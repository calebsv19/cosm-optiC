#ifndef APP_MATERIAL_PREVIEW_HEADLESS_H
#define APP_MATERIAL_PREVIEW_HEADLESS_H

#include <stdbool.h>
#include <stddef.h>

#include "app/material_preview_request.h"

bool MaterialPreviewHeadlessRun(const MaterialPreviewRequest* request,
                                char* out_diagnostics,
                                size_t out_diagnostics_size);

#endif
