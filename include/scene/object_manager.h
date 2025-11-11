#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#define MAX_POINTS 10
#define MAX_OBJECTS 10

typedef enum {
    OBJECT_CIRCLE = 0,   // Circle objects
    OBJECT_POLYGON = 1     // Custom polygon objects
} ObjectType;

typedef struct {
    char type[20];         // e.g., "rectangle", "polygon", "circle"
    int numPoints;         // Number of vertices (0 for circles)

    double baseShapePoints[MAX_POINTS][2];  // Base points (unused for circles)
    double shapePoints[MAX_POINTS][2];      // Transformed points (unused for circles)

    double x, y;           // Object center position
    double scale;          // Scale factor
    double rotation;       // Rotation angle (radians)

    double radius;         // Only used for circles

    char texture[256];     // Texture path
    int color;             // Color (integer for now, e.g., RGB packed)
    double opacity;        // Opacity (0.0 to 1.0)
    double reflectivity;   // 0 (matte) .. 1 (mirror)
    double roughness;      // 0 (sharp) .. 1 (diffuse)
    int textureId;         // Procedural texture selector

    bool dirty;            // Needs update?
} SceneObject;


// Object Initialization
void InitObject(SceneObject* obj, int type, double x, double y, double param1, double param2, double points[][2], int numPoints);
void SetPolygonRadius(SceneObject* obj);

// Updates
void UpdateObjects(void);
void UpdateObject(SceneObject* obj);
void ResetObjectTransform(SceneObject* obj);

// Object Transformations
void RotateObject(SceneObject* obj, double angleDelta);
void ScaleObject(SceneObject* obj, double scaleFactor);
void MoveObject(SceneObject* obj, int dx, int dy);


//  Dirty Flag System
bool IsInsideObject(int mx, int my, SceneObject* obj);
void ComputeObjectBounds(const SceneObject* obj, double* minX, double* minY, double* maxX, double* maxY);
void MarkObjectDirty(SceneObject* obj);
bool IsObjectDirty(SceneObject* obj);

#endif // OBJECT_MANAGER_H
