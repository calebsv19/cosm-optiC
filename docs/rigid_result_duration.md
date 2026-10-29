# Exact rigid result duration

The retained local development workflow accepts up to **14,400 ticks at 240 Hz**:
60 simulated seconds and 14,401 stored states including tick zero.
This replaces the former 2,400-tick envelope. Shared `core_scene` development
1.11.0 owns `CORE_SCENE_RIGID_RESULT_TICKS`; Ball native admission, command
limits, Python cache/manifest checks and optiC playback agree with it.
Existing v1 schemas and shorter recordings remain compatible.

Stored immutable bundles have no automatic expiry. This is a simulated-duration
limit, independent of retention. Byte and node guards still apply: 64 MiB per
result bundle, 64 MiB JSON, four million JSON value nodes, 32 bodies, up to
12 convex children/body and the existing 496 child-pair budget. Dense worlds
can reach a size guard before the duration ceiling.

The local producer `run --wall-seconds` and explicit native `verify
--wall-seconds` accept a deadline in (0, 1800], default 60 seconds. A run deadline
covers recording, independent replay, verification, lock wait and publication.
Long captures can be expensive with the fixed-capacity diagnostic world; select
a budget explicitly. Timeout refuses publication and never fabricates a tail.

A one-body analytic free-motion control records and independently replays the
full minute. All 14,401 states are retained, the endpoint position error is below
3e-12 m, and over-cap/invalid-budget/reuse guards pass. Native runtime 38 checks
and immutable result/store 49 checks pass. Normal and ASan/UBSan optiC readers
qualify the duration separately from numerical solver behavior.

The earlier seven-body Orbital Repair Bay result genuinely covered tick 3202
(13.3417 s), then refused tick 3203 with `contact_advance_budget`. That historical
partial result is preserved. A separately qualified measured-contact repair now
records and independently replays the same scene through tick 7200 / 30 seconds,
with exact normal/sanitized equality and unchanged physical/rollback gates. See
[deck-contact continuation](rigid_deck_contact_continuation.md) for the repair,
full qualification and finite-deck travel/settling limits.

This is development-source qualification. Canonical adoption, installed app,
release/package versions, remote execution and aesthetic render acceptance are
separate.
