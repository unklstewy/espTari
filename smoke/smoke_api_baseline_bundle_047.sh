#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/api_baseline_bundle_047_smoke_postfix_${TS}.txt"
BUNDLE_JSON="$ROOT_DIR/docs/emu_engine_v2/api_schema_baseline_bundle_v1.json"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

if [[ ! -f "$BUNDLE_JSON" ]]; then
  echo "required_file=missing path=$BUNDLE_JSON" | tee -a "$OUT"
  exit 1
fi
echo "required_file=present path=$BUNDLE_JSON" >> "$OUT"

python3 - <<'PY' "$ROOT_DIR" "$BUNDLE_JSON" >> "$OUT"
import json
import os
import sys

root, bundle_path = sys.argv[1:3]

with open(bundle_path, "r", encoding="utf-8") as f:
    bundle = json.load(f)

assert bundle["bundle_id"] == "espTari-v2-api-baseline"
assert bundle["bundle_version"] == "1.0.0"
assert bundle["api_version"] == "v2"
assert bundle["status"] == "normative"

artifacts = bundle["artifacts"]
assert isinstance(artifacts, list) and len(artifacts) >= 2

artifact_ids = {item["id"] for item in artifacts}
assert {"envelope", "lifecycle"}.issubset(artifact_ids)

for artifact in artifacts:
    assert "path" in artifact and isinstance(artifact["path"], str)
    abs_path = os.path.join(root, artifact["path"])
    assert os.path.isfile(abs_path), f"missing artifact file: {artifact['path']}"
    assert "api_spec_sections" in artifact and isinstance(artifact["api_spec_sections"], list)
    assert len(artifact["api_spec_sections"]) >= 1

examples = bundle["examples"]
for key in ["session_start", "session_reset", "invalid_transition_error"]:
    assert key in examples

session_start = examples["session_start"]
assert session_start["request"]["machine"] == "atari_st"
assert session_start["request"]["profile"] == "st_520_pal"
assert session_start["success_data"]["state"] == "running"

session_reset = examples["session_reset"]
assert session_reset["request"]["mode"] == "warm"
assert session_reset["request"]["preserve_media"] is True

invalid_transition = examples["invalid_transition_error"]["error"]
assert invalid_transition["code"] == "INVALID_SESSION_STATE"
assert invalid_transition["category"] == "engine"

cross_links = bundle["cross_links"]
for link_key in ["api_spec", "implementation_plan", "indexed_spec_root"]:
    assert link_key in cross_links
    abs_path = os.path.join(root, cross_links[link_key])
    assert os.path.isfile(abs_path), f"missing cross-link file: {cross_links[link_key]}"

print("baseline_bundle_metadata=pass")
print("baseline_bundle_examples=pass")
print("baseline_bundle_cross_links=pass")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"