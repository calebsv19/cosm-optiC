#ifndef RENDER_RUNTIME_NATIVE_3D_PREPARE_DIAGNOSTICS_H
#define RENDER_RUNTIME_NATIVE_3D_PREPARE_DIAGNOSTICS_H

void RuntimeNative3DPrepareDiagnostics_Reset(void);
void RuntimeNative3DPrepareDiagnostics_Set(const char* message);
void RuntimeNative3DPrepareDiagnostics_SetFormatted(const char* format, ...);
const char* RuntimeNative3DPrepareDiagnostics_Get(void);

#endif
