# core_sim_trace

Optional `core_sim` to `core_trace` adapter.

## Scope
- Map `CoreSimFrameRecord` control-plane outcomes into standard trace lanes.
- Emit frame markers for tick/render/clamp/single-step/failure reasons.
- Keep `core_sim` dependency-free by living as a sibling adapter module.

## Boundary
`core_sim_trace` owns only simulation control-plane trace vocabulary.

It does not own:
- app entity/world snapshots
- solver state
- replay execution
- UI or HUD rendering
- `core_data` or `core_pack` table/chunk schemas

Apps should add domain-specific trace lanes beside these shared lanes, not by
expanding the generic `core_sim` trace vocabulary.

## Build

```sh
make -C shared/core/core_sim_trace
```

## Test

```sh
make -C shared/core/core_sim_trace test
```

## Change Notes
- `0.1.0`: initial frame-record to trace mapping helper.
