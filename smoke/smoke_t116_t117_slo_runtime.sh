#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/t116_t117_slo_runtime_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
AUTH_SCOPE="${AUTH_SCOPE:-engine:control inspect:read}"
AUTH_ARGS=()

mint_auth_if_needed() {
  if [[ -n "$AUTH_HEADER" ]]; then
    return 0
  fi
  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
    return 0
  fi

  local mint_resp minted_token
  mint_resp="$(curl --max-time 15 -sS -X POST "${BASE_URL}/api/v2/auth/token" \
    -H "Content-Type: application/json" \
    -d "{\"grant_type\":\"client_credentials\",\"client_id\":\"${AUTH_CLIENT_ID}\",\"client_secret\":\"${AUTH_CLIENT_SECRET}\",\"scope\":\"${AUTH_SCOPE}\"}" || true)"

  minted_token="$(printf '%s' "$mint_resp" | python3 -c 'import json,sys
raw=sys.stdin.read().strip()
token=""
try:
    d=json.loads(raw) if raw else {}
    token=d.get("data",{}).get("access_token","") if d.get("ok") else ""
except Exception:
    token=""
print(token)')"

  if [[ -n "$minted_token" ]]; then
    AUTH_BEARER="$minted_token"
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
  fi
}

append_auth_args() {
  AUTH_ARGS=()
  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
  fi
  if [[ -n "$AUTH_HEADER" ]]; then
    AUTH_ARGS+=( -H "$AUTH_HEADER" )
  fi
}

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${AUTH_ARGS[@]}" "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      echo "health_check=ok attempts=${i}" | tee -a "$OUT"
      return 0
    fi
    sleep "$interval"
  done
  echo "health_check=timeout attempts=${attempts}" | tee -a "$OUT"
  exit 1
}

call_json() {
  local name="$1" method="$2" path="$3" data="$4" expected="$5"
  local body_file code body
  body_file="$(mktemp)"
  if [[ -n "$data" ]]; then
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}")
  fi
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "$method $path"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ ! "$code" =~ $expected ]]; then
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | python3 -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

mint_auth_if_needed
append_auth_args
if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
  echo "auth_header=none" | tee -a "$OUT"
else
  echo "auth_header=configured" | tee -a "$OUT"
fi

echo "determinism_runs=3" | tee -a "$OUT"
wait_for_health

run_signature_ref=""
for run in 1 2 3; do
  call_json "session_reset_run_${run}" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
  call_json "session_start_run_${run}" POST "/api/v2/engine/session/start" "" "^(200|409)$"

  cfg='{"session_id":"ses_local","sampling_interval_ms":500,"window_ms":5000,"collectors":{"input_latency_ms":{"enabled":true},"jitter_ms":{"enabled":true},"dropped_frame_percent":{"enabled":true}},"emit_history":true}'
  call_json "collectors_config_run_${run}" POST "/api/v2/metrics/performance/collectors/config" "$cfg" "^200$"
  collector_state="$(extract_json 'd["data"]["state"]')"

  call_json "metrics_performance_run_${run}" GET "/api/v2/metrics/performance?session_id=ses_local" "" "^200$"
  perf_status_ok="$(extract_json 'str(d["data"]["input_latency_ms"]["status"] in ["ok","breach"] and d["data"]["jitter_ms"]["status"] in ["ok","breach"] and d["data"]["dropped_frame_percent"]["status"] in ["ok","breach"])')"

  call_json "metrics_samples_run_${run}" GET "/api/v2/metrics/performance/samples?session_id=ses_local&limit=3" "" "^200$"
  samples_seq_ok="$(extract_json 'str(all(curr["sample_seq"]==prev["sample_seq"]+1 for prev,curr in zip(d["data"]["samples"], d["data"]["samples"][1:])))')"
  samples_ts_ok="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["samples"], d["data"]["samples"][1:])))')"

  call_json "metrics_thresholds_run_${run}" GET "/api/v2/metrics/performance/thresholds?session_id=ses_local" "" "^200$"
  threshold_jitter="$(extract_json 'd["data"]["thresholds"]["jitter_ms_p95_max"]')"
  threshold_drop="$(extract_json 'd["data"]["thresholds"]["dropped_frame_percent_max"]')"

  call_json "metrics_alarms_run_${run}" GET "/api/v2/metrics/performance/alarms?session_id=ses_local&limit=2" "" "^200$"
  alarms_seq_ok="$(extract_json 'str(all(curr["alarm_seq"]==prev["alarm_seq"]+1 for prev,curr in zip(d["data"]["alarms"], d["data"]["alarms"][1:])))')"
  alarms_state_sig="$(extract_json '",".join(a["state"] for a in d["data"]["alarms"])')"

  if [[ "$collector_state" != "active" || "$perf_status_ok" != "True" || "$samples_seq_ok" != "True" || "$samples_ts_ok" != "True" || "$alarms_seq_ok" != "True" ]]; then
    echo "slo_contract=failed run=${run} collector=${collector_state} perf=${perf_status_ok} samples_seq=${samples_seq_ok} samples_ts=${samples_ts_ok} alarms_seq=${alarms_seq_ok}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="collector=${collector_state};thresholds=${threshold_jitter}:${threshold_drop};alarms=${alarms_state_sig}"
  echo "run_${run}_signature=${run_signature}" | tee -a "$OUT"

  if [[ -z "$run_signature_ref" ]]; then
    run_signature_ref="$run_signature"
  elif [[ "$run_signature" != "$run_signature_ref" ]]; then
    echo "determinism_check=failed run=${run}" | tee -a "$OUT"
    exit 1
  fi
done

echo "determinism_check=pass" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
