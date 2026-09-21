# Surface material M2: editable axial brick mapping

Historical checkpoint: [M3](surface_material_m3_contract.md) now adds retained
source editing and primitive face regions; restrictions below describe this phase.

M2 completes the first curved-surface checkpoint. An agent-authored axial mapping
can be edited in the Scene inspector, saved, and reopened. The viewport and ray
renderer use the same position-based coordinates and brick/material response.
M3 graph/region integration and M4 authored UVs remain separate work.

The [M1 planar contract](surface_material_m1_contract.md) remains supported.
Legacy files retain their existing runtime behavior. Desktop and worker product
versions are unchanged; the additive shared module is `core_authored_texture`
0.4.0. This source checkpoint does not imply package or Main Edit adoption.

## Authoring

Select an object in the Scene workspace and expand **Surface mapping** in the
Inspector. Choose Planar for a plane/prism or Axial brick for an explicitly
selected mesh projection. A plain material receives a brick source as part of
that retained command. Existing source graphs, image bindings and incompatible
stacks are preserved and rejected rather than replaced. Source-layer/region
editing through the older Materials controls is unavailable for mapped objects;
that pane directs the user to the mapping inspector. JSON can supply a supported
brick/solid source stack.

The inspector exposes object/world space, axis selection, tile width/height,
offsets, origin, seed, and planar rotation or axial seam/reference radius/pole
fade. It reports the effective brick count and width. **Detail min/max** set the
height interval accelerated by the viewport cache; they do not clip geometry or
change runtime source coordinates. Values outside that interval use the direct
source sampler. Enter commits numeric input; Escape or leaving the field cancels
its draft. Invalid edits leave the retained document and its revision unchanged.
Undo/redo, duplication, saving and reopening use existing document transactions.
Unknown producer fields survive inspector value edits and serialization.

An axial binding is stored at the same object extension as M1:

```json
{
  "surface_mapping": {
    "version": 2,
    "required_capability": "optic.axial_surface_v2",
    "method": "axial_height",
    "space": "object_rest",
    "source_domain": "brick_cells_v1",
    "scale_policy": "stretch_with_object",
    "origin_m": [0, 0, 0],
    "axis_u": [1, 0, 0],
    "axis_v": [0, 0, 1],
    "tile_m": [0.5, 0.25],
    "offset_m": [0, 0],
    "pivot_m": [0, 0],
    "rotation_rad": 0,
    "seed": 1729,
    "reference_radius_m": 1,
    "seam_rad": 0,
    "pole_radius_m": 0.12,
    "height_range_m": [-1, 1],
    "repeat_policy": "integer_circumference",
    "pole_policy": "fade_to_base"
  }
}
```

All fields shown are required. Place this inside
`objects[i].extensions.ray_tracing`. As in M1, the matching authoring material
row contains an explicit `material_texture_stack` with one to eight uniquely
identified brick/solid layers. Stable IDs and mapping seed determine variation;
triangle indices, mesh tessellation, object order and the camera do not.

M2 accepts explicit `mesh_asset_instance` projections and existing plane/prism
bindings. It does not infer a sphere from bounds. Mesh transforms must use the
authored-origin pivot with finite positive scales; mirrored, vanishing-scale,
custom-pivot and bounds-center-pivot mesh transforms are rejected. Axes are
orthonormal; object IDs are nonempty, unique, and shorter than 64 bytes. A v1
planar binding remains limited to plane/prism geometry. Asset-side procedural
graph/material composition is not part of the M2 acceptance set.

Unknown versions, policies, required capabilities, invalid frames/radii, and
incompatible sources fail validation. Old readers can ignore unknown extensions:
producers must choose a consumer supporting `optic.axial_surface_v2`. No existing
public release manifest is changed by this work.

## Axial meaning and limitations

`axis_v` is the height direction. `axis_u` is the zero-angle radial reference;
`cross(axis_v, axis_u)` supplies the positive angular direction. Projection occurs
at each interpolated surface position, never by interpolating wrapped vertex
angles. Object-rest mapping inverts the authored transform; world mapping uses
world meters. Nonuniform scaling stretches the attached pattern by design.

Let `R` be the declared reference radius, `w` the requested tile width, `h` the
tile height, and `theta` the angle around the axis minus `seam_rad`:

- Circumference repeats: `N = round(2*pi*R/w)`, restricted to 1–64.
- Effective reference width: `2*pi*R/N`, displayed in the inspector.
- Horizontal coordinate: `fract(theta/(2*pi) + offset_u/(2*pi*R)) * N`.
- Vertical coordinate: `(height_above_origin + offset_v) / h`.

Courses follow physical height, not angular latitude. Width narrows with distance
from the reference circumference toward a sphere's poles. The periodic source
wraps brick identity and grain at the declared circumference, including staggered
rows. Layer placement offsets remain cell units; axial layer scale must be 1 and
layer rotation 0. Global planar rotation and pivot must be zero for axial mapping.
These restrictions keep the declared courses and seam coherent.

For radial distance `r` below `pole_radius_m`, source weight is
`smoothstep(0, 1, r/pole_radius_m)`. The material blends continuously to its base
response at the axis. Exact axis points are finite, valid and tagged singular,
with zero source contribution. This deliberately exposes the distortion and
special treatment instead of claiming an undistorted seamless sphere unwrap.
Cylinder end caps inherit the axial projection: constant-height spokes fade near
the center. Separate cap charts and region assignment are later work.

The height acceleration interval must be finite, increasing, and span no more
than 64 tile heights. Coordinates remain bounded to the existing million-tile
query domain. Normals affect lighting, not mapping identity. Authored UVs,
tangents and runtime derivative footprints remain absent; the viewport computes
its own screen derivatives for filtering.

## Viewport and runtime

The shared core owns pure coordinate meaning and validation. optiC adapts scene
transforms, evaluates periodic brick channels, persists documents, and controls
UI/filtering. Existing scene, mesh and preview contracts carry geometry; no new
mesh format or kit was introduced.

The mapped-mesh viewport interpolates positions before evaluating coordinates.
It caches 512×256 source samples plus mip levels in four bounded chart slots
(about 22 MiB), independent of the camera. Analytic angular derivatives avoid
seam discontinuities when selecting mip levels. Bilinear/trilinear filtering and
the pole fade provide the minimal minification treatment for this checkpoint.
The existing primitive cache remains separate. More than four visible mapped
objects may evict/rebuild chart entries; this is not a large-scene performance
qualification. Mapping/source edits rebuild caches; orbit and workspace changes
reuse them.

Mesh rest positions come from displayed geometry, so provisional object-space
transforms keep the pattern attached. World-space samples use displayed world
positions. Committed mesh transforms are independently checked against runtime
hits. M1's legacy primitive provisional-grid limitation remains documented there.

The common unfiltered query is the material parity boundary. Cached viewport
values are approximations. Studio lighting, transparency display, reflections
and final transport remain distinct; M2 does not make shaded screenshots
pixel-identical. Outside the declared detail interval direct sampling preserves
meaning but bypasses the chart mip filter. Arbitrary frequencies, cap charts,
large scenes, graph execution and production performance belong to later gates.

## Reproduction and evidence

From the repository root, with native desktop access:

```sh
make -j4 BUILD_TOOLCHAIN=clang scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/test_surface_mapping_m2.py --output-root build/surface_material_m2/new-run
python3 tests/integration/test_surface_mapping_m1.py --output-root build/surface_material_m2/m1-run
python3 tests/integration/test_material_viewport_parity.py --output-root build/surface_material_m2/legacy-run
python3 tests/integration/check_surface_material_m1_legacy.py --output-root build/surface_material_m2/legacy-run
make -C third_party/codework_shared/core/core_authored_texture test
```

Use new output directories. M2 generates low/high sphere and cylinder assets plus
an exact midpoint-subdivided copy of the low sphere. Actual ray samples are
checked against independent angle/height formulas and all eight unlit channels.
The exact subdivision compares hit positions and channels; low/high curved
meshes are not asserted to have identical geometry or whole rendered images.
Seam-adjacent samples, poles, invalid edits, positive nonuniform transforms,
world coordinates, stable duplication/reindexing, inspector source creation,
undo/redo, save/reopen, provenance, cache reuse, minified motion and native orbit
are checked separately. Logs record orbit timing rather than promising 60 fps.
M1's unchanged eight legacy BMP hashes remain the backward-compatibility gate.
Native captures and direct-light renders are retained for review; physical user
acceptance is separate from these software checks.
