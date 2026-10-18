# Finite-light visibility and optical transport

Authored rectangular lights participate in nearest-emitter intersections as well
as direct-light sampling. Reflected and transmitted rays use the same
rectangle position, normalized authored axes, width and height as the direct
sampler. The nearest geometry or emitter wins. A one-sided back face is a dark,
occluding surface, not a hole through which a farther emitter becomes visible.
Camera queries omit `legacy_intensity` rectangular lighting helpers, preserving
historical scene visibility without changing their illumination, photon emission,
or reflected/transmitted emitter hits. This applies by source type and radiometry,
not by scene or light name. Camera feature guides and adaptive emitter detection
use the same policy. `lambertian_radiance` rectangles remain visible physical
panels, including their dark back faces. Material emission geometry is unchanged.
Primary tile occupancy conservatively includes authored rectangles and every enabled supported
finite light, so background-only culling cannot hide a visible emitter.
Disabled light-set entries do not fall back to a duplicate compatibility sphere.
When no light set exists, the historical single-light wrapper remains supported.

Emitter intersections retain RGB radiance. Camera volume transmittance multiplies
RGB channel by channel; reflection, transmission and recursive consumers preserve
those channels. Scalar-only compatibility hit records still resolve as white.
Material-emitter sampling proxies do not become duplicate visible rectangles;
the existing material geometry/emission path owns those surfaces.

## Radiometric modes

`lambertian_radiance` rectangles reuse the existing radiometry evaluation and
its spectral radiance. On an emitting side, emitted radiance is independent of
viewing distance and angle. Do not multiply it by the direct-light sampler's
area, cosine or inverse-square factors; those convert radiance to irradiance in
that sampler. One-sided sources emit only on their declared front; two-sided
sources emit on both sides. Physical qualification covers nondegenerate,
orthonormal rectangular frames with a consistent authored normal.

`legacy_intensity` remains explicitly a compatibility convention. Its emitter
hits retain intensity, configured falloff and emission-profile weighting, now
with authored RGB. This is not a conversion to calibrated physical radiance.
Existing scene values, including water absorption and light intensity, are not
rewritten. Finite-radius legacy sphere/point lights retain their geometry and
scalar intensity convention while honoring authored RGB.

## Contribution ownership

The compatibility Disney V2 finite-light MIS probabilities do not describe an
individual authored rectangle. Authored rectangles therefore use a disjoint
estimator at opaque non-delta receivers: direct sampling contributes with unit
weight, and the competing BSDF-hit contribution is zero. Rectangle-only direct
radiance is retained separately, so other source contributions keep their
existing weights in mixed-light scenes. Camera rays, delta continuations and
physical-glass random walks receive full emitter radiance when they hit it.
The glass random walk still has no marginal directional PDF; no fabricated PDF
or ordinary directional MIS is introduced for it.

This bounded partition avoids adding a second estimate when rectangle visibility
is enabled. It does not qualify the generic compatibility MIS estimator, rewrite
legacy material emission conventions, or provide light-importance sampling at a
rough-glass boundary. Source-specific matching PDFs and direct-light/BSDF-only
reference comparisons remain a separate standardization step.

## Qualification boundary

Focused tests cover rectangle sidedness, distance/angle invariance in radiance
mode, rotated geometry, bounds/range/parallel/degenerate cases, RGB and chromatic
volume attenuation, legacy intensity, compatibility fallback, and estimator
ownership. Real water-material resolution and physical-glass transport are used
for a normal-incidence Fresnel expectation, exact total-internal-reflection RGB,
and an intervening opaque receiver. Surrounding material, photon, native render,
volume and topology regressions pass. A white legacy glass control retains exact
native BMP bytes.

Local native comparisons isolate an IOR 1.333, roughness 0.008 interface and a
warm rectangular emitter. They are controlled emitter proofs, not acceptance of
an entire aquarium, wave-normal footprint transport, temporal stability,
photon-enabled appearance, denoising or a new installed/released package.

Physical reference: [PBRT area lights](https://pbr-book.org/4ed/Light_Sources/Area_Lights).
