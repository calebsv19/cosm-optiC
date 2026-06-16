# Mirror Glossy Preservation Visual Matrix

Canonical D2.18 proof scene for mirror/glossy preservation. The scene puts a
true low-roughness mirror floor and a glossy vertical panel into the first-hit
view so the Disney v2 denoise pass can prove it preserves those surfaces rather
than blurring reflected or sharp glossy detail.

Use `request_disney_v2_denoise_off_12.json` and
`request_disney_v2_denoise_on_12.json` for the apples-to-apples comparison.
