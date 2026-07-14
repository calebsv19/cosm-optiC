#include "platform/ray_tracing_path_opener.h"

#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__) && !defined(RAY_TRACING_PATH_OPENER_FORCE_LINUX)
#define RAY_TRACING_PATH_OPENER_MACOS 1
#else
#define RAY_TRACING_PATH_OPENER_MACOS 0
#endif

static bool path_is_directory(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static RayTracingPathOpenerResult run_opener(const char *const argv[]) {
    pid_t child = fork();
    int wait_status = 0;
    if (child < 0) return RAY_TRACING_PATH_OPENER_FAILED;
    if (child == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(errno == ENOENT ? 127 : 126);
    }
    if (waitpid(child, &wait_status, 0) < 0 || !WIFEXITED(wait_status)) {
        return RAY_TRACING_PATH_OPENER_FAILED;
    }
    if (WEXITSTATUS(wait_status) == 0) return RAY_TRACING_PATH_OPENER_OPENED;
    if (WEXITSTATUS(wait_status) == 127) return RAY_TRACING_PATH_OPENER_UNAVAILABLE;
    return RAY_TRACING_PATH_OPENER_FAILED;
}

RayTracingPathOpenerResult RayTracing_PathOpener_OpenDirectory(const char *path) {
    if (!path_is_directory(path)) return RAY_TRACING_PATH_OPENER_INVALID_PATH;
#if RAY_TRACING_PATH_OPENER_MACOS
    {
        const char *argv[] = {"/usr/bin/open", path, NULL};
        return run_opener(argv);
    }
#elif defined(__linux__) || defined(RAY_TRACING_PATH_OPENER_FORCE_LINUX)
    {
        const char *xdg_argv[] = {"xdg-open", path, NULL};
        const char *gio_argv[] = {"gio", "open", path, NULL};
        RayTracingPathOpenerResult result = run_opener(xdg_argv);
        if (result != RAY_TRACING_PATH_OPENER_UNAVAILABLE) return result;
        return run_opener(gio_argv);
    }
#else
    return RAY_TRACING_PATH_OPENER_UNAVAILABLE;
#endif
}
