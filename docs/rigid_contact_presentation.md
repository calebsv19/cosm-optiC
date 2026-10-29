# Rigid contact presentation proof

The suspected light gap between the resting blocks was a visual-review mistake.
The retained render already shows continuous contact. At center column X=192,
row 148 is orange RGB(100,79,49), and row 149 is blue RGB(50,73,86), with no background
row between them. The world surfaces meet at Z=0.5 m. No physics, geometry,
material, camera, shading or renderer runtime change was needed.

A six-times nearest-neighbor crop of the original image makes the seam readable.
Six fresh renders are byte-identical to their originals: resting at ticks 0,600,1200
and impact at0,84,180. This confirms both the original evidence and reproduction;
it does not imply an installed-app refresh or operator visual acceptance.

## Regression boundary

`tests/rigid_contact_pixel_probe.c` reads the ordinary typed request and prepares
the normal evaluated scene. It compares flattened and TLAS first hits explicitly,
not through potentially thread-local summary counters. Across six 384×288 frames,
663,552 paired comparisons have zero identity/depth mismatches.

`tests/test_rigid_contact_presentation.py` independently reconstructs the camera
rays and transformed visible source triangles from the retained immutable cache.
Its two-sided Moller–Trumbore reference agrees at 105,570 pixels, with maximum depth
error 3.56e-15 m. The rendered RGB pixels agree with the controlled blue/orange
materials. An injected 64-pixel one-row gray band is detected in each of six cases.
No solver is executed, and input scene/cache hashes remain unchanged.

The RGB classifier is deliberately limited to this known camera, light and opaque
material fixture. It is not a general material-recognition or optical-quality gate.
Native camera, object identity and geometry are checked separately from RGB.

## Repeat locally

Build tools into fresh task-owned roots:

```sh
make -C ray_tracing BUILD_DIR_BASE=<task>/ray   ray-tracing-render-headless rigid-contact-pixel-probe
make -C ball_bounce_sim CONTRACT_BUILD_DIR=<task>/pack rigid-scene-cache-io-build
python3 ray_tracing/tests/test_rigid_contact_presentation.py   --renderer <task>/ray/toolchains/clang/<arch>/tools/cli/ray_tracing_render_headless   --pixel-probe <task>/ray/toolchains/clang/<arch>/tests/rigid_contact_pixel_probe   --pack-io <task>/pack/rigid_scene_cache_io   --parent <retained-resting-proof>/scenes --out <fresh-proof>
```

The parent has `stack-rest/scene_review.json` and `stack-impact/scene_review.json`,
each beside its verified `result/`. The test creates requests and renders, reads
actual lossless frame pixels, and preserves the parent. The selected pack reader
decodes saved poses only. A genuine missing strip fails the same detector that
passes the original image.

This closes the contact-presentation hold for the bounded resting/impact proof.
The [physical qualification](rigid_resting_qualification.md) and its limits stand.
Six-brick structures, general stacking, CCD, sleeping/waking, fracture and installed
or worker delivery remain separate work.
