#!/usr/bin/env python3
"""Validate a rayEngine UHD CRT capture suite without image dependencies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys


EXPECTED_LAYOUT = {
    "source": {"width": 320, "height": 192},
    "signal": {"width": 910, "height": 262},
    "raster": {"width": 3840, "height": 2160, "format": "RGBA16F"},
    "tubeRect": {"x": 160, "y": 24, "width": 3520, "height": 2112},
    "nativePresentationScale": 11,
    "contentStartLine": 44,
    "contentLines": 192,
    "finalFilterAtReferenceSize": "none_1_to_1",
}


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as image:
        header = image.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    return struct.unpack(">II", header[16:24])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("suite", type=Path)
    args = parser.parse_args()
    errors: list[str] = []
    manifest_path = args.suite / "manifest.json"
    if not manifest_path.is_file():
        print(f"ERROR: missing {manifest_path}", file=sys.stderr)
        return 1

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("complete") is not True:
        errors.append("manifest is not complete")
    if (manifest.get("referenceRasterWidth"),
            manifest.get("referenceRasterHeight")) != (3840, 2160):
        errors.append("manifest reference raster is not 3840x2160")
    entries = manifest.get("entries", [])
    if len(entries) != 8:
        errors.append(f"expected 8 capture entries, got {len(entries)}")

    for entry in entries:
        name = entry.get("file", "")
        image_path = args.suite / name
        sidecar_path = image_path.with_suffix(".json")
        if not image_path.is_file() or not sidecar_path.is_file():
            errors.append(f"missing image or sidecar for {name!r}")
            continue
        try:
            dimensions = png_size(image_path)
        except (OSError, ValueError) as error:
            errors.append(f"{name}: {error}")
            continue
        if dimensions != (3840, 2160):
            errors.append(f"{name}: expected 3840x2160, got {dimensions}")
        sidecar = json.loads(sidecar_path.read_text(encoding="utf-8"))
        if (sidecar.get("width"), sidecar.get("height")) != dimensions:
            errors.append(f"{name}: sidecar dimensions disagree with PNG")
        if sidecar.get("renderLayout") != EXPECTED_LAYOUT:
            errors.append(f"{name}: render layout differs from UHD contract")
        expected_crt = bool(entry.get("crtEnabled"))
        if sidecar.get("crtEnabled") is not expected_crt:
            errors.append(f"{name}: requested CRT path was not applied")
        if expected_crt:
            receiver = sidecar.get("receiverState", {})
            for state_name in ("burstPll", "videoAgcAfc", "verticalPll"):
                state = receiver.get(state_name)
                if not isinstance(state, list) or len(state) != 4:
                    errors.append(f"{name}: missing {state_name} diagnostics")
                elif float(state[2]) < 0.90:
                    errors.append(
                        f"{name}: {state_name} is not locked ({state[2]!r})")

    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    if errors:
        return 1
    print(f"OK: {args.suite} contains 8 locked UHD CRT captures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
