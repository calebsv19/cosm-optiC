# Compound scene handoff fixture

`compound_scene_renderer_handoff_v1.txt` is the frozen Phase 43 S8 packet
produced by Ball Bounce. Its file SHA-256 is
`dc15b7376ab82fad5a45d33096e24c905c012a99e49b3fd8acc2fc3526b5ef6d`.

The packet is copied into this fixture so optiC's importer contract is
self-contained. Runtime and test code must not read the Ball Bounce checkout,
load collision hulls, or link the Ball Bounce generator, solver, installed
representation loader, compound builder, or V-HACD path.

The frozen packet contains two source identities and 721 exact transform
frames. It contains no mesh bytes, materials, cameras, lights, sampling, or
final-image policy.

S9-E adds `source_scene_runtime.json` plus the exact C2 U-channel authored
source mesh as `assets/mesh_assets/mesh_c2_u_channel.runtime.json`. The runtime
mesh was compiled from the source STL whose packet-bound SHA-256 is
`0da0eacd10e7197c77923c29bcb6fcb0325616e9c21f7f18e2523a7440057aac`.
It has 48 crease-aware render vertices and 28 triangles. This is source art,
not a collision hull. The fixture scene supplies only renderer-owned mesh and
material identity; S9-E's local proof supplies renderer-owned camera, light,
sampling, static set dressing, and final-image policy without changing runtime
defaults.

S9-F adds `mesh_c1_l_bracket.runtime.json`, compiled from the packet-bound
`c1_l_bracket_v1` source whose SHA-256 is
`1be7be9153ecce8d681dff00dd320b22bb316f75d5defb660c2984381d2867d9`.
The frozen runtime fixture SHA-256 is
`388cefb45dc3efe12f905c39945cea96de0a987441104dae3ac82d20487e8f43`.
It has 36 crease-aware render vertices and 20 source triangles. Together the
C2 and C1 fixtures prove typed two-body assembly; neither file contains
collision-proxy geometry.

S9-G does not mutate this producer packet. Its RayTracing-owned archive refers
to both runtime mesh files externally and records four assembly ticks. Because
the packet contains no static-wall planes, current floor/backdrop/marker
records are explicitly `renderer_set_dressing`, never simulated collision
surfaces. A future room packet must export exact wall-plane provenance before
RayTracing can make a collision-aligned wall claim.
