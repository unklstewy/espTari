#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="$ROOT_DIR/captures"
mkdir -p "$OUT_DIR"
OUT_FILE="${OUT_FILE:-$OUT_DIR/prq002_fixture_matrix_$(date +%Y%m%d_%H%M%S).txt}"

MAP_ID="prq2_fx_in_01"
SNAP_ID="prq2_fx_sr_01"

PASS=0
FAIL=0

request() {
  local label="$1"
  local expected_code="$2"
  shift 2

  local response
  response="$(curl --retry 2 --retry-delay 1 --retry-all-errors --max-time 8 -sS -w '\nHTTP_STATUS:%{http_code}\n' "$@")"
  local code
  code="$(echo "$response" | sed -n 's/^HTTP_STATUS://p' | tail -n1)"

  {
    echo "--- $label"
    echo "$response"
  } >> "$OUT_FILE"

  if [[ "$code" == "$expected_code" ]]; then
    PASS=$((PASS + 1))
  else
    FAIL=$((FAIL + 1))
    echo "ASSERTION_FAILED label='$label' expected=$expected_code got=$code" >> "$OUT_FILE"
  fi
}

{
  echo "base_url=$BASE_URL"
  echo "started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT_FILE"

request "health" 200 "$BASE_URL/api/v2/engine/health"
request "status_pre" 200 "$BASE_URL/api/v2/engine/status"

# FX-LC-01 lifecycle checks
request "lifecycle_stop_to_stopped" 200 -X POST "$BASE_URL/api/v2/engine/session/stop"
request "lifecycle_resume_from_stopped_denied" 409 -X POST "$BASE_URL/api/v2/engine/session/resume"
request "lifecycle_start" 200 -X POST "$BASE_URL/api/v2/engine/session"
request "lifecycle_pause" 200 -X POST "$BASE_URL/api/v2/engine/session/pause"
request "lifecycle_resume_from_paused" 200 -X POST "$BASE_URL/api/v2/engine/session/resume"

# FX-IN-01 input mapping checks
request "input_create" 201 -X POST "$BASE_URL/api/v2/input/mappings" -H "Content-Type: application/json" -d "{\"mapping_profile_id\":\"$MAP_ID\",\"machine\":\"st\",\"profile\":\"default\",\"entries\":[]}"
request "input_apply" 200 -X POST "$BASE_URL/api/v2/input/mappings/apply" -H "Content-Type: application/json" -d "{\"mapping_profile_id\":\"$MAP_ID\"}"
request "input_conflict_expected_revision" 409 -X POST "$BASE_URL/api/v2/input/mappings/apply" -H "Content-Type: application/json" -d "{\"mapping_profile_id\":\"$MAP_ID\",\"expected_revision\":999}"
request "input_not_found_get" 404 "$BASE_URL/api/v2/input/mappings/prq2_missing"

# FX-SR-01 save/restore checks
request "sr_suspend_save" 200 -X POST "$BASE_URL/api/v2/engine/session/suspend-save" -H "Content-Type: application/json" -d "{\"snapshot_id\":\"$SNAP_ID\"}"
request "sr_validate_strict" 200 -X POST "$BASE_URL/api/v2/engine/state/restore/validate" -H "Content-Type: application/json" -d "{\"snapshot_id\":\"$SNAP_ID\",\"strict\":true}"
request "sr_restore_resume" 200 -X POST "$BASE_URL/api/v2/engine/session/restore-resume" -H "Content-Type: application/json" -d "{\"snapshot_id\":\"$SNAP_ID\",\"resume_mode\":\"running\"}"

# FX-OB-01 observability checks
request "obs_video_breach" 200 "$BASE_URL/api/v2/stream/video?slo=breach"
request "obs_video_recover" 200 "$BASE_URL/api/v2/stream/video?slo=recover"

{
  echo "pass_count=$PASS"
  echo "fail_count=$FAIL"
  echo "completed_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >> "$OUT_FILE"

if [[ "$FAIL" -ne 0 ]]; then
  echo "PRQ-002 fixture matrix completed with failures. See: $OUT_FILE" >&2
  exit 1
fi

echo "PRQ-002 fixture matrix passed. Evidence: $OUT_FILE"
