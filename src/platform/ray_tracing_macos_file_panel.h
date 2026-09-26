#ifndef RAY_TRACING_MACOS_FILE_PANEL_H
#define RAY_TRACING_MACOS_FILE_PANEL_H

#include "platform/ray_tracing_folder_picker.h"

/* Run on the app's main thread. The panel returns a path only after Open. */
RayTracingFolderPickerResult RayTracing_MacOSFilePanelSelect(const char *prompt,
                                                             const char *initial_directory,
                                                             char *out_path,
                                                             size_t out_path_size);

#endif
