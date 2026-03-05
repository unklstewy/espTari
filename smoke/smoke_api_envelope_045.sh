#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/api_envelope_045_smoke_postfix_${TS}.txt"
ENVELOPE_JSON="$ROOT_DIR/docs/emu_engine_v2/api_schema_envelope_v1.json"
BASELINE_JSON="$ROOT_DIR/docs/emu_engine_v2/api_schema_baseline_bundle_v1.json"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

for path in "$ENVELOPE_JSON" "$BASELINE_JSON"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$ENVELOPE_JSON" "$BASELINE_JSON" >> "$OUT"
import json
import re
import sys

envelope_path, baseline_path = sys.argv[1:3]

with open(envelope_path, "r", encoding="utf-8") as f:
    envelope = json.load(f)
with open(baseline_path, "r", encoding="utf-8") as f:
    baseline = json.load(f)

assert envelope["schema_bundle"] == "espTari-v2-envelope"
assert envelope["schema_version"] == "1.0.0"
assert envelope["api_version"] == "v2"
assert envelope["base_path"] == "/api/v2"

versioning = envelope["versioning_policy"]
assert versioning["compatible_additions"] == "minor"
assert versioning["breaking_changes"] == "major-path"

success = envelope["response_envelope"]["success"]
error = envelope["response_envelope"]["error"]
assert success["ok"] is True
assert error["ok"] is False
assert success["required"] == ["ok", "request_id", "timestamp_us", "data"]
assert error["required"] == ["ok", "request_id", "timestamp_us", "error"]
assert error["error_required"] == ["code", "category", "message", "retryable", "details"]

pattern = re.compile(success["request_id_pattern"])
assert pattern.match("req_01H8Z7Y6")

error_schema = envelope["error_schema"]
assert error_schema["code_format"] == "UPPER_SNAKE_CASE"
assert error_schema["code_stability"] == "immutable-after-publication"
assert error_schema["fallback"] == {"code": "INTERNAL_ERROR", "category": "internal"}

categories = envelope["error_schema"]["categories"]
canonical = envelope["canonical_error_codes"]
assert set(categories) == set(canonical.keys())

for category, codes in canonical.items():
    assert isinstance(codes, list) and len(codes) >= 1
    for code in codes:
        assert re.match(r"^[A-Z][A-Z0-9_]*$", code), f"invalid error code format: {code}"

invalid_transition = envelope["examples"]["error"]["error"]
assert invalid_transition["code"] == "INVALID_SESSION_STATE"
assert invalid_transition["category"] == "engine"

assert baseline["bundle_id"] == "espTari-v2-api-baseline"
assert baseline["bundle_version"] == "1.0.0"
assert baseline["api_version"] == "v2"

artifacts = baseline["artifacts"]
artifact_ids = {a["id"] for a in artifacts}
assert "envelope" in artifact_ids

envelope_artifact = next(a for a in artifacts if a["id"] == "envelope")
assert envelope_artifact["path"] == "docs/emu_engine_v2/api_schema_envelope_v1.json"
assert "canonical" in envelope_artifact["role"]

print("api_envelope_schema=pass")
print("versioning_policy=pass")
print("canonical_error_schema=pass categories=%d" % len(categories))
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"