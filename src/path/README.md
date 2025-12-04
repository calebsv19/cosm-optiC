# src › path

Bézier path logic used to animate the light source and camera.

- `path_system.c` – Implements de Casteljau interpolation for quadratic and cubic segments, normalized traversal via arc-length approximation, path mode toggling, and debug rendering helpers. Uses shared math vectors from `math/vec2`.
