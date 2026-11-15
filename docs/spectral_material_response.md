# Visible source spectra and selective reflection through ordinary V3

`tools/spectral_response_testbench.py` creates matched opaque scenes with saved
`spectral_transport_v1` profiles and ordinary V3 requests. The request omits
`spectral`, so it exercises the same scene-profile route as ordinary native and
editor rendering. See the [transport backend](spectral_transport_backend.md) for
estimator ownership, admission and diagnostic lifetime.

This is a qualification bench. Source spectra and reflectances are declared
synthetic controls, not measured lamps or pigments. It uses the existing
piecewise-linear spectral source and Lambertian material models, with no
renderer/reconstruction changes.

## Matched controls

The room has neutral surfaces, one selective wall, a shadowing box and seven
back-wall reflectors. Image left to right, the reflectors are neutral, orange,
amber, green, cyan, blue and violet. Geometry, area-source position/shape,
camera, exposure, surface depth and scene policy stay fixed when a source changes.

The eight source profiles are broad white, a warm 2800 K Planck-shaped visible
curve, broad green bias, broad blue bias, violet 405 nm, blue 450 nm, green 535 nm,
and a three-peak source matched to broad white in CIE XYZ. Common curve knots
include the band peaks and remain within the qualified 32-knot limit.
Emitted power is zero at and below 400 nm; no UV extension or fluorescence is used.

All sources are normalized to the same incident CIE Y integral. They do **not**
have equal radiant power. The source-input CSV records both XYZ and integrated
radiance/power. This distinction especially matters for violet: its equal-Y
example requires much more radiometric power than broad white. Exposure3 and
the spectral display transform are fixed; no automatic white balance or
per-image brightness adjustment is used. Saturated spectral colors have RGB
display-gamut limits, so quantitative claims use unexposed XYZ.

The white-matched pair asks a different question from simply coloring a lamp:
can two sources with the same incident white XYZ produce different reflected
colors when their energy occupies different wavelengths? The amber reflector
is designed to distinguish them. A neutral reflector is the negative control.

## Independent physical checks

A separate source→selective panel→neutral receiver fixture has zero direct
source contribution at the receiver. The independently integrated reference is
the four-dimensional geometric transfer factor times the spectral integral of
source radiance and both reflectances. Geometry and spectral quadrature are
refined independently of the native sampler. CIE table data and the declared
vacuum-to-air observer convention are shared inputs.

The initial gate covers eight sources × six materials, with four independent
seeds × 8192 camera samples for nonblack cases and an exact black control. Its
predeclared relative XYZ L2 tolerance is 10%; this is a finite-sample qualification
bound, not a universal accuracy claim. A 16-seed precision follow-up uses the
same white source pair with neutral, amber and blue reflectors. It retains the
initial results and reuses the same validated case identities.

A separate shadow-point study uses eight independent seeds × 8192 samples at
the same floor target as the shadow camera. It narrows the field of view for
point precision. This is not a full-sensor crop or a replacement for the matched
images. Orange, neutral, green, black and amber wall controls retain the source
and geometry; the directly shadowed receiver has zero direct contribution.
Subtracting the black-wall **linear XYZ measurement** identifies the incremental
colored indirect contribution. RGB image subtraction is not an energy measure.

## Visual and sampling proof

The main set is 320×240 at 1024 camera samples/pixel; shadow views are 256×192
at 1536 samples. Both retain 12 surface interactions and satisfy the unchanged
1.2 billion nominal work guard. The bench keeps the total, surface-indirect and
direct component views separate. Displayed component images cannot be added
as radiometric values because their display transform is nonlinear.

Independent seed replicas of broad white, three-peak white and broad green
check full-film displayed RMS against the predeclared 5/255 comparison bound.
This is a limited repeatability gate. It does not certify every spectrum,
every local shadow block, finite-depth bias or arbitrary-scene convergence.
Projected swatch interiors provide additional labeled display-RGB measurements;
they are explicitly not linear XYZ measurements.

## Reproduction

Use the actual local headless binary printed by the build or a hash-verified
retained qualified binary. Choose a fresh contained output root:

```sh
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage preview
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage reference
# Inspect the four native preview PNGs before recording review.
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage accept-preview
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage final
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage shadow-probe
python3 tools/spectral_response_testbench.py --cli <cli> --output <root> --stage precision
```

Rendering uses only the Python standard library and the existing native CLI.
The final visual set uses up to four local processes. Each case has a 900-second
timeout; completed recipe/runtime/input/summary/image hashes are verified on
reuse. Uncertain or different existing cases refuse resubmission. No remote
jobs, installation, publication or scene-pointer promotion occurs.

For static charts, exact-pixel contact sheets, measured summaries and the HTML
gallery, use an analysis environment with Matplotlib and Pillow:

```sh
python3 tools/spectral_response_gallery.py --root <root>
python3 tests/test_spectral_response_fixture.py
```

The gallery contains native lossless renders, source/reflectance charts,
independent-reference results, source normalization CSV, component readback,
seed comparisons and raw per-case requests/receipts. Keep the saved scene,
ordinary request and summary together. These local fixtures do not update
latest-good or published scene pointers.
