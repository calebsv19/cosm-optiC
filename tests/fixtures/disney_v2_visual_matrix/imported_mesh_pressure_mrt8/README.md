# Imported Mesh Pressure MRT8 Visual Matrix

This fixture is the first repo-local imported-mesh Disney v2 promotion matrix.
It uses the existing `asset_sphere_128x64` runtime mesh pressure scene so the
matrix exercises material shading, BVH traversal, route comparison, denoise
readback, and performance-threshold reporting without depending on external
large STL/skull sidecars.

The skull-scale lane remains a follow-up once a portable/readable large-mesh
sidecar is available in the proof workspace.
