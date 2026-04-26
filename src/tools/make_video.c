#include "tools/make_video.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config/config_file_io.h"

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} FrameNameList;

static bool path_points_to_executable(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return access(path, X_OK) == 0;
}

bool ResolveFFmpegBinary(char *out, size_t out_size) {
    static const char *const direct_candidates[] = {
        "/opt/homebrew/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/usr/bin/ffmpeg"
    };
    char path_copy[4096];
    const char *path_env = getenv("PATH");
    const char *env_override = getenv("RAY_TRACING_FFMPEG_BIN");
    char *cursor = NULL;

    if (!out || out_size == 0u) return false;
    out[0] = '\0';

    if (env_override && env_override[0] && path_points_to_executable(env_override)) {
        if (snprintf(out, out_size, "%s", env_override) >= (int)out_size) return false;
        return true;
    }

    for (size_t i = 0; i < sizeof(direct_candidates) / sizeof(direct_candidates[0]); ++i) {
        if (!path_points_to_executable(direct_candidates[i])) continue;
        if (snprintf(out, out_size, "%s", direct_candidates[i]) >= (int)out_size) return false;
        return true;
    }

    if (!path_env || !path_env[0]) return false;
    if (snprintf(path_copy, sizeof(path_copy), "%s", path_env) >= (int)sizeof(path_copy)) {
        return false;
    }
    cursor = path_copy;
    while (cursor && *cursor) {
        char *sep = strchr(cursor, ':');
        char candidate[PATH_MAX];
        if (sep) *sep = '\0';
        if (cursor[0]) {
            if (snprintf(candidate, sizeof(candidate), "%s/ffmpeg", cursor) < (int)sizeof(candidate) &&
                path_points_to_executable(candidate)) {
                if (snprintf(out, out_size, "%s", candidate) >= (int)out_size) return false;
                return true;
            }
        }
        if (!sep) break;
        cursor = sep + 1;
    }
    return false;
}

static bool is_frame_dump_name(const char *name) {
    size_t len = 0;
    if (!name) return false;
    if (strncmp(name, "frame_", 6) != 0) return false;
    len = strlen(name);
    if (len < 11) return false;
    return strcmp(name + len - 4, ".bmp") == 0;
}

static int frame_name_compare(const void *lhs, const void *rhs) {
    const char *const *a = (const char *const *)lhs;
    const char *const *b = (const char *const *)rhs;
    return strcmp(*a, *b);
}

static void frame_name_list_free(FrameNameList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool frame_name_list_push(FrameNameList *list, const char *name) {
    char **next_items = NULL;
    size_t next_capacity = 0;
    size_t name_len = 0;
    if (!list || !name) return false;
    if (list->count == list->capacity) {
        next_capacity = (list->capacity == 0u) ? 8u : (list->capacity * 2u);
        next_items = (char **)realloc(list->items, next_capacity * sizeof(char *));
        if (!next_items) return false;
        list->items = next_items;
        list->capacity = next_capacity;
    }
    name_len = strlen(name);
    list->items[list->count] = (char *)malloc(name_len + 1u);
    if (!list->items[list->count]) return false;
    memcpy(list->items[list->count], name, name_len + 1u);
    list->count += 1u;
    return true;
}

static bool collect_frame_names(const char *frame_dir, FrameNameList *out_list) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!frame_dir || !frame_dir[0] || !out_list) return false;
    memset(out_list, 0, sizeof(*out_list));
    dir = opendir(frame_dir);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        if (!is_frame_dump_name(entry->d_name)) continue;
        if (!frame_name_list_push(out_list, entry->d_name)) {
            closedir(dir);
            frame_name_list_free(out_list);
            return false;
        }
    }
    closedir(dir);
    if (out_list->count == 0u) return false;
    qsort(out_list->items, out_list->count, sizeof(char *), frame_name_compare);
    return true;
}

static int renumber_frames(const char *frame_dir) {
    FrameNameList frames = {0};
    int status = 1;
    if (!collect_frame_names(frame_dir, &frames)) {
        fprintf(stderr, "Error: No frames found in '%s'.\n", frame_dir ? frame_dir : "(null)");
        return 1;
    }
    for (size_t i = 0; i < frames.count; ++i) {
        char src[PATH_MAX];
        char tmp[PATH_MAX];
        if (snprintf(src, sizeof(src), "%s/%s", frame_dir, frames.items[i]) >= (int)sizeof(src) ||
            snprintf(tmp, sizeof(tmp), "%s/.tmp_frame_%04zu.bmp", frame_dir, i) >= (int)sizeof(tmp)) {
            fprintf(stderr, "Error: Frame path too long while renumbering.\n");
            goto cleanup;
        }
        if (rename(src, tmp) != 0) {
            fprintf(stderr, "Error: Failed to move %s -> %s (%s)\n", src, tmp, strerror(errno));
            goto cleanup;
        }
    }
    for (size_t i = 0; i < frames.count; ++i) {
        char tmp[PATH_MAX];
        char dst[PATH_MAX];
        if (snprintf(tmp, sizeof(tmp), "%s/.tmp_frame_%04zu.bmp", frame_dir, i) >= (int)sizeof(tmp) ||
            snprintf(dst, sizeof(dst), "%s/frame_%04zu.bmp", frame_dir, i) >= (int)sizeof(dst)) {
            fprintf(stderr, "Error: Frame path too long while finalizing renumber.\n");
            goto cleanup;
        }
        if (rename(tmp, dst) != 0) {
            fprintf(stderr, "Error: Failed to move %s -> %s (%s)\n", tmp, dst, strerror(errno));
            goto cleanup;
        }
    }
    status = 0;

cleanup:
    frame_name_list_free(&frames);
    return status;
}

static int normalize_process_exit_status(int raw_status) {
    if (raw_status == -1) {
        return 1;
    }
    if (WIFEXITED(raw_status)) {
        return WEXITSTATUS(raw_status);
    }
    if (WIFSIGNALED(raw_status)) {
        return 128 + WTERMSIG(raw_status);
    }
    return raw_status;
}

static void maybe_emit_progress(MakeVideoProgressCallback progress_cb,
                                void *user_data,
                                size_t current_frame,
                                size_t total_frames,
                                int *last_percent) {
    int percent_complete = 0;
    if (!progress_cb || total_frames == 0u) {
        return;
    }
    if (current_frame > total_frames) {
        current_frame = total_frames;
    }
    percent_complete = (int)((current_frame * 100u) / total_frames);
    if (percent_complete > 100) {
        percent_complete = 100;
    }
    if (last_percent && *last_percent == percent_complete && current_frame < total_frames) {
        return;
    }
    if (last_percent) {
        *last_percent = percent_complete;
    }
    progress_cb(current_frame, total_frames, percent_complete, user_data);
}

int MakeVideoFromFrames(const char *frameDir,
                        const char *outputFile,
                        int fps) {
    return MakeVideoFromFramesWithProgress(frameDir, outputFile, fps, NULL, NULL);
}

int MakeVideoFromFramesWithProgress(const char *frameDir,
                                    const char *outputFile,
                                    int fps,
                                    MakeVideoProgressCallback progress_cb,
                                    void *user_data) {
    struct stat st;
    FrameNameList frames = {0};
    char command[(PATH_MAX * 4) + 256];
    char ffmpeg_bin[PATH_MAX];
    FILE *pipe = NULL;
    int raw_status = 0;
    int last_percent = -1;
    size_t total_frames = 0u;

    if (!frameDir || !frameDir[0] || !outputFile || !outputFile[0]) {
        fprintf(stderr, "Error: MakeVideoFromFrames requires frameDir and outputFile.\n");
        return 1;
    }
    if (fps <= 0) fps = 30;

    if (stat(frameDir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Frames directory '%s' does not exist.\n", frameDir);
        return 1;
    }
    if (!config_io_ensure_parent_directory_for_file(outputFile)) {
        fprintf(stderr, "Error: Failed to prepare output path '%s'.\n", outputFile);
        return 1;
    }
    if (!ResolveFFmpegBinary(ffmpeg_bin, sizeof(ffmpeg_bin))) {
        fprintf(stderr, "Error: ffmpeg is not available.\n");
        return 1;
    }

    printf("Checking frames directory: %s\n", frameDir);
    printf("Ensuring frames are sequentially numbered...\n");
    if (!collect_frame_names(frameDir, &frames)) {
        fprintf(stderr, "Error: No frames found in '%s'.\n", frameDir);
        return 1;
    }
    total_frames = frames.count;
    if (renumber_frames(frameDir) != 0) {
        fprintf(stderr, "Error: Failed to rename frames.\n");
        frame_name_list_free(&frames);
        return 1;
    }

    if (snprintf(command,
                 sizeof(command),
                 "\"%s\" -y -start_number 0 -framerate %d -i \"%s/frame_%%04d.bmp\" -c:v libx264 -pix_fmt yuv420p -nostats -progress pipe:1 \"%s\" 2>/dev/null",
                 ffmpeg_bin,
                 fps,
                 frameDir,
                 outputFile) >= (int)sizeof(command)) {
        fprintf(stderr, "Error: FFmpeg command path too long.\n");
        frame_name_list_free(&frames);
        return 1;
    }

    printf("Executing command: %s\n", command);
    maybe_emit_progress(progress_cb, user_data, 0u, total_frames, &last_percent);
    pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "FFmpeg command failed to start.\n");
        frame_name_list_free(&frames);
        return 1;
    }
    while (!feof(pipe)) {
        char line[256];
        if (!fgets(line, sizeof(line), pipe)) {
            break;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "frame=", 6) == 0) {
            long parsed = strtol(line + 6, NULL, 10);
            if (parsed > 0) {
                maybe_emit_progress(progress_cb,
                                    user_data,
                                    (size_t)parsed,
                                    total_frames,
                                    &last_percent);
            }
        } else if (strcmp(line, "progress=end") == 0) {
            maybe_emit_progress(progress_cb, user_data, total_frames, total_frames, &last_percent);
        }
    }
    raw_status = pclose(pipe);
    frame_name_list_free(&frames);
    raw_status = normalize_process_exit_status(raw_status);
    if (raw_status != 0) {
        fprintf(stderr, "FFmpeg command failed with return code %d\n", raw_status);
        return raw_status;
    }

    printf("Video successfully created: %s\n", outputFile);
    return 0;
}
