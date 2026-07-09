#ifndef CAMERA_PATH_3D_H
#define CAMERA_PATH_3D_H

#include <stdbool.h>
#include <json-c/json.h>

#include "path/path_system.h"

typedef struct CameraPath3D {
    double point_z[MAX_BEZIER_POINTS];
    double handles_vz[MAX_BEZIER_POINTS][2];
    double point_pitch[MAX_BEZIER_POINTS];
} CameraPath3D;

void CameraPath3D_Reset(CameraPath3D* path3d);
void CameraPath3D_SyncDefaults(CameraPath3D* path3d, const Path* path, double default_z);
bool CameraPath3D_InsertPoint(CameraPath3D* path3d,
                              Path* path,
                              double x,
                              double y,
                              double z,
                              double default_handle_length);
void CameraPath3D_RemovePoint(CameraPath3D* path3d, int index, int num_points_before);
void CameraPath3D_ScaleWorldUnits(CameraPath3D* path3d, const Path* path, double factor);
double CameraPath3D_GetPositionZ(const Path* path, const CameraPath3D* path3d, double t);
double CameraPath3D_GetPositionZNormalized(const Path* path, const CameraPath3D* path3d, double t);
bool CameraPath3D_GetHandleWorldPosition(const Path* path,
                                         const CameraPath3D* path3d,
                                         int segment_index,
                                         int handle_index,
                                         double* out_x,
                                         double* out_y,
                                         double* out_z,
                                         double* out_anchor_x,
                                         double* out_anchor_y,
                                         double* out_anchor_z);
struct json_object* CameraPath3D_ToJsonObject(const CameraPath3D* path3d, const Path* path);
bool CameraPath3D_LoadFromJsonObject(struct json_object* object,
                                     CameraPath3D* out_path3d,
                                     const Path* path,
                                     bool allow_empty);

#endif
