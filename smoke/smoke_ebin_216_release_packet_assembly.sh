#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/ebin_216_release_packet_assembly_${TS}.txt"
ASSEMBLER="$ROOT_DIR/tools/ebin_builder/assemble_st_520_1040_release_packet.sh"
DB_PATH="$ROOT_DIR/TRACKING/tracking.db"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

packet_output="$(bash "$ASSEMBLER" "$ROOT_DIR/captures" "$DB_PATH" | tail -n1 | sed 's/^Release packet: //')"

if [[ ! -f "$packet_output" ]]; then
  echo "release_packet=missing path=$packet_output" | tee -a "$OUT"
  exit 1
fi

echo "release_packet=present path=$packet_output" >> "$OUT"

python3 - <<'PY' "$packet_output" >> "$OUT"
import json
import sys

packet_path = sys.argv[1]

with open(packet_path, "r", encoding="utf-8") as f:
    packet = json.load(f)

assert packet["schema"] == "st_release_packet_v1"
assert packet["machine"] == "atari_st"
assert packet["layout_version"] == "st520_st1040_layout_v1"

bundles = packet["bundles"]
assert len(bundles) == 2
bundle_ids = {b["profile_module"] for b in bundles}
assert bundle_ids == {"st.profile.520", "st.profile.1040"}

acceptance_chain = packet["task_acceptance_chain"]
task_ids = [t["task_id"] for t in acceptance_chain]

for required in [
    "EBIN-209", "EBIN-210", "EBIN-211", "EBIN-212", "EBIN-213", "EBIN-214", "EBIN-215"
]:
    assert required in task_ids, f"missing required task in packet: {required}"

accepted_required = {
    "EBIN-211", "EBIN-212", "EBIN-213", "EBIN-214", "EBIN-215"
}
for task in acceptance_chain:
    tid = task["task_id"]
    acceptance = task["acceptance"]
    if tid in accepted_required:
        assert acceptance["decision"] == "Accepted", f"expected Accepted for {tid}"
        assert len(acceptance["evidence_links"]) >= 1, f"missing evidence links for {tid}"

print("release_packet_schema=pass")
print("release_packet_bundles=pass profiles=st.profile.520,st.profile.1040")
print("release_packet_acceptance_chain=pass")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"