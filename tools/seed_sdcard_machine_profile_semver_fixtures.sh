#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TARGET_ROOT="${PROJECT_ROOT}/sdcard"
DRY_RUN=0
FORCE=0

MODULE_ID="st.profile.520"
DEFAULT_VERSIONS=("1.0.0" "1.1.0")

atomic_write_text() {
  local target="$1"
  local content="$2"
  local dir base tmp

  dir="$(dirname "$target")"
  base="$(basename "$target")"
  mkdir -p "$dir"
  tmp="$(mktemp "$dir/.${base}.tmp.XXXXXX")"

  printf '%s' "$content" > "$tmp"
  mv -f "$tmp" "$target"
}

usage() {
  cat <<'EOF'
Usage: seed_sdcard_machine_profile_semver_fixtures.sh [TARGET_ROOT] [--dry-run] [--force] [--versions v1,v2,...]

Seed versioned machine_profile EBIN fixture files and print expected semver winner.

Arguments:
  TARGET_ROOT          Mounted SD-card root path (default: ./sdcard in this repo)

Options:
  --versions CSV       Comma-separated semver values (default: 1.0.0,1.1.0)
  --dry-run            Print planned actions without writing
  --force              Recreate files even if they already exist
  -h, --help           Show this help

Examples:
  ./tools/seed_sdcard_machine_profile_semver_fixtures.sh /media/$USER/ESP-SD
  ./tools/seed_sdcard_machine_profile_semver_fixtures.sh /media/$USER/ESP-SD --versions 1.0.0,1.1.0,2.0.0
  ./tools/seed_sdcard_machine_profile_semver_fixtures.sh --dry-run
EOF
}

VERSIONS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --versions)
      [[ $# -ge 2 ]] || { echo "[ERR ] --versions requires a value" >&2; exit 1; }
      IFS=',' read -r -a VERSIONS <<< "$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -* )
      echo "[ERR ] Unknown option: $1" >&2
      usage
      exit 1
      ;;
    *)
      TARGET_ROOT="$1"
      shift
      ;;
  esac
done

if [[ ${#VERSIONS[@]} -eq 0 ]]; then
  VERSIONS=("${DEFAULT_VERSIONS[@]}")
fi

is_valid_semver() {
  [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
}

compare_semver() {
  local a="$1"
  local b="$2"
  local a1 a2 a3 b1 b2 b3
  IFS='.' read -r a1 a2 a3 <<< "$a"
  IFS='.' read -r b1 b2 b3 <<< "$b"

  if (( a1 > b1 )); then echo 1; return; fi
  if (( a1 < b1 )); then echo -1; return; fi
  if (( a2 > b2 )); then echo 1; return; fi
  if (( a2 < b2 )); then echo -1; return; fi
  if (( a3 > b3 )); then echo 1; return; fi
  if (( a3 < b3 )); then echo -1; return; fi
  echo 0
}

pick_winner() {
  local best=""
  local v cmp
  for v in "$@"; do
    if [[ -z "$best" ]]; then
      best="$v"
      continue
    fi
    cmp="$(compare_semver "$v" "$best")"
    if [[ "$cmp" == "1" ]]; then
      best="$v"
    fi
  done
  printf '%s\n' "$best"
}

created=0
updated=0
skipped=0
meta_created=0
meta_updated=0

DIR="$TARGET_ROOT/ebins/atari_st/machine_profile"

echo "[INFO] Target root: $TARGET_ROOT"
echo "[INFO] Fixture dir : $DIR"

for version in "${VERSIONS[@]}"; do
  if ! is_valid_semver "$version"; then
    echo "[ERR ] Invalid semver '$version' (expected X.Y.Z)" >&2
    exit 1
  fi

  file="$DIR/${MODULE_ID}-${version}.ebin"
  rel="ebins/atari_st/machine_profile/${MODULE_ID}-${version}.ebin"

  if [[ -f "$file" && $FORCE -eq 0 ]]; then
    echo "[SKIP] exists   $rel"
    skipped=$((skipped + 1))
    continue
  fi

  if [[ $DRY_RUN -eq 1 ]]; then
    if [[ -f "$file" ]]; then
      echo "[DRY] overwrite $rel"
      updated=$((updated + 1))
    else
      echo "[DRY] create    $rel"
      created=$((created + 1))
    fi
    continue
  fi

  mkdir -p "$DIR"
  if [[ -f "$file" ]]; then
    updated=$((updated + 1))
  else
    created=$((created + 1))
  fi
  atomic_write_text "$file" ""
  echo "[ OK ] wrote    $rel"
done

winner="$(pick_winner "${VERSIONS[@]}")"

INDEX_FILE="$DIR/index.json"
MANIFEST_FILE="$DIR/manifest.json"
index_rel="ebins/atari_st/machine_profile/index.json"
manifest_rel="ebins/atari_st/machine_profile/manifest.json"

versions_json=""
files_json=""
for version in "${VERSIONS[@]}"; do
  if [[ -n "$versions_json" ]]; then
    versions_json+=" , "
    files_json+=" , "
  fi
  versions_json+="\"${version}\""
  files_json+="\"${MODULE_ID}-${version}.ebin\""
done

index_json="$(cat <<EOF
{
  "schema": "machine_profile_index_v1",
  "module_id": "${MODULE_ID}",
  "versions": [ ${versions_json} ],
  "winner": "${winner}",
  "files": [ ${files_json} ]
}
EOF
)"

manifest_json="$(cat <<EOF
{
  "schema": "machine_profile_manifest_v1",
  "machine": "atari_st",
  "component": "machine_profile",
  "module_id": "${MODULE_ID}",
  "winner": "${MODULE_ID}@${winner}",
  "resolved_profile": "st_520_pal",
  "index": "index.json"
}
EOF
)"

if [[ $DRY_RUN -eq 1 ]]; then
  if [[ -f "$INDEX_FILE" ]]; then
    echo "[DRY] overwrite $index_rel"
    meta_updated=$((meta_updated + 1))
  else
    echo "[DRY] create    $index_rel"
    meta_created=$((meta_created + 1))
  fi

  if [[ -f "$MANIFEST_FILE" ]]; then
    echo "[DRY] overwrite $manifest_rel"
    meta_updated=$((meta_updated + 1))
  else
    echo "[DRY] create    $manifest_rel"
    meta_created=$((meta_created + 1))
  fi
else
  if [[ -f "$INDEX_FILE" ]]; then
    meta_updated=$((meta_updated + 1))
  else
    meta_created=$((meta_created + 1))
  fi
  if [[ -f "$MANIFEST_FILE" ]]; then
    meta_updated=$((meta_updated + 1))
  else
    meta_created=$((meta_created + 1))
  fi

  atomic_write_text "$INDEX_FILE" "$index_json"
  echo "[ OK ] wrote    $index_rel"
  atomic_write_text "$MANIFEST_FILE" "$manifest_json"
  echo "[ OK ] wrote    $manifest_rel"
fi

echo
echo "[INFO] Expected loader winner: ${MODULE_ID}@${winner}"
echo "[INFO] Expected resolved profile: st_520_pal"

if [[ $DRY_RUN -eq 1 ]]; then
  echo "[INFO] Dry-run complete. create=$created update=$updated skip=$skipped meta_create=$meta_created meta_update=$meta_updated"
else
  echo "[INFO] Seed complete. create=$created update=$updated skip=$skipped meta_create=$meta_created meta_update=$meta_updated"
fi
