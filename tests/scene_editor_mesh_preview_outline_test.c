#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "editor/scene_editor_mesh_preview_outline.h"

static int g_failures = 0;

static void expect_true(const char* name, int condition) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", name);
    g_failures += 1;
}

static size_t pixel_at(int x, int y, int width) {
    return (size_t)y * (size_t)width + (size_t)x;
}

static void seed_pixel(uint8_t* rgba,
                       double* depth,
                       int* owner,
                       int width,
                       int x,
                       int y,
                       int object_id,
                       double z) {
    const size_t pixel = pixel_at(x, y, width);
    rgba[pixel * 4u + 0u] = 40u;
    rgba[pixel * 4u + 1u] = 50u;
    rgba[pixel * 4u + 2u] = 60u;
    rgba[pixel * 4u + 3u] = 255u;
    depth[pixel] = z;
    owner[pixel] = object_id;
}

static void clear_fixture(uint8_t* rgba,
                          double* depth,
                          int* owner,
                          int width,
                          int height) {
    memset(rgba, 0, (size_t)width * (size_t)height * 4u);
    for (int i = 0; i < width * height; ++i) {
        depth[i] = INFINITY;
        owner[i] = -1;
    }
}

static void test_silhouette_keeps_interior_surface(void) {
    enum { WIDTH = 5, HEIGHT = 5 };
    uint8_t rgba[WIDTH * HEIGHT * 4];
    double depth[WIDTH * HEIGHT];
    int owner[WIDTH * HEIGHT];
    size_t count = 0u;
    clear_fixture(rgba, depth, owner, WIDTH, HEIGHT);
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) seed_pixel(rgba, depth, owner, WIDTH, x, y, 0, 2.0);
    }
    count = SceneEditorMeshPreviewApplyOutlines(rgba, depth, owner, WIDTH, HEIGHT, -1, -1);
    expect_true("silhouette_pixel_count", count == 8u);
    expect_true("silhouette_preserves_interior",
                rgba[pixel_at(2, 2, WIDTH) * 4u + 0u] == 40u);
}

static void test_object_boundary_uses_each_object_accent(void) {
    enum { WIDTH = 7, HEIGHT = 5 };
    uint8_t rgba[WIDTH * HEIGHT * 4];
    double depth[WIDTH * HEIGHT];
    int owner[WIDTH * HEIGHT];
    const SDL_Color left = SceneEditorMeshPreviewOutlineColor(0, -1, -1);
    const SDL_Color right = SceneEditorMeshPreviewOutlineColor(1, -1, -1);
    clear_fixture(rgba, depth, owner, WIDTH, HEIGHT);
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) seed_pixel(rgba, depth, owner, WIDTH, x, y, 0, 2.0);
        for (int x = 4; x <= 5; ++x) seed_pixel(rgba, depth, owner, WIDTH, x, y, 1, 2.0);
    }
    (void)SceneEditorMeshPreviewApplyOutlines(rgba, depth, owner, WIDTH, HEIGHT, -1, -1);
    expect_true("object_boundary_left_accent",
                rgba[pixel_at(3, 2, WIDTH) * 4u + 0u] == left.r);
    expect_true("object_boundary_right_accent",
                rgba[pixel_at(4, 2, WIDTH) * 4u + 0u] == right.r);
    expect_true("object_boundary_accents_differ", left.r != right.r || left.g != right.g);
}

static void test_selected_and_hover_colors_take_priority(void) {
    const SDL_Color selected = SceneEditorMeshPreviewOutlineColor(3, 3, 3);
    const SDL_Color hover = SceneEditorMeshPreviewOutlineColor(4, -1, 4);
    expect_true("selected_outline_priority",
                selected.r == 255u && selected.g == 168u && selected.b == 76u);
    expect_true("hover_outline_color",
                hover.r == 104u && hover.g == 232u && hover.b == 255u);
}

int main(void) {
    test_silhouette_keeps_interior_surface();
    test_object_boundary_uses_each_object_accent();
    test_selected_and_hover_colors_take_priority();
    if (g_failures != 0) return 1;
    puts("scene editor mesh preview outline tests passed");
    return 0;
}
