#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_215_integration_matrix_bundles_${TS}.txt"
COMPOSE_520="$ROOT_DIR/tools/ebin_builder/compose_st_520_package.sh"
COMPOSE_1040="$ROOT_DIR/tools/ebin_builder/compose_st_1040_package.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"

bash "$COMPOSE_520" "$TARGET" >> "$OUT" 2>&1
bash "$COMPOSE_1040" "$TARGET" >> "$OUT" 2>&1

python3 - <<'PY' "$TARGET" >> "$OUT"
import json
import os
import sys

target = sys.argv[1]
base = os.path.join(target, "ebins", "atari_st")

required_artifacts = [
    os.path.join(base, "cpu", "st.cpu.m68k-1.0.0.ebin"),
    os.path.join(base, "chipset", "st.chipset.glue_mmu_shifter-1.0.0.ebin"),
    os.path.join(base, "chipset", "st.chipset.mfp-1.0.0.ebin"),
    os.path.join(base, "io", "st.io.acia_ikbd-1.0.0.ebin"),
    os.path.join(base, "storage", "st.storage.dma_fdc-1.0.0.ebin"),
    os.path.join(base, "audio_gpio", "st.audio_gpio.psg-1.0.0.ebin"),
    os.path.join(base, "machine_profile", "st.profile.520-1.0.0.ebin"),
    os.path.join(base, "machine_profile", "st.profile.1040-1.0.0.ebin"),
    os.path.join(base, "packages", "st.profile.520", "index.json"),
    os.path.join(base, "packages", "st.profile.1040", "index.json"),
]

for path in required_artifacts:
    assert os.path.isfile(path), f"missing artifact: {path}"

def load_package(profile_module: str):
    with open(os.path.join(base, "packages", profile_module, "index.json"), "r", encoding="utf-8") as f:
        index = json.load(f)
    with open(os.path.join(base, "packages", profile_module, "manifest.json"), "r", encoding="utf-8") as f:
        manifest = json.load(f)
    return index, manifest

bundle_520 = load_package("st.profile.520")
bundle_1040 = load_package("st.profile.1040")

for profile_module, (index, manifest) in {
    "st.profile.520": bundle_520,
    "st.profile.1040": bundle_1040,
}.items():
    assert index["schema"] == "st_machine_package_index_v1"
    assert manifest["schema"] == "st_machine_package_manifest_v1"
    assert index["machine"] == "atari_st"
    assert manifest["machine"] == "atari_st"
    assert index["profile_module"] == profile_module
    assert manifest["profile_module"] == profile_module
    assert index["layout_version"] == "st520_st1040_layout_v1"
    assert manifest["layout_version"] == "st520_st1040_layout_v1"

matrix = {
    "st.profile.520": {
        "interrupt": ["st.chipset.mfp@1.0.0", "st.io.acia_ikbd@1.0.0"],
        "startup": ["st.profile.520@1.0.0", "st.chipset.glue_mmu_shifter@1.0.0"],
        "suspend_restore": ["st.storage.dma_fdc@1.0.0", "st.audio_gpio.psg@1.0.0", "st.cpu.m68k@1.0.0"],
    },
    "st.profile.1040": {
        "interrupt": ["st.chipset.mfp@1.0.0", "st.io.acia_ikbd@1.0.0"],
        "startup": ["st.profile.1040@1.0.0", "st.chipset.glue_mmu_shifter@1.0.0"],
        "suspend_restore": ["st.storage.dma_fdc@1.0.0", "st.audio_gpio.psg@1.0.0", "st.cpu.m68k@1.0.0"],
    },
}

def bundle_modules(index):
    comps = index["components"]
    modules = {
        comps["cpu"],
        comps["io"],
        comps["storage"],
        comps["audio_gpio"],
        comps["machine_profile"],
    }
    modules.update(comps["chipset"])
    return modules

for profile_module, (index, _manifest) in {
    "st.profile.520": bundle_520,
    "st.profile.1040": bundle_1040,
}.items():
    modules = bundle_modules(index)
    for area, required in matrix[profile_module].items():
        missing = [module for module in required if module not in modules]
        assert not missing, f"matrix_missing profile={profile_module} area={area} missing={missing}"
        print(f"matrix_area=pass profile={profile_module} area={area}")

print("integration_matrix=pass bundles=st.profile.520,st.profile.1040")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"