#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TOKEN_FILE="${TOKEN_FILE:-}"
AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
AUTH_SCOPE="${AUTH_SCOPE:-ebin:manage}"

if [[ -z "$AUTH_BEARER" && -n "$TOKEN_FILE" ]]; then
  if [[ -f "$TOKEN_FILE" ]]; then
    AUTH_BEARER="$(tr -d '[:space:]' < "$TOKEN_FILE")"
  fi
fi

if [[ -z "$AUTH_HEADER" && -n "$AUTH_BEARER" ]]; then
  AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
fi

if [[ -z "$AUTH_HEADER" ]]; then
  mint_resp="$(curl --max-time 15 -sS -X POST "${BASE_URL}/api/v2/auth/token" \
    -H "Content-Type: application/json" \
    -d "{\"grant_type\":\"client_credentials\",\"client_id\":\"${AUTH_CLIENT_ID}\",\"client_secret\":\"${AUTH_CLIENT_SECRET}\",\"scope\":\"${AUTH_SCOPE}\"}" || true)"
  minted_token="$(printf '%s' "$mint_resp" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys; raw=sys.stdin.read().strip(); token=""; 
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
fi

if [[ -z "$AUTH_HEADER" ]]; then
  echo "error=missing_auth_header required_scope=ebin:manage"
  echo "usage_hint=export AUTH_BEARER=<token> or TOKEN_FILE=<path_to_token>"
  echo "usage_hint=or set AUTH_CLIENT_ID/AUTH_CLIENT_SECRET for /api/v2/auth/token"
  exit 2
fi

echo "base_url=${BASE_URL}"
echo "auth=provided"

BASE_URL="$BASE_URL" AUTH_HEADER="$AUTH_HEADER" bash ./smoke/smoke_ebin_s10_phase_a.sh
