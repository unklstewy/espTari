#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/snapshot_schema_contract_039_smoke_postfix_${TS}.txt"
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
    echo "Step failed: $name HTTP=$code"
    echo "$body"
    exit 1
  fi

  LAST_BODY="$body"
}

validate_schema_contract() {
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys
payload=json.load(sys.stdin)
data=payload["data"]
required_top=["snapshot_id","schema_version","profile","abi","hash","created_at_us","saved_at_us","state_blocks","scheduler","media_bindings"]
for key in required_top:
  if key not in data:
    raise SystemExit(f"missing top-level key: {key}")
if data["schema_version"] != 1:
  raise SystemExit("schema_version must equal 1")
for block in ["cpu","glue_mmu_shifter","mfp","acia_ikbd","dma_fdc","psg"]:
  if block not in data["state_blocks"]:
    raise SystemExit(f"missing state block: {block}")
  if "required" not in data["state_blocks"][block] or not isinstance(data["state_blocks"][block]["required"], list):
    raise SystemExit(f"missing required-list in block: {block}")
print("schema_contract=ok")'
}

wait_for_health
call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "save_snapshot_schema" POST "/api/v2/engine/state/save" '{"session_id":"ses_local","name":"schema_039"}' "^200$"
validate_schema_contract | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
