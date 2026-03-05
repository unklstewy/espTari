#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_203_cpu_shared_build_${TS}.txt"
BUILD_SCRIPT="$ROOT_DIR/tools/ebin_builder/build_st_cpu_m68k_shared.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"

bash "$BUILD_SCRIPT" "$TARGET" >> "$OUT" 2>&1

CPU_DIR="$TARGET/ebins/atari_st/cpu"
EBIN_PATH="$CPU_DIR/st.cpu.m68k-1.0.0.ebin"
INDEX_PATH="$CPU_DIR/index.json"
MANIFEST_PATH="$CPU_DIR/manifest.json"

for path in "$EBIN_PATH" "$INDEX_PATH" "$MANIFEST_PATH"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$EBIN_PATH" "$INDEX_PATH" "$MANIFEST_PATH" >> "$OUT"
import json
import struct
import sys

ebin_path, index_path, manifest_path = sys.argv[1:4]

with open(ebin_path, "rb") as f:
    hdr = f.read(60)

vals = struct.unpack("<IHHIIIIIIIIIIIII", hdr)
magic = vals[0]
component_type = vals[2]
code_size = vals[4]

assert magic == 0x4E494245, f"invalid magic: 0x{magic:08x}"
assert component_type == 1, f"component type must be CPU(1), got {component_type}"
assert code_size > 0, "code section must be non-empty"

with open(index_path, "r", encoding="utf-8") as f:
    index = json.load(f)
with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

assert index["schema"] == "st_component_index_v1"
assert index["machine"] == "atari_st"
assert index["component"] == "cpu"
assert index["module_id"] == "st.cpu.m68k"
assert index["winner"] == "1.0.0"
assert index["versions"] == ["1.0.0"]
assert index["files"] == ["st.cpu.m68k-1.0.0.ebin"]
assert index["layout_version"] == "st520_st1040_layout_v1"

assert manifest["schema"] == "st_component_manifest_v1"
assert manifest["machine"] == "atari_st"
assert manifest["component"] == "cpu"
assert manifest["module_id"] == "st.cpu.m68k"
assert manifest["winner"] == "st.cpu.m68k@1.0.0"
assert manifest["index"] == "index.json"
assert manifest["layout_version"] == "st520_st1040_layout_v1"

print("cpu_ebin_header=pass")
print("cpu_metadata_contract=pass")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"