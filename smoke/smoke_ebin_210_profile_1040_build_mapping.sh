#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_210_profile_1040_build_mapping_${TS}.txt"
BUILD_SCRIPT="$ROOT_DIR/tools/ebin_builder/build_st_profile_1040_machine_profile.sh"
LOADER_C="$ROOT_DIR/components/esptari_loader/esptari_loader.c"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"

bash "$BUILD_SCRIPT" "$TARGET" >> "$OUT" 2>&1

PROFILE_DIR="$TARGET/ebins/atari_st/machine_profile"
EBIN_PATH="$PROFILE_DIR/st.profile.1040-1.0.0.ebin"
INDEX_PATH="$PROFILE_DIR/index.json"
MANIFEST_PATH="$PROFILE_DIR/manifest.json"

for path in "$EBIN_PATH" "$INDEX_PATH" "$MANIFEST_PATH"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$EBIN_PATH" "$INDEX_PATH" "$MANIFEST_PATH" "$LOADER_C" >> "$OUT"
import json
import struct
import sys
from pathlib import Path

ebin_path, index_path, manifest_path, loader_path = sys.argv[1:5]

with open(ebin_path, "rb") as f:
    hdr = f.read(60)

vals = struct.unpack("<IHHIIIIIIIIIIIII", hdr)
magic = vals[0]
component_type = vals[2]
code_size = vals[4]

assert magic == 0x4E494245, f"invalid magic: 0x{magic:08x}"
assert component_type == 5, f"component type must be SYSTEM(5), got {component_type}"
assert code_size > 0, "code section must be non-empty"

with open(index_path, "r", encoding="utf-8") as f:
    index = json.load(f)
with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

assert index["schema"] == "st_component_index_v1"
assert index["machine"] == "atari_st"
assert index["component"] == "machine_profile"
assert index["module_id"] == "st.profile.1040"
assert index["winner"] == "1.0.0"
assert index["versions"] == ["1.0.0"]
assert index["files"] == ["st.profile.1040-1.0.0.ebin"]
assert index["layout_version"] == "st520_st1040_layout_v1"

assert manifest["schema"] == "st_component_manifest_v1"
assert manifest["machine"] == "atari_st"
assert manifest["component"] == "machine_profile"
assert manifest["module_id"] == "st.profile.1040"
assert manifest["winner"] == "st.profile.1040@1.0.0"
assert manifest["resolved_profile"] == "st_default"
assert manifest["index"] == "index.json"
assert manifest["layout_version"] == "st520_st1040_layout_v1"

loader_text = Path(loader_path).read_text(encoding="utf-8")
assert 'strcmp(module_id, "st.profile.1040") == 0' in loader_text
assert 'return "st_default";' in loader_text

print("profile_1040_ebin_header=pass")
print("profile_1040_metadata_contract=pass")
print("runtime_mapping_proof=pass module=st.profile.1040 profile=st_default")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"