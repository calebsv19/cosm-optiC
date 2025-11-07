#include "scene/object_manager.h"
#include "config/config_manager.h"
#include <string.h>
#include <math.h>

static void InitDefaultMaterial(SceneObject* obj) {
    obj->texture[0] = '\0';
    obj->color = 0xFFFFFF;
    obj->opacity = 1.0;
    obj->reflectivity = 0.35;
    obj->roughness = 0.65;
    obj->textureId = 0;
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
