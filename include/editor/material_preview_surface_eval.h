#ifndef MATERIAL_PREVIEW_SURFACE_EVAL_H
#define MATERIAL_PREVIEW_SURFACE_EVAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "render/runtime_material_texture_stack_3d.h"

bool MaterialPreviewSurfaceEvaluateObject(const SceneObject* object,
                                          int scene_object_index,
                                          const RuntimeMaterialTextureLayer* preview_overlay,
                                          double u,
                                          double v,
                                          RuntimeMaterialSurfaceEval* out_eval);

bool MaterialPreviewSurfaceEvaluateFace(const SceneObject* object,
                                        int scene_object_index,
                                        int face_group_index,
                                        double u,
                                        double v,
                                        RuntimeMaterialSurfaceEval* out_eval);

void MaterialPreviewSurfaceShadePixel(const RuntimeMaterialSurfaceEval* eval,
                                     const SceneObject* object,
                                     double u,
                                     double v,
                                     Uint8 bg_r,
                                     Uint8 bg_g,
                                     Uint8 bg_b,
                                     Uint8* out_r,
                                     Uint8* out_g,
                                     Uint8* out_b);

#endif
