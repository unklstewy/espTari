#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SAMPLES="${SAMPLES:-24}"
INTERVAL_S="${INTERVAL_S:-1}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_stability_soak_003_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_stability_soak_bundle_003_${TS}.json"
MATRIX="TRACKING/evidence/s6_long_run_stability_matrix_003.json"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
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
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path")
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
    echo "Step failed: $name HTTP=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

read_state_field() {
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys
try:
  d=json.load(sys.stdin)
except Exception:
  print('unknown')
  raise SystemExit(0)
state='unknown'
data=d.get('data',{}) if isinstance(d,dict) else {}
if isinstance(data,dict):
  if isinstance(data.get('state'),str):
    state=data['state']
  elif isinstance(data.get('session'),dict) and isinstance(data['session'].get('state'),str):
    state=data['session']['state']
print(state)" <<< "$LAST_BODY"
}

wait_for_health
if [[ ! -f "$MATRIX" ]]; then
  echo "missing_matrix=$MATRIX" | tee -a "$OUT"
  exit 1
fi

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

health_pass=0
status_pass=0
transport_fail=0
parse_fail=0
state_running=0
state_paused=0
state_idle=0
state_unknown=0

for i in $(seq 1 "$SAMPLES"); do
  call_json "sample_${i}_health" GET "/api/v2/engine/health" "" "^200$"
  health_ok="$(extract_json 'str(d.get("ok") is True)')"
  if [[ "$health_ok" == "True" ]]; then
    health_pass=$((health_pass + 1))
  else
    parse_fail=$((parse_fail + 1))
  fi

  call_json "sample_${i}_status" GET "/api/v2/engine/status" "" "^200$"
  status_ok="$(extract_json 'str(d.get("ok") is True)')"
  if [[ "$status_ok" == "True" ]]; then
    status_pass=$((status_pass + 1))
  else
    parse_fail=$((parse_fail + 1))
  fi

  state="$(read_state_field)"
  case "$state" in
    running) state_running=$((state_running + 1)) ;;
    paused)  state_paused=$((state_paused + 1)) ;;
    idle)    state_idle=$((state_idle + 1)) ;;
    *)       state_unknown=$((state_unknown + 1)) ;;
  esac

  echo "sample=${i} state=${state} health_ok=${health_ok} status_ok=${status_ok}" | tee -a "$OUT"
  if [[ "$i" -lt "$SAMPLES" ]]; then
    sleep "$INTERVAL_S"
  fi
done

if [[ "$health_pass" -ne "$SAMPLES" || "$status_pass" -ne "$SAMPLES" || "$transport_fail" -ne 0 || "$parse_fail" -ne 0 ]]; then
  echo "stability_soak=failed samples=${SAMPLES} health_pass=${health_pass} status_pass=${status_pass} transport_fail=${transport_fail} parse_fail=${parse_fail}" | tee -a "$OUT"
  exit 1
fi

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-003",
  "generated_at": "${TS}",
  "samples": ${SAMPLES},
  "interval_seconds": ${INTERVAL_S},
  "harness_capture": "${OUT}",
  "stability_matrix": "${MATRIX}",
  "summary": {
    "health_pass": ${health_pass},
    "status_pass": ${status_pass},
    "transport_fail": ${transport_fail},
    "parse_fail": ${parse_fail},
    "state_running": ${state_running},
    "state_paused": ${state_paused},
    "state_idle": ${state_idle},
    "state_unknown": ${state_unknown}
  }
}
EOF

echo "stability_soak=pass samples=${SAMPLES} health_pass=${health_pass} status_pass=${status_pass} transport_fail=${transport_fail} parse_fail=${parse_fail}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
