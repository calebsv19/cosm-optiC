# core_units

Shared unit vocabulary and conversion primitives for cross-app scene/object workflows.

## Scope (bootstrap)
- Canonical unit identifiers and parser helpers
- Unit-to-unit scalar conversion
- World-scale conversion helpers for scene interchange

## Dependencies
- `core_base`

## Current Contract Notes
- Supported unit kinds are `meters` / `m`, `centimeters` / `cm`, `millimeters` / `mm`, `inches` / `in`, and `feet` / `ft`.
- Parsing is case-insensitive for the current plural names and symbols only; singular aliases such as `meter` are not part of the current surface.
- Conversion is scalar and length-like only. Scene schema meaning, object geometry semantics, UI formatting/rounding, compiler dimension analysis, and runtime solver semantics remain host-owned.
- Negative scalar values are supported for conversion helpers, which keeps signed offsets and distances valid at host call sites.
- World scale must now be finite and greater than zero.
- Parse and conversion failure paths clear output values deterministically before returning an error.

## Status
- Hardened bootstrap implementation (`v0.1.1`) with full unit vocabulary coverage, case-insensitive parse tests, negative scalar conversion coverage, and explicit finite world-scale validation.
