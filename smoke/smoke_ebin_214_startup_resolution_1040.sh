#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_214_startup_resolution_1040_${TS}.txt"
COMPOSE_SCRIPT="$ROOT_DIR/tools/ebin_builder/compose_st_1040_package.sh"
LOADER_C="$ROOT_DIR/components/esptari_loader/esptari_loader.c"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

TARGET="$TMP_ROOT/sdcard"
bash "$COMPOSE_SCRIPT" "$TARGET" >> "$OUT" 2>&1

PROFILE_DIR="$TARGET/ebins/atari_st/machine_profile"
PACKAGE_INDEX="$TARGET/ebins/atari_st/packages/st.profile.1040/index.json"

touch "$PROFILE_DIR/st.profile.1040-0.9.0.ebin"
touch "$PROFILE_DIR/st.profile.1040-1.2.0.ebin"

for path in \
  "$PROFILE_DIR/st.profile.1040-0.9.0.ebin" \
  "$PROFILE_DIR/st.profile.1040-1.0.0.ebin" \
  "$PROFILE_DIR/st.profile.1040-1.2.0.ebin" \
  "$PACKAGE_INDEX" \
  "$LOADER_C"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$PROFILE_DIR" "$PACKAGE_INDEX" "$LOADER_C" >> "$OUT"
import itertools
import json
import os
import re
import sys
from pathlib import Path

profile_dir, package_index_path, loader_path = sys.argv[1:4]

with open(package_index_path, "r", encoding="utf-8") as f:
    package_index = json.load(f)

assert package_index["profile_module"] == "st.profile.1040"
assert package_index["winner"] == "st.profile.1040@1.0.0"

loader_text = Path(loader_path).read_text(encoding="utf-8")
assert 'strcmp(module_id, "st.profile.1040") == 0' in loader_text
assert 'return "st_default";' in loader_text

pattern = re.compile(r"^(?P<module>.+)-(?P<ver>\d+\.\d+\.\d+)\.ebin$")
entries = []
for name in os.listdir(profile_dir):
    match = pattern.match(name)
    if match:
        entries.append((match.group("module"), match.group("ver"), name))

assert len(entries) >= 3, f"expected >=3 semver entries, got {len(entries)}"

def semver_key(version: str):
    return tuple(int(part) for part in version.split("."))

def resolve(ordered_entries):
    loaded = False
    selected_module = "st.profile.520"
    selected_version = "1.0.0"
    for module_id, version, _ in ordered_entries:
        if not loaded or semver_key(version) > semver_key(selected_version):
            selected_module = module_id
            selected_version = version
            loaded = True
    profile = "st_520_pal" if selected_module == "st.profile.520" else "st_default"
    return selected_module, selected_version, profile

sample = entries[:5]
fingerprints = set()
for perm in itertools.permutations(sample, len(sample)):
    fingerprints.add(resolve(perm))
    if len(fingerprints) > 1:
        break

assert len(fingerprints) == 1, f"non-deterministic resolution: {fingerprints}"
selected_module, selected_version, selected_profile = next(iter(fingerprints))

assert selected_module == "st.profile.1040"
assert selected_version == "1.2.0"
assert selected_profile == "st_default"

print("startup_resolution_determinism=pass")
print("startup_resolution_winner=st.profile.1040@1.2.0")
print("startup_resolution_profile=st_default")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"