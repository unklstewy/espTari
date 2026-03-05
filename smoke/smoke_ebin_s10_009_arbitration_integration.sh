#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_009_arbitration_integration_${TS}.txt"
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

normalize_owner_seq() {
  local owners_csv="$1"
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import sys
owners=sys.argv[1].split(",")
ring=["glue","mmu","shifter","cpu","dma"]
idx={v:i for i,v in enumerate(ring)}
if not owners or owners[0] not in idx:
    print("invalid")
    raise SystemExit(0)
base=idx[owners[0]]
vals=[]
for owner in owners:
    if owner not in idx:
        print("invalid")
        raise SystemExit(0)
    vals.append(str((idx[owner]-base)%len(ring)))
print(",".join(vals))' "$owners_csv"
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

  owners=()
  waits=()
  for n in 1 2 3 4 5; do
    call_json "integration_trace_run_${run}_${n}" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_local" "" "^200$"

    checks_pass="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
    order_ok="$(extract_json 'str(d["data"]["last_integration"]["chipset_order"] == ["glue","mmu","shifter"])')"
    owner="$(extract_json 'd["data"]["last_integration"]["bus_owner"]')"
    wait_cycles="$(extract_json 'int(d["data"]["last_integration"]["wait_cycles"])')"

    if [[ "$checks_pass" != "True" || "$order_ok" != "True" ]]; then
      echo "integration_contract=failed run=${run} sample=${n} checks=$checks_pass order=$order_ok" | tee -a "$OUT"
      exit 1
    fi

    owners+=("$owner")
    waits+=("$wait_cycles")
  done

  owners_csv="$(IFS=,; echo "${owners[*]}")"
  waits_csv="$(IFS=,; echo "${waits[*]}")"
  owner_norm="$(normalize_owner_seq "$owners_csv")"
  wait_norm="$(/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import sys
vals=[int(x) for x in sys.argv[1].split(",") if x]
if not vals:
  print("invalid")
  raise SystemExit(0)
base=vals[0]
out=[str(((v-base)%3)) for v in vals]
print(",".join(out))' "$waits_csv")"

  [[ "$owner_norm" != "invalid" ]] || { echo "integration_owner_pattern=failed run=${run} owners=${owners_csv}" | tee -a "$OUT"; exit 1; }
  [[ "$wait_norm" != "invalid" ]] || { echo "integration_wait_pattern=failed run=${run} waits=${waits_csv}" | tee -a "$OUT"; exit 1; }

  call_json "dma_arbitration_trace_run_${run}" GET "/api/v2/inspect/chipset/dma/arbitration?session_id=ses_local&limit=3" "" "^200$"
  dma_seq_delta_1="$(extract_json 'd["data"]["events"][1]["request_seq"] - d["data"]["events"][0]["request_seq"]')"
  dma_seq_delta_2="$(extract_json 'd["data"]["events"][2]["request_seq"] - d["data"]["events"][1]["request_seq"]')"
  dma_grants="$(extract_json '",".join(e["grant_state"] for e in d["data"]["events"])')"
  dma_requesters="$(extract_json '",".join(e["requester"] for e in d["data"]["events"])')"

  if [[ "$dma_seq_delta_1" != "1" || "$dma_seq_delta_2" != "1" ]]; then
    echo "dma_arbitration_seq=failed run=${run} deltas=${dma_seq_delta_1},${dma_seq_delta_2}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="owner_norm=${owner_norm};wait_norm=${wait_norm};dma_grants=${dma_grants};dma_requesters=${dma_requesters}"
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
