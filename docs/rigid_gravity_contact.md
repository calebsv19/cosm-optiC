> Later October23: the separately qualified [friction-bearing/tangential support profile](rigid_friction_support.md) extends the zero-friction checkpoint below. Its force-line certificate, closed-form impact and bounded slide/stop ownership are explicit; the dated friction refusal below is historical.

# Gravity-driven resting and recontact continuation

October 23, 2026 — bounded Main Edit source/headless qualification.
The retained zero-restitution rest refusal and small-restitution return refusal
now complete their impact tick and two seconds of subsequent resting motion.
The profile remains one dynamic body against one static body, with a supported
face patch, normal gravity, zero friction and zero rolling/torsional resistance.

## Force and event ownership

`rigid_scene_gravity_contact.c` owns continuation after the earlier quadratic
free-segment certificate fails. A nonnegative normal reaction may balance closing
normal gravity only when the measured gap and all residual normal/rotational
travel fit the SAME 1e-12 m coordinate allowance throughout the interval.
A convex pressure certificate requires the projected COM and its bounded
excursion strictly inside the contact hull. The resultant then has zero COM
torque. Any tangential gravity remains in the net acceleration.

Bodies advance using their solved velocity, net acceleration and the existing
free-rotation integrator. Pressure force work is `m a_reaction dot delta_p`;
gravity work remains `m g dot delta_p`. Position, attitude and velocity are never
projected or clamped. A stationary-looking result can retain motion below the
coordinate resolution; the implementation still integrates it.

If a supported face separates and returns, the owner advances through the actual
ballistic arc to its retained contact-gap level. With normal recession `r > 0`
and normal acceleration `a < 0`, return time is `2r/(-a)`. The measured starting
gap stays in the full-interval quadratic certificate; it is never reset to zero.
The new retained pose must again pass whole-compound gap and face-patch checks
before a contact solve. There is no fabricated minimum elapsed time. The existing
64-event cap includes these return solves and the resting-contact solve.

`rigid_scene_sweep_impulse.c` factors the existing cold impact operation without
changing its behavior. Captured resting contacts and their return events use the
existing zero-cutoff coupled point-contact kernel, representing the face's at-most
four points as single-point constraints. This applies genuine sub-pN·s impulses
under the SAME sweep budget, material restitution and velocity threshold. It
avoids accumulating a residual left by the legacy absolute impulse cutoff.
No global solver setting changes.

## Captured state and rollback

Only a newly accepted ballistic impact can create the private captured-contact
state. Legacy initial-contact/support scenes keep their existing owner. Future
captured ticks perform one physical contact solve at the current retained pose,
then the same certified reaction/return continuation. They do not manufacture a
full-tick gravity-kick impact.

The contact identity and cumulative work/energy ledger are copied into the tick's
staging context. Bodies, world, caches, clock and this state commit together only
after validation. A refusal preserves the entire pre-step state and latches the
fault. Immutable input/result formats are unchanged; replay reconstructs this
transient state deterministically from the authored scene.

The energy ledger is cumulative across the captured episode with a fixed
incoming-energy/work reference and the existing 0.1% gate. It gets no fresh
per-tick allowance. Existing displacement, envelope, finite-state, static-body,
source-overlap and momentum gates remain in force.

## Qualification and limits

Sixteen normal/repeat/sanitizer cases pass: 13 accepted and 3 refused. The two
retained physical inputs are unchanged except rebinding authenticated collision
digests. Controls cover two-second rest/recontact,50m/s,3kg,reversed IDs/object
order,gravity0.01–49m/s² and a timing control with a genuine finite rebound.
The retained return case makes four contact solves. Each accepted case proves
next-tick rollback, and an injected elastic micro-bounce state exhausts the
unchanged 64-event cap with full rollback in both builds.

Fourteen independent square/triangle pressure tests and16 quadratic-gap tests
pass in both builds. All 36 preceding response controls and13 unchanged gravity
controls pass. The two former gravity refusals are exercised by the new contact
matrix. All 20 prior response/resting histories and all 10 accepted gravity
histories remain exact. The isolated final observers recheck 73 earlier owner
probes with normal/sanitizer parity.

Fixed physical gates remain0.5 µm analytic position,1e-6m/s velocity,0.1%
work-adjusted energy,1e-8 kg m/s momentum-ledger error and2 mm source overlap.
Normal and angular closed-system gates remain protected by the retained response
controls; static support reactions are explicitly external. Sanitizer runs use
leak detection disabled, as before.

Friction, rolling/torsional resistance, larger rotating residuals, coupled groups
and general curved/rotating contact remain outside this qualification. Their
retained refusals publish no result. Generic non-impact gravity integration and
legacy support policy are unchanged. No installed app, package, release or new
rendered-visual acceptance is implied. See the earlier
[gravity impact profile](rigid_gravity_sweep.md).

Both producer builds pass 38 runtime and 49 result checks. All 15 bracket/arch
histories reproduce 6,735 full frames exactly. The 58 current/retained playback
scene cases pass 8,315 ticks and 976,776 source-vertex comparisons per build,
maximum error 1.555e-15 m. All 34 current scene-case bundles verify/replay after
relocation. Matrix counts include overlapping physical inputs.
