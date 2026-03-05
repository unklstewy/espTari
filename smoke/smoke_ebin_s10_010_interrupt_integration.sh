#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_010_interrupt_integration_${TS}.txt"
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

  call_json "interrupt_hierarchy_run_${run}" GET "/api/v2/inspect/chipset/interrupts/hierarchy?session_id=ses_local" "" "^200$"
  levels_ok="$(extract_json 'str(d["data"]["cpu_level_order"] == [7,6,5,4,3,2,1])')"
  sources_ok="$(extract_json 'str(set(s["source_id"] for s in d["data"]["sources"]) == {"mfp","acia","fdc","blitter","vbl"})')"
  map01_ok="$(extract_json 'str(d["data"]["checks"]["INT-MAP-01"] == "pass")')"
  if [[ "$levels_ok" != "True" || "$sources_ok" != "True" || "$map01_ok" != "True" ]]; then
    echo "interrupt_hierarchy_contract=failed run=${run} levels=${levels_ok} sources=${sources_ok} map01=${map01_ok}" | tee -a "$OUT"
    exit 1
  fi

  call_json "interrupt_routes_run_${run}" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=4" "" "^200$"
  routes_checks_ok="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
  routes_seq_ok="$(extract_json 'str(all(curr["route_seq"]==prev["route_seq"]+1 for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
  routes_ts_ok="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
  routes_tick_ok="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
  routes_order_ok="$(extract_json 'str([r["source_id"] for r in d["data"]["routes"]] == ["vbl","mfp","acia","fdc"])')"
  if [[ "$routes_checks_ok" != "True" || "$routes_seq_ok" != "True" || "$routes_ts_ok" != "True" || "$routes_tick_ok" != "True" || "$routes_order_ok" != "True" ]]; then
    echo "interrupt_routes_contract=failed run=${run} checks=${routes_checks_ok} seq=${routes_seq_ok} ts=${routes_ts_ok} tick=${routes_tick_ok} order=${routes_order_ok}" | tee -a "$OUT"
    exit 1
  fi

  routes_order_csv="$(extract_json '",".join(r["source_id"] for r in d["data"]["routes"])')"

  call_json "interrupt_wiring_run_${run}" GET "/api/v2/inspect/chipset/interrupts/wiring?session_id=ses_local" "" "^200$"
  subsystems_ok="$(extract_json 'str(set(s["subsystem_id"] for s in d["data"]["subsystems"]) == {"mfp","acia","fdc","blitter","vbl"})')"
  lines_ok="$(extract_json 'str(all(s["cpu_interrupt_line"] in {"irq1","irq2","irq3","irq4","irq5","irq6","irq7"} for s in d["data"]["subsystems"]))')"
  if [[ "$subsystems_ok" != "True" || "$lines_ok" != "True" ]]; then
    echo "interrupt_wiring_contract=failed run=${run} subsystems=${subsystems_ok} lines=${lines_ok}" | tee -a "$OUT"
    exit 1
  fi

  call_json "interrupt_wiring_checks_run_${run}" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=4" "" "^200$"
  wiring_conf_ok="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
  wiring_seq_ok="$(extract_json 'str(all(curr["check_seq"]==prev["check_seq"]+1 for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
  wiring_ts_ok="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
  wiring_tick_ok="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
  wiring_match_ok="$(extract_json 'str(all((c["result"]!="pass") or (c["observed_cpu_line"]==c["expected_cpu_line"] and c["observed_vector"]==c["expected_vector"]) for c in d["data"]["checks"]))')"
  if [[ "$wiring_conf_ok" != "True" || "$wiring_seq_ok" != "True" || "$wiring_ts_ok" != "True" || "$wiring_tick_ok" != "True" || "$wiring_match_ok" != "True" ]]; then
    echo "interrupt_wiring_checks_contract=failed run=${run} conf=${wiring_conf_ok} seq=${wiring_seq_ok} ts=${wiring_ts_ok} tick=${wiring_tick_ok} match=${wiring_match_ok}" | tee -a "$OUT"
    exit 1
  fi

  wiring_pairs="$(extract_json '",".join(f"{c["observed_cpu_line"]}:{c["observed_vector"]}" for c in d["data"]["checks"])')"
  wiring_results="$(extract_json '",".join(c["result"] for c in d["data"]["checks"])')"

  run_signature="routes=${routes_order_csv};wiring=${wiring_pairs};results=${wiring_results}"
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
