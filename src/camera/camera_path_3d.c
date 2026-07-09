#include "camera/camera_path_3d.h"

#include "math/math_utils.h"

#include <math.h>
#include <string.h>

static double camera_path_3d_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

static double camera_path_3d_de_casteljau(const double* control_points, int count, double t) {
    double temp[4] = {0.0, 0.0, 0.0, 0.0};
    int i = 0;
    int k = 0;
    if (!control_points || count < 1) return 0.0;
    if (count > 4) count = 4;
    for (i = 0; i < count; ++i) {
        temp[i] = control_points[i];
    }
    for (k = 1; k < count; ++k) {
        for (i = 0; i < count - k; ++i) {
            temp[i] = camera_path_3d_lerp(temp[i], temp[i + 1], t);
        }
    }
    return temp[0];
}

void CameraPath3D_Reset(CameraPath3D* path3d) {
    if (!path3d) return;
    memset(path3d, 0, sizeof(*path3d));
}

void CameraPath3D_SyncDefaults(CameraPath3D* path3d, const Path* path, double default_z) {
    int i = 0;
    if (!path3d) return;
    if (!path || path->numPoints <= 0) {
        CameraPath3D_Reset(path3d);
        return;
    }
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        if (!isfinite(path3d->point_z[i])) {
            path3d->point_z[i] = default_z;
        }
        if (!isfinite(path3d->point_pitch[i])) {
            path3d->point_pitch[i] = 0.0;
        }
    }
}

bool CameraPath3D_InsertPoint(CameraPath3D* path3d,
                              Path* path,
                              double x,
                              double y,
                              double z,
                              double default_handle_length) {
    double prev_x = 0.0;
    double prev_y = 0.0;
    int previous_count = 0;
    if (!path3d || !path) return false;
    previous_count = path->numPoints;
    if (previous_count >= MAX_BEZIER_POINTS) {
        return false;
    }
    if (!(default_handle_length > 0.0) || !isfinite(default_handle_length)) {
        default_handle_length = 0.0;
    }
    if (previous_count > 0) {
        prev_x = path->points[previous_count - 1].x;
        prev_y = path->points[previous_count - 1].y;
    }
    path->points[previous_count].x = x;
    path->points[previous_count].y = y;
    path->rotations[previous_count] = 0.0;
    path->rotationSet[previous_count] = false;
    if (previous_count < MAX_BEZIER_POINTS - 1) {
        path->handles[previous_count][0].vx = 0.0;
        path->handles[previous_count][0].vy = 0.0;
        path->handles[previous_count][1].vx = 0.0;
        path->handles[previous_count][1].vy = 0.0;
    }
    if (previous_count > 0) {
        double dx = x - prev_x;
        double dy = y - prev_y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist > 1e-6) {
            double dir_x = dx / dist;
            double dir_y = dy / dist;
            double handle_length = fmin(default_handle_length, dist * 0.20);
            double min_handle_length = fmin(default_handle_length * 0.35, dist * 0.10);
            if (handle_length < min_handle_length) {
                handle_length = min_handle_length;
            }
            path->handles[previous_count - 1][0].vx = dir_x * handle_length;
            path->handles[previous_count - 1][0].vy = dir_y * handle_length;
            path->handles[previous_count - 1][1].vx = -dir_x * handle_length;
            path->handles[previous_count - 1][1].vy = -dir_y * handle_length;
        } else {
            path->handles[previous_count - 1][0].vx = 0.0;
            path->handles[previous_count - 1][0].vy = 0.0;
            path->handles[previous_count - 1][1].vx = 0.0;
            path->handles[previous_count - 1][1].vy = 0.0;
        }
    } else {
        path->handles[0][0].vx = 0.0;
        path->handles[0][0].vy = 0.0;
        path->handles[0][1].vx = 0.0;
        path->handles[0][1].vy = 0.0;
    }
    path->numPoints += 1;
    path->handleLink[previous_count] = false;
    path3d->point_z[path->numPoints - 1] = z;
    path3d->point_pitch[path->numPoints - 1] = 0.0;
    if (path->numPoints == 1) {
        path3d->handles_vz[0][0] = 0.0;
        path3d->handles_vz[0][1] = 0.0;
        return true;
    }
    {
        int index = path->numPoints - 1;
        double prev_z = path3d->point_z[index - 1];
        double dz = z - prev_z;
        double handle_length = 0.0;
        double dist = fabs(dz);
        if (default_handle_length > 0.0 && isfinite(default_handle_length)) {
            handle_length = fmin(default_handle_length, dist * 0.20);
            if (handle_length < fmin(default_handle_length * 0.35, dist * 0.10)) {
                handle_length = fmin(default_handle_length * 0.35, dist * 0.10);
            }
        }
        if (dist <= 1e-6 || handle_length <= 1e-6) {
            path3d->handles_vz[index - 1][0] = 0.0;
            path3d->handles_vz[index - 1][1] = 0.0;
        } else {
            double dir = (dz >= 0.0) ? 1.0 : -1.0;
            path3d->handles_vz[index - 1][0] = dir * handle_length;
            path3d->handles_vz[index - 1][1] = -dir * handle_length;
        }
    }
    return true;
}

void CameraPath3D_RemovePoint(CameraPath3D* path3d, int index, int num_points_before) {
    int i = 0;
    if (!path3d || index < 0 || num_points_before <= 0 || index >= num_points_before) return;
    for (i = index; i < num_points_before; ++i) {
        if (i + 1 < MAX_BEZIER_POINTS) {
            path3d->point_z[i] = path3d->point_z[i + 1];
            path3d->point_pitch[i] = path3d->point_pitch[i + 1];
        } else {
            path3d->point_z[i] = 0.0;
            path3d->point_pitch[i] = 0.0;
        }
    }
    if (index == 0) {
        for (i = index; i < num_points_before - 1; ++i) {
            path3d->handles_vz[i][0] = path3d->handles_vz[i + 1][0];
            path3d->handles_vz[i][1] = path3d->handles_vz[i + 1][1];
        }
    } else if (index < num_points_before - 1) {
        path3d->handles_vz[index - 1][1] = path3d->handles_vz[index][1];
        for (i = index + 1; i < num_points_before - 1; ++i) {
            path3d->handles_vz[i][0] = path3d->handles_vz[i + 1][0];
            path3d->handles_vz[i][1] = path3d->handles_vz[i + 1][1];
        }
    }
    if (num_points_before - 1 >= 0 && num_points_before - 1 < MAX_BEZIER_POINTS) {
        path3d->handles_vz[num_points_before - 1][0] = 0.0;
        path3d->handles_vz[num_points_before - 1][1] = 0.0;
        path3d->point_z[num_points_before - 1] = 0.0;
        path3d->point_pitch[num_points_before - 1] = 0.0;
    }
}

void CameraPath3D_ScaleWorldUnits(CameraPath3D* path3d, const Path* path, double factor) {
    int i = 0;
    if (!path3d || !path) return;
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        path3d->point_z[i] *= factor;
        if (i < MAX_BEZIER_POINTS - 1) {
            path3d->handles_vz[i][0] *= factor;
            path3d->handles_vz[i][1] *= factor;
        }
    }
}

double CameraPath3D_GetPositionZ(const Path* path, const CameraPath3D* path3d, double t) {
    double control_points[4] = {0.0, 0.0, 0.0, 0.0};
    int segment_count = 0;
    double clamped_t = 0.0;
    double segment_t = 0.0;
    int segment_index = 0;
    double local_t = 0.0;
    double p0 = 0.0;
    double p3 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
    if (!path || !path3d || path->numPoints < 2) {
        return (path3d && path && path->numPoints > 0) ? path3d->point_z[0] : 0.0;
    }
    segment_count = path->numPoints - 1;
    clamped_t = clampd(t, 0.0, 1.0);
    if (clamped_t >= 1.0) {
        segment_index = segment_count - 1;
        local_t = 1.0;
    } else {
        segment_t = clamped_t * segment_count;
        segment_index = (int)segment_t;
        local_t = segment_t - segment_index;
        if (segment_index >= segment_count) segment_index = segment_count - 1;
        if (segment_index < 0) segment_index = 0;
    }
    p0 = path3d->point_z[segment_index];
    p3 = path3d->point_z[segment_index + 1];
    p1 = p0 + path3d->handles_vz[segment_index][0];
    p2 = p3 + path3d->handles_vz[segment_index][1];
    if (path->mode == BEZIER_QUADRATIC) {
        control_points[0] = p0;
        control_points[1] = p1;
        control_points[2] = p3;
        return camera_path_3d_de_casteljau(control_points, 3, local_t);
    }
    control_points[0] = p0;
    control_points[1] = p1;
    control_points[2] = p2;
    control_points[3] = p3;
    return camera_path_3d_de_casteljau(control_points, 4, local_t);
}

double CameraPath3D_GetPositionZNormalized(const Path* path, const CameraPath3D* path3d, double t) {
    int segment = 0;
    double local_t = 0.0;
    int segments = 1;
    double global_t = 0.0;
    if (!path || !path3d || path->numPoints < 2) {
        return (path3d && path && path->numPoints > 0) ? path3d->point_z[0] : 0.0;
    }
    PathMapNormalizedT(path, t, &segment, &local_t);
    segments = path->numPoints - 1;
    global_t = ((double)segment + local_t) / (double)segments;
    return CameraPath3D_GetPositionZ(path, path3d, global_t);
}

bool CameraPath3D_GetHandleWorldPosition(const Path* path,
                                         const CameraPath3D* path3d,
                                         int segment_index,
                                         int handle_index,
                                         double* out_x,
                                         double* out_y,
                                         double* out_z,
                                         double* out_anchor_x,
                                         double* out_anchor_y,
                                         double* out_anchor_z) {
    int point_index = 0;
    if (!path || !path3d) return false;
    if (segment_index < 0 || segment_index >= path->numPoints - 1) return false;
    if (handle_index < 0 || handle_index > 1) return false;
    point_index = (handle_index == 0) ? segment_index : (segment_index + 1);
    if (out_anchor_x) *out_anchor_x = path->points[point_index].x;
    if (out_anchor_y) *out_anchor_y = path->points[point_index].y;
    if (out_anchor_z) *out_anchor_z = path3d->point_z[point_index];
    if (out_x) {
        *out_x = path->points[point_index].x + path->handles[segment_index][handle_index].vx;
    }
    if (out_y) {
        *out_y = path->points[point_index].y + path->handles[segment_index][handle_index].vy;
    }
    if (out_z) {
        *out_z = path3d->point_z[point_index] + path3d->handles_vz[segment_index][handle_index];
    }
    return true;
}

struct json_object* CameraPath3D_ToJsonObject(const CameraPath3D* path3d, const Path* path) {
    struct json_object* object = NULL;
    struct json_object* points = NULL;
    int i = 0;
    if (!path3d || !path) return NULL;
    object = json_object_new_object();
    points = json_object_new_array();
    if (!object || !points) {
        if (points) json_object_put(points);
        if (object) json_object_put(object);
        return NULL;
    }
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        struct json_object* point = json_object_new_object();
        if (!point) {
            json_object_put(points);
            json_object_put(object);
            return NULL;
        }
        json_object_object_add(point, "z", json_object_new_double(path3d->point_z[i]));
        json_object_object_add(point, "lookPitch", json_object_new_double(path3d->point_pitch[i]));
        if (i < path->numPoints - 1) {
            struct json_object* velocity_1 = json_object_new_object();
            json_object_object_add(velocity_1, "vz", json_object_new_double(path3d->handles_vz[i][0]));
            json_object_object_add(point, "velocity1", velocity_1);
        }
        if (i > 0) {
            struct json_object* velocity_2 = json_object_new_object();
            json_object_object_add(velocity_2, "vz", json_object_new_double(path3d->handles_vz[i - 1][1]));
            json_object_object_add(point, "velocity2", velocity_2);
        }
        json_object_array_add(points, point);
    }
    json_object_object_add(object, "points", points);
    return object;
}

bool CameraPath3D_LoadFromJsonObject(struct json_object* object,
                                     CameraPath3D* out_path3d,
                                     const Path* path,
                                     bool allow_empty) {
    struct json_object* points = NULL;
    int count = 0;
    int i = 0;
    if (!object || !out_path3d) return false;
    CameraPath3D_Reset(out_path3d);
    if (!json_object_is_type(object, json_type_object) ||
        !json_object_object_get_ex(object, "points", &points) ||
        !json_object_is_type(points, json_type_array)) {
        return false;
    }
    count = json_object_array_length(points);
    if (count < 1) return allow_empty;
    if (path && count > path->numPoints) count = path->numPoints;
    if (count > MAX_BEZIER_POINTS) count = MAX_BEZIER_POINTS;
    for (i = 0; i < count; ++i) {
        struct json_object* point = json_object_array_get_idx(points, i);
        struct json_object* z_obj = NULL;
        struct json_object* pitch_obj = NULL;
        if (!point || !json_object_is_type(point, json_type_object)) continue;
        if (json_object_object_get_ex(point, "z", &z_obj)) {
            out_path3d->point_z[i] = json_object_get_double(z_obj);
        }
        if (json_object_object_get_ex(point, "lookPitch", &pitch_obj)) {
            out_path3d->point_pitch[i] = json_object_get_double(pitch_obj);
        }
        if (i < count - 1) {
            struct json_object* velocity_1 = NULL;
            struct json_object* vz_obj = NULL;
            if (json_object_object_get_ex(point, "velocity1", &velocity_1) &&
                json_object_is_type(velocity_1, json_type_object) &&
                json_object_object_get_ex(velocity_1, "vz", &vz_obj)) {
                out_path3d->handles_vz[i][0] = json_object_get_double(vz_obj);
            }
        }
        if (i > 0) {
            struct json_object* velocity_2 = NULL;
            struct json_object* vz_obj = NULL;
            if (json_object_object_get_ex(point, "velocity2", &velocity_2) &&
                json_object_is_type(velocity_2, json_type_object) &&
                json_object_object_get_ex(velocity_2, "vz", &vz_obj)) {
                out_path3d->handles_vz[i - 1][1] = json_object_get_double(vz_obj);
            }
        }
    }
    return true;
}
