# M0 surface-mapping fixtures

These are test inputs and proposed numeric acceptance contracts, not a runtime
mapping schema or an implementation of new material behavior.

From the checkout root:

```sh
python3 tests/integration/test_surface_material_m0_contract.py
python3 tests/fixtures/surface_material_m0/generate.py --output-root build/surface_material_m0/geometry
```

The generator reuses the existing sphere fixture builder. It emits seven runtime
mesh assets and individual legacy-brick scenes: two alternate plane diagonals,
an exactly coplanar 8 by 4 subdivision, low/high spheres, and low/high cylinders.
All scenes use the same stable object ID and explicit material seed. Generated
assets contain no UVs. Cylinder caps have separate stable surface groups. Output
must be a new directory, so retained evidence is never overwritten.

`labeled_grid.svg` is a lossless labeled coordinate reference with origin at the
bottom left; `labeled_grid.ppm` is its raster equivalent with numeric cell labels.
These are oracle assets for the later sampler tests, not bindings to arbitrary
mesh UVs (which are unsupported today). The generated legacy scenes use brick.
`contract.json` records independent expected samples for new planar/axial mapping.
Those expectations become production adapter assertions in M1/M2. M0 tests only
validate geometry, labels, tolerances, and the fixture data itself.

Strict same-surface tests compare physical points reconstructed through different
plane triangles, then require the future mapping/material values at those points
to match. Low/high sphere and cylinder meshes approximate different surfaces:
compare common analytic sample inputs exactly, and separately measure geometry
error. Declared centroid radial deficits bound this fixture's geometric error;
they are not universal image, Hausdorff, normal, or material tolerances. Never
permit a large color error simply because the geometry is coarse.

The axial oracle uses Z height for courses, eight repeats about +Z, seam at +X,
positive rotation toward +Y, and V=(z+1)/0.25. Seam closure needs an integer
horizontal cell count and periodic cell/noise identity; vertical repetition of
staggered rows additionally needs an even row count. Sphere poles explicitly report
singularity and choose U=0 for finite sampling. M2 must also probe both sides of
the seam and the cap transition; no distortion-free pole claim is made.
