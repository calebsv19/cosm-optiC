# Water body boundary W2 fixtures

These renderer-local fixtures prove the optional `water_body_boundary_v1` contract,
legacy manifest compatibility, fail-closed validation, dry-container fill policy,
closed-body topology, and legacy shell suppression. The dynamic-unified fixture
also preserves the established accepted-aquarium shape: dynamic closure may omit
`legacy_shell_object_id` when the runtime scene contains no earlier water-shell
geometry. They do not regenerate or change PhysicsSim sidecars.
