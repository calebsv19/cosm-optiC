#include "platform/ray_tracing_folder_picker.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__APPLE__) && !defined(RAY_TRACING_FOLDER_PICKER_FORCE_LINUX)
#define RAY_TRACING_FOLDER_PICKER_MACOS 1
#else
#define RAY_TRACING_FOLDER_PICKER_MACOS 0
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void trim_dialog_newline(char *text) {
    size_t length = 0u;
    if (!text) return;
    length = strlen(text);
    while (length > 0u && (text[length - 1u] == '\n' || text[length - 1u] == '\r')) {
        text[--length] = '\0';
    }
}

static void reset_request(RayTracingFolderPickerRequest *request) {
    if (!request) return;
    request->child_pid = 0;
    request->output_fd = -1;
    request->output_used = 0u;
    request->active = false;
    request->select_file = false;
    request->linux_fallback_started = false;
    request->output_overflow = false;
    request->output[0] = '\0';
    request->prompt[0] = '\0';
    request->initial_directory[0] = '\0';
}

void RayTracing_FolderPicker_RequestInit(RayTracingFolderPickerRequest *request) {
    reset_request(request);
}

static bool copy_text(char *destination, size_t destination_size, const char *source) {
    int written = 0;
    if (!destination || destination_size == 0u) return false;
    if (!source) source = "";
    written = snprintf(destination, destination_size, "%s", source);
    return written >= 0 && (size_t)written < destination_size;
}

static bool path_is_directory(const char *path) {
    struct stat info;
    return path && path[0] && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static bool make_absolute_candidate(const char *initial_path,
                                    char *out_path,
                                    size_t out_path_size) {
    const char *runtime_root = getenv("RAY_TRACING_RUNTIME_DIR");
    char cwd[PATH_MAX];
    const char *base = NULL;
    int written = 0;
    if (!initial_path || !initial_path[0] || !out_path || out_path_size == 0u) return false;
    if (initial_path[0] == '/') return copy_text(out_path, out_path_size, initial_path);
    if (runtime_root && runtime_root[0] == '/' && path_is_directory(runtime_root)) {
        base = runtime_root;
    } else {
        if (!getcwd(cwd, sizeof(cwd))) return false;
        base = cwd;
    }
    written = snprintf(out_path, out_path_size, "%s/%s", base, initial_path);
    return written >= 0 && (size_t)written < out_path_size;
}

bool RayTracing_FolderPicker_ResolveInitialDirectory(const char *initial_path,
                                                     char *out_directory,
                                                     size_t out_directory_size) {
    char candidate[PATH_MAX];
    char resolved[PATH_MAX];
    char *slash = NULL;
    if (!out_directory || out_directory_size == 0u) return false;
    out_directory[0] = '\0';
    if (!make_absolute_candidate(initial_path, candidate, sizeof(candidate))) return false;
    while (!path_is_directory(candidate)) {
        slash = strrchr(candidate, '/');
        if (!slash) return false;
        if (slash == candidate) {
            candidate[1] = '\0';
        } else {
            *slash = '\0';
        }
        if (candidate[0] == '/' && candidate[1] == '\0') break;
    }
    if (!path_is_directory(candidate) || !realpath(candidate, resolved)) return false;
    return copy_text(out_directory, out_directory_size, resolved);
}

static bool start_picker_process(RayTracingFolderPickerRequest *request,
                                 const char *const argv[]) {
    int pipe_fds[2] = {-1, -1};
    pid_t child = 0;
    int flags = 0;

    if (!request || request->active || pipe(pipe_fds) != 0) return false;
    child = fork();
    if (child < 0) {
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return false;
    }
    if (child == 0) {
        (void)close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);
        (void)close(pipe_fds[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(errno == ENOENT ? 127 : 126);
    }

    (void)close(pipe_fds[1]);
    flags = fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) != 0) {
        (void)close(pipe_fds[0]);
        (void)kill(child, SIGTERM);
        (void)waitpid(child, NULL, 0);
        return false;
    }
    request->child_pid = child;
    request->output_fd = pipe_fds[0];
    request->output_used = 0u;
    request->output[0] = '\0';
    request->active = true;
    return true;
}

#if RAY_TRACING_FOLDER_PICKER_MACOS
static bool escape_applescript_literal(const char *input, char *output, size_t output_size) {
    size_t out_index = 0u;
    if (!input || !output || output_size == 0u) return false;
    output[0] = '\0';
    for (size_t index = 0u; input[index] != '\0'; ++index) {
        const char character = input[index];
        if ((character == '\\' || character == '"') && out_index + 2u >= output_size) return false;
        if (character != '\\' && character != '"' && out_index + 1u >= output_size) return false;
        if (character == '\\' || character == '"') output[out_index++] = '\\';
        output[out_index++] = character;
    }
    output[out_index] = '\0';
    return true;
}

static bool begin_macos_path(RayTracingFolderPickerRequest *request,
                             bool select_file,
                             const char *prompt,
                             const char *initial_path) {
    char escaped_prompt[512];
    char escaped_path[2048];
    char resolved_directory[PATH_MAX];
    char script[3072];
    const char *argv[4];
    const char *chooser = select_file ? "choose file" : "choose folder";

    if (!escape_applescript_literal(prompt, escaped_prompt, sizeof(escaped_prompt))) {
        return false;
    }
    if (RayTracing_FolderPicker_ResolveInitialDirectory(initial_path,
                                                       resolved_directory,
                                                       sizeof(resolved_directory))) {
        if (!escape_applescript_literal(resolved_directory, escaped_path, sizeof(escaped_path))) {
            return false;
        }
        snprintf(script, sizeof(script),
                 "POSIX path of (%s with prompt \"%s\" default location POSIX file \"%s\")",
                 chooser, escaped_prompt, escaped_path);
    } else {
        snprintf(script, sizeof(script),
                 "POSIX path of (%s with prompt \"%s\")", chooser, escaped_prompt);
    }
    argv[0] = "/usr/bin/osascript";
    argv[1] = "-e";
    argv[2] = script;
    argv[3] = NULL;
    return start_picker_process(request, argv);
}
#else
static bool begin_linux_path(RayTracingFolderPickerRequest *request,
                             bool use_kdialog) {
    const char *prompt = request->prompt;
    const char *initial_path = request->initial_directory;
    const char *zenity_argv[8] = {"zenity", "--file-selection", "--title", prompt, NULL, NULL, NULL, NULL};
    const char *kdialog_argv[6] = {"kdialog", NULL, NULL, "--title", prompt, NULL};
    size_t zenity_index = 4u;

    if (!request->select_file) zenity_argv[zenity_index++] = "--directory";
    if (initial_path && initial_path[0]) {
        zenity_argv[zenity_index++] = "--filename";
        zenity_argv[zenity_index] = initial_path;
    }
    kdialog_argv[1] = request->select_file ? "--getopenfilename" : "--getexistingdirectory";
    kdialog_argv[2] = initial_path && initial_path[0] ? initial_path : ".";
    if (use_kdialog) request->linux_fallback_started = true;
    return start_picker_process(request, use_kdialog ? kdialog_argv : zenity_argv);
}
#endif

static bool begin_picker(RayTracingFolderPickerRequest *request,
                         bool select_file,
                         const char *prompt,
                         const char *initial_path) {
    if (!request || request->active || !prompt || !prompt[0]) return false;
    reset_request(request);
    request->select_file = select_file;
    if (!copy_text(request->prompt, sizeof(request->prompt), prompt) ||
        !copy_text(request->initial_directory,
                   sizeof(request->initial_directory),
                   initial_path ? initial_path : "")) {
        reset_request(request);
        return false;
    }
#if RAY_TRACING_FOLDER_PICKER_MACOS
    if (!begin_macos_path(request, select_file, prompt, initial_path)) {
        reset_request(request);
        return false;
    }
#elif defined(__linux__) || defined(RAY_TRACING_FOLDER_PICKER_FORCE_LINUX)
    if (!begin_linux_path(request, false)) {
        reset_request(request);
        return false;
    }
#else
    (void)initial_path;
    reset_request(request);
    return false;
#endif
    return true;
}

bool RayTracing_FolderPicker_Begin(RayTracingFolderPickerRequest *request,
                                   const char *prompt,
                                   const char *initial_directory) {
    return begin_picker(request, false, prompt, initial_directory);
}

bool RayTracing_FilePicker_Begin(RayTracingFolderPickerRequest *request,
                                 const char *prompt,
                                 const char *initial_path) {
    return begin_picker(request, true, prompt, initial_path);
}

static RayTracingFolderPickerResult finish_process(RayTracingFolderPickerRequest *request,
                                                   int wait_status,
                                                   char *out_path,
                                                   size_t out_path_size) {
    RayTracingFolderPickerResult result = RAY_TRACING_FOLDER_PICKER_FAILED;
    trim_dialog_newline(request->output);
    if (!request->output_overflow && WIFEXITED(wait_status)) {
        const int exit_status = WEXITSTATUS(wait_status);
        if (exit_status == 0) {
            result = request->output[0] ? RAY_TRACING_FOLDER_PICKER_SELECTED
                                        : RAY_TRACING_FOLDER_PICKER_CANCELLED;
        } else if (exit_status == 1) {
            result = RAY_TRACING_FOLDER_PICKER_CANCELLED;
        } else if (exit_status == 127) {
            result = RAY_TRACING_FOLDER_PICKER_UNAVAILABLE;
        }
    }
    if (result == RAY_TRACING_FOLDER_PICKER_SELECTED) {
        if (!out_path || out_path_size < 2u ||
            !copy_text(out_path, out_path_size, request->output)) {
            result = RAY_TRACING_FOLDER_PICKER_FAILED;
        }
    }
    reset_request(request);
    return result;
}

RayTracingFolderPickerResult RayTracing_FolderPicker_Poll(
    RayTracingFolderPickerRequest *request,
    char *out_path,
    size_t out_path_size) {
    ssize_t bytes_read = 0;
    char discard[512];
    int wait_status = 0;
    pid_t waited = 0;
    if (!request || !request->active) return RAY_TRACING_FOLDER_PICKER_FAILED;
    while (request->output_used + 1u < sizeof(request->output)) {
        bytes_read = read(request->output_fd,
                          request->output + request->output_used,
                          sizeof(request->output) - request->output_used - 1u);
        if (bytes_read > 0) {
            request->output_used += (size_t)bytes_read;
            request->output[request->output_used] = '\0';
            continue;
        }
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            RayTracing_FolderPicker_Cancel(request);
            return RAY_TRACING_FOLDER_PICKER_FAILED;
        }
        break;
    }
    if (request->output_used + 1u >= sizeof(request->output)) {
        while ((bytes_read = read(request->output_fd, discard, sizeof(discard))) > 0) {
            request->output_overflow = true;
        }
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            RayTracing_FolderPicker_Cancel(request);
            return RAY_TRACING_FOLDER_PICKER_FAILED;
        }
    }
    waited = waitpid(request->child_pid, &wait_status, WNOHANG);
    if (waited == 0) return RAY_TRACING_FOLDER_PICKER_PENDING;
    if (waited < 0) {
        RayTracing_FolderPicker_Cancel(request);
        return RAY_TRACING_FOLDER_PICKER_FAILED;
    }
    while (request->output_used + 1u < sizeof(request->output) &&
           (bytes_read = read(request->output_fd,
                              request->output + request->output_used,
                              sizeof(request->output) - request->output_used - 1u)) > 0) {
        request->output_used += (size_t)bytes_read;
    }
    while ((bytes_read = read(request->output_fd, discard, sizeof(discard))) > 0) {
        request->output_overflow = true;
    }
    request->output[request->output_used] = '\0';
    (void)close(request->output_fd);
    request->output_fd = -1;
#if !RAY_TRACING_FOLDER_PICKER_MACOS && (defined(__linux__) || defined(RAY_TRACING_FOLDER_PICKER_FORCE_LINUX))
    if (WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 127 &&
        !request->linux_fallback_started) {
        request->active = false;
        return begin_linux_path(request, true) ? RAY_TRACING_FOLDER_PICKER_PENDING
                                               : RAY_TRACING_FOLDER_PICKER_FAILED;
    }
#endif
    return finish_process(request, wait_status, out_path, out_path_size);
}

void RayTracing_FolderPicker_Cancel(RayTracingFolderPickerRequest *request) {
    int attempts = 0;
    pid_t waited = 0;
    if (!request) return;
    if (request->output_fd >= 0) (void)close(request->output_fd);
    if (request->active && request->child_pid > 0) {
        (void)kill(request->child_pid, SIGTERM);
        do {
            waited = waitpid(request->child_pid, NULL, WNOHANG);
            if (waited != 0) break;
            (void)usleep(1000u);
        } while (++attempts < 100);
        if (waited == 0) {
            (void)kill(request->child_pid, SIGKILL);
            (void)waitpid(request->child_pid, NULL, 0);
        }
    }
    reset_request(request);
}

static RayTracingFolderPickerResult select_blocking(bool select_file,
                                                    const char *prompt,
                                                    const char *initial_path,
                                                    char *out_path,
                                                    size_t out_path_size) {
    RayTracingFolderPickerRequest request;
    RayTracingFolderPickerResult result = RAY_TRACING_FOLDER_PICKER_PENDING;
    if (!out_path || out_path_size < 2u) return RAY_TRACING_FOLDER_PICKER_FAILED;
    out_path[0] = '\0';
    RayTracing_FolderPicker_RequestInit(&request);
    if (!begin_picker(&request, select_file, prompt, initial_path)) {
#if !RAY_TRACING_FOLDER_PICKER_MACOS && !defined(__linux__) && !defined(RAY_TRACING_FOLDER_PICKER_FORCE_LINUX)
        return RAY_TRACING_FOLDER_PICKER_UNAVAILABLE;
#else
        return RAY_TRACING_FOLDER_PICKER_FAILED;
#endif
    }
    while (result == RAY_TRACING_FOLDER_PICKER_PENDING) {
        result = RayTracing_FolderPicker_Poll(&request, out_path, out_path_size);
        if (result == RAY_TRACING_FOLDER_PICKER_PENDING) (void)usleep(1000u);
    }
    if (result != RAY_TRACING_FOLDER_PICKER_SELECTED) out_path[0] = '\0';
    return result;
}

RayTracingFolderPickerResult RayTracing_FolderPicker_Select(const char *prompt,
                                                            const char *initial_directory,
                                                            char *out_path,
                                                            size_t out_path_size) {
    return select_blocking(false, prompt, initial_directory, out_path, out_path_size);
}

RayTracingFolderPickerResult RayTracing_FilePicker_Select(const char *prompt,
                                                          const char *initial_path,
                                                          char *out_path,
                                                          size_t out_path_size) {
    return select_blocking(true, prompt, initial_path, out_path, out_path_size);
}
