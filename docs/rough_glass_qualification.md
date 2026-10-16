# Rough-glass qualification checkpoint

The explicit Disney V2 glass transport repair is closed at a bounded local
qualification checkpoint on October 16, 2026. This does not qualify all rough
surfaces or opaque mirrors. See [physical glass transport](physical_glass_transport.md)
for estimator ownership, roulette-prefix semantics and remaining finite policies.

## Qualified behavior

- Unified medium-aware explicit glass transport retains Fresnel, Snell/TIR,
  tint, absorption, equal-index transmission and receiver filtering.
- Enabled explicit-glass macro paths continue beyond positive specular and
  transmission depth prefixes with compensated roulette. Zero disables a lobe.
- Runtime depth values support 0–64 where applicable; defaults remain 3/3/8.
  Opaque/diffuse and legacy limits remain distinct from explicit glass prefixes.
- Prior contained application/test builds, fourteen affected groups, termination
  helper sanitizers, installed package identity/signature and native controls passed.
  These are retained qualification results, not new tests run during checkpointing.
- Selected unit-white controls at roughness 0.4 and 1 stayed within 0.226% of
  unit radiance. Installed IOR 1 versus absent-orb controls were byte exact at two seeds.

## Native 512-square comparison

Four stills used IOR 1.5, roughness 0.4 or 1, 32 temporal passes, 32 transmission
samples, seed 3201 and denoise off. Camera, geometry, lighting and exposure were
fixed. Differences below are extended 32/32/64 relative to balanced 16/16/32.

| Roughness | Interior raw linear luminance | Rim raw linear luminance |
| --- | ---: | ---: |
| 0.4 | +0.0201% | +0.1113% |
| 1 | -0.0661% | -0.0032% |

Each mean uses 32 passes at 41 selected interior or 16 selected rim coordinates.
All four native terminal receipts, image/source hashes, dimensions and actual
32 probe pass indices passed. The operator accepted the visual quality of these
stills. One matched seed cannot distinguish sampling variation from residual
bias; these controls are not whole-scene flux measurements or a universal
energy-conservation certificate. General continuation safety still occurred in
some roughness-0.4 samples and remains a separate finite-policy limitation.

## Working presets and stop boundary

| Use | Diffuse/specular/transmission depth | Sampling |
| --- | --- | --- |
| Quick review | 8/8/16 | Choose sampling for review speed; no universal low-sample guarantee |
| Qualified orb still | 16/16/32 | 32 temporal passes, 32 transmission samples |
| Proposed orb animation | 16/16/32 | Same sampling; temporal acceptance pending |
| Extended spot check | 32/32/64 | Match sampling and seed to the control |

For future sweeps retain 64 values at 16 FPS for four-second playback. Existing
sweep videos retain their original bytes and renderer provenance; the repair did
not update those videos. No replacement animation was rendered in this checkpoint.

Nested/overlapping solids, complex normal maps, rough thin-sheet multiple
scattering, caustics, animation stability and arbitrary scene calibration remain
separate qualifications. Stop further glass changes unless a new case exposes a
measured defect. Mirror reflection detail and repeated-bounce qualification is the
next proposed behavioral slice; glass results must not be used to certify it.
