#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_slo_alarm_006_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_slo_alarm_bundle_006_${TS}.json"
MATRIX="TRACKING/evidence/s6_slo_alarm_matrix_006.json"
PYTHON_BIN="/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python"
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
[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "collectors_config_activate" POST "/api/v2/metrics/performance/collectors/config" '{"session_id":"ses_local","sampling_interval_ms":200,"window_ms":2000,"collectors":{"input_latency_ms":{"enabled":true},"jitter_ms":{"enabled":true},"dropped_frame_percent":{"enabled":true}},"emit_history":true}' '^200$'
collector_state="$(extract_json 'd["data"]["state"]')"
collector_revision="$(extract_json 'd["data"]["collector_revision"]')"
if [[ "$collector_state" != "active" || "$collector_revision" != slo_col_rev_* ]]; then
  echo "collector_config=failed state=$collector_state revision=$collector_revision" | tee -a "$OUT"
  exit 1
fi

call_json "performance_snapshot" GET "/api/v2/metrics/performance?session_id=ses_local" "" '^200$'
perf_ok="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
required=["input_latency_ms","jitter_ms","dropped_frame_percent","window_ms","window_end_us"]
print("true" if all(k in d for k in required) else "false")
' <<<"$LAST_BODY")"
if [[ "$perf_ok" != "true" ]]; then
  echo "performance_snapshot=failed" | tee -a "$OUT"
  exit 1
fi

call_json "samples_limit_6" GET "/api/v2/metrics/performance/samples?session_id=ses_local&limit=6" "" '^200$'
sample_checks="$($PYTHON_BIN -c 'import json,sys
samples=json.load(sys.stdin)["data"]["samples"]
seq=[int(s["sample_seq"]) for s in samples]
wnd=[int(s["window_end_us"]) for s in samples]
jitter=[float(s["jitter_ms_p95"]) for s in samples]
drop=[float(s["dropped_frame_percent"]) for s in samples]
ok_seq=all(b>a for a,b in zip(seq,seq[1:]))
ok_wnd=all(b>=a for a,b in zip(wnd,wnd[1:]))
has_jitter_breach=any(v>30.0 for v in jitter)
has_drop_breach=any(v>1.0 for v in drop)
print(f"{ok_seq},{ok_wnd},{has_jitter_breach},{has_drop_breach},{seq[-1]}")
' <<<"$LAST_BODY")"
IFS=',' read -r seq_ok wnd_ok jitter_breach drop_breach last_seq <<<"$sample_checks"
if [[ "$seq_ok" != "True" || "$wnd_ok" != "True" || "$jitter_breach" != "True" || "$drop_breach" != "True" ]]; then
  echo "samples=failed seq_ok=$seq_ok wnd_ok=$wnd_ok jitter_breach=$jitter_breach drop_breach=$drop_breach" | tee -a "$OUT"
  exit 1
fi

call_json "thresholds" GET "/api/v2/metrics/performance/thresholds?session_id=ses_local" "" '^200$'
thr_revision="$(extract_json 'd["data"]["active_revision"]')"
thr_jitter="$(extract_json 'float(d["data"]["thresholds"]["jitter_ms_p95_max"])')"
if [[ "$thr_revision" != "slo_thr_rev_02" || "$thr_jitter" != "30.0" ]]; then
  echo "thresholds=failed revision=$thr_revision jitter=$thr_jitter" | tee -a "$OUT"
  exit 1
fi

call_json "alarms_limit_4" GET "/api/v2/metrics/performance/alarms?session_id=ses_local&limit=4" "" '^200$'
alarm_checks="$($PYTHON_BIN -c 'import json,sys
alarms=json.load(sys.stdin)["data"]["alarms"]
states=[a["state"] for a in alarms]
metric_ok=all(a["metric"]=="jitter_ms_p95" for a in alarms)
timing_ok=all(int(a["timestamp_us"])>int(a["window_end_us"]) for a in alarms)
alternate_ok=all(states[i]!=states[i-1] for i in range(1,len(states)))
print(f"{metric_ok},{timing_ok},{alternate_ok},{states[0] if states else 'none'},{states[-1] if states else 'none'}")
' <<<"$LAST_BODY")"
IFS=',' read -r alarm_metric_ok alarm_timing_ok alarm_alternate_ok first_state last_state <<<"$alarm_checks"
if [[ "$alarm_metric_ok" != "True" || "$alarm_timing_ok" != "True" || "$alarm_alternate_ok" != "True" ]]; then
  echo "alarms=failed metric_ok=$alarm_metric_ok timing_ok=$alarm_timing_ok alternate_ok=$alarm_alternate_ok" | tee -a "$OUT"
  exit 1
fi

call_json "history_limit_3" GET "/api/v2/metrics/performance/history?session_id=ses_local&limit=3" "" '^200$'
history_count="$(extract_json 'len(d["data"]["history"])')"
if [[ "$history_count" -lt 1 ]]; then
  echo "history=failed count=$history_count" | tee -a "$OUT"
  exit 1
fi

call_json "alarms_bad_limit" GET "/api/v2/metrics/performance/alarms?session_id=ses_local&limit=0" "" '^400$'
bad_limit_code="$(extract_json 'd["error"]["code"]')"
if [[ "$bad_limit_code" != "BAD_REQUEST" ]]; then
  echo "alarms_bad_limit=failed code=$bad_limit_code" | tee -a "$OUT"
  exit 1
fi

call_json "post_checks_health" GET "/api/v2/engine/health" "" '^200$'
post_ok="$(extract_json 'str(d.get("ok") is True)')"
[[ "$post_ok" == "True" ]] || { echo "post_checks_health=failed" | tee -a "$OUT"; exit 1; }

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-006",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "slo_alarm_matrix": "${MATRIX}",
  "summary": {
    "collector_revision": "${collector_revision}",
    "last_sample_seq": ${last_seq},
    "first_alarm_state": "${first_state}",
    "last_alarm_state": "${last_state}",
    "history_count": ${history_count}
  }
}
EOF

echo "slo_alarm=pass collector_revision=$collector_revision last_sample_seq=$last_seq first_alarm_state=$first_state last_alarm_state=$last_state history_count=$history_count" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
