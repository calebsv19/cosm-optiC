#ifndef RAY_TRACING_SURFACE_AUTHORING_CANVAS_H
#define RAY_TRACING_SURFACE_AUTHORING_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RAY_TRACING_SURFACE_AUTHORING_CANVAS_NODE_CAP = 32,
    RAY_TRACING_SURFACE_AUTHORING_CANVAS_EDGE_CAP = 48,
    RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP = 128,
    RAY_TRACING_SURFACE_AUTHORING_CANVAS_ERROR_CAP = 192
};

typedef struct RayTracingSurfaceAuthoringCanvasNode {
    char id[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char kind[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char label[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char digest_sha256[65];
    int32_t x;
    int32_t y;
    uint32_t output_domains;
} RayTracingSurfaceAuthoringCanvasNode;

typedef struct RayTracingSurfaceAuthoringCanvasEdge {
    char from[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char to[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
} RayTracingSurfaceAuthoringCanvasEdge;

typedef struct RayTracingSurfaceAuthoringCanvasSnapshot {
    bool valid;
    bool read_only;
    bool can_select;
    bool can_zoom;
    bool can_pan;
    bool can_edit;
    bool can_save;
    bool can_promote;
    char document_id[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char document_digest_sha256[65];
    char source_object_id[RAY_TRACING_SURFACE_AUTHORING_CANVAS_STRING_CAP];
    char source_mesh_digest_sha256[65];
    size_t node_count;
    RayTracingSurfaceAuthoringCanvasNode nodes[RAY_TRACING_SURFACE_AUTHORING_CANVAS_NODE_CAP];
    size_t edge_count;
    RayTracingSurfaceAuthoringCanvasEdge edges[RAY_TRACING_SURFACE_AUTHORING_CANVAS_EDGE_CAP];
    char error[RAY_TRACING_SURFACE_AUTHORING_CANVAS_ERROR_CAP];
} RayTracingSurfaceAuthoringCanvasSnapshot;

void RayTracingSurfaceAuthoringCanvasSnapshot_Init(
    RayTracingSurfaceAuthoringCanvasSnapshot* snapshot);
bool RayTracingSurfaceAuthoringCanvasSnapshot_DefaultCube(
    RayTracingSurfaceAuthoringCanvasSnapshot* snapshot);
bool RayTracingSurfaceAuthoringCanvasSnapshot_LoadJsonFile(
    const char* path,
    RayTracingSurfaceAuthoringCanvasSnapshot* snapshot);

#endif
