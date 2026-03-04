#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_011_startup_integration_${TS}.txt"
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

  minted_token="$(printf '%s' "$mint_resp" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys
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
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
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

  call_json "startup_defaults_run_${run}" GET "/api/v2/inspect/chipset/startup/defaults?session_id=ses_local" "" "^200$"
  defaults_conf_ok="$(extract_json 'str(d["data"]["conformance"]["PWR-BASE-01"]=="pass" and d["data"]["conformance"]["PWR-BASE-04"]=="pass")')"
  defaults_video="$(extract_json 'd["data"]["video_standard"]')"
  defaults_boot="$(extract_json 'd["data"]["boot_device"]')"
  defaults_revision="$(extract_json 'd["data"].get("defaults_revision", d["data"].get("revision", "none"))')"
  if [[ "$defaults_conf_ok" != "True" ]]; then
    echo "startup_defaults_contract=failed run=${run} conf=${defaults_conf_ok}" | tee -a "$OUT"
    exit 1
  fi

  call_json "startup_baseline_run_${run}" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp" "" "^200$"
  baseline_conf_ok="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
  baseline_group_ok="$(extract_json 'str(d["data"]["group"]=="mfp")')"
  baseline_unique_ok="$(extract_json 'str(len({r["address"] for r in d["data"]["registers"]})==len(d["data"]["registers"]))')"
  baseline_register_sample="$(extract_json '";".join(f"{r["address"]}:{r.get("value", r.get("reset_value", "na"))}" for r in d["data"]["registers"][:4])')"
  if [[ "$baseline_conf_ok" != "True" || "$baseline_group_ok" != "True" || "$baseline_unique_ok" != "True" ]]; then
    echo "startup_baseline_contract=failed run=${run} conf=${baseline_conf_ok} group=${baseline_group_ok} unique=${baseline_unique_ok}" | tee -a "$OUT"
    exit 1
  fi

  call_json "startup_sequence_run_${run}" GET "/api/v2/inspect/chipset/startup/sequence?session_id=ses_local" "" "^200$"
  seq_conf01_ok="$(extract_json 'str(d["data"]["conformance"]["RST-SEQ-01"]=="pass")')"
  seq_phase="$(extract_json 'd["data"]["phase"]')"
  seq_status="$(extract_json 'd["data"]["verification_status"]')"
  if [[ "$seq_conf01_ok" != "True" ]]; then
    echo "startup_sequence_contract=failed run=${run} conf01=${seq_conf01_ok}" | tee -a "$OUT"
    exit 1
  fi

  call_json "startup_verification_run_${run}" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=4" "" "^200$"
  verify_conf_ok="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
  verify_evseq_ok="$(extract_json 'str(all(curr["event_seq"]==prev["event_seq"]+1 for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
  verify_stepseq_ok="$(extract_json 'str(all(curr["step_seq"]==prev["step_seq"]+1 for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
  verify_ts_ok="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
  verify_tick_ok="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
  verify_pass_match_ok="$(extract_json 'str(all((e["result"]!="pass") or (e["expected"]==e["observed"]) for e in d["data"]["events"]))')"
  verify_events_sig="$(extract_json '";".join(f"{e["check_id"]}:{e["result"]}" for e in d["data"]["events"])')"
  if [[ "$verify_conf_ok" != "True" || "$verify_evseq_ok" != "True" || "$verify_stepseq_ok" != "True" || "$verify_ts_ok" != "True" || "$verify_tick_ok" != "True" || "$verify_pass_match_ok" != "True" ]]; then
    echo "startup_verification_contract=failed run=${run} conf=${verify_conf_ok} evseq=${verify_evseq_ok} stepseq=${verify_stepseq_ok} ts=${verify_ts_ok} tick=${verify_tick_ok} match=${verify_pass_match_ok}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="video=${defaults_video};boot=${defaults_boot};revision=${defaults_revision};baseline=${baseline_register_sample};phase=${seq_phase};status=${seq_status};verify=${verify_events_sig}"
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
