# Per-pixel sampling and final reconstruction

The Main Edit renderer uses per-pixel convergence evidence within one output
frame. It does not reuse animation-frame history or require editor UI changes.
Existing temporal-pass and adaptive-sampling settings remain the controls.

## Sampling policy

Each pixel retains an arithmetic mean of accepted render samples. Separately,
unfiltered and unclamped RGB means and second moments track sampling uncertainty
using Welford updates. Display conversion and denoising never feed this evidence.

For every RGB channel, estimated standard error must be at most
`0.005 + 0.02 * abs(raw_mean)`. Checks start at eight actual samples and must pass
on three consecutive sampled updates: the earliest eligible sleep is sample ten.
This is a bounded heuristic, not a formal guarantee that rare events were sampled.
An absolute tolerance handles dark pixels without dividing by nearly zero luma.
Nonfinite input is rejected before mutating the region. Variance overflow remains
nonconverged. Masked pixels keep their own means, counts and evidence.

The existing geometry/material/transparency/direct-light risk policy and neighbor
padding still apply. High-risk pixels remain active. Sleeping pixels are probed
on the existing four-pass cadence while the frame continues; a new noisy sample
resets their stable streak. Rechecks use completed passes rather than the sleeping
pixel's frozen sample count. Adaptive sampling can still be disabled to obtain a
fixed-budget reference. The configured temporal count is a maximum, not a target
that every ordinary interior pixel must reach.

Moments and streaks cost an additional 25 bytes per pixel and are included in
render-unit memory accounting. Checkpoint binary schema 3 persists them alongside
sample counts and means. Sampling identity also names the policy revision;
older checkpoint histories are incompatible and cannot silently mix estimators.
Interrupted/resumed output must equal uninterrupted output for the same policy.

## Final reconstruction

The final Disney v2 pass retains its existing object, normal, depth and
radiance-edge guards, transparent/mirror/glossy protection, and temporal-activity
exclusion. Two candidate expansions were tested and rejected: loosening glossy
protection and allowing all noisy opaque interiors through the activity gate.
Both increased displayed-image error against the retained reference. The original
filter is preserved; the new sampling evidence does not yet drive reconstruction.

This pass does not claim separated diffuse/specular/transmission filtering,
a universal noise cure, or physically unbiased energy: the existing render-sample
firefly clamp is still history dependent. Raw moments deliberately precede it so
clamping cannot falsely declare a pixel stable. A later variance-aware filter
must show better reference error, not merely a smoother-looking image.

Existing summary fields expose traced/skipped sample counts, budget buckets,
risk/probe counts and denoise filtered/preserved counts. Progressive tiles remain
raw accumulated previews; final reconstruction is separate. Dedicated UI status
labels and component-specific uncertainty views can follow this backend contract.

## Reproducible validation

```sh
python3 tools/pixel_quality/validate.py \
  --renderer /absolute/path/to/ray_tracing_render_headless \
  --request /absolute/path/to/request.json \
  --out build/new_pixel_quality_comparison
```

The request must contain an absolute scene path. The output must not already
exist. The tool retains a 96-pass nonadaptive reference, 48-pass fixed-budget,
48-pass adaptive raw, and 48-pass adaptive denoised runs. It reports actual sample
counts, route mismatches and displayed RGB RMSE against the reference. These are
mapped-image diagnostics, not linear-radiance ground truth or multiple-seed proof.

Focused tests cover known means/variance, masked counts, periodic probes, rare
unclamped bright events, finite-value rejection, frame reset, geometry/visual
edge protection, mirrors/transparency, tile/worker behavior and checkpoint recovery.
Local evidence is retained in `build/pro_pixel_validation/`, `build/pro_pixel_matte/`
and `build/disney_quality_pass/`.

On the 160x128 dresser comparison, fixed 48 passes traced 983040 pixel samples;
adaptive traced 473624 (51.8% fewer). Displayed RGB RMSE against the 96-pass
reference was 1.175 fixed versus 1.179 adaptive on the 0–255 scale. This is one
scene and one sampling sequence; it is not a general speedup or quality guarantee.
The rejected broader glossy filter scored 1.740, motivating its exclusion.
On the rough opaque variant, fixed/adaptive errors were 1.311/1.313 with
51.8% fewer samples; the expanded opaque denoiser scored 1.642 and was also
rejected. The retained `denoised48` images in these two artifact roots are
rejected candidate evidence, not the committed reconstruction policy.

## Next quality boundaries

- Validate more seeds, thin geometry, multi-light rare events and moving scenes
  before tuning thresholds or relaxing material risk holds.
- Audit clamping bias/energy independently of reconstruction.
- Introduce separate diffuse/specular/transmission evidence before filtering sharp
  reflected or transmitted detail. Preserve albedo/material boundaries explicitly.
- Any future cross-frame reuse requires camera/geometry/material/light invalidation
  and validated reprojection; current state resets for each new output frame.

Reuse decision: app-local renderer policy (reuse-deferred). `core_math` owns
numeric primitives, `core_trace` diagnostics and `core_queue/core_workers` execution;
none requires a new shared API for this pass. No shared version or adoption change.
