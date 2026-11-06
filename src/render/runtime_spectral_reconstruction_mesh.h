#pragma once
/* Bounded convex obstacle. Face-visible partition of unity prevents a kernel
 * link crossing a solid, while avoiding discontinuities at extended planes. */
enum { SPECTRAL_MESH_FACES = 8, SPECTRAL_MESH_PARTS = 9 };
typedef struct {
  Vec3 n;
  double d;
} SpectralMeshPlane;
typedef struct {
  SpectralMeshPlane plane[SPECTRAL_MESH_FACES];
  int count;
  Vec3 lo, hi;
} SpectralMeshBoundary;
bool spectral_mesh_admit(SpectralReconstruction *, const RuntimeScene3D *, int);
unsigned spectral_mesh_node_parts(const RuntimeSpectralVolume *, const int[3],
                                  int);
bool spectral_mesh_weights(const SpectralMeshBoundary *, Vec3,
                           double[SPECTRAL_MESH_PARTS]);
double spectral_mesh_integral(SpectralReconstruction *, const double[3],
                              const double[3], const double[3], Vec3, Vec3,
                              int);
