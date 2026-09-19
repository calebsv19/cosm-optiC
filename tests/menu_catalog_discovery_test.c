#define _POSIX_C_SOURCE 200809L
#include "ui/menu_catalog_discovery.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static int blocked_calls;
static bool release_scan;

/* Model a directory call that never completes until explicitly released. Other
 * roots must still publish, refresh must not duplicate it, and no UI state may
 * be retained by a late completion. The real collectors have separate fixtures. */
size_t scene_source_catalog_collect(SceneSourceCatalogEntry *out, size_t capacity,
                                    const char *const *roots, size_t count,
                                    const char *manifest, const char *runtime) {
    (void)manifest; (void)runtime;
    if (!count || !capacity) return 0;
    if (strcmp(roots[0], "blocked") == 0) {
        pthread_mutex_lock(&gate);
        ++blocked_calls;
        while (!release_scan) pthread_cond_wait(&condition, &gate);
        pthread_mutex_unlock(&gate);
    }
    snprintf(out[0].path, sizeof(out[0].path), "%s/scene_runtime.json", roots[0]);
    out[0].source = 2;
    return 1;
}
size_t volume_source_catalog_collect(VolumeSourceCatalogEntry *out, size_t capacity,
                                     const char *const *roots, size_t count,
                                     const char *selected) {
    (void)out; (void)capacity; (void)roots; (void)count; (void)selected;
    return 0;
}
static bool wait_for_path(uint64_t *revision, const char *path) {
    SceneSourceCatalogEntry scenes[8];
    VolumeSourceCatalogEntry volumes[8];
    for (int attempt = 0; attempt < 1000; ++attempt) {
        size_t scene_count, volume_count;
        RayTracingRenderExportStatus frames; bool frames_ready;
        if (menu_catalog_discovery_read(revision, scenes, &scene_count, volumes, &volume_count, 8, &frames, &frames_ready)) {
            for (size_t i = 0; i < scene_count; ++i)
                if (strcmp(scenes[i].path, path) == 0) return true;
        }
        nanosleep(&(struct timespec){.tv_nsec = 1000000}, NULL);
    }
    return false;
}
bool ray_tracing_render_export_describe_paths(const char *frame_dir, const char *video_path,
                                               RayTracingRenderExportStatus *status) {
    (void)video_path;
    pthread_mutex_lock(&gate);
    if (strcmp(frame_dir, "blocked-frames") == 0)
        while (!release_scan) pthread_cond_wait(&condition, &gate);
    pthread_mutex_unlock(&gate);
    memset(status, 0, sizeof(*status)); status->frame_count = 7; return true;
}
int main(void) {
    const char *roots[] = {"blocked", "healthy"};
    uint64_t revision = 0;
    menu_catalog_discovery_request(roots, 2, "", "", "", "", "");
    assert(wait_for_path(&revision, "healthy/scene_runtime.json"));
    for (int i = 0; i < 20; ++i) menu_catalog_discovery_request(roots, 2, "", "", "", "", "");
    assert(wait_for_path(&revision, "healthy/scene_runtime.json"));
    pthread_mutex_lock(&gate);
    assert(blocked_calls == 1);
    pthread_mutex_unlock(&gate);
    const char *replacement[] = {"replacement"};
    menu_catalog_discovery_request(replacement, 1, "", "", "", "", "");
    assert(wait_for_path(&revision, "replacement/scene_runtime.json"));
    pthread_mutex_lock(&gate);
    release_scan = true;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&gate);
    nanosleep(&(struct timespec){.tv_nsec = 20000000}, NULL);
    SceneSourceCatalogEntry scenes[8]; VolumeSourceCatalogEntry volumes[8];
    size_t scene_count = 0, volume_count = 0;
    RayTracingRenderExportStatus frames; bool frames_ready;
    revision = 0;
    assert(menu_catalog_discovery_read(&revision, scenes, &scene_count, volumes, &volume_count, 8, &frames, &frames_ready));
    assert(scene_count == 1 && strcmp(scenes[0].path, "replacement/scene_runtime.json") == 0);
    /* Exiting with an outstanding filesystem scan must not require a join. */
    pthread_mutex_lock(&gate); release_scan = false; pthread_mutex_unlock(&gate);
    menu_catalog_discovery_request(roots, 2, "", "", "", "blocked-frames", "output.mp4");
    assert(wait_for_path(&revision, "healthy/scene_runtime.json"));
    revision = 0;
    assert(menu_catalog_discovery_read(&revision, scenes, &scene_count, volumes, &volume_count, 8, &frames, &frames_ready));
    assert(!frames_ready);
    puts("menu catalog discovery: blocked root, healthy discovery, bounded refresh, stale completion, exit passed");
    return 0;
}
