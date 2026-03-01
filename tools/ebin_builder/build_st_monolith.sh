#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
OUT_DIR="$PROJECT_ROOT/build/ebins/system"
mkdir -p "$OUT_DIR"

IDF_BASE="${IDF_PATH:-}"
if [[ -z "$IDF_BASE" ]]; then
  IDF_BASE="$HOME/.espressif/v5.5.2/esp-idf"
fi

python3 "$SCRIPT_DIR/ebin_builder.py" \
  "$PROJECT_ROOT/cores/system/st_monolith/st_monolith.c" \
  -o "$OUT_DIR/st_monolith.ebin" \
  -t system \
  -e component_entry \
  --interface-version 0x00010000 \
  -I "$PROJECT_ROOT/build/config" \
  -I "$PROJECT_ROOT/components/esptari_loader/include" \
  -I "$IDF_BASE/components/esp_common/include" \
  -v

echo "Built: $OUT_DIR/st_monolith.ebin"
