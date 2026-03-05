#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_ebin_resolve_004_smoke_postfix_${TS}.txt"
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

wait_for_health

call_json "resolve_latest_success" POST "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu","video"],"version_policy":"latest_compatible"}' "^200$"
OK_OK="$(extract_json 'str(d["ok"] is True)')"
COUNT_OK="$(extract_json 'str(d["data"]["resolved_count"]==2)')"
CPU_VER_OK="$(extract_json 'str(any(x["component"]=="cpu" and x["version"]=="1.0.0" for x in d["data"]["resolved"]))')"
if [[ "$OK_OK" != "True" || "$COUNT_OK" != "True" || "$CPU_VER_OK" != "True" ]]; then
  echo "resolve_latest_success_contract=failed ok=$OK_OK count=$COUNT_OK cpu_latest=$CPU_VER_OK" | tee -a "$OUT"
  exit 1
fi
echo "resolve_latest_success_contract=pass ok=$OK_OK count=$COUNT_OK cpu_latest=$CPU_VER_OK" | tee -a "$OUT"

call_json "resolve_machine_missing" POST "/api/v2/ebins/resolve" '{"machine":"amiga","components":["cpu"],"version_policy":"latest_compatible"}' "^404$"
MACHINE_CODE="$(extract_json 'd["error"]["code"]')"
MACHINE_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$MACHINE_CODE" != "EBIN_NOT_FOUND" || "$MACHINE_REASON" != "resolver_machine_not_indexed" ]]; then
  echo "resolve_machine_missing=failed code=$MACHINE_CODE reason=$MACHINE_REASON" | tee -a "$OUT"
  exit 1
fi
echo "resolve_machine_missing=pass code=$MACHINE_CODE reason=$MACHINE_REASON" | tee -a "$OUT"

call_json "resolve_component_missing" POST "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["dsp"],"version_policy":"latest_compatible"}' "^404$"
COMP_CODE="$(extract_json 'd["error"]["code"]')"
COMP_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$COMP_CODE" != "EBIN_NOT_FOUND" || "$COMP_REASON" != "resolver_component_not_found" ]]; then
  echo "resolve_component_missing=failed code=$COMP_CODE reason=$COMP_REASON" | tee -a "$OUT"
  exit 1
fi
echo "resolve_component_missing=pass code=$COMP_CODE reason=$COMP_REASON" | tee -a "$OUT"

call_json "resolve_ambiguous_simulated" POST "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu"],"version_policy":"latest_compatible","force_ambiguous_component":"cpu"}' "^409$"
AMB_CODE="$(extract_json 'd["error"]["code"]')"
AMB_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$AMB_CODE" != "EBIN_INVALID" || "$AMB_REASON" != "resolver_ambiguous_selection" ]]; then
  echo "resolve_ambiguous_simulated=failed code=$AMB_CODE reason=$AMB_REASON" | tee -a "$OUT"
  exit 1
fi
echo "resolve_ambiguous_simulated=pass code=$AMB_CODE reason=$AMB_REASON" | tee -a "$OUT"

call_json "resolve_pinned_success" POST "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu"],"version_policy":"pinned","pinned_versions":{"cpu":"0.9.0"}}' "^200$"
PIN_OK="$(extract_json 'str(any(x["component"]=="cpu" and x["version"]=="0.9.0" for x in d["data"]["resolved"]))')"
if [[ "$PIN_OK" != "True" ]]; then
  echo "resolve_pinned_success=failed cpu_pinned=$PIN_OK" | tee -a "$OUT"
  exit 1
fi
echo "resolve_pinned_success=pass cpu_pinned=$PIN_OK" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
