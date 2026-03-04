#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TARGET_ROOT="${PROJECT_ROOT}/sdcard"
DRY_RUN=0

usage() {
  cat <<'EOF'
Usage: prep_sdcard_engine_v2_layout.sh [TARGET_ROOT] [--dry-run]

Prepare the Emulation_Engine_V2 required SD-card directory structure.

Arguments:
  TARGET_ROOT   Mounted SD-card root path (default: ./sdcard in this repo)

Options:
  --dry-run     Print planned actions without writing
  -h, --help    Show this help

Example:
  ./tools/prep_sdcard_engine_v2_layout.sh /media/$USER/ESPTARI_SD
  ./tools/prep_sdcard_engine_v2_layout.sh --dry-run
EOF
}

for arg in "$@"; do
  case "$arg" in
    --dry-run)
      DRY_RUN=1
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

# Canonical + implementation-required tree for Engine V2.
REQUIRED_DIRS=(
  "roms/st"
  "disks/st"
  "cartridges/st"
  "ebins"
  "ebins/atari_st"
  "ebins/atari_st/cpu"
  "ebins/atari_st/video"
  "ebins/atari_st/io"
  "ebins/atari_st/storage"
  "ebins/atari_st/audio"
  "ebins/atari_st/machine_profile"
  "saves/states"
  "saves/nvram"
  "config/engine_v2"
  "config/engine_v2/machines"
  "config/engine_v2/machines/atari_st"
  "config/engine_v2/input"
  "config/engine_v2/input/mappings"
  "config/engine_v2/input/mappings/atari_st"
  "conformance"
)

created=0
existing=0

echo "[INFO] Target root: $TARGET_ROOT"

if [[ $DRY_RUN -eq 0 ]]; then
  mkdir -p "$TARGET_ROOT"
fi

for rel in "${REQUIRED_DIRS[@]}"; do
  full="$TARGET_ROOT/$rel"
  if [[ -d "$full" ]]; then
    echo "[ OK ] exists   $rel/"
    existing=$((existing + 1))
    continue
  fi

  if [[ $DRY_RUN -eq 1 ]]; then
    echo "[DRY] mkdir -p $rel/"
  else
    mkdir -p "$full"
    echo "[ OK ] created  $rel/"
  fi
  created=$((created + 1))
done

echo
if [[ $DRY_RUN -eq 1 ]]; then
  echo "[INFO] Dry-run complete: would create $created dir(s), $existing already exist."
else
  echo "[INFO] Complete: created $created dir(s), $existing already existed."
fi
