# Environment Lighting Menu-to-Runtime Contract

Status: active correctness lane on `codex/ray-tracing-main-edit`.

## Objective

Make the existing top-level environment controls change the next prepared
native `3D` scene reliably, and expose the effective normalized environment
state in the menu before adding new ambient-color authoring controls.

## Current control map

| Menu control | Authored setting | Effective runtime meaning |
| --- | --- | --- |
| Environment mode | `environmentLightMode` | Exclusive Off, Top Fill, or Ambient policy |
| Ambient Brightness | `environmentBrightness` byte-domain compatibility value | Ambient surface strength normalized to `0..1` |
| Top Fill Strength | `topFillStrength` | Top-fill intensity normalized to `0..20` |
| Preset | `environmentPreset` | Neutral, Sky, or Warm Sky background gradient |
| BG Auto / Custom | `environmentBackgroundBrightnessAuto` | Auto derives a grayscale level from ambient strength; Custom uses its retained brightness and RGB tint |
| BG Brightness | `environmentBackgroundBrightness` | Background miss-radiance multiplier |
| BG Red / Green / Blue | `environmentBackgroundColorR/G/B` | Tint multiplied into the selected background preset gradient |

The prepared-scene cache owns a resolved `RuntimeEnvironment3D`. A menu value
change is not applied merely because `animSettings` changed: the application
must invalidate the prepared scene and allow the next preview/frame request to
resolve a fresh environment. Reapplying the same effective value must not
invalidate the cache again.

## Readback contract

The Lighting panel reports resolved rather than raw values:

```text
Ambient | Ambient 0.50 white | BG sky 0.50 auto | applied
```

`refresh pending` means the authored state changed and the prepared-scene
cache has not yet stored the corresponding generation. `applied` means the
cached and current generations match. This is a preview/next-frame readback;
an already launched deep or async render retains its immutable request.

Ambient surface color remains white in this slice. The three background RGB
sliders author the already-persisted Custom background tint and do not recolor hit
surfaces. Background preset/tint and ambient surface color are related but
separate controls. Custom brightness and RGB sliders are shown only while
Ambient and Custom are active. Auto retains the Custom values while presenting
an ambient-derived grayscale preview.

Environment button and slider changes persist immediately. Start captures the
menu-authored environment state and reapplies it after the selected runtime
scene restores its geometry and authoring overlay, so scene restoration cannot
silently replace the menu background for that run.

## Correctness acceptance

1. Mode, ambient strength, top-fill strength, preset, background mode, and
   manual background brightness use one app-local application boundary.
2. A real value change invalidates the prepared scene exactly once per setter.
3. A no-op write does not invalidate it.
4. The next prepared static scene contains the new environment state.
5. The menu exposes normalized effective strength, background source, and
   applied/pending state.
6. Configuration persistence and existing headless request compatibility stay
   unchanged.

## Queued authoring improvements

After this correctness slice, add an explicit ambient-color policy:

- `white` for backward-compatible scenes;
- `follow_background` for intentional sky-tinted fill;
- `custom` for independently authored RGB.

The compact background tint editor now exposes the already-supported background
RGB fields. A later slice can conditionally enable controls by active
environment mode and extend headless summaries with the same effective-state
explanation.

## Shared-first decision

Decision: `reuse-deferred`, app-local.

Shared `core_config` does not own application schema or persistence policy,
and shared UI kits do not own RayTracing action dispatch or renderer cache
lifetime. RayTracing therefore retains the environment application boundary,
cache invalidation, readback wording, and rendering policy. No shared module,
version, adoption matrix, or vendored subtree changes are required.
