#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f build/flash_args ]]; then
  echo "build/flash_args is missing; run idf.py build first" >&2
  exit 1
fi

esptool.py --chip esp32s3 merge_bin -o build/merged.bin @build/flash_args
echo "Created build/merged.bin"

