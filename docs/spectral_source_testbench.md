# Violet, blue and green source testbench

`tools/spectral_source_testbench.py` prepares or renders a small local comparison
using an existing V3 photon-map request. The default sources are exact 405 nm
violet, 450 nm blue and 530 nm green lines, each at 1 W. Two independent seed
streams use 16 full-camera maps each. Camera, emitter geometry, medium, exposure,
photon count, wavelength bins and reconstruction come from the base request.

From the repository root, prepare six cells without rendering:

```sh
python3 tools/spectral_source_testbench.py \
  --request tests/fixtures/spectral_surface_map/request.json \
  --output-root <fresh-output-root>
```

Add `--cli <headless-renderer>` to preflight and render each cell. For an existing
fog scene, pass that scene's v2 headless request instead. Open the resulting
`index.html` for images, authored spectra, status and links to each request and
receipt. Input paths are resolved against the base request; external scene and
volume dependencies remain required. This tool is not a portable asset packager.

Useful controls:

| Option | Meaning |
|---|---|
| `--wavelengths 405 450 530` | Centers in vacuum nm, within 380–780. |
| `--fwhm-nm 10` | Gaussian spectral width; 0 selects exact lines. Allowed nonzero widths are 0.03–100 nm. |
| `--watts 1` | Integrated source power, up to 10 W. Gaussian power is normalized over the supported visible domain. |
| `--seeds 19 104729` | Distinct admitted seeds; every map inside a render uses the native independent sequence. |
| `--passes 16 32` | Fixed budgets, each a multiple of eight from 16 to 64. |
| `--camera-sequence progressive` | Per-pixel progressive camera sampling; compatible default is `balanced`. |
| `--camera-seed 77` | Hold camera quadrature fixed while varying photon seeds; omission uses each photon seed. |
| `--coverage full` | Full-camera reference; `adaptive` selects the ordinary sparse schedule with full validation. |
| `--size 240 172` | Diagnostic dimensions, at most 512 per side. Must preserve the base camera's exact aspect ratio. Omission chooses a size up to 240 per side where an exact integer ratio is available. |
| `--timeout 300` | Per-cell render timeout in seconds, at most 600. |

There are at most 18 cells per invocation. Existing destinations are refused;
completed and failed cells remain reviewable. A new run requires a new output
root. The tool does not resume or retry uncertain work. A timeout terminates
the owned CLI and fails the invocation. No remote jobs or animations are started.

All cells retain the same exposure. Equal watts do not mean equal perceived
brightness, particularly near the violet end. The source plot describes the
authored line or relative Gaussian profile; it is not a measured receiver
spectrum. Per-band native measurements remain in each `summary.json`.

The gallery distinguishes `converged` from `pass_limit`. A fixed budget can
produce a useful comparison while still failing the selected image-quality
limits. Inspect local error and minimum/maximum pixel sample counts. Native
energy-ledger checks apply where volume band ledgers are present; an absent
ledger is not an independent energy proof.

Exact lines retain separate spectral channels. Gaussian sources use the existing
continuum bins, so narrow-profile wavelength-resolution qualification remains
separate. This tool adds no UV-domain extension, fluorescence, reconstructed
color from RGB, denoising or reconstruction changes.

Authoring tests: `python3 tests/test_spectral_source_testbench.py`.
Sampling semantics and native checks: [sampling guide](spectral_sampling.md).

To separate the two noise sources, compare `--seeds 19 104729 --camera-seed 77`
with the same request using `--camera-seed 78`. All four cells must use the same
wavelength, photon count, pass budget and reconstruction. Use fresh roots for
the two invocations. A progressive 32-pass full-camera comparison supplies 16
camera samples to each independent convergence group. It remains a capped test,
not an automatic quality guarantee.
