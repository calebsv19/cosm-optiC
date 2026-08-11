#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifndef RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY
#define RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY 4096
#endif

#ifndef RAY_TRACING_FOLDER_PICKER_PROMPT_CAPACITY
#define RAY_TRACING_FOLDER_PICKER_PROMPT_CAPACITY 512
#endif

typedef enum {
    RAY_TRACING_FOLDER_PICKER_SELECTED = 0,
    RAY_TRACING_FOLDER_PICKER_CANCELLED,
    RAY_TRACING_FOLDER_PICKER_UNAVAILABLE,
    RAY_TRACING_FOLDER_PICKER_FAILED,
    RAY_TRACING_FOLDER_PICKER_PENDING
} RayTracingFolderPickerResult;

typedef struct {
    pid_t child_pid;
    int output_fd;
    size_t output_used;
    bool active;
    bool select_file;
    bool linux_fallback_started;
    bool output_overflow;
    char output[RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY];
    char prompt[RAY_TRACING_FOLDER_PICKER_PROMPT_CAPACITY];
    char initial_directory[RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY];
} RayTracingFolderPickerRequest;

/*
 * Resolves a picker default to an existing absolute directory. Relative paths
 * are rooted at RAY_TRACING_RUNTIME_DIR when it is absolute, otherwise cwd.
 * Missing leaf components and file paths fall back to their nearest existing
 * parent. Failure leaves out_directory empty so callers can omit a default.
 */
bool RayTracing_FolderPicker_ResolveInitialDirectory(const char *initial_path,
                                                     char *out_directory,
                                                     size_t out_directory_size);

void RayTracing_FolderPicker_RequestInit(RayTracingFolderPickerRequest *request);
bool RayTracing_FolderPicker_Begin(RayTracingFolderPickerRequest *request,
                                   const char *prompt,
                                   const char *initial_directory);
bool RayTracing_FilePicker_Begin(RayTracingFolderPickerRequest *request,
                                 const char *prompt,
                                 const char *initial_path);
RayTracingFolderPickerResult RayTracing_FolderPicker_Poll(
    RayTracingFolderPickerRequest *request,
    char *out_path,
    size_t out_path_size);
void RayTracing_FolderPicker_Cancel(RayTracingFolderPickerRequest *request);

/* Opens the host folder chooser without routing prompt or path text through a shell. */
RayTracingFolderPickerResult RayTracing_FolderPicker_Select(const char *prompt,
                                                            const char *initial_directory,
                                                            char *out_path,
                                                            size_t out_path_size);

/* Opens the host file chooser without routing prompt or path text through a shell. */
RayTracingFolderPickerResult RayTracing_FilePicker_Select(const char *prompt,
                                                          const char *initial_path,
                                                          char *out_path,
                                                          size_t out_path_size);
