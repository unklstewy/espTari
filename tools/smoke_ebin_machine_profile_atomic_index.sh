#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_machine_profile_atomic_index_${TS}.txt"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SEED_SCRIPT="$ROOT_DIR/tools/seed_sdcard_machine_profile_semver_fixtures.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"
VERSIONS="1.0.0,1.2.0,1.1.5"

{
  echo "target=$TARGET"
  echo "versions=$VERSIONS"
} >> "$OUT"

bash "$SEED_SCRIPT" "$TARGET" --versions "$VERSIONS" >> "$OUT" 2>&1

PROFILE_DIR="$TARGET/ebins/atari_st/machine_profile"
INDEX_JSON="$PROFILE_DIR/index.json"
MANIFEST_JSON="$PROFILE_DIR/manifest.json"

for path in \
  "$PROFILE_DIR/st.profile.520-1.0.0.ebin" \
  "$PROFILE_DIR/st.profile.520-1.2.0.ebin" \
  "$PROFILE_DIR/st.profile.520-1.1.5.ebin" \
  "$INDEX_JSON" \
  "$MANIFEST_JSON"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

leftover_tmp_count="$(find "$PROFILE_DIR" -maxdepth 1 -type f -name '.*.tmp.*' | wc -l | tr -d ' ')"
if [[ "$leftover_tmp_count" != "0" ]]; then
  echo "tmp_cleanup=failed count=$leftover_tmp_count" | tee -a "$OUT"
  exit 1
fi

echo "tmp_cleanup=pass" >> "$OUT"

python3 - <<'PY' "$INDEX_JSON" "$MANIFEST_JSON" >> "$OUT"
import json
import sys

index_path, manifest_path = sys.argv[1], sys.argv[2]

with open(index_path, 'r', encoding='utf-8') as f:
    index = json.load(f)
with open(manifest_path, 'r', encoding='utf-8') as f:
    manifest = json.load(f)

assert index.get('schema') == 'machine_profile_index_v1'
assert index.get('module_id') == 'st.profile.520'
assert index.get('winner') == '1.2.0'
assert index.get('versions') == ['1.0.0', '1.2.0', '1.1.5']
assert len(index.get('files', [])) == 3

assert manifest.get('schema') == 'machine_profile_manifest_v1'
assert manifest.get('machine') == 'atari_st'
assert manifest.get('component') == 'machine_profile'
assert manifest.get('winner') == 'st.profile.520@1.2.0'
assert manifest.get('index') == 'index.json'

print('json_contract=pass winner=1.2.0 files=3')
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
