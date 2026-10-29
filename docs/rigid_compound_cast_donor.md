> Subsequent source update: the additive [classified query](rigid_cast_contract_hardening.md)
> clears the retained gates in46 cases. The legacy query and historical baseline
> below are preserved; this is not runtime swept-path adoption.

# Compound-cast donor qualification on retained fast/thin failures

October 22, 2026 — read-only Main Edit donor qualification complete. The existing
sweep helper finds the retained missed crossings, but it is **not yet qualified
as a generic adapter clear/hit decision or an impulse-normal provider**. No
numerical owner, native adapter, limit, shared API or runtime behavior changed.

## Measured coverage

A test-only native probe loads the exact retained collision artifacts and calls
`ball_rigid3d_compound_cast_3d` directly. It does not step a runtime world or
recompile/relabel the failure inputs. Forty-one queries reproduce exactly under
normal repeat and ASan/UBSan; inputs and all corpus files remain unchanged.
Thirty-three declared checks pass and eight remain blocked:

| Coverage | Result |
| --- | --- |
| 19 original translation queries | All pass:17 hits and2 open passages; correct bracket/arch child identities |
| Three original missed plate crossings | All found; impact location error about0.1µm |
| Three body-order reversals | Same impact time; reversed normals as expected |
| Finite plate passing laterally by target | Clear; overlapping swept AABBs do not create a false hit |
| Four actually exhausted distance budgets | Explicit `distance_iteration_limit` failure; not a successful clear result |
| Three invalid configurations | Refuse with caller output unchanged |
| Three one-iteration thin-plate controls | Converge successfully; a small budget alone does not prove exhaustion |
| Two initial-contact states | Blocked: compound wrapper drops lower-level touching/overlap states |
| Three far-origin and three rebased cases | Impact times pass, normals fail the declared strict gate |

The predeclared impact-position gate is0.5µm, five times the existing0.1µm cast
target tolerance. The normal-vector gate is1e-6 against the analytic face normal;
it is an explicit readiness requirement for this evaluation, not a new guarantee
of the existing API. At the original coordinates the maximum normal error is
4.50e-9. Translating the scene by(100000,-200000,300000)m preserves impact
location within1.001e-7m but increases maximum normal error to4.11334e-4.
Pair-local rebasing reduces that maximum to5.19590e-5, which still fails1e-6.
Rebasing alone is therefore not established as a sufficient normal repair.

## Initial-state contract gap

At exact plate touching, the lower convex query reports `initial_touching`;
at coincident plate overlap it reports `initial_overlap`. The compound wrapper
returns success with `hit=false`, `toi=1` and no selected child for both. Its
current selection loop retains only future `HIT` results. A caller must not
interpret that false hit flag as proof of an initially clear sweep.

This does not authorize changing existing resting/contact behavior. The staged
adapter already has initial depth and support handling; an adapter-facing sweep
contract must preserve that ownership and explicitly distinguish initial contact,
future impact, clear motion and query failure. Initial-contact states must never
be silently erased or turned into zero-time response loops.

## Guard and response boundaries

Several pure sweep queries hit even though the native runtime refuses the same
proposed step on its travel or contact-envelope guard. A geometric hit does not
override those refusals. All retained native results and wrong trajectories are
unchanged. Query exhaustion returns false plus a failure reason; false must never
fall back silently to endpoint-only motion. Failed-query work counters are not
complete consumption measurements; reported successful-query counts should not
be used as the sole global work budget.

This establishes bounded translation-only detection and time-of-impact evidence.
It does not establish a validated collision response, general normals, rotation,
many-body CCD, sustained contact, native image/video acceptance or improved
runtime behavior. The older generic compound-CCD contract also passes in normal
and sanitizer builds; its original rotation fixture is surrounding regression
coverage, not new rotation qualification for these plate cases.

## Concrete improvement boundary

1. Add an adapter-facing result contract that preserves lower-level initial
   touching/overlap separately from future hits, clear paths and query failures.
   Keep current overlap/support policy and transactional refusal behavior.
2. Establish a reliable response-normal path. Test pair-local coordinates together
   with independent contact/manifold normal reconstruction or validated witness
   refinement. The measured rebasing experiment alone is insufficient; keep the
   failed far-origin normal checks until a declared repair passes them.
3. Then integrate a bounded swept-contact path into the staged adapter, locating
   the first retained impact pose, solving there and continuing only inside its
   event/work/travel budgets. Preserve atomic rollback and refuse unhandled
   query/response failures. The three original misses must become valid contact
   responses or explicit safe refusals; endpoint-only fallback is not success.

Donor contract/normal hardening is the next bounded implementation slice. Adapter
adoption, impulse/energy/overlap qualification and optiC result playback follow
separately. No public/package/source adoption or version change occurred here.

## Reproduction

From the Ball checkout, select fresh contained build and output roots:

```sh
make -f tests/rigid_compound_cast_probe.mk BUILD_DIR=<normal-build> \
  rigid-compound-cast-probe-build
make -f tests/rigid_compound_cast_probe.mk BUILD_DIR=<san-build> \
  DEBUG='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  rigid-compound-cast-probe-build
PYTHONDONTWRITEBYTECODE=1 python3 tests/qualify_rigid_compound_cast_donor.py \
  --probe <normal-build>/contracts/rigid_compound_cast_probe \
  --sanitizer-probe <san-build>/contracts/rigid_compound_cast_probe \
  --corpus <retained-fast-thin-diagnostic> --out <new-proof>
```

The corpus needs its original `diagnostic.json`, `cases/` and `assets/` files.
The separate make fragment adds only an explicit test build; it leaves the
hashed native adapter make lane unchanged. `qualification.json` distinguishes
completion of this evaluation from `donor_fully_qualified:false` and retains
every blocking check. No additional release version change is recommended.
