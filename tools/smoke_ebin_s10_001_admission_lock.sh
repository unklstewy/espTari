#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_001_admission_lock_${TS}.txt"
mkdir -p captures
: > "$OUT"

AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_ARGS=()
if [[ -n "$AUTH_BEARER" ]]; then
  AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
fi
if [[ -n "$AUTH_HEADER" ]]; then
  AUTH_ARGS+=( -H "$AUTH_HEADER" )
fi
if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
  echo "auth_header=none" | tee -a "$OUT"
else
  echo "auth_header=configured" | tee -a "$OUT"
fi

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

post_json() {
  local name="$1" path="$2" data="$3" expected="$4"
  local body_file code body
  body_file="$(mktemp)"
  code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X POST "$BASE_URL$path" "${AUTH_ARGS[@]}" -H "Content-Type: application/json" -d "$data")
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "POST $path"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ ! "$code" =~ $expected ]]; then
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    exit 1
  fi

  printf '%s' "$body"
}

canonical_validate_success() {
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys; d=json.load(sys.stdin); print("|".join([str(d.get("ok")), str(d["data"]["valid"]), d["data"]["module_id"], d["data"]["normalized"]["machine"], d["data"]["normalized"]["module_type"], str(d["data"]["normalized"]["abi_major"]), str(d["data"]["normalized"]["api_contract_major"]), str(d["data"]["normalized"]["dependency_count"]), str(d["data"]["normalized"]["export_count"])]))'
}

extract_err() {
  local expr="$1"
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print(${expr})"
}

VALID_PAYLOAD='{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init","step","deinit"],"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}],"build_fingerprint":"build_s10_001","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"}}'

wait_for_health

echo "determinism_runs=10" | tee -a "$OUT"
base_fp=""
for i in $(seq 1 10); do
  body="$(post_json "validate_success_run_${i}" "/api/v2/ebins/validate" "$VALID_PAYLOAD" "^200$")"
  fp="$(printf '%s' "$body" | canonical_validate_success)"
  echo "run_${i}_fingerprint=$fp" >> "$OUT"
  if [[ -z "$base_fp" ]]; then
    base_fp="$fp"
  elif [[ "$fp" != "$base_fp" ]]; then
    echo "determinism_check=failed run=${i}" | tee -a "$OUT"
    exit 1
  fi
done

echo "determinism_check=pass" | tee -a "$OUT"

body="$(post_json "validate_missing_module_id" "/api/v2/ebins/validate" '{"module_type":"cpu"}' "^400$")"
code="$(printf '%s' "$body" | extract_err 'd["error"]["code"]')"
field="$(printf '%s' "$body" | extract_err 'd["error"]["details"]["field"]')"
[[ "$code" == "BAD_REQUEST" && "$field" == "module_id" ]] || { echo "missing_required_check=failed" | tee -a "$OUT"; exit 1; }
echo "missing_required_check=pass code=$code field=$field" | tee -a "$OUT"

body="$(post_json "validate_bad_module_type" "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"quantum","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}' "^400$")"
code="$(printf '%s' "$body" | extract_err 'd["error"]["code"]')"
[[ "$code" == "EBIN_INVALID" ]] || { echo "module_type_check=failed" | tee -a "$OUT"; exit 1; }
echo "module_type_check=pass code=$code" | tee -a "$OUT"

body="$(post_json "validate_bad_sha" "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"1.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"xyz"}' "^400$")"
code="$(printf '%s' "$body" | extract_err 'd["error"]["code"]')"
[[ "$code" == "EBIN_INVALID" ]] || { echo "sha_check=failed" | tee -a "$OUT"; exit 1; }
echo "sha_check=pass code=$code" | tee -a "$OUT"

body="$(post_json "validate_abi_mismatch" "/api/v2/ebins/validate" '{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"2.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}' "^409$")"
code="$(printf '%s' "$body" | extract_err 'd["error"]["code"]')"
[[ "$code" == "EBIN_ABI_MISMATCH" ]] || { echo "abi_mismatch_check=failed" | tee -a "$OUT"; exit 1; }
echo "abi_mismatch_check=pass code=$code" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
