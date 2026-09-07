#!/usr/bin/env python3
"""Validate PNG signatures and IHDR dimensions without third-party modules."""

import pathlib
import struct
import sys


def validate(path: pathlib.Path) -> None:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"{path}: not a valid PNG header")
    width, height = struct.unpack(">II", data[16:24])
    if (width, height) != (800, 480):
        raise ValueError(f"{path}: {width}x{height}; expected 800x480")


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "sample-keymaps")
    files = sorted(root.glob("layer_*.png"))
    if not files:
        raise ValueError(f"{root}: no layer_*.png files")
    for path in files:
        validate(path)
        print(f"OK {path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)

