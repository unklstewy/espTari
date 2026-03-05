#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
TARGET_ROOT="${1:-$PROJECT_ROOT/sdcard}"

MODULE_ID="st.profile.1040"
VERSION="1.0.0"
COMPONENT="machine_profile"
LAYOUT_VERSION="st520_st1040_layout_v1"

COMPONENT_DIR="$TARGET_ROOT/ebins/atari_st/$COMPONENT"
ARTIFACT_NAME="${MODULE_ID}-${VERSION}.ebin"
ARTIFACT_PATH="$COMPONENT_DIR/$ARTIFACT_NAME"
SIGNATURE_PATH="$ARTIFACT_PATH.sig"
INDEX_PATH="$COMPONENT_DIR/index.json"
MANIFEST_PATH="$COMPONENT_DIR/manifest.json"

mkdir -p "$COMPONENT_DIR"

if [[ -z "${IDF_PATH:-}" ]]; then
  if [[ -f "$HOME/esp/esp-idf/export.sh" ]]; then
    source "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
  elif [[ -f "$HOME/.espressif/v5.5.2/esp-idf/export.sh" ]]; then
    source "$HOME/.espressif/v5.5.2/esp-idf/export.sh" >/dev/null 2>&1
  fi
fi

python3 "$SCRIPT_DIR/ebin_builder.py" \
  "$SCRIPT_DIR/st_profile_1040_component.c" \
  -o "$ARTIFACT_PATH" \
  -t system \
  --interface-version 0x00010000 \
  --min-ram 32768 \
  -v

printf 'ESPTARI-DEV-SIG:%s@%s\n' "$MODULE_ID" "$VERSION" > "$SIGNATURE_PATH"

python3 - <<'PY' "$INDEX_PATH" "$MANIFEST_PATH" "$MODULE_ID" "$VERSION" "$ARTIFACT_NAME" "$LAYOUT_VERSION"
import json
import tempfile
import os
import sys

index_path, manifest_path, module_id, version, artifact_name, layout_version = sys.argv[1:7]

index_obj = {
    "schema": "st_component_index_v1",
    "machine": "atari_st",
    "component": "machine_profile",
    "module_id": module_id,
    "winner": version,
    "versions": [version],
    "files": [artifact_name],
    "layout_version": layout_version,
}

manifest_obj = {
    "schema": "st_component_manifest_v1",
    "machine": "atari_st",
    "component": "machine_profile",
    "module_id": module_id,
    "winner": f"{module_id}@{version}",
    "resolved_profile": "st_1040_pal",
    "index": "index.json",
    "layout_version": layout_version,
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

print(f"metadata_written index={index_path} manifest={manifest_path}")
PY

echo "Built artifact: $ARTIFACT_PATH"
echo "Built signature: $SIGNATURE_PATH"