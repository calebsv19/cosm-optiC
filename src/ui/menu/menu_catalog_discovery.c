#include "ui/menu_catalog_discovery.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Process-lifetime slots: a filesystem call may never return. Never join it, reuse
 * its storage, or spawn another copy on refresh. Completed unused slots are freed.
 * Independent roots keep a denied/offline directory from hiding healthy libraries. */
#define DISCOVERY_SLOTS 40
#define DISCOVERY_ENTRIES 128

typedef struct CatalogScan {
    char root[PATH_MAX], manifest[PATH_MAX], runtime[PATH_MAX], volume[PATH_MAX];
    char frame_dir[PATH_MAX], video_path[PATH_MAX];
    RayTracingRenderExportStatus frames;
    bool active, running;
    size_t scene_count, volume_count;
    SceneSourceCatalogEntry scenes[DISCOVERY_ENTRIES];
    VolumeSourceCatalogEntry volumes[DISCOVERY_ENTRIES];
} CatalogScan;

static pthread_mutex_t discovery_mutex = PTHREAD_MUTEX_INITIALIZER;
static CatalogScan *scans[DISCOVERY_SLOTS];
static uint64_t discovery_revision = 1;

static void *scan_catalog(void *context) {
    CatalogScan *scan = context;
    const char *root = scan->root;
    fprintf(stderr, "[catalog] scan begin: %s\n", root[0] ? root : "selected sources");
    if (scan->frame_dir[0]) {
        fprintf(stderr, "[catalog] frame summary begin: %s\n", scan->frame_dir);
        ray_tracing_render_export_describe_paths(scan->frame_dir, scan->video_path, &scan->frames);
    } else {
        scan->scene_count = scene_source_catalog_collect(scan->scenes, DISCOVERY_ENTRIES,
                                                         &root, root[0] ? 1 : 0,
                                                         scan->manifest, scan->runtime);
        scan->volume_count = volume_source_catalog_collect(scan->volumes, DISCOVERY_ENTRIES,
                                                           &root, root[0] ? 1 : 0, scan->volume);
    }
    fprintf(stderr, "[catalog] scan complete: %s\n", root[0] ? root : "selected sources");
    pthread_mutex_lock(&discovery_mutex);
    scan->running = false;
    ++discovery_revision;
    pthread_mutex_unlock(&discovery_mutex);
    return NULL;
}

static void request_scan(const char *root, const char *manifest,
                         const char *runtime, const char *volume,
                         const char *frame_dir, const char *video_path) {
    CatalogScan *scan = NULL;
    size_t slot = DISCOVERY_SLOTS;
    for (size_t i = 0; i < DISCOVERY_SLOTS; ++i) {
        CatalogScan *candidate = scans[i];
        if (candidate && strcmp(candidate->root, root) == 0 &&
            strcmp(candidate->manifest, manifest) == 0 &&
            strcmp(candidate->runtime, runtime) == 0 &&
            strcmp(candidate->volume, volume) == 0 &&
            strcmp(candidate->frame_dir, frame_dir) == 0 &&
            strcmp(candidate->video_path, video_path) == 0) {
            candidate->active = true;
            return; /* Includes blocked scans: refresh cannot multiply them. */
        }
        if (!candidate && slot == DISCOVERY_SLOTS) slot = i;
    }
    if (slot == DISCOVERY_SLOTS) {
        fprintf(stderr, "[catalog] background scan limit reached; skipped: %s\n", root);
        return;
    }
    scan = calloc(1, sizeof(*scan));
    if (!scan) return;
    snprintf(scan->root, sizeof(scan->root), "%s", root);
    snprintf(scan->manifest, sizeof(scan->manifest), "%s", manifest);
    snprintf(scan->runtime, sizeof(scan->runtime), "%s", runtime);
    snprintf(scan->volume, sizeof(scan->volume), "%s", volume);
    snprintf(scan->frame_dir, sizeof(scan->frame_dir), "%s", frame_dir);
    snprintf(scan->video_path, sizeof(scan->video_path), "%s", video_path);
    scan->active = scan->running = true;
    scans[slot] = scan;
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, scan_catalog, scan) != 0) {
        scans[slot] = NULL;
        free(scan);
    }
    pthread_attr_destroy(&attr);
}

void menu_catalog_discovery_request(const char *const *roots, size_t root_count,
                                    const char *manifest, const char *runtime,
                                    const char *volume, const char *frame_dir, const char *video_path) {
    pthread_mutex_lock(&discovery_mutex);
    for (size_t i = 0; i < DISCOVERY_SLOTS; ++i) {
        if (scans[i] && !scans[i]->running) {
            free(scans[i]);
            scans[i] = NULL;
        } else if (scans[i]) {
            scans[i]->active = false;
        }
    }
    /* Explicit selected files get their own slot even if a library is stalled. */
    request_scan("", manifest ? manifest : "", runtime ? runtime : "", volume ? volume : "", "", "");
    if (frame_dir && frame_dir[0]) request_scan("", "", "", "", frame_dir, video_path ? video_path : "");
    for (size_t i = 0; roots && i < root_count; ++i) {
        if (roots[i] && roots[i][0]) request_scan(roots[i], "", "", "", "", "");
    }
    ++discovery_revision;
    pthread_mutex_unlock(&discovery_mutex);
}

bool menu_catalog_discovery_read(uint64_t *revision,
                                 SceneSourceCatalogEntry *scenes, size_t *scene_count,
                                 VolumeSourceCatalogEntry *volumes, size_t *volume_count,
                                 size_t capacity, RayTracingRenderExportStatus *frames, bool *frames_ready) {
    bool changed = false;
    pthread_mutex_lock(&discovery_mutex);
    if (*revision != discovery_revision) {
        *scene_count = *volume_count = 0;
        *frames_ready = false;
        for (size_t i = 0; i < DISCOVERY_SLOTS; ++i) {
            CatalogScan *scan = scans[i];
            if (!scan || !scan->active || scan->running) continue;
            if (scan->frame_dir[0]) { *frames = scan->frames; *frames_ready = true; continue; }
            for (size_t j = 0; j < scan->scene_count && *scene_count < capacity; ++j) {
                bool duplicate = false;
                for (size_t k = 0; k < *scene_count; ++k)
                    if (strcmp(scenes[k].path, scan->scenes[j].path) == 0) duplicate = true;
                if (!duplicate) scenes[(*scene_count)++] = scan->scenes[j];
            }
            for (size_t j = 0; j < scan->volume_count && *volume_count < capacity; ++j) {
                bool duplicate = false;
                for (size_t k = 0; k < *volume_count; ++k)
                    if (strcmp(volumes[k].path, scan->volumes[j].path) == 0) duplicate = true;
                if (!duplicate) volumes[(*volume_count)++] = scan->volumes[j];
            }
        }
        *revision = discovery_revision;
        changed = true;
    }
    pthread_mutex_unlock(&discovery_mutex);
    return changed;
}
