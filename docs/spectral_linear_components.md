# Ordinary V3 linear component films

Ordinary `surface_path_v1` final renders export `<image>.components.npy` beside
the BMP. Native editor jobs and the local headless CLI use the same streaming
writer. This includes the qualified smooth glass and homogeneous fog route.
Interactive previews and specialized calibration routes do not emit this file.

The saved scene control is optional and defaults to true:

```json
{
  "extensions": {
    "ray_tracing": {
      "spectral_transport_v1": {
        "quality_profile": "surface_path_v1",
        "surface_path_v1": {
          "linear_components": false
        }
      }
    }
  }
}
```

The example opts out of the file. Order diagnostics remain available in the
ordinary transport report. This switch changes output storage, not transport,
sampling, exposure, reconstruction, or the displayed component. It requires a
JSON boolean. Save/reopen preserves the explicit control.

## File and receipt contract

The file is NumPy format v1, C order, little-endian float64 (`<f8`) with shape
`(height, width, 11, 3)`. Rows start at the top. The final axis is unexposed CIE
1931 XYZ. Values have no display transform, gamut clamp, denoising or exposure
baked in. This is a tristimulus component film, not a wavelength-resolved cube.
The ordinary `spectral_transport.linear_components` report gives the schema,
component names, basename relative to the image directory, byte size, SHA-256,
and `written` status. Film dimensions and display policy are in the same report.
Native jobs retain it in `transport.json`; headless summaries embed it.

| Index | Component | Ownership |
|---|---|---|
| 0 | `total` | Native film before display conversion |
| 1 | `direct` | Ordinary no-medium contribution classified at camera surface depth zero |
| 2 | `surface_indirect` | Ordinary no-medium contribution at positive surface depth, including dielectric continuation |
| 3 | `volume` | Ordinary camera-scattered fog plus mapped volume caustics |
| 4 | `caustic` | Surface-map gather before a prior matte camera interaction |
| 5 | `caustic_bounce` | Surface-map gather after a prior matte camera interaction |
| 6 | `volume_caustic` | Mapped glass-to-fog contribution, already included in `volume` |
| 7 | `scatter_1` | Ordinary camera fog contributions with one medium collision |
| 8 | `scatter_2` | Ordinary camera fog contributions with two medium collisions |
| 9 | `scatter_3_4` | Ordinary camera fog contributions with three or four collisions |
| 10 | `scatter_5_plus` | Ordinary camera fog contributions with five or more collisions |

For each pixel and XYZ channel, preserve this operation order:

```python
reconstructed = ((direct + surface_indirect) + volume) + (caustic + caustic_bounce)
```

The result is bit-for-bit equal to the stored `total` in the qualified native
float64 path. `volume_caustic` and the scattering-order layers are **subsets**;
adding them to this total double counts light. Their sum reproduces `volume`
within floating-point accumulation roundoff. Subtraction can similarly leave a
small residual. Summing XYZ before applying exposure and the declared display
transform reproduces the native BMP. Independently transforming or clipping
layers and then adding their displayed images does not reconstruct that BMP.

The writer holds one pixel's components, streams to a unique adjacent temporary
file, and publishes only after a complete successful transport render. Failure
or cooperative cancellation closes and removes that temporary file. A hard
process crash can leave a temporary file, which is never a completed artifact.
No-overwrite publication is atomic even if another publisher creates the final
path after preparation; authorized headless overwrite uses atomic replacement.
The component file and the BMP/receipt are separate publications: consume the
film through a **completed** ordinary job/summary and verify its hash, rather
than treating file existence alone as job completion. These diagnostic files
are not render checkpoints.

The file uses `128 + 264 * width * height` bytes for current admitted dimensions:
about 12.4 MiB at 256×192, 522 MiB at 1920×1080, and 2.04 GiB at 3840×2160.
The fixed memory footprint avoids an additional full-film allocation. Use the
saved opt-out for jobs that do not need retained raw diagnostic layers.

## Scattering-order report

`scattering_orders.groups` reports XYZ sums and the number of primary samples
with a nonzero ordinary contribution to each group. These are contribution
counts, not collision counts or emitted watts. The existing `volume_events`
and termination diagnostics remain event counts. Split glass branches are
combined within their original primary sample, with their original weights.

The retained `direct`/`surface_indirect` labels follow camera surface depth;
dielectric interfaces count toward that depth. `surface_indirect` is therefore
not an exclusively diffuse-bounce layer. Surface-map classes can also include
camera-side medium continuation and remain outside the ordinary fog groups.

Order counts actual analog camera medium collisions. Surface reflections and
dielectric interfaces do not increment it. A light or surface contribution
following those collisions stays in that order. This classifies the ordinary
fog estimator, including its later surface interactions. Mapped volume and
surface-caustic families retain their existing separate ownership. A mapped
volume gather contains a forward first scatter and may also have camera-side
scattering; it must not be labeled universally as first order. This schema does
not assign total physical scattering order to those mapped families.

`ordinary_volume_xyz` sums the four ordinary groups.
`volume_accounting_residual_xyz` is `volume - (ordinary + volume_caustic)` and
should be floating-point roundoff. Film sums are image-space observer response,
not a global energy-conservation ledger. The existing per-band photon ledger
continues to report transport power. Group brightness alone is not an error bar
or a convergence certificate; independent seed comparisons are required to
measure remaining noise.

## Verification

The dependency-free checker verifies file identity, finite values, canonical
bit-exact recombination, native display pixels, component/order report sums,
and the fog subset residual:

```bash
python3 tools/spectral_components.py \
  --summary <job-summary-or-transport.json> --image <frame.bmp>
```

`test-spectral-components` covers incomplete films, cancellation cleanup,
no-overwrite publication races, replacement, and file hashing.
`test-headless-spectral-ordinary-glass` exercises the exporter on the existing
physical glass/fog controls, opt-out pixel identity, malformed settings, and an
independent event-trace reconstruction of scattering orders. The native editor
spectral integration test verifies File Save/reopen, raw file identity with the
headless route using the exact saved camera, and in-flight cancellation after a
committed strip. These tests do not establish general scene convergence or
remove the existing material/medium/geometry admission limits.

For bounded seed/covariance and independent mixed-path experiments, see
[spectral error diagnostics](spectral_error_diagnostics.md).

For bounded independent still campaigns, exact sample weighting and empirical
variance films, see [independent batch accumulation](spectral_batch_accumulation.md).
