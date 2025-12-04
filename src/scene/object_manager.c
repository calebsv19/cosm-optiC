#include "scene/object_manager.h"
#include "config/config_manager.h"
#include "material/material_manager.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

static void InitDefaultMaterial(SceneObject* obj) {
    obj->texture[0] = '\0';
    obj->color = 0xFFFFFF;
    obj->opacity = 1.0;
    obj->reflectivity = 0.35;
    obj->roughness = 0.65;
    obj->textureId = 0;
    obj->material_id = MaterialManagerDefaultId();
}

void InitObject(SceneObject* obj, int type, double x, double y, double param1, double param2, double points[][2], int numPoints) {
    (void)param2;
    obj->x = x;
    obj->y = y;
    obj->scale = 1.0;
    obj->rotation = 0.0;
    obj->dirty = true;
    InitDefaultMaterial(obj);

    if (type == OBJECT_CIRCLE) {
        // Circle creation: param1 = radius
        strcpy(obj->type, "circle");
        obj->radius = param1;
        obj->numPoints = 0;  // No need for vertex data

    } else if (type == OBJECT_POLYGON) {
        // Custom polygon creation with manually defined points
        strcpy(obj->type, "polygon");
        obj->numPoints = numPoints > MAX_POINTS ? MAX_POINTS : numPoints;
        obj->radius = 0;  // Not a circle

        for (int i = 0; i < obj->numPoints; i++) {
            obj->baseShapePoints[i][0] = points[i][0];
            obj->baseShapePoints[i][1] = points[i][1];
        }
	// Compute and set the initial radius
        SetPolygonRadius(obj);
    }
}

void SetPolygonRadius(SceneObject* obj) {
    if (strcmp(obj->type, "circle") == 0) {
        // Circles already have a radius
        return;
    }

    if (obj->numPoints <= 0) {
        obj->radius = 0;
        return;
    }

    double sumDistance = 0;
    for (int i = 0; i < obj->numPoints; i++) {
        double dx = obj->baseShapePoints[i][0];
        double dy = obj->baseShapePoints[i][1];
        sumDistance += sqrt(dx * dx + dy * dy);
    }

    obj->radius = sumDistance / obj->numPoints;
}

void UpdateObjects(void) {
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
	UpdateObject(obj);
    }
}


// Updates an object's transformed points based on its position, scale, and rotation
void UpdateObject(SceneObject* obj) {
    if (IsObjectDirty(obj)) {
        // Compute transformed points based on current scale & rotation
        double cosTheta = cos(obj->rotation);
        double sinTheta = sin(obj->rotation);

        for (int j = 0; j < obj->numPoints; j++) {
            double bx = obj->baseShapePoints[j][0] * obj->scale;
            double by = obj->baseShapePoints[j][1] * obj->scale;

            obj->shapePoints[j][0] = cosTheta * bx - sinTheta * by;
            obj->shapePoints[j][1] = sinTheta * bx + cosTheta * by;
        }

        obj->dirty = false;  // Reset dirty flag after update
    }
}

bool IsInsideObject(int mx, int my, SceneObject* obj) {
    if (strcmp(obj->type, "circle") == 0) {
        double dx = mx - obj->x;
        double dy = my - obj->y;
        return (dx * dx + dy * dy) < (obj->radius * obj->scale * obj->radius * obj->scale);
    } else {
        // Polygon collision using ray-casting method
        int inside = 0;
        int j = obj->numPoints - 1;

        for (int i = 0; i < obj->numPoints; i++) {
            double xi = obj->shapePoints[i][0] + obj->x;
            double yi = obj->shapePoints[i][1] + obj->y;
            double xj = obj->shapePoints[j][0] + obj->x;
            double yj = obj->shapePoints[j][1] + obj->y;

            if (((yi > my) != (yj > my)) &&
                (mx < (xj - xi) * (my - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
            j = i;
        }
        return inside;
    }
}

void ComputeObjectBounds(const SceneObject* obj, double* minX, double* minY, double* maxX, double* maxY) {
    double localMinX = obj->x;
    double localMaxX = obj->x;
    double localMinY = obj->y;
    double localMaxY = obj->y;

    if (strcmp(obj->type, "circle") == 0) {
        double r = obj->radius * obj->scale;
        localMinX = obj->x - r;
        localMaxX = obj->x + r;
        localMinY = obj->y - r;
        localMaxY = obj->y + r;
    } else {
        for (int i = 0; i < obj->numPoints; i++) {
            double wx = obj->shapePoints[i][0] + obj->x;
            double wy = obj->shapePoints[i][1] + obj->y;
            if (wx < localMinX) localMinX = wx;
            if (wx > localMaxX) localMaxX = wx;
            if (wy < localMinY) localMinY = wy;
            if (wy > localMaxY) localMaxY = wy;
        }
    }

    if (minX) *minX = localMinX;
    if (maxX) *maxX = localMaxX;
    if (minY) *minY = localMinY;
    if (maxY) *maxY = localMaxY;
}


// Moves an object by a given amount
void MoveObject(SceneObject* obj, int dx, int dy) {
    obj->x += dx;
    obj->y += dy;
    obj->dirty = true;
}

void RotateObject(SceneObject* obj, double angleDelta) {
    if (strcmp(obj->type, "circle") == 0) {
	obj->rotation += angleDelta;
        return;  // Circles do not rotate visually
    }

    obj->rotation += angleDelta;  // Keep track of total rotation

    double cosTheta = cos(obj->rotation);
    double sinTheta = sin(obj->rotation);

    // Rotate each point relative to the base shape points
    for (int i = 0; i < obj->numPoints; i++) {
        double bx = obj->baseShapePoints[i][0] * obj->scale;
        double by = obj->baseShapePoints[i][1] * obj->scale;

        obj->shapePoints[i][0] = cosTheta * bx - sinTheta * by;
        obj->shapePoints[i][1] = sinTheta * bx + cosTheta * by;
    }

    obj->dirty = true;
}

// Scales an object uniformly
void ScaleObject(SceneObject* obj, double scaleFactor) {
    obj->scale *= scaleFactor;
    obj->dirty = true;
}

// Marks an object as needing updates
void MarkObjectDirty(SceneObject* obj) {
    obj->dirty = true;
}

// Checks if an object needs to be updated
bool IsObjectDirty(SceneObject* obj) {
    return obj->dirty;
}

void SegmentPathInit(SegmentPath* path) {
    if (!path) return;
    path->vertices = NULL;
    path->count = 0;
    path->capacity = 0;
}

void SegmentPathFree(SegmentPath* path) {
    if (!path) return;
    free(path->vertices);
    path->vertices = NULL;
    path->count = 0;
    path->capacity = 0;
}

static void NormalizeVec(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len < 1e-9) {
        *x = 0.0;
        *y = 1.0;
        return;
    }
    *x /= len;
    *y /= len;
}

static bool SegmentPathEnsure(SegmentPath* path, int required) {
    if (!path || required <= 0) return false;
    if (path->capacity >= required) {
        return true;
    }
    int newCap = path->capacity > 0 ? path->capacity : 16;
    while (newCap < required) {
        newCap *= 2;
    }
    PathVertex* verts = (PathVertex*)realloc(path->vertices, (size_t)newCap * sizeof(PathVertex));
    if (!verts) {
        return false;
    }
    path->vertices = verts;
    path->capacity = newCap;
    return true;
}

static bool BuildPolygonPath(const SceneObject* obj,
                             SegmentPath* path) {
    if (!obj || !path) return false;
    if (obj->numPoints <= 1) return false;
    if (!SegmentPathEnsure(path, obj->numPoints)) return false;
    double centerX = obj->x;
    double centerY = obj->y;
    for (int i = 0; i < obj->numPoints; i++) {
        double wx = obj->shapePoints[i][0] + centerX;
        double wy = obj->shapePoints[i][1] + centerY;
        double nx = wx - centerX;
        double ny = wy - centerY;
        NormalizeVec(&nx, &ny);
        /* Fallback for near-center vertices (degenerate polygons) */
        if (fabs(nx) + fabs(ny) < 1e-6) {
            int prev = (i - 1 + obj->numPoints) % obj->numPoints;
            int next = (i + 1) % obj->numPoints;
            double prevX = obj->shapePoints[prev][0] + centerX;
            double prevY = obj->shapePoints[prev][1] + centerY;
            double nextX = obj->shapePoints[next][0] + centerX;
            double nextY = obj->shapePoints[next][1] + centerY;
            double edgeNX = prevY - wy;
            double edgeNY = -(prevX - wx);
            NormalizeVec(&edgeNX, &edgeNY);
            double edgeNX2 = wy - nextY;
            double edgeNY2 = -(wx - nextX);
            NormalizeVec(&edgeNX2, &edgeNY2);
            nx = edgeNX + edgeNX2;
            ny = edgeNY + edgeNY2;
            NormalizeVec(&nx, &ny);
        }
        path->vertices[i].x = wx;
        path->vertices[i].y = wy;
        path->vertices[i].nx = nx;
        path->vertices[i].ny = ny;
    }
    path->count = obj->numPoints;
    return true;
}

static bool BuildCirclePath(const SceneObject* obj,
                            double maxSegmentLength,
                            SegmentPath* path) {
    if (!obj || !path) return false;
    double radius = obj->radius * obj->scale;
    if (radius <= 0.0) return false;
    double circumference = 2.0 * M_PI * radius;
    double segmentLen = (maxSegmentLength > 0.5) ? maxSegmentLength : 8.0;
    int samples = (int)ceil(circumference / segmentLen);
    if (samples < 12) samples = 12;
    if (!SegmentPathEnsure(path, samples)) return false;
    for (int i = 0; i < samples; i++) {
        double t = (double)i / (double)samples;
        double angle = t * 2.0 * M_PI;
        double cosA = cos(angle);
        double sinA = sin(angle);
        double px = obj->x + radius * cosA;
        double py = obj->y + radius * sinA;
        path->vertices[i].x = px;
        path->vertices[i].y = py;
        path->vertices[i].nx = cosA;
        path->vertices[i].ny = sinA;
    }
    path->count = samples;
    return true;
}

bool OM_BuildSegmentPath(const SceneObject* obj,
                         double maxSegmentLength,
                         SegmentPath* outPath) {
    if (!obj || !outPath) return false;
    /* reuse existing allocation */
    outPath->count = 0;
    if (strcmp(obj->type, "circle") == 0 || obj->numPoints == 0) {
        return BuildCirclePath(obj, maxSegmentLength, outPath);
    }
    return BuildPolygonPath(obj, outPath);
}
