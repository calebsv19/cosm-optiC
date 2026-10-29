# Dynamic bracket/arch compound-pair qualification

October 22, 2026 — bounded Main Edit source qualification. The existing
child-aware Ball adapter and optiC immutable-result consumer need no further
runtime changes for these cases.

Six zero-gravity, two-second controls run at the native 240 Hz clock:
aligned and spinning bracket/bracket, arch/arch and bracket/arch pairs. The
pinned bracket has two children; the closed collision-source arch has three.
Both bodies are dynamic, with masses 2 and 3 kg, restitution 1, friction 0 and
the existing zero-cutoff policy. Spinning controls use initial angular velocities
(0.1, 0.2, 0.15) and (-0.15, 0.1, -0.2) rad/s. Source visible geometry, physical
properties and principal-frame interpretation retain their previous contracts.

All six pass the pre-existing physical gates. Maximum source-solid overlap is
0.128109 mm (limit 2 mm); maximum linear/angular momentum errors are
4.45e-16 kg m/s and 1.54e-13 kg m²/s (each absolute limit 1e-8). Maximum relative
energy deviation is 2.237e-6 (limit 0.001); this energy result is bounded, not an
exact-conservation claim. Both bodies change velocity by more than 0.1 m/s in
every case, and quaternion norm error stays below 1e-12.

All 2,886 scene snapshots repeat exactly through native replay and ASan/UBSan.
Each body remains a single compound physical object. Ten owner checks per build
confirm live two-compound contact, loaded-contact whole-world/body/cache/clock
rollback for all three pair families, and initial-overlap refusal. The aligned
arch/arch witness contacts all three corresponding child pairs: beam/beam,
left/left and right/right. Normal/sanitizer owner results match exactly.

optiC verifies every accepted tick and 415,584 original source triangle vertices
per build, normally and with its rigid modules/probe instrumented by ASan/UBSan.
Maximum source-position error is 8.89e-16 m (limit 2e-12). Surrounding renderer
objects are reused, so this is not whole-app sanitizer coverage. Portable bundles
reopen at both endpoints after relocation. Retained arch-child, bracket-seam and
bracket-impact controls reproduce all 1,083 prior frames exactly.

## Reproduction

Use fresh task-owned build and output roots. From the Ball checkout:

```sh
make BUILD_DIR=<normal-build> rigid-scene-native-build \
  rigid-scene-cache-io-build rigid-scene-compound-contract-build
make BUILD_DIR=<san-build> \
  DEBUG='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  rigid-scene-native-build rigid-scene-compound-contract-build
python3 tests/qualify_rigid_compound_bridge.py --profile compound-pairs \
  --backend <normal-build>/contracts/rigid_scene_native \
  --pack-io <normal-build>/contracts/rigid_scene_cache_io \
  --sanitizer <san-build>/contracts/rigid_scene_native --out <new-proof>
python3 tests/test_rigid_compound_pairs_owner.py \
  --backend <normal-build>/contracts/rigid_scene_native \
  --observer <normal-build>/contracts/rigid_scene_compound_contract \
  --qualification <new-proof> --out <new-owner-proof>
```

For the sanitizer owner check, pass the sanitizer backend/observer and
`--asset-set <new-proof>/san-assets/asset_set.json`. In optiC, build
`rigid-result-consumer-probe` / `rigid-result-consumer-sanitize`, then use
`tests/test_rigid_elastic_playback.py --help` to compare the portable `scenes/`.
The new pair profile and owner test reuse the existing Ball numerical owner,
core_scene 1.9.0 development contracts and core_pack persistence. No new shared
API, solver equation or version change is introduced by this qualification.

The earlier elastic bracket support quietness failure and room travel refusal
remain unchanged. These finite pairs do not qualify arbitrary compound meshes,
many-body piles, sustained simultaneous contacts, all seam configurations, fast
or thin-feature collision, CCD, native image/video appearance or long-term
accuracy. Program/canonical/installed/release states remain unchanged. No
additional release version change is recommended for this tests/docs slice;
the previous feature's minor-release recommendation remains separate.

Next boundary: a bounded fast/thin-feature motion diagnostic that preserves
travel/refusal controls and determines where discrete stepping needs CCD.
