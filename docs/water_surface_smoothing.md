# Smooth normals on unified water bodies

The top of a unified closed water body uses the existing per-vertex normal and
barycentric interpolation path used by smooth meshes. Normal reconstruction uses
neighboring prepared heights and the actual grid spacing, including the authored
boundary inset. Interior derivatives use central differences; edges use one-sided
differences. Normalization follows interpolation at each ray hit.

Neighboring top triangles share vertex normals, so the shading direction is
continuous across their shared edges. Side walls and the bottom retain flat
normals and their intentional creases. Intersection positions, geometric normals,
boundary orientation, volume closure, material parameters and absorption remain
owned by the existing geometry and dielectric contracts. This changes shading
normal reconstruction; it does not add geometric subdivision or a wave solver.

`RuntimeHeightfieldNormals3D_ApplyTopQuads` constructs the app-local generated
surface normals. It reuses the renderer's `RuntimeTriangle3D` vertex-normal carrier.
No shared mesh-asset schema, UI control, MCP command or saved-scene field is added.
Legacy unclosed heightfield/cutout import is outside this bounded closed-body
qualification and retains its existing construction policy.

## Optical footprint transport

Smooth optical continuations reuse the same affine barycentric normal field as
opaque smooth-mesh reflections. The field is evaluated at the two differential
hit positions, then each neighboring optical event is sampled with the same
random numbers and its corresponding normal. Bounded transport requires valid
incoming differentials, a compatible unperturbed normal field, consistent event
branches and geometric sides. Discontinuities and unsupported microdetail retain
the conservative unbounded fallback. Constant-normal ideal transport retains its
existing path. No marginal PDF or directional MIS is fabricated for the rough
microsurface random walk.

The opt-in reflection-footprint observer now includes optical departures,
arrivals and emitter terminals, with the existing selected-pixel and record caps.
It does not change accumulation or material response.

## Qualification and limits

Tests cover analytic curved-height gradients, unit normals, geometric hemispheres,
all shared top vertices, diagonal continuity, preserved side/bottom creases,
unchanged hit positions/geometric normals, and actual smooth-hit normal gradients.
Targeted ASan/UBSan checks cover construction, both axis conventions and invalid
inputs. Material, optical, photon-policy and native-render regression groups pass.

Native low-light comparisons reuse an existing 64x64 rippled Aquarium surface,
IOR 1.333 and roughness 0.008, with a finite rectangular radiance source beyond
the water. The original container geometry is retained with neutral index-matched
glass in the diagnostic copy. A matched 512x384 pair uses 16 passes and 16
transmission samples, with denoising, adaptive stopping and photons off. A separate
24-pixel 64-pass/64-transmission reference measures sampling convergence; it is
not a full-frame render. Flat-water and non-water glass BMP controls remain exact.

These checks establish bounded local smooth-surface/glint behavior, not full
Aquarium, temporal, photon-enabled appearance or production-package acceptance.
The original smoothing comparison used pixel-center camera positions; more
lighting passes alone did not integrate subpixel geometry/glint coverage. Native
multipass rendering now supports [pixel-area camera sampling](pixel_area_camera_sampling.md),
qualified separately from that retained smoothing comparison. No colors, IOR, roughness or light strengths
are adjusted to hide sampling artifacts.

References: [PBRT shading normals](https://pbr-book.org/4ed/Reflection_Models/BSDF_Representation)
and [specular ray differentials](https://pbr-book.org/4ed/Textures_and_Materials/Texture_Sampling_and_Antialiasing).

## Local reflected-ray differential recovery

A one-pixel neighbor can choose transmission while the central stochastic sample
chooses reflection. For a stable single-scatter reflection, that finite branch
change does not imply that the central path lacks a local derivative. The optical
path can now recover that derivative using symmetric perturbations of the same
normal field, incident ray and fixed random inputs. Two step sizes must agree;
neighbor samples must remain first-order reflections, and the full differential
must pass the geometric-side guard. The actual ray, Fresnel choice, PDF and path
weight are unchanged. Multiple scattering, perturbed normals, missing incoming
differentials and unstable derivatives retain the conservative fallback.

This is an app-local filtering repair, not added surface energy or a brighter
water material. Opt-in reflection observations report `optical_footprint_fallback`
reasons and `optical_footprint_recovered` events. Numerical/analytic derivative
checks and matched real-scene crops qualify this boundary separately from scene
lighting and full-frame water appearance.
