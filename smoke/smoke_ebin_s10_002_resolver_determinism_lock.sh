#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_002_resolver_lock_${TS}.txt"
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

canon_resolve() {
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys; d=json.load(sys.stdin); parts=[d["data"]["machine"],d["data"]["version_policy"],str(d["data"]["resolved_count"])]; rows=["{component}:{module_id}@{version}:{path}:{selection_policy}".format(**x) for x in d["data"]["resolved"]]; print("|".join(parts+rows))'
}

extract_expr() {
  local expr="$1"
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print(${expr})"
}

REQ='{"machine":"atari_st","components":["cpu","video","io","storage","audio","machine_profile"],"version_policy":"latest_compatible"}'

wait_for_health

echo "determinism_runs=10" | tee -a "$OUT"
base_fp=""
for i in $(seq 1 10); do
  body="$(post_json "resolve_latest_run_${i}" "/api/v2/ebins/resolve" "$REQ" "^200$")"
  fp="$(printf '%s' "$body" | canon_resolve)"
  echo "run_${i}_fingerprint=$fp" >> "$OUT"
  if [[ -z "$base_fp" ]]; then
    base_fp="$fp"
  elif [[ "$fp" != "$base_fp" ]]; then
    echo "determinism_check=failed run=${i}" | tee -a "$OUT"
    exit 1
  fi
done
echo "determinism_check=pass" | tee -a "$OUT"

body="$(post_json "resolve_ambiguous" "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu"],"version_policy":"latest_compatible","force_ambiguous_component":"cpu"}' "^409$")"
code="$(printf '%s' "$body" | extract_expr 'd["error"]["code"]')"
reason="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["reason"]')"
[[ "$code" == "EBIN_INVALID" && "$reason" == "resolver_ambiguous_selection" ]] || { echo "ambiguous_check=failed" | tee -a "$OUT"; exit 1; }
echo "ambiguous_check=pass code=$code reason=$reason" | tee -a "$OUT"

body="$(post_json "resolve_machine_missing" "/api/v2/ebins/resolve" '{"machine":"amiga","components":["cpu"],"version_policy":"latest_compatible"}' "^404$")"
code="$(printf '%s' "$body" | extract_expr 'd["error"]["code"]')"
reason="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["reason"]')"
[[ "$code" == "EBIN_NOT_FOUND" && "$reason" == "resolver_machine_not_indexed" ]] || { echo "machine_missing_check=failed" | tee -a "$OUT"; exit 1; }
echo "machine_missing_check=pass code=$code reason=$reason" | tee -a "$OUT"

body="$(post_json "resolve_pinned" "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu"],"version_policy":"pinned","pinned_versions":{"cpu":"0.9.0"}}' "^200$")"
pin_ok="$(printf '%s' "$body" | extract_expr 'str(any(x["component"]=="cpu" and x["version"]=="0.9.0" for x in d["data"]["resolved"]))')"
[[ "$pin_ok" == "True" ]] || { echo "pinned_check=failed" | tee -a "$OUT"; exit 1; }
echo "pinned_check=pass" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
