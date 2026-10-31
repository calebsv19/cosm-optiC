#pragma once
#include "animation/evaluated_scene_snapshot.h"
/* Called only after successful image output. Legacy frames produce no sidecar. */
bool SequenceFrameReceiptWrite(const char *image_path,const RayEvaluatedSceneSnapshot *snapshot,
                               const char *scene_path,bool overwrite);
