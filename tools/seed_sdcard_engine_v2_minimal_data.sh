#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TARGET_ROOT="${PROJECT_ROOT}/sdcard"
DRY_RUN=0
FORCE=0

usage() {
  cat <<'EOF'
Usage: seed_sdcard_engine_v2_minimal_data.sh [TARGET_ROOT] [--dry-run] [--force]

Seed minimal Emulation_Engine_V2 placeholder files on SD card.

Arguments:
  TARGET_ROOT   Mounted SD-card root path (default: ./sdcard in this repo)

Options:
  --dry-run     Print planned actions without writing
  --force       Overwrite existing files
  -h, --help    Show this help

Examples:
  ./tools/seed_sdcard_engine_v2_minimal_data.sh /media/$USER/SDCARD
  ./tools/seed_sdcard_engine_v2_minimal_data.sh --dry-run
EOF
}

for arg in "$@"; do
  case "$arg" in
    --dry-run)
      DRY_RUN=1
      ;;
    --force)
      FORCE=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -* )
      echo "[ERR ] Unknown option: $arg" >&2
      usage
      exit 1
      ;;
    *)
      TARGET_ROOT="$arg"
      ;;
  esac
done

created=0
updated=0
skipped=0

write_json() {
  local rel_path="$1"
  local payload="$2"
  local full_path="$TARGET_ROOT/$rel_path"
  local dir_path
  local existed=0
  dir_path="$(dirname "$full_path")"

  if [[ -f "$full_path" ]]; then
    existed=1
  fi

  if [[ -f "$full_path" && $FORCE -eq 0 ]]; then
    echo "[SKIP] exists   $rel_path"
    skipped=$((skipped + 1))
    return
  fi

  if [[ $DRY_RUN -eq 1 ]]; then
    if [[ $existed -eq 1 ]]; then
      echo "[DRY] overwrite $rel_path"
      updated=$((updated + 1))
    else
      echo "[DRY] create    $rel_path"
      created=$((created + 1))
    fi
    return
  fi

  mkdir -p "$dir_path"
  printf '%s\n' "$payload" > "$full_path"

  if [[ -f "$full_path" && $FORCE -eq 1 ]]; then
    echo "[ OK ] wrote    $rel_path"
  else
    echo "[ OK ] wrote    $rel_path"
  fi

  if [[ $existed -eq 1 ]]; then
    updated=$((updated + 1))
  else
    created=$((created + 1))
  fi
}

echo "[INFO] Target root: $TARGET_ROOT"

write_json "config/engine_v2/machines/atari_st/st_520_pal.json" '{
  "manifest_version": 1,
  "machine": "atari_st",
  "profile": "st_520_pal",
  "region": "pal",
  "ram_kb": 512,
  "modules": {
    "cpu": "st.cpu.m68k@1.0.0",
    "video": "st.video.shifter@1.0.0",
    "io": "st.io.ikbd@1.0.0",
    "storage": "st.storage.fdc@1.0.0",
    "machine_profile": "st.profile.520@1.0.0"
  },
  "scheduler": {
    "tick_hz": 2000000,
    "step_order": ["cpu", "video", "io", "storage", "machine_profile"]
  }
}'

write_json "config/engine_v2/machines/atari_st/st_520_pal_wiring_bad.json" '{
  "manifest_version": 1,
  "machine": "atari_st",
  "profile": "st_520_pal_wiring_bad",
  "region": "pal",
  "ram_kb": 512,
  "modules": {
    "cpu": "st.cpu.m68k@2.0.0",
    "video": "st.video.shifter@1.0.0",
    "io": "st.io.ikbd@1.0.0",
    "storage": "st.storage.fdc@1.0.0",
    "machine_profile": "st.profile.520@1.0.0"
  },
  "scheduler": {
    "tick_hz": 2000000,
    "step_order": ["cpu", "video", "io", "storage", "machine_profile"]
  }
}'

write_json "config/engine_v2/input/mappings/atari_st/atari_st_default_v1.json" '{
  "schema_version": 1,
  "mapping_profile_id": "atari_st_default_v1",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "entries": []
}'

write_json "config/engine_v2/rom_catalog.json" '{
  "schema_version": 1,
  "name": "roms",
  "entries": []
}'

write_json "config/engine_v2/disk_catalog.json" '{
  "schema_version": 1,
  "name": "floppies",
  "entries": []
}'

write_json "config/engine_v2/tos_catalog.json" '{
  "schema_version": 1,
  "name": "tos",
  "entries": []
}'

write_json "config/engine_v2/catalog_sync_schedules.json" '{
  "schema_version": 1,
  "schedules": []
}'

write_json "ebins/index.json" '{
  "schema_version": 1,
  "generated_by": "seed_sdcard_engine_v2_minimal_data.sh",
  "entries": []
}'

echo
if [[ $DRY_RUN -eq 1 ]]; then
  echo "[INFO] Dry-run complete. create=$created update=$updated skip=$skipped"
else
  echo "[INFO] Seed complete. create=$created update=$updated skip=$skipped"
fi
