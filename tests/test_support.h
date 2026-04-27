#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "SDL.h"

#include "cJSON.h"

#include "import/runtime_scene_bridge.h"
#include "path/path_system.h"
#include "render/material_bsdf.h"
#include "render/runtime_scene_3d.h"
#include "ui/scene_source_catalog.h"

extern const char* kRuntimeSceneConfigPath;
extern const char* kRuntimeAnimationConfigPath;

void test_support_reset_failures(void);
int test_support_failures(void);

int test_rect_right(const SDL_Rect* rect);
int test_rect_bottom(const SDL_Rect* rect);

void assert_close(const char* name, double a, double b, double tol);
void assert_true(const char* name, bool cond);

char* read_text_file_alloc(const char* path, size_t* out_size);
bool write_text_file(const char* path, const char* text);
bool path_exists(const char* path);
void restore_env_or_unset(const char* name, const char* value);

cJSON* find_named_dataset_entry(cJSON* items, const char* name);

bool path_list_contains(const char* const* paths, size_t count, const char* needle);
bool catalog_contains_path_source(const SceneSourceCatalogEntry* entries,
                                  size_t count,
                                  const char* path,
                                  int source);
bool catalog_contains_path_any_source(const SceneSourceCatalogEntry* entries,
                                      size_t count,
                                      const char* path);
size_t catalog_count_source(const SceneSourceCatalogEntry* entries, size_t count, int source);

int digest_count_kind(const RuntimeSceneBridge3DDigestState* digest,
                      RuntimeSceneBridgePrimitiveKind kind);
const RuntimeSceneBridgePrimitiveSeed* find_primitive_seed_by_object_id(
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    const char* object_id);
int find_runtime_primitive_index_by_object_id(const RuntimeScene3D* scene,
                                              const char* object_id);
int count_runtime_triangles_for_primitive(const RuntimeScene3D* scene,
                                          int primitive_index);

double expected_camera_pitch_for_t(const Path* camera_path,
                                   const CameraPath3D* camera_path3d,
                                   double normalized_t);

void restore_runtime_scene_config(char* backup, size_t backup_size);
void restore_runtime_animation_config(char* backup, size_t backup_size);

MaterialBSDF make_diffuse(double albedo);
