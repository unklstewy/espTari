#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_ebin_validate_002_smoke_postfix_${TS}.txt"
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

VALID_PAYLOAD='{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init","step","deinit"],"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}],"build_fingerprint":"build_20260304_a","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"}}'

wait_for_health
call_json "ebin_validate_success" POST "/api/v2/ebins/validate" "$VALID_PAYLOAD" "^200$"
OK_OK="$(extract_json 'str(d["ok"] is True)')"
VALID_OK="$(extract_json 'str(d["data"]["valid"] is True)')"
MOD_OK="$(extract_json 'str(d["data"]["module_id"]=="st.cpu.m68k")')"
ABI_OK="$(extract_json 'str(d["data"]["normalized"]["abi_major"]==1)')"
if [[ "$OK_OK" != "True" || "$VALID_OK" != "True" || "$MOD_OK" != "True" || "$ABI_OK" != "True" ]]; then
  echo "ebin_validate_success_contract=failed ok=$OK_OK valid=$VALID_OK module=$MOD_OK abi=$ABI_OK" | tee -a "$OUT"
  exit 1
fi
echo "ebin_validate_success_contract=pass ok=$OK_OK valid=$VALID_OK module=$MOD_OK abi=$ABI_OK" | tee -a "$OUT"

call_json "ebin_validate_missing_required" POST "/api/v2/ebins/validate" '{"module_type":"cpu"}' "^400$"
MISS_CODE="$(extract_json 'd["error"]["code"]')"
MISS_FIELD="$(extract_json 'd["error"]["details"]["field"]')"
if [[ "$MISS_CODE" != "BAD_REQUEST" || "$MISS_FIELD" != "module_id" ]]; then
  echo "ebin_validate_missing_required=failed code=$MISS_CODE field=$MISS_FIELD" | tee -a "$OUT"
  exit 1
fi
echo "ebin_validate_missing_required=pass code=$MISS_CODE field=$MISS_FIELD" | tee -a "$OUT"

call_json "ebin_validate_bad_module_type" POST "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"quantum","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}' "^400$"
MT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MT_CODE" != "EBIN_INVALID" ]]; then
  echo "ebin_validate_bad_module_type=failed code=$MT_CODE" | tee -a "$OUT"
  exit 1
fi
echo "ebin_validate_bad_module_type=pass code=$MT_CODE" | tee -a "$OUT"

call_json "ebin_validate_bad_sha" POST "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"xyz"}' "^400$"
SHA_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SHA_CODE" != "EBIN_INVALID" ]]; then
  echo "ebin_validate_bad_sha=failed code=$SHA_CODE" | tee -a "$OUT"
  exit 1
fi
echo "ebin_validate_bad_sha=pass code=$SHA_CODE" | tee -a "$OUT"

call_json "ebin_validate_abi_mismatch" POST "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"2.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}' "^409$"
ABI_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ABI_CODE" != "EBIN_ABI_MISMATCH" ]]; then
  echo "ebin_validate_abi_mismatch=failed code=$ABI_CODE" | tee -a "$OUT"
  exit 1
fi
echo "ebin_validate_abi_mismatch=pass code=$ABI_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
