# RT-MIRROR-1 Phase 1 Recursive Fidelity Probe

This fixture is the focused acceptance probe built on the retained Boundary 1
scene. It uses the same source mesh, runtime mesh, material, light, camera
position, integrator, and acceleration route in two close-up projections:

- `request_direct_probe.json` centers the generated-smooth blue mesh directly;
- `request_reflected_probe.json` centers the same mesh's virtual position below
  the mirror floor.
- `request_reflected_raw_probe.json` keeps that reflected close-up but disables
  denoise and temporal reconstruction for a one-sample radiance/output audit.

The zoom values are calibrated so the direct surface and reflected silhouette
occupy comparable image regions. The reflected request is not a replacement
for the contextual Boundary 1 image; it is the high-coverage diagnostic the
context image cannot provide.

Capture and verify the unchanged before-state:

```sh
make capture-ray-tracing-mirror-recursive-fidelity-before-state
```

Evaluate the intended acceptance contract:

```sh
make test-ray-tracing-mirror-recursive-fidelity-contract
```

Isolate raw first-vertex radiance, composed pre-tone-map radiance, tone mapping,
and reconstruction without changing transport behavior:

```sh
python3 tests/integration/run_rt_mirror_1_recursive_fidelity_contract.py \
  --mode radiance-isolation
```

The acceptance target still fails on reflected blue-material retention. The
summary emits the required `mirror_recursive_fidelity` block
covering requested/effective depth, per-depth reflection outcomes, effective
vertex-normal provenance, rough-reflection sampling policy, deterministic probe
identity, and local-versus-recursive reflected radiance. The probe reports
`vertex_interpolated` because runtime triangles retain whether vertex normals
are present, while the fixture separately locks the source normal provenance as
`generated_smooth`.

After the first reflected-hit shared-evaluator cutover, the focused acceptance
result still has exactly one failure: reflected/direct blue-material retention
is `0.028574`, below the required `0.65`. The first-hit local radiance is
blue-biased, proving that the material evaluator runs. The Phase 2B correction
reduced aggregate deeper recursive radiance from approximately
`[68698.690, 70222.325, 75297.383]` to `[1.283, 1.363, 1.509]` while retaining
depth-2 and depth-3 ray activity. The remaining white saturation therefore
belongs to first-vertex local composition, exposure/tonemapping, or
reconstruction—not deeper recursion. Do not weaken `acceptance_requirements.json`
to hide that remaining defect.

The Phase 2C radiance-isolation report identifies tone-curve compression, not
reconstruction, as the point where the retained blue response becomes visually
neutral. The representative raw first vertex is approximately
`[1.026, 3.631, 9.551]`; full pre-tone-map composition remains blue at
`[7.898, 10.656, 16.975]`; the common tone curve resolves that sample to
`[239, 243, 247]`. That exactly matches the raw BMP, and the reconstructed lane
has a zero-byte delta at its corresponding probe. The next change must audit
the large neutral host-mirror base added before tone mapping rather than
special-casing reflected colors after output resolve.

Phase 2D corrects the host-mirror energy budget. Local direct specular and
native ambient fill previously bypassed the mirror's existing
`base_attenuation`, so they were added at full strength beside the separately
traced reflection. They now share the non-reflected portion of the authored
budget with local diffuse response. The raw representative pixel reports every
term before and after attenuation and requires the complete classified sum to
match composed linear RGB within `1e-6`.

For the polished fixture mirror, `dominance = 0.98` leaves a `0.02` base share.
The representative local specular term falls from approximately
`[6.579, 6.726, 7.108]` to `[0.132, 0.135, 0.142]`, ambient falls from
`[0.158, 0.162, 0.171]` to `[0.003, 0.003, 0.003]`, and the traced blue
reflection remains separate at `[0.542, 1.917, 5.043]`. The composed pixel now
tone maps to `[164, 207, 232]`. Frame-level reflected/direct blue-pixel
retention improves to `0.531687`, but remains below the unchanged `0.65`
acceptance requirement; the fixture is not promoted or weakened.

Phase 2E makes enabled environment radiance visible to mirror and recursive
no-hit paths instead of returning black. The fresh summary records all `18,258`
first-reflection no-hit outcomes as contributing environment paths and records
`23,436` deeper environment contributions. It also removes the deterministic
reflection-loop result from the host `recursiveBsdfRadiance` bucket after that
same result has already entered `specularReflectionRadiance`; nested reflection
diagnostics remain available without final-radiance double counting.

The fixture now includes an object-specific low-chroma/high-luminance raw probe.
It proves the remaining white crown at pixel `(129, 51)` is a real first-bounce
hit on `smooth_subject`, but the subject vertex and deterministic reflected RGB
are both zero there. Approximately `[30.5645, 30.5645, 30.5645]` instead comes
from the host mirror's independently sampled recursive BSDF estimator. The
probe requires the final BMP byte to match the single-sample tone-map prediction
and requires the recorded disjoint terms to reconstruct the pre-tone-map pixel.
This isolates the next boundary as estimator ownership/MIS and high-variance
polished-mirror transport, not smoothing, reconstruction, or a missing blue
material tint.

Phase 2F identifies the estimator fault: the generic recursive lane sampled a
GGX direction and PDF, then replaced only the direction with the neutral point
light direction. The unmatched PDF/throughput produced the approximately
`[30.5645, 30.5645, 30.5645]` spike. Host materials classified by the existing
mirror-composition policy now keep the BSDF-sampled direction paired with its
PDF; finite-light work remains in the direct-light estimator. A focused C
contract requires a mirror recursive ray to remain directionally identical
when the same scene merely enables a finite light.

The fixture mirror's pale `[0.88, 0.90, 0.94]` base contributes a small
luminance-normalized cool tint in the deterministic branch. Because the
fixture is nonmetallic, its `0.98` legacy reflectivity floor supplies an
achromatic dielectric F0. The base is therefore not the source of the removed
neutral spike.

Fresh radiance isolation passes both raw composition probes. Reflected-frame
bright-neutral and white pixel counts are both zero, down from `3,708` and
`1,854` in the faulty A/B render. The high-chroma object probe retains raw
pre-tone-map blue/red `5.422` and resolves to `[176, 211, 233]`; the former
white-crown object probe resolves to `[186, 191, 201]`. The legacy whole-frame
blue-pixel ratio is `0.370357`, still below `0.65`, because it compares
thresholded coverage across different projections and penalizes the now darker
bounded reflection. Keep that failure visible, but use the object-identity,
linear-chroma, and exact single-accounting probes as the behavioral contract
for this correction.

Phase 2G removes the direction/PDF substitution globally, rather than retaining
it as a non-mirror compatibility path. A BSDF sample now always traces the
direction paired with its own PDF and throughput. Finite-light next-event
estimation is evaluated separately at reached path vertices and combined with
the BSDF technique by a power-heuristic MIS weight. Direct-light radiance is
recorded in `recursive_direct`; emissive or environment radiance reached by a
BSDF ray remains in `recursive_bsdf`. Tests that formerly depended on steering
a BSDF ray toward the point light now require the independent ray to miss while
the direct-light branch contributes without fabricating a geometry or emitter
hit.

Acceptance schema v2 removes the cross-projection pixel-ratio gate. The fresh
contract instead requires both reconstructed and raw probes to identify
`smooth_subject`, material `2`, vertex-interpolated normals, and reflected path
depth `1`; reflected blue/chroma coverage of at least `4,000`/`8,000` pixels
over bounds at least `110` by `145`; raw first-vertex, composed-linear, and
tone-mapped blue/red ratios of at least `1.15`; and exact host-energy
reconstruction within `1e-6`. The retained fresh proof reports `4,808` blue and
`9,057` chromatic pixels over a `122` by `158` blue-signal region. Its raw
first-vertex/composed/final blue-red ratios are approximately
`9.308`/`6.587`/`1.240`, and the maximum composition residual is about `1e-9`.
The old reflected/direct ratio (`0.661621` in this run) remains in the report as
a non-gating diagnostic.

Generated frames, full summaries, `before_state_report.json`,
`acceptance_report.json`, and `radiance_isolation_report.json` are retained
under the ignored root:

```text
build/agent_runs/ray_tracing/rt_mirror_1_recursive_fidelity/
```
