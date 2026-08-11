# Surface Authoring Document V2

`ray_tracing.surface_authoring_document` v2 is editable orchestration source.
It is neither a renderer, a geometry generator, nor a replacement for the
existing material, microdetail, inset, growth, or curve compilers.

## Canonical model

Every effect is an explicit typed path:

```text
receipt-bound selector carrier
  -> receipt-bound scalar field
  -> domain output
  -> closed consumer adapter
```

Nodes declare input and output ports with domains. A connection is valid only
when its source and target ports share a domain; each input port has exactly
one source; the graph must be acyclic. This prevents a material value from
being silently used as geometry or an attachment request.

The scalar control domains are `selector_mask` and `scalar_field`. The six
physical output domains are `material`, `microdetail_normal`, `signed_relief`,
`attached_asset`, `deep_inset`, and `curve_groom`.

## Receipt-bound resources

Every resource records a stable ID, exact digest, exact source-mesh digest,
declared output domain, and compiler receipt ID/digest. The resource kind fixes
the allowed output domain:

| Resource kind | Domain | Consumer adapter |
| --- | --- | --- |
| `selector_carrier` | `selector_mask` | `selector_carrier_binding` |
| `scalar_field` | `scalar_field` | feeds a `domain_output` |
| `material_graph` | `material` | `material_runtime_binding` |
| `microdetail_field` | `microdetail_normal` | `microdetail_field_binding` |
| `attachment_recipe` | `attached_asset` | `attachment_compile_request` |
| `curve_groom_recipe` | `curve_groom` | `curve_groom_compile_request` |
| `signed_relief_recipe` | `signed_relief` | `geometry_derived_asset_request` |
| `deep_inset_recipe` | `deep_inset` | `geometry_derived_asset_request` |

`selector_carrier_binding` is resource-free: its connected selector node is
the sole owner of the carrier. A selector rewire therefore changes the resolved
carrier with one digest-guarded `edit-node-resource` operation; the binding
cannot retain a contradictory second carrier.

The recipe adapters are requests for existing family compilers. They do
not mutate the source mesh, generate an attachment, or promote a scene.

## Signed-relief request readback

The executable v2 resolver turns each `signed_relief` geometry request into a
receipt-bound `signed_relief_requests` entry. It records the exact source
identity, upstream scalar-field node/resource/receipt, and signed-relief recipe
artifact/resource/receipt, and designates
`procedural_surface_feature_relief_shell` as the future executor. Its execution
mode is explicitly `request_only_no_geometry_mutation`: resolution does not
compile a shell, write a mesh, attach an asset, or alter a scene.

## Deep-inset request readback

A `deep_inset` request must declare unique positive `feature_ids`. The resolver
receipt-checks the inset recipe and its inspectable feature field, then rejects
missing IDs or non-negative feature values. A valid request names
`procedural_surface_feature_inset_compiler`, preserves the scalar-field and
recipe identities, and requires a distinct closed derived shell. It remains
`request_only_no_geometry_mutation`; the compiler is not invoked.

## Attachment request readback

An attachment request requires `carrier_weighted_surface` root policy, a
positive bounded clearance factor, and a receipt-bound material-graph target.
The resolver records the selector/carrier root, scalar-field identity,
attachment-recipe identity, clearance policy, and material target for the
future `procedural_imported_surface_growth` executor. It requires a separate
closed attached asset and remains `request_only_no_geometry_mutation`.

## Curve-groom request and execution

`curve_groom_compile_request` is a distinct PSG-23E path, rather than a
growth attachment with a different name. Its selector carrier determines where
roots may appear; the editable groom payload determines how many and what
shape they have. A top patch, annular ring, or localized side clump is therefore
three carrier resources, not three hard-coded modes.

Every request requires `carrier_weighted_surface`, a receipt-bound
`curve_groom_recipe`, a separate material-graph target, and a complete bounded
groom payload: exact `strand_count` and guide count, point count, length and
taper, positive root penetration, comb/part directions and strengths,
bend/curl/clump controls, and seed. `edit-curve-groom` atomically replaces that
payload under an expected document digest and returns the preceding digest for
undo. Changing 50 strands to 100 changes only the selected consumer's groom
payload; it does not rewrite the carrier or source mesh.

The resolver emits `curve_groom_requests` naming
`procedural_carrier_curve_groom_authoring`. The bounded executor applies the
receipt-bound controls to the referenced PSG-23E authoring input and emits a
distinct `curve_asset_runtime_v1`; it requires exact source/carrier identity,
root provenance, finite tapered curves, complete guide assignment, and exact
repeat. The runtime materializer emits a separate `curve_asset_instance` with
its own guarded material target. Its roughness and metallic fields follow the
normal scene-material bridge and never modify the host mesh material.

## Agent operations

`procedural_surface_authoring_document_v2.py` provides `create`, `inspect`,
`compile`, `readback`, `edit-resource`, `edit-curve-groom`, and
`check-staleness`.

`edit-resource` is digest-guarded and returns the prior document digest for
undo. `check-staleness` compares observed resource bytes to declared resource
digests and reports every downstream consumer invalidated through graph edges.

`procedural_surface_authoring_v2_contract_matrix.py` derives a valid upstream
subgraph for each requested consumer set. It is the no-render proof layer for
individual lanes and compositions. It does not execute arbitrary commands.

## Current boundary

V2 plans and validates safe family-adapter requests. A later, separately
authorized integration may map those requests into scene/runtime loading and
execute the referenced compilers. Material variation, shading-normal detail,
derived-shell relief, deep topology, and attached assets remain distinct
physical claims with their own receipts and proof gates.
