#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/captures/lifecycle_schema_046_smoke_postfix_${TS}.txt"
LIFECYCLE_JSON="$ROOT_DIR/docs/emu_engine_v2/api_schema_lifecycle_v1.json"
ENVELOPE_JSON="$ROOT_DIR/docs/emu_engine_v2/api_schema_envelope_v1.json"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

for path in "$LIFECYCLE_JSON" "$ENVELOPE_JSON"; do
  if [[ ! -f "$path" ]]; then
    echo "required_file=missing path=$path" | tee -a "$OUT"
    exit 1
  fi
  echo "required_file=present path=$path" >> "$OUT"
done

python3 - <<'PY' "$LIFECYCLE_JSON" "$ENVELOPE_JSON" >> "$OUT"
import json
import sys

lifecycle_path, envelope_path = sys.argv[1:3]

with open(lifecycle_path, "r", encoding="utf-8") as f:
    lifecycle = json.load(f)
with open(envelope_path, "r", encoding="utf-8") as f:
    envelope = json.load(f)

assert lifecycle["schema_bundle"] == "espTari-v2-lifecycle"
assert lifecycle["schema_version"] == "1.0.0"
assert lifecycle["api_version"] == "v2"
assert lifecycle["base_path"] == "/api/v2/engine"
assert lifecycle["envelope_ref"] == "docs/emu_engine_v2/api_schema_envelope_v1.json"

assert envelope["schema_bundle"] == "espTari-v2-envelope"

endpoints = lifecycle["endpoints"]
required_endpoints = {
    "session_start",
    "session_stop",
    "session_pause",
    "session_resume",
    "session_reset",
    "session_state_get",
    "session_status_get",
}
assert required_endpoints.issubset(set(endpoints.keys()))

for endpoint_name in ["session_start", "session_stop", "session_pause", "session_resume", "session_reset"]:
    ep = endpoints[endpoint_name]
    assert ep["method"] == "POST"
    assert ep["path"].startswith("/api/v2/engine/session")
    assert "request" in ep and "required" in ep["request"] and "types" in ep["request"]
    assert "success_data" in ep and "required" in ep["success_data"] and "types" in ep["success_data"]
    assert "guard" in ep and "allowed_current_states" in ep["guard"]
    assert ep["guard"]["invalid_transition_error"] == "INVALID_SESSION_STATE"

session_state_get = endpoints["session_state_get"]
assert session_state_get["method"] == "GET"
assert session_state_get["path"] == "/api/v2/engine/session"
assert "query" in session_state_get and "types" in session_state_get["query"]

session_status_get = endpoints["session_status_get"]
assert session_status_get["method"] == "GET"
assert session_status_get["path"] == "/api/v2/engine/status"

canonical_errors = set(lifecycle["canonical_errors"])
required_errors = {
    "BAD_REQUEST",
    "INVALID_SESSION_STATE",
    "ENGINE_NOT_RUNNING",
    "ENGINE_ALREADY_RUNNING",
    "MACHINE_PROFILE_NOT_FOUND",
    "INTERNAL_ERROR",
}
assert required_errors.issubset(canonical_errors)

example_pause = lifecycle["examples"]["pause_request"]
assert set(example_pause.keys()) == {"session_id", "reason"}

print("lifecycle_schema_core=pass")
print("lifecycle_endpoint_set=pass count=%d" % len(required_endpoints))
print("lifecycle_canonical_errors=pass")
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"