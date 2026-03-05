#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
TARGET_ROOT="${1:-$PROJECT_ROOT/sdcard}"

MACHINE="atari_st"
PROFILE_MODULE="st.profile.520"
PROFILE_VERSION="1.0.0"
LAYOUT_VERSION="st520_st1040_layout_v1"

BASE_DIR="$TARGET_ROOT/ebins/$MACHINE"
PACKAGE_DIR="$BASE_DIR/packages/$PROFILE_MODULE"
PACKAGE_INDEX_PATH="$PACKAGE_DIR/index.json"
PACKAGE_MANIFEST_PATH="$PACKAGE_DIR/manifest.json"

bash "$SCRIPT_DIR/build_st_cpu_m68k_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_chipset_glue_mmu_shifter_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_chipset_mfp_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_io_acia_ikbd_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_storage_dma_fdc_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_audio_gpio_psg_shared.sh" "$TARGET_ROOT"
bash "$SCRIPT_DIR/build_st_profile_520_machine_profile.sh" "$TARGET_ROOT"

mkdir -p "$PACKAGE_DIR"

python3 - <<'PY' "$PACKAGE_INDEX_PATH" "$PACKAGE_MANIFEST_PATH" "$MACHINE" "$PROFILE_MODULE" "$PROFILE_VERSION" "$LAYOUT_VERSION"
import json
import os
import sys
import tempfile

index_path, manifest_path, machine, profile_module, profile_version, layout_version = sys.argv[1:7]

winner = f"{profile_module}@{profile_version}"

index_obj = {
    "schema": "st_machine_package_index_v1",
    "machine": machine,
    "profile_module": profile_module,
    "components": {
        "cpu": "st.cpu.m68k@1.0.0",
        "chipset": [
            "st.chipset.glue_mmu_shifter@1.0.0",
            "st.chipset.mfp@1.0.0"
        ],
        "io": "st.io.acia_ikbd@1.0.0",
        "storage": "st.storage.dma_fdc@1.0.0",
        "audio_gpio": "st.audio_gpio.psg@1.0.0",
        "machine_profile": winner
    },
    "winner": winner,
    "layout_version": layout_version,
}

manifest_obj = {
    "schema": "st_machine_package_manifest_v1",
    "machine": machine,
    "profile_module": profile_module,
    "package_index": "index.json",
    "winner": winner,
    "layout_version": layout_version,
    "placement": {
        "root_rel": "ebins/atari_st",
        "package_rel": f"packages/{profile_module}",
        "component_dirs": {
            "cpu": "cpu",
            "chipset": "chipset",
            "io": "io",
            "storage": "storage",
            "audio_gpio": "audio_gpio",
            "machine_profile": "machine_profile",
        }
    }
}

for path, payload in ((index_path, index_obj), (manifest_path, manifest_obj)):
    directory = os.path.dirname(path) or "."
    fd, temp_path = tempfile.mkstemp(prefix=".tmp.", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_path, path)
        dir_fd = os.open(directory, os.O_DIRECTORY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
    finally:
        if os.path.exists(temp_path):
            os.unlink(temp_path)

print(f"package_written index={index_path} manifest={manifest_path}")
PY

echo "Composed package: $PACKAGE_DIR"