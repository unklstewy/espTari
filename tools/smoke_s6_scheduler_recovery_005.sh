#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_scheduler_recovery_005_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_scheduler_recovery_bundle_005_${TS}.json"
MATRIX="TRACKING/evidence/s6_scheduler_recovery_matrix_005.json"
PYTHON_BIN="/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python"
mkdir -p captures
: > "$OUT"

LAST_BODY=""
CREATED_IDS=()

cleanup() {
  local schedule_id
  for schedule_id in "${CREATED_IDS[@]:-}"; do
    if [[ -n "$schedule_id" ]]; then
      curl --max-time 10 -sS -X DELETE "${BASE_URL}/api/v2/catalog-sync/schedules/${schedule_id}" >/dev/null 2>&1 || true
    fi
  done
}
trap cleanup EXIT

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
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi
  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | "$PYTHON_BIN" -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health

if [[ ! -f "$MATRIX" ]]; then
  echo "missing_matrix=$MATRIX" | tee -a "$OUT"
  exit 1
fi

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "create_schedule_a" POST "/api/v2/catalog-sync/schedules" '{"job_type":"floppy_catalog_sync","mode":"catalog_and_probe_links","cron":"0 */6 * * *","enabled":true,"catch_up":false}' '^201$'
schedule_a_id="$(extract_json 'd["data"]["schedule_id"]')"
schedule_a_next="$(extract_json 'int(d["data"]["next_run_at_us"])')"
CREATED_IDS+=("$schedule_a_id")

call_json "create_schedule_a_conflict" POST "/api/v2/catalog-sync/schedules" '{"job_type":"floppy_catalog_sync","mode":"catalog_and_probe_links","cron":"0 */6 * * *","enabled":true,"catch_up":false}' '^409$'
conflict_code="$(extract_json 'd["error"]["code"]')"
if [[ "$conflict_code" != "CONFLICT" ]]; then
  echo "create_conflict=failed code=$conflict_code" | tee -a "$OUT"
  exit 1
fi

call_json "create_schedule_b" POST "/api/v2/catalog-sync/schedules" '{"job_type":"rom_catalog_sync","mode":"catalog_and_probe_links","cron":"0 */6 * * *","enabled":true,"catch_up":true}' '^201$'
schedule_b_id="$(extract_json 'd["data"]["schedule_id"]')"
schedule_b_next="$(extract_json 'int(d["data"]["next_run_at_us"])')"
CREATED_IDS+=("$schedule_b_id")

call_json "list_schedules" GET "/api/v2/catalog-sync/schedules" "" '^200$'
order_check="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)
targets=[]
for item in d["data"]["schedules"]:
    sid=item.get("schedule_id")
    if sid in {"'"$schedule_a_id"'","'"$schedule_b_id"'"}:
        targets.append((sid,int(item.get("next_run_at_us",0))))
if len(targets)!=2:
    print("missing")
    raise SystemExit(1)
targets.sort(key=lambda x:x[1])
if targets[0][1]==targets[1][1]:
    print("tie_by_schedule_id" if targets[0][0] < targets[1][0] else "tie_bad")
else:
    print("next_run_ordered" if targets[0][1] < targets[1][1] else "order_bad")
' <<<"$LAST_BODY")"
if [[ "$order_check" != "tie_by_schedule_id" && "$order_check" != "next_run_ordered" ]]; then
  echo "list_order=failed mode=$order_check" | tee -a "$OUT"
  exit 1
fi
echo "list_order=pass mode=$order_check schedule_a_next=$schedule_a_next schedule_b_next=$schedule_b_next" | tee -a "$OUT"

call_json "patch_schedule_b_disable" PATCH "/api/v2/catalog-sync/schedules/${schedule_b_id}" '{"enabled":false}' '^200$'
b_updated_disable="$(extract_json 'int(d["data"]["updated_at_us"])')"
b_enabled_disable="$(extract_json 'str(d["data"]["enabled"])')"
if [[ "$b_enabled_disable" != "False" ]]; then
  echo "patch_disable=failed enabled=$b_enabled_disable" | tee -a "$OUT"
  exit 1
fi

call_json "patch_schedule_b_enable_and_toggle_catchup" PATCH "/api/v2/catalog-sync/schedules/${schedule_b_id}" '{"enabled":true,"catch_up":false}' '^200$'
b_updated_enable="$(extract_json 'int(d["data"]["updated_at_us"])')"
b_next_enable="$(extract_json 'int(d["data"]["next_run_at_us"])')"
b_catchup_enable="$(extract_json 'str(d["data"]["catch_up"])')"
if [[ "$b_catchup_enable" != "False" || "$b_updated_enable" -le "$b_updated_disable" || "$b_next_enable" -le 0 ]]; then
  echo "patch_enable=failed catch_up=$b_catchup_enable updated_disable=$b_updated_disable updated_enable=$b_updated_enable next=$b_next_enable" | tee -a "$OUT"
  exit 1
fi

call_json "patch_schedule_b_invalid" PATCH "/api/v2/catalog-sync/schedules/${schedule_b_id}" '{"catch_up":"yes"}' '^400$'
invalid_patch_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_patch_code" != "SCRAPER_SCHEDULE_INVALID" ]]; then
  echo "patch_invalid=failed code=$invalid_patch_code" | tee -a "$OUT"
  exit 1
fi

call_json "get_schedule_b" GET "/api/v2/catalog-sync/schedules/${schedule_b_id}" "" '^200$'
get_schedule_b_id="$(extract_json 'd["data"]["schedule"]["schedule_id"]')"
get_schedule_b_catchup="$(extract_json 'str(d["data"]["schedule"]["catch_up"])')"
if [[ "$get_schedule_b_id" != "$schedule_b_id" || "$get_schedule_b_catchup" != "False" ]]; then
  echo "get_schedule=failed schedule_id=$get_schedule_b_id catch_up=$get_schedule_b_catchup" | tee -a "$OUT"
  exit 1
fi

call_json "recovery_report" GET "/api/v2/catalog-sync/recovery" "" '^200$'
recovery_loaded="$(extract_json 'int(d["data"]["loaded"])')"
recovery_validated="$(extract_json 'int(d["data"]["validated"])')"
recovery_recomputed="$(extract_json 'int(d["data"]["recomputed_next_run"])')"
recovery_quarantined="$(extract_json 'int(d["data"]["quarantined"])')"
recovery_run_id="$(extract_json 'd["data"]["recovery_run_id"]')"
recovery_schema_ok="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
required=["recovery_run_id","scheduler_now_us","loaded","validated","recomputed_next_run","quarantined","quarantine"]
print("true" if all(k in d for k in required) else "false")
' <<<"$LAST_BODY")"
if [[ "$recovery_schema_ok" != "true" ]]; then
  echo "recovery_schema=failed" | tee -a "$OUT"
  exit 1
fi

if [[ "$recovery_quarantined" -gt 0 ]]; then
  quarantine_codes="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
codes={item.get("error_code","") for item in d.get("quarantine",[])}
print("ok" if codes and codes=={"SCRAPER_SCHEDULE_INVALID"} else "bad")
' <<<"$LAST_BODY")"
  if [[ "$quarantine_codes" != "ok" ]]; then
    echo "recovery_quarantine=failed codes=$quarantine_codes" | tee -a "$OUT"
    exit 1
  fi
fi

call_json "delete_schedule_a" DELETE "/api/v2/catalog-sync/schedules/${schedule_a_id}" "" '^200$'
delete_a_status="$(extract_json 'd["data"]["status"]')"
if [[ "$delete_a_status" != "deleted" ]]; then
  echo "delete_schedule_a=failed status=$delete_a_status" | tee -a "$OUT"
  exit 1
fi
CREATED_IDS=("$schedule_b_id")

call_json "delete_schedule_b" DELETE "/api/v2/catalog-sync/schedules/${schedule_b_id}" "" '^200$'
delete_b_status="$(extract_json 'd["data"]["status"]')"
if [[ "$delete_b_status" != "deleted" ]]; then
  echo "delete_schedule_b=failed status=$delete_b_status" | tee -a "$OUT"
  exit 1
fi
CREATED_IDS=()

call_json "post_checks_health" GET "/api/v2/engine/health" "" '^200$'
post_ok="$(extract_json 'str(d.get("ok") is True)')"
if [[ "$post_ok" != "True" ]]; then
  echo "post_checks_health=failed ok=$post_ok" | tee -a "$OUT"
  exit 1
fi

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-005",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "scheduler_recovery_matrix": "${MATRIX}",
  "summary": {
    "schedule_a_id": "${schedule_a_id}",
    "schedule_b_id": "${schedule_b_id}",
    "list_order_mode": "${order_check}",
    "recovery_run_id": "${recovery_run_id}",
    "recovery_loaded": ${recovery_loaded},
    "recovery_validated": ${recovery_validated},
    "recovery_recomputed_next_run": ${recovery_recomputed},
    "recovery_quarantined": ${recovery_quarantined}
  }
}
EOF

echo "scheduler_recovery=pass order_mode=$order_check recovery_run_id=$recovery_run_id loaded=$recovery_loaded validated=$recovery_validated recomputed=$recovery_recomputed quarantined=$recovery_quarantined" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
