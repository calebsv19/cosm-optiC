#!/usr/bin/env python3
"""Write a public-safe JSON copy by redacting local/private path strings."""

import argparse
import json
import re
from pathlib import Path


PRIVATE_PATH_RE = re.compile(
    r"(^|[\s:=|\"'])(/Users/[^\s|\"']+|/private/[^\s|\"']+|[^\s|\"']*_private_workspace_artifacts/[^\s|\"']*)"
)


def redact_string(value: str) -> str:
    if (
        "/Users/" not in value
        and "/private/" not in value
        and "_private_workspace_artifacts/" not in value
    ):
        return value

    def replace(match: re.Match[str]) -> str:
        prefix = match.group(1)
        path_value = match.group(2)
        basename = Path(path_value).name or "path"
        return f"{prefix}<redacted-local-path:{basename}>"

    return PRIVATE_PATH_RE.sub(replace, value)


def redact_json(value):
    if isinstance(value, dict):
        return {key: redact_json(item) for key, item in value.items()}
    if isinstance(value, list):
        return [redact_json(item) for item in value]
    if isinstance(value, str):
        return redact_string(value)
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("src", help="source JSON path")
    parser.add_argument("dst", help="redacted destination JSON path")
    args = parser.parse_args()

    src = Path(args.src)
    dst = Path(args.dst)
    with src.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    dst.parent.mkdir(parents=True, exist_ok=True)
    with dst.open("w", encoding="utf-8") as handle:
        json.dump(redact_json(data), handle, indent=2, sort_keys=True)
        handle.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
