#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/startup_baseline_108_smoke_postfix_${TS}.txt"
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
call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "startup_defaults" GET "/api/v2/inspect/chipset/startup/defaults?session_id=ses_local" "" "^200$"
VIDEO_OK="$(extract_json 'str(d["data"]["video_standard"] in {"pal","ntsc"})')"
BOOT_OK="$(extract_json 'str(d["data"]["boot_device"] in {"floppy","harddisk","rom"})')"
CONF_DEFAULTS_OK="$(extract_json 'str(d["data"]["conformance"]["PWR-BASE-01"]=="pass" and d["data"]["conformance"]["PWR-BASE-04"]=="pass")')"
if [[ "$VIDEO_OK" != "True" || "$BOOT_OK" != "True" || "$CONF_DEFAULTS_OK" != "True" ]]; then
  echo "startup_defaults_contract=failed video=$VIDEO_OK boot=$BOOT_OK conf=$CONF_DEFAULTS_OK" | tee -a "$OUT"
  exit 1
fi

echo "startup_defaults_contract=pass video=$VIDEO_OK boot=$BOOT_OK conf=$CONF_DEFAULTS_OK" | tee -a "$OUT"

call_json "startup_baseline_mfp" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp" "" "^200$"
GROUP_OK="$(extract_json 'str(d["data"]["group"]=="mfp")')"
REGS_OK="$(extract_json 'str(len(d["data"]["registers"])>0)')"
ADDR_UNIQUE_OK="$(extract_json 'str(len({r["address"] for r in d["data"]["registers"]})==len(d["data"]["registers"]))')"
CONF_BASE_OK="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
if [[ "$GROUP_OK" != "True" || "$REGS_OK" != "True" || "$ADDR_UNIQUE_OK" != "True" || "$CONF_BASE_OK" != "True" ]]; then
  echo "startup_baseline_contract=failed group=$GROUP_OK regs=$REGS_OK addr_unique=$ADDR_UNIQUE_OK conf=$CONF_BASE_OK" | tee -a "$OUT"
  exit 1
fi

echo "startup_baseline_contract=pass group=$GROUP_OK regs=$REGS_OK addr_unique=$ADDR_UNIQUE_OK conf=$CONF_BASE_OK" | tee -a "$OUT"

call_json "startup_baseline_bad_group" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=foo" "" "^400$"
BAD_GROUP_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_GROUP_CODE" != "BAD_REQUEST" ]]; then
  echo "startup_group_guard=failed code=$BAD_GROUP_CODE" | tee -a "$OUT"
  exit 1
fi

echo "startup_group_guard=pass code=$BAD_GROUP_CODE" | tee -a "$OUT"

call_json "startup_unknown_session" GET "/api/v2/inspect/chipset/startup/defaults?session_id=ses_unknown" "" "^409$"
SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "startup_session_guard=failed code=$SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "startup_session_guard=pass code=$SESSION_CODE" | tee -a "$OUT"

call_json "startup_revision_unavailable" GET "/api/v2/inspect/chipset/startup/defaults?session_id=ses_local&force_revision_unavailable=1" "" "^500$"
REV_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$REV_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "startup_revision_failfast=failed code=$REV_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "startup_revision_failfast=pass code=$REV_FAIL_CODE" | tee -a "$OUT"

call_json "pwr_base01_failfast" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp&force_missing_defaults=1" "" "^500$"
BASE01_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BASE01_CODE" != "INTERNAL_ERROR" ]]; then
  echo "pwr_base01_failfast=failed code=$BASE01_CODE" | tee -a "$OUT"
  exit 1
fi

echo "pwr_base01_failfast=pass code=$BASE01_CODE" | tee -a "$OUT"

call_json "pwr_base02_failfast" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp&force_nondeterministic=1" "" "^500$"
BASE02_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BASE02_CODE" != "INTERNAL_ERROR" ]]; then
  echo "pwr_base02_failfast=failed code=$BASE02_CODE" | tee -a "$OUT"
  exit 1
fi

echo "pwr_base02_failfast=pass code=$BASE02_CODE" | tee -a "$OUT"

call_json "pwr_base03_failfast" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp&force_duplicate_address=1" "" "^500$"
BASE03_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BASE03_CODE" != "INTERNAL_ERROR" ]]; then
  echo "pwr_base03_failfast=failed code=$BASE03_CODE" | tee -a "$OUT"
  exit 1
fi

echo "pwr_base03_failfast=pass code=$BASE03_CODE" | tee -a "$OUT"

call_json "pwr_base04_failfast" GET "/api/v2/inspect/chipset/startup/baseline?session_id=ses_local&group=mfp&force_timestamp_regression=1" "" "^500$"
BASE04_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BASE04_CODE" != "INTERNAL_ERROR" ]]; then
  echo "pwr_base04_failfast=failed code=$BASE04_CODE" | tee -a "$OUT"
  exit 1
fi

echo "pwr_base04_failfast=pass code=$BASE04_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
