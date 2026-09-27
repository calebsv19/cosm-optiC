#include "scene_editor_viewport_authoring_bounds.h"
#include "config/config_manager.h"
#include <math.h>

static void include_point(double x, double y, double z, double padding,
                          double minimum[3], double maximum[3]) {
    const double point[3]={x,y,z};
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) return;
    for (int axis=0;axis<3;++axis) {
        minimum[axis]=fmin(minimum[axis],point[axis]-padding);
        maximum[axis]=fmax(maximum[axis],point[axis]+padding);
    }
}

static void include_path(const Path* path, const CameraPath3D* depth,
                         double padding, double minimum[3], double maximum[3]) {
    if (path->numPoints<0 || path->numPoints>MAX_BEZIER_POINTS) return;
    for (int i=0;i<path->numPoints;++i)
        include_point(path->points[i].x,path->points[i].y,depth->point_z[i],
                      padding,minimum,maximum);
    for (int segment=0;segment+1<path->numPoints;++segment) {
        const int count=path->mode==BEZIER_CUBIC?2:1;
        for (int handle=0;handle<count;++handle) {
            double x,y,z;
            if (CameraPath3D_GetHandleWorldPosition(path,depth,segment,handle,
                    &x,&y,&z,NULL,NULL,NULL))
                include_point(x,y,z,padding,minimum,maximum);
        }
    }
}

void SceneEditorViewportExpandAuthoringBounds(double padding,
    double minimum[3], double maximum[3]) {
    if (!minimum || !maximum || !isfinite(padding) || padding<0) return;
    include_path(&sceneSettings.cameraPath,&sceneSettings.cameraPath3D,
                  padding,minimum,maximum);
    include_path(&sceneSettings.bezierPath,&sceneSettings.bezierPath3D,
                  padding,minimum,maximum);
}
