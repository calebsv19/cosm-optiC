# Physical glass transport in Disney V2

Imported water uses the separate [clear-water optical contract](clear_water_transport.md):
neutral interfaces, no implicit diffuse mixture, and distance-only bulk absorption.

Smooth unified-water normals and shared optical normal-field differentials follow
the [water smoothing contract](water_surface_smoothing.md).

Finite rectangular light visibility and RGB follow the shared
[emitter transport contract](light_emitter_transport.md).

Explicit optical-response materials use the same boundary transport for camera,
reflection and sampled BSDF paths. Legacy materials without an optical-response
override retain their previous material estimator and saved representation.
No scene schema, UI command, MCP tool, preference or attachment change is required.

At each boundary, the medium stack supplies the actual incident and destination
indices. Exact unpolarized Fresnel divides reflected and transmitted events.
Solid rough glass uses a Smith GGX microsurface random walk (Heitz et al., 2016).
Each sampled free flight either escapes or reaches another microscopic facet;
exact Fresnel selects reflection or Snell refraction, and transmission changes
which side of the microscopic height field the ray occupies. Masked paths continue
inside that field rather than disappearing. Radiance eta scaling telescopes to
the ratio of the incident and final media. Rare walks beyond 64 interactions use
compensated roulette, not a fixed scattering-order cutoff.

The walk samples the total boundary transport directly. Its output explicitly has
no analytic marginal directional PDF (`hasMarginalPdf=false`, `pdf=0`); the glass
continuation consumes its already weighted direction without directional MIS.
The old sampler/evaluator remain explicitly named `SingleScatter` references for
diagnostics and thin-sheet compatibility. They must not be used as the total rough
BSDF or its PDF. The smooth and equal-index delta limits retain their exact paths.
Authored thin sheets retain their prior paired-interface approximation. Medium state
changes only on transmission, including equal-index transitions. Interface tint
and distance-based Beer–Lambert absorption remain separate quantities.

Equal indices give zero Fresnel reflection and a straight delta transmission for
any microfacet orientation. This is the analytic limit of the interface equations,
including non-air media. Geometry is still intersected and the stack still changes;
absorption, tint, emission and transmission mixtures still apply. A straight delta
event retains the incoming ray parameterization and differentials, so an unchanged
ray reaches the receiver with unchanged filtering. The former material-predicate
geometry bypass has been removed.

Paths advance through glass before classifying the next opaque receiver, emitter or
environment. They do not directly light glass as an extra opaque receiver. Each
segment's emission, attenuation and atmosphere are composed once. Nested optical
continuations use one sample per incoming path rather than restarting a full sample
set. The normal transmission sample budget remains at the primary glass vertex;
new temporal passes continue to draw their own stratified samples. Explicit-glass
reflection/transmission depth values now select a roulette-free prefix; enabled
lobes continue with compensated termination beyond that prefix. A zero value
still disables the lobe. Opaque/diffuse and legacy paths retain their hard limits. Null boundaries do not consume a scattering depth. At the start of a newly
parameterized ray segment, an outward first boundary can classify an origin
inside one solid even after an earlier segment entered and exited it. This
handles near-contact secondary-ray offsets without discarding the continuation.
Boundary matching remains strict within a segment, and this origin bootstrap
never replaces a known nested medium stack.

Denoising guides follow an evaluated, deterministic straight-transmission path
to its radiance receiver, including its throughput. Scattering paths keep the
conservative transparent filtering policy. Receiver visibility categories also
propagate to the native filter; the front boundary is not substituted for the
receiver when it cannot change the light direction.

The transformed mesh BLAS can return a conservative candidate outside the requested
world interval. Rejecting that candidate now continues the instance search rather
than reporting an instance miss and hiding the next boundary. The authoritative
world-space triangle intersection still decides admission. A bounded retry returns
unsupported when remapping cannot progress; it never invents an exit surface.

## Verification scope

### Deeper-path qualification and termination observation

The runtime diffuse, specular and transmission depth ranges now extend to 64.
Defaults remain 3/3/8. The three limits retain their existing meaning: they
compare continuation depth against the value for the next lobe, rather than
counting each lobe separately. For explicit-glass reflection/transmission that
comparison now starts roulette instead of discarding the path. Saved values and null-interface depth accounting
are unchanged. A profile above the previous ranges (8/8/16) explicitly enables
up to 64 general continuation iterations; historical profiles retain the previous
eight-iteration guard. The eight-entry diagnostic path history is independent
of execution and does not grow with the requested depth. Explicit glass retains a guard of 128 consecutive straight/null boundaries.
Scattering resets that guard and has no fixed 128-hit truncation; representational
integer overflow remains a separately observed safety failure.
Legacy non-optical primary transmission retains its separate historical policy;
this qualification targets explicit optical-response glass in Disney V2.

Set `RAY_TRACING_PATH_TERMINATION_PROBE=1` together with
`RAY_TRACING_NATIVE_3D_PIXEL_PROBE=x,y;...` to observe selected native pixels.
Up to 64 coordinates are supported. `PATH_TERMINATION` JSONL records summarize
one primary shading call/subpass using fixed-size thread-local storage and no
random-number draws. They include glass starts, receiver/environment/emitter
exits, lobe-depth cutoffs, medium/sample/side rejections, safety guards and
observed general-continuation and separately named glass roulette outcomes. Depth histograms count recorded
events, not unique paths. Nested walks are included. Local RGB weight sums are
diagnostic proxies, not estimates of omitted radiance or total energy loss.
The observer does not cover every legacy, visibility or photon-map termination.

For explicit-glass macro reflection/transmission, positive lobe values specify
an exact prefix. Beyond it, survival is the maximum RGB path weight with net
refraction eta-squared scaling removed, clamped to [0.05, 0.98]. Surviving future
contributions are divided by survival; already accumulated emission is unchanged.
The strict upper bound also terminates trapped lossless paths. Roulette uses a
full-range domain-separated stream keyed by path, sample, subpass and boundary;
it does not reuse a persistent 2D stratum. Straight delta boundaries do not draw
roulette or consume depth. Absorption and interface tint remain in path weight.
This policy does not use the general rouletteThreshold preference, whose existing
general-continuation contract remains unchanged.

The microsurface's existing roulette is independent. The observer itself still
adds no random draws. This removes black truncation from enabled glass macro
lobes, not all finite-depth bias from opaque/diffuse receivers, general continuation,
legacy transmission, geometry admission or malformed null chains. Existing depth
UI labels and saved values are preserved; in this glass lane the values now trade
variance and work rather than imposing a black cutoff. Compare matched depth
prefixes and independent seeds with denoising disabled before choosing presets.

The focused tests cover exact Fresnel, Snell/TIR, matched media at several angles
and roughnesses, sample/evaluation PDF and energy consistency, white-furnace no-gain,
nested equal-index stack transitions and retained Beer–Lambert absorption. A
transformed two-surface mesh test checks entry then exit with `t_min` advanced past
the entry. That test fails against the previous query implementation.

Native MCP qualification uses separate small saved fixtures and fixed-seed replays.
The IOR 1 comparison must match a no-sphere image across roughness, with no geometry
bypass available. Normal glass, frost, absorption, partial transmission, legacy
materials and Direct Light are separate regression cases. Large saved matrices
remain historical results until explicitly rerendered.

The solid dielectric random walk is qualified against independent reference
angular moments, two-sided equilibrium and white-environment slab/sphere controls.
This is not qualification of every normal-map configuration, rough thin-sheet
multiple scattering, caustic convergence, or arbitrary overlapping non-manifold
solids. Opaque/diffuse and legacy depth limits still bound those scene paths; glass
roulette prefix depth is distinct from microscopic scattering order. Existing direct-visibility and photon-map
policies remain separate from this camera/BSDF boundary correction.

## Microsurface references

- [Heitz et al., Multiple-Scattering Microfacet BSDFs with the Smith Model](https://eheitzresearch.wordpress.com/240-2/)
- [Dupuy and Benyoub, Sampling Visible GGX Normals with Spherical Caps](https://doi.org/10.1111/cgf.14867)

The implementation uses the published free-flight equations in log-CDF space and
a spherical-cap VNDF sampler that supports the below-horizon directions occurring
within the random walk. It does not import the tutorial into the application.

## Macro-path termination reference

- [PBRT, A Better Path Tracer: roulette compensation and eta scaling](https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer)
