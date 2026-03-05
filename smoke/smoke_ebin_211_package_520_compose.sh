#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_211_package_520_compose_${TS}.txt"
COMPOSE_SCRIPT="$ROOT_DIR/tools/ebin_builder/compose_st_520_package.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"

bash "$COMPOSE_SCRIPT" "$TARGET" >> "$OUT" 2>&1

BASE_DIR="$TARGET/ebins/atari_st"
PACKAGE_DIR="$BASE_DIR/packages/st.profile.520"
PACKAGE_INDEX="$PACKAGE_DIR/index.json"
PACKAGE_MANIFEST="$PACKAGE_DIR/manifest.json"

for path in \
  "$BASE_DIR/cpu/st.cpu.m68k-1.0.0.ebin" \
  "$BASE_DIR/chipset/st.chipset.glue_mmu_shifter-1.0.0.ebin" \
  "$BASE_DIR/chipset/st.chipset.mfp-1.0.0.ebin" \
  "$BASE_DIR/io/st.io.acia_ikbd-1.0.0.ebin" \
  "$BASE_DIR/storage/st.storage.dma_fdc-1.0.0.ebin" \
  "$BASE_DIR/audio_gpio/st.audio_gpio.psg-1.0.0.ebin" \
  "$BASE_DIR/machine_profile/st.profile.520-1.0.0.ebin" \
  "$PACKAGE_INDEX" \
  "$PACKAGE_MANIFEST"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$PACKAGE_INDEX" "$PACKAGE_MANIFEST" >> "$OUT"
import json
import sys

index_path, manifest_path = sys.argv[1:3]

with open(index_path, "r", encoding="utf-8") as f:
    index = json.load(f)
with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

assert index["schema"] == "st_machine_package_index_v1"
assert index["machine"] == "atari_st"
assert index["profile_module"] == "st.profile.520"
assert index["winner"] == "st.profile.520@1.0.0"
assert index["layout_version"] == "st520_st1040_layout_v1"

components = index["components"]
assert components["cpu"] == "st.cpu.m68k@1.0.0"
assert components["chipset"] == [
    "st.chipset.glue_mmu_shifter@1.0.0",
    "st.chipset.mfp@1.0.0",
]
assert components["io"] == "st.io.acia_ikbd@1.0.0"
assert components["storage"] == "st.storage.dma_fdc@1.0.0"
assert components["audio_gpio"] == "st.audio_gpio.psg@1.0.0"
assert components["machine_profile"] == "st.profile.520@1.0.0"

assert manifest["schema"] == "st_machine_package_manifest_v1"
assert manifest["machine"] == "atari_st"
assert manifest["profile_module"] == "st.profile.520"
assert manifest["package_index"] == "index.json"
assert manifest["winner"] == "st.profile.520@1.0.0"
assert manifest["layout_version"] == "st520_st1040_layout_v1"

placement = manifest["placement"]
assert placement["root_rel"] == "ebins/atari_st"
assert placement["package_rel"] == "packages/st.profile.520"
assert placement["component_dirs"] == {
    "cpu": "cpu",
    "chipset": "chipset",
    "io": "io",
    "storage": "storage",
    "audio_gpio": "audio_gpio",
    "machine_profile": "machine_profile",
}

print("package_520_metadata_contract=pass")
print("package_520_placement_contract=pass path=ebins/atari_st/packages/st.profile.520")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"