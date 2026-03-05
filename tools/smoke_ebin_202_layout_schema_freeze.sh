#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_202_layout_schema_freeze_${TS}.txt"
SEED_SCRIPT="$ROOT_DIR/tools/seed_sdcard_machine_profile_semver_fixtures.sh"
VALIDATOR="$ROOT_DIR/tools/validate_st_ebin_layout_schema.py"
CONTRACT="$ROOT_DIR/docs/emu_engine_v2/reference_ebin_packages/st520_st1040_ebin_freeze_v1.json"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"

python3 "$VALIDATOR" --contract "$CONTRACT" >> "$OUT"

bash "$SEED_SCRIPT" "$TARGET" --versions 1.0.0,1.2.0,1.1.5 >> "$OUT" 2>&1

python3 - <<'PY' "$TARGET/ebins/atari_st/machine_profile/index.json" "$TARGET/ebins/atari_st/machine_profile/manifest.json" >> "$OUT"
import json
import sys

index_path, manifest_path = sys.argv[1], sys.argv[2]
with open(index_path, "r", encoding="utf-8") as f:
    index = json.load(f)
with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

index["schema"] = "st_component_index_v1"
index["machine"] = "atari_st"
index["component"] = "machine_profile"
index["layout_version"] = "st520_st1040_layout_v1"

manifest["schema"] = "st_component_manifest_v1"
manifest["module_id"] = index["module_id"]
manifest["layout_version"] = "st520_st1040_layout_v1"

with open(index_path, "w", encoding="utf-8") as f:
    json.dump(index, f, indent=2)
    f.write("\n")

with open(manifest_path, "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")

print("metadata_upgrade=pass schema=st_component_*_v1")
PY

mkdir -p "$TARGET/ebins/atari_st/cpu" "$TARGET/ebins/atari_st/chipset" "$TARGET/ebins/atari_st/io" \
  "$TARGET/ebins/atari_st/storage" "$TARGET/ebins/atari_st/audio_gpio" "$TARGET/ebins/atari_st/packages"

python3 "$VALIDATOR" --contract "$CONTRACT" --artifact-root "$TARGET" >> "$OUT"

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"