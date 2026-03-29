# Shape Assets (`config/objects/`)

Canonical, flattened geometry assets shared across programs. Each file uses the ShapeAsset JSON schema:

```json
{
  "schema": 1,
  "name": "example_shape",
  "paths": [
    { "closed": true, "points": [ { "x": 0, "y": 0 }, { "x": 1, "y": 0 } ] }
  ]
}
```

Use the CLI converters to turn legacy ShapeLib JSON from `import/` into this format:

```
make cli-tools
./tools/cli/bin/shape_asset_tool --max-error 0.5 --out config/objects/airfoil_basic.asset.json import/airfoil.json
```

By default, `shape_asset_tool` writes to `config/objects/<basename>.asset.json` if `--out` is omitted. The runtime loads every `*.asset.json` in this directory into the ShapeAsset library; imported shapes can then be added to scenes via adapter helpers or future UI hooks.
