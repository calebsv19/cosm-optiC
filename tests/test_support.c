#include "test_support.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char* kRuntimeSceneConfigPath = "data/runtime/scene_config.json";
const char* kRuntimeAnimationConfigPath = "data/runtime/animation_config.json";

static int g_test_support_failures = 0;

void test_support_reset_failures(void) {
    g_test_support_failures = 0;
}

int test_support_failures(void) {
    return g_test_support_failures;
}

int test_rect_right(const SDL_Rect* rect) {
    return rect ? rect->x + rect->w : 0;
}

int test_rect_bottom(const SDL_Rect* rect) {
    return rect ? rect->y + rect->h : 0;
}

void assert_close(const char* name, double a, double b, double tol) {
    if (fabs(a - b) > tol) {
        printf("FAIL %-32s got=%.6f expected=%.6f (tol=%.6f)\n", name, a, b, tol);
        g_test_support_failures += 1;
    }
}

void assert_true(const char* name, bool cond) {
    if (!cond) {
        printf("FAIL %-32s condition=false\n", name);
        g_test_support_failures += 1;
    }
}

char* read_text_file_alloc(const char* path, size_t* out_size) {
    if (out_size) *out_size = 0;
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buffer = (char*)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';
    if (out_size) *out_size = (size_t)size;
    return buffer;
}

bool write_text_file(const char* path, const char* text) {
    if (!path || !text) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    return written == len;
}

bool path_exists(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0;
}

void restore_env_or_unset(const char* name, const char* value) {
    if (!name) return;
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
}

cJSON* find_named_dataset_entry(cJSON* items, const char* name) {
    if (!cJSON_IsArray(items) || !name) return NULL;
    cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, items) {
        cJSON* entry_name = cJSON_GetObjectItem(entry, "name");
        if (cJSON_IsString(entry_name) && strcmp(entry_name->valuestring, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

bool path_list_contains(const char* const* paths, size_t count, const char* needle) {
    if (!paths || !needle || !needle[0]) return false;
    for (size_t i = 0; i < count; ++i) {
        if (paths[i] && strcmp(paths[i], needle) == 0) {
            return true;
        }
    }
    return false;
}

bool catalog_contains_path_source(const SceneSourceCatalogEntry* entries,
                                  size_t count,
                                  const char* path,
                                  int source) {
    char resolved[PATH_MAX];
    const char* needle = path;
    if (!entries || !path || !path[0]) return false;
    if (realpath(path, resolved)) {
        needle = resolved;
    }
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].source == source && strcmp(entries[i].path, needle) == 0) {
            return true;
        }
    }
    return false;
}

bool catalog_contains_path_any_source(const SceneSourceCatalogEntry* entries,
                                      size_t count,
                                      const char* path) {
    char resolved[PATH_MAX];
    const char* needle = path;
    if (!entries || !path || !path[0]) return false;
    if (realpath(path, resolved)) {
        needle = resolved;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].path, needle) == 0) {
            return true;
        }
    }
    return false;
}

size_t catalog_count_source(const SceneSourceCatalogEntry* entries,
                            size_t count,
                            int source) {
    size_t total = 0;
    if (!entries) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].source == source) total += 1;
    }
    return total;
}

int digest_count_kind(const RuntimeSceneBridge3DDigestState* digest,
                      RuntimeSceneBridgePrimitiveKind kind) {
    int total = 0;
    if (!digest) return 0;
    for (int i = 0; i < digest->primitive_count; ++i) {
        if (digest->primitives[i].kind == kind) total += 1;
    }
    return total;
}

const RuntimeSceneBridgePrimitiveSeed* find_primitive_seed_by_object_id(
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    const char* object_id) {
    if (!seed_state || !object_id || !object_id[0]) return NULL;
    for (int i = 0; i < seed_state->primitive_count; ++i) {
        if (strcmp(seed_state->primitives[i].object_id, object_id) == 0) {
            return &seed_state->primitives[i];
        }
    }
    return NULL;
}

int find_runtime_primitive_index_by_object_id(const RuntimeScene3D* scene,
                                              const char* object_id) {
    if (!scene || !object_id || !object_id[0]) return -1;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (strcmp(scene->primitives[i].source.objectId, object_id) == 0) {
            return i;
        }
    }
    return -1;
}

int count_runtime_triangles_for_primitive(const RuntimeScene3D* scene,
                                          int primitive_index) {
    int total = 0;
    if (!scene || primitive_index < 0) return 0;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        if (scene->triangleMesh.triangles[i].primitiveIndex == primitive_index) total += 1;
    }
    return total;
}

double expected_camera_pitch_for_t(const Path* camera_path,
                                   const CameraPath3D* camera_path3d,
                                   double normalized_t) {
    int segment = 0;
    int next = 0;
    double local_t = 0.0;
    double pitch0 = 0.0;
    double pitch1 = 0.0;
    if (!camera_path || !camera_path3d || camera_path->numPoints <= 0) {
        return 0.0;
    }
    if (camera_path->numPoints == 1) {
        return camera_path3d->point_pitch[0];
    }
    PathMapNormalizedT(camera_path, normalized_t, &segment, &local_t);
    if (segment < 0) segment = 0;
    if (segment >= camera_path->numPoints) segment = camera_path->numPoints - 1;
    next = (segment + 1 < camera_path->numPoints) ? (segment + 1) : segment;
    pitch0 = camera_path3d->point_pitch[segment];
    pitch1 = camera_path3d->point_pitch[next];
    return pitch0 + (pitch1 - pitch0) * local_t;
}

void restore_runtime_scene_config(char* backup, size_t backup_size) {
    if (backup) {
        FILE* f = fopen(kRuntimeSceneConfigPath, "wb");
        if (f) {
            fwrite(backup, 1, backup_size, f);
            fclose(f);
        }
        free(backup);
        return;
    }
    remove(kRuntimeSceneConfigPath);
}

void restore_runtime_animation_config(char* backup, size_t backup_size) {
    if (backup) {
        FILE* f = fopen(kRuntimeAnimationConfigPath, "wb");
        if (f) {
            fwrite(backup, 1, backup_size, f);
            fclose(f);
        }
        free(backup);
        return;
    }
    remove(kRuntimeAnimationConfigPath);
}

MaterialBSDF make_diffuse(double albedo) {
    MaterialBSDF m = {0};
    m.albedo = albedo;
    m.diffuseWeight = 1.0;
    m.specWeight = 0.0;
    m.reflectivity = 0.0;
    m.roughness = 0.5;
    m.weightSum = 1.0;
    m.model = MATERIAL_BSDF_LAMBERT;
    return m;
}
