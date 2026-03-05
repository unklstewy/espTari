#!/usr/bin/env bash

smoke_infer_task_ref() {
  local script_name="$1"
  local base
  base="$(basename "$script_name")"

  local refs=()
  local token

  while IFS= read -r token; do
    refs+=("EBIN-${token#ebin_}")
  done < <(printf '%s' "$base" | grep -oE 'ebin_[0-9]{3}' || true)

  while IFS= read -r token; do
    refs+=("T-${token#t}")
  done < <(printf '%s' "$base" | grep -oE 't[0-9]{3}' || true)

  if [[ ${#refs[@]} -eq 0 ]]; then
    while IFS= read -r token; do
      refs+=("T-${token}")
    done < <(printf '%s' "$base" | grep -oE '[0-9]{3}' || true)
  fi

  if [[ ${#refs[@]} -eq 0 ]]; then
    printf '%s\n' "UNSPECIFIED"
    return 0
  fi

  local dedup=()
  local seen=""
  local ref
  for ref in "${refs[@]}"; do
    if [[ ",${seen}," != *",${ref},"* ]]; then
      dedup+=("$ref")
      seen+="${ref},"
    fi
  done

  local joined=""
  for ref in "${dedup[@]}"; do
    if [[ -n "$joined" ]]; then
      joined+=","
    fi
    joined+="$ref"
  done
  printf '%s\n' "$joined"
}

smoke_auth_init() {
  local base_url="$1"
  local script_path="$2"
  local explicit_task_ref="${3:-}"

  AUTH_BEARER="${AUTH_BEARER:-}"
  AUTH_HEADER="${AUTH_HEADER:-}"
  AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
  AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
  AUTH_SCOPE="${AUTH_SCOPE:-engine:control inspect:read input:write ebin:manage}"
  AUTH_ARGS=()

  if [[ -n "$AUTH_HEADER" ]]; then
    :
  elif [[ -n "$AUTH_BEARER" ]]; then
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
  else
    local mint_resp minted_token
    mint_resp="$(command curl --max-time 15 -sS -X POST "${base_url}/api/v2/auth/token" \
      -H "Content-Type: application/json" \
      -d "{\"grant_type\":\"client_credentials\",\"client_id\":\"${AUTH_CLIENT_ID}\",\"client_secret\":\"${AUTH_CLIENT_SECRET}\",\"scope\":\"${AUTH_SCOPE}\"}" || true)"

    minted_token="$(printf '%s' "$mint_resp" | python3 -c 'import json,sys
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
  fi

  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
  fi
  if [[ -n "$AUTH_HEADER" ]]; then
    AUTH_ARGS+=( -H "$AUTH_HEADER" )
  fi

  local task_ref
  if [[ -n "$explicit_task_ref" ]]; then
    task_ref="$explicit_task_ref"
  else
    task_ref="$(smoke_infer_task_ref "$script_path")"
  fi

  echo "task_alignment=${task_ref}"
  if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
    echo "auth_header=none"
  else
    echo "auth_header=configured"
  fi

  curl() {
    command curl "${AUTH_ARGS[@]}" "$@"
  }
}