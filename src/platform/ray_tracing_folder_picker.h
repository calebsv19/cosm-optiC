#pragma once

#include <stddef.h>

typedef enum {
    RAY_TRACING_FOLDER_PICKER_SELECTED = 0,
    RAY_TRACING_FOLDER_PICKER_CANCELLED,
    RAY_TRACING_FOLDER_PICKER_UNAVAILABLE,
    RAY_TRACING_FOLDER_PICKER_FAILED
} RayTracingFolderPickerResult;

/* Opens the host folder chooser without routing prompt or path text through a shell. */
RayTracingFolderPickerResult RayTracing_FolderPicker_Select(const char *prompt,
                                                            const char *initial_directory,
                                                            char *out_path,
                                                            size_t out_path_size);
