#include "render/runtime_native_3d_prepare_diagnostics.h"

#include <stdarg.h>
#include <stdio.h>

static char g_runtime_native_3d_prepare_diagnostics[4096] = "ok";

void RuntimeNative3DPrepareDiagnostics_Reset(void) {
    RuntimeNative3DPrepareDiagnostics_Set("ok");
}

void RuntimeNative3DPrepareDiagnostics_Set(const char* message) {
    snprintf(g_runtime_native_3d_prepare_diagnostics,
             sizeof(g_runtime_native_3d_prepare_diagnostics),
             "%s",
             message ? message : "unknown");
}

void RuntimeNative3DPrepareDiagnostics_SetFormatted(const char* format, ...) {
    va_list args;
    if (!format) {
        RuntimeNative3DPrepareDiagnostics_Set("unknown");
        return;
    }
    va_start(args, format);
    vsnprintf(g_runtime_native_3d_prepare_diagnostics,
              sizeof(g_runtime_native_3d_prepare_diagnostics),
              format,
              args);
    va_end(args);
}

const char* RuntimeNative3DPrepareDiagnostics_Get(void) {
    return g_runtime_native_3d_prepare_diagnostics;
}
