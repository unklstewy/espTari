#!/usr/bin/env bash
set -euo pipefail

SCHEME="http"
HOST="esptari.local"
PORT="80"
TOKEN="${ESPTARI_API_TOKEN:-}"
CLIENT_ID="${ESPTARI_API_CLIENT_ID:-}"
CLIENT_SECRET="${ESPTARI_API_CLIENT_SECRET:-}"
SCOPE=""
TIMEOUT=20
DRY_RUN=0
VERBOSE=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TMP_DIR=""
cleanup() {
  if [[ -n "$TMP_DIR" && -d "$TMP_DIR" ]]; then
    rm -rf "$TMP_DIR"
  fi
}
trap cleanup EXIT

usage() {
  cat <<'EOF'
Usage: remote_files.sh [global options] <command> [command options]

Remote SD-card file management over espTari web API.

Global options:
  --host HOST              Device host (default: esptari.local)
  --port PORT              Device port (default: 80)
  --scheme http|https      URL scheme (default: http)
  --token TOKEN            Bearer token (or set ESPTARI_API_TOKEN)
  --client-id ID           OAuth client id for token mint
  --client-secret SECRET   OAuth client secret for token mint
  --scope SCOPE            Token scope request (optional)
  --timeout SEC            Curl timeout seconds (default: 20)
  --dry-run                Print intended actions, do not call API
  --verbose                Extra diagnostics
  -h, --help               Show this help

Commands:
  auth-token
      Mint token and print it to stdout.

  list --path /sdcard/...
  stat --path /sdcard/...
  mkdir --path /sdcard/...
  delete --path /sdcard/...
  move --from /sdcard/a --to /sdcard/b
  upload --path /sdcard/file [--from local_file]
  download --path /sdcard/file
  txn-apply --ops-file ops.txt [--simulate-fail-at N] [--report-file path.json]
      Apply multi-file operations atomically (best-effort): rollback runs in reverse order on failure.
    Optional idempotency controls:
      --idempotency-key KEY      Replay-protect a transaction key.
      --idempotency-store PATH   Local ledger file (default: .cache/remote_files_txn_idempotency.tsv)

      Ops file format (pipe-delimited, one operation per line; # comments allowed):
        mkdir|/sdcard/path
        upload|/sdcard/file|/local/file
        upload|/sdcard/file
        move|/sdcard/from|/sdcard/to
        delete|/sdcard/path

  setup-engine-v2 [--module-id st.profile.520] [--versions 1.0.0,1.1.0]
      Prepare Engine V2 directories and seed baseline JSON + machine_profile fixtures.

Examples:
  ./tools/remote_files.sh --host esptari.local auth-token --client-id dev --client-secret dev
  ./tools/remote_files.sh --host esptari.local --token "$ESPTARI_API_TOKEN" list --path /sdcard/ebins
  ./tools/remote_files.sh --host esptari.local --token "$ESPTARI_API_TOKEN" setup-engine-v2 --versions 1.0.0,1.1.0,2.0.0

Notes:
  - Current firmware file API may accept metadata-only uploads; this script still sends content metadata
    so it remains forward-compatible when binary transfer is implemented server-side.
EOF
}

json_escape() {
  python3 - "$1" <<'PY'
import json,sys
print(json.dumps(sys.argv[1]))
PY
}

txn_ops_signature() {
  local ops_file="$1"
  python3 - "$ops_file" <<'PY'
import hashlib
import sys

ops_file = sys.argv[1]
canonical = []

with open(ops_file, 'r', encoding='utf-8') as handle:
    for raw in handle:
        line = raw.rstrip('\n').rstrip('\r').strip()
        if not line or line.startswith('#'):
            continue
        parts = [segment.strip() for segment in line.split('|')]
        if len(parts) > 3:
            parts = parts[:3]
        canonical.append('|'.join(parts))

payload = ('\n'.join(canonical) + ('\n' if canonical else '')).encode('utf-8')
digest = hashlib.sha256(payload).hexdigest()
print(f"{digest} {len(canonical)}")
PY
}

write_txn_report() {
  local report_file="$1"
  local status="$2"
  local signature="$3"
  local steps_total="$4"
  local steps_applied="$5"
  local rollback_attempted="$6"
  local rollback_warnings="$7"
  local finalize_count="$8"

  [[ -n "$report_file" ]] || return 0

  local report_tmp
  report_tmp="$(mktemp "${report_file}.tmp.XXXXXX")"

  cat > "$report_tmp" <<EOF
{
  "schema": "remote_files_txn_report_v1",
  "status": "${status}",
  "ops_signature": "${signature}",
  "steps_total": ${steps_total},
  "steps_applied": ${steps_applied},
  "rollback_attempted": ${rollback_attempted},
  "rollback_warnings": ${rollback_warnings},
  "finalize_count": ${finalize_count}
}
EOF

  mv -f "$report_tmp" "$report_file"
}

is_valid_idempotency_key() {
  [[ "$1" =~ ^[A-Za-z0-9._:-]{1,128}$ ]]
}

idempotency_lookup() {
  local store_file="$1"
  local key="$2"

  [[ -f "$store_file" ]] || return 1
  awk -F'|' -v k="$key" '$1 == k { line=$0 } END { if (line != "") print line; else exit 1 }' "$store_file"
}

idempotency_record() {
  local store_file="$1"
  local key="$2"
  local signature="$3"
  local status="$4"
  local exit_code="$5"
  local steps_total="$6"
  local steps_applied="$7"
  local rollback_attempted="$8"
  local rollback_warnings="$9"
  local finalize_count="${10}"

  local store_dir
  store_dir="$(dirname "$store_file")"
  mkdir -p "$store_dir"
  printf '%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
    "$key" "$signature" "$status" "$exit_code" "$steps_total" "$steps_applied" "$rollback_attempted" "$rollback_warnings" "$finalize_count" "$(date +%s)" \
    >> "$store_file"
}

is_valid_semver() {
  [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
}

compare_semver() {
  local a="$1"
  local b="$2"
  local a1 a2 a3 b1 b2 b3
  IFS='.' read -r a1 a2 a3 <<< "$a"
  IFS='.' read -r b1 b2 b3 <<< "$b"

  if (( a1 > b1 )); then echo 1; return; fi
  if (( a1 < b1 )); then echo -1; return; fi
  if (( a2 > b2 )); then echo 1; return; fi
  if (( a2 < b2 )); then echo -1; return; fi
  if (( a3 > b3 )); then echo 1; return; fi
  if (( a3 < b3 )); then echo -1; return; fi
  echo 0
}

pick_winner() {
  local best=""
  local v cmp
  for v in "$@"; do
    if [[ -z "$best" ]]; then
      best="$v"
      continue
    fi
    cmp="$(compare_semver "$v" "$best")"
    if [[ "$cmp" == "1" ]]; then
      best="$v"
    fi
  done
  printf '%s\n' "$best"
}

base_url() {
  printf '%s://%s:%s' "$SCHEME" "$HOST" "$PORT"
}

auth_header_args() {
  if [[ -n "$TOKEN" ]]; then
    printf -- '-H\nAuthorization: Bearer %s\n' "$TOKEN"
  fi
}

call_api() {
  local method="$1"
  local path="$2"
  local body="${3:-}"

  local url
  url="$(base_url)$path"

  if [[ $DRY_RUN -eq 1 ]]; then
    if [[ -n "$body" ]]; then
      echo "[DRY] $method $url body=$body"
    else
      echo "[DRY] $method $url"
    fi
    return 0
  fi

  TMP_DIR="$(mktemp -d)"
  local out="$TMP_DIR/resp.json"
  local code

  local -a curl_args
  curl_args=(--silent --show-error --location --max-time "$TIMEOUT" --request "$method" "$url" --output "$out" --write-out "%{http_code}" -H "Content-Type: application/json")
  if [[ -n "$TOKEN" ]]; then
    curl_args+=( -H "Authorization: Bearer $TOKEN" )
  fi
  if [[ -n "$body" ]]; then
    curl_args+=( --data "$body" )
  fi

  code="$(curl "${curl_args[@]}")"

  if [[ $VERBOSE -eq 1 ]]; then
    echo "[DBG] HTTP $code $method $path" >&2
  fi

  if [[ "$code" -lt 200 || "$code" -ge 300 ]]; then
    echo "[ERR ] HTTP $code for $method $path" >&2
    cat "$out" >&2 || true
    return 1
  fi

  cat "$out"
}

require_token() {
  if [[ $DRY_RUN -eq 1 ]]; then
    return 0
  fi
  if [[ -n "$TOKEN" ]]; then
    return 0
  fi
  if [[ -n "$CLIENT_ID" && -n "$CLIENT_SECRET" ]]; then
    mint_token >/dev/null
    return 0
  fi
  echo "[ERR ] Missing auth: provide --token or --client-id/--client-secret" >&2
  exit 1
}

mint_token() {
  if [[ -z "$CLIENT_ID" || -z "$CLIENT_SECRET" ]]; then
    echo "[ERR ] auth-token requires --client-id and --client-secret" >&2
    exit 1
  fi

  local scope_json
  if [[ -n "$SCOPE" ]]; then
    scope_json=",\"scope\":$(json_escape "$SCOPE")"
  else
    scope_json=""
  fi

  local payload
  payload="{\"grant_type\":\"client_credentials\",\"client_id\":$(json_escape "$CLIENT_ID"),\"client_secret\":$(json_escape "$CLIENT_SECRET")$scope_json}"

  local resp
  resp="$(call_api POST "/api/v2/auth/token" "$payload")"
  TOKEN="$(python3 - <<'PY' "$resp"
import json,sys
data=json.loads(sys.argv[1])
print(data.get("data",{}).get("access_token",""))
PY
)"
  if [[ -z "$TOKEN" ]]; then
    echo "[ERR ] Failed to parse access token" >&2
    exit 1
  fi
  echo "$TOKEN"
}

cmd_list() {
  local path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown list option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] list requires --path" >&2; exit 1; }
  require_token
  call_api GET "/api/v2/files/list?path=$path"
}

cmd_stat() {
  local path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown stat option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] stat requires --path" >&2; exit 1; }
  require_token
  call_api GET "/api/v2/files/stat?path=$path"
}

cmd_mkdir() {
  local path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown mkdir option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] mkdir requires --path" >&2; exit 1; }
  require_token
  call_api POST "/api/v2/files/mkdir" "{\"path\":$(json_escape "$path")}" >/dev/null
  echo "[ OK ] mkdir $path"
}

cmd_delete() {
  local path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown delete option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] delete requires --path" >&2; exit 1; }
  require_token
  call_api POST "/api/v2/files/delete" "{\"path\":$(json_escape "$path")}" >/dev/null
  echo "[ OK ] delete $path"
}

cmd_move() {
  local from=""
  local to=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --from) from="$2"; shift 2 ;;
      --to) to="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown move option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$from" && -n "$to" ]] || { echo "[ERR ] move requires --from and --to" >&2; exit 1; }
  require_token
  call_api POST "/api/v2/files/move" "{\"from\":$(json_escape "$from"),\"to\":$(json_escape "$to")}" >/dev/null
  echo "[ OK ] move $from -> $to"
}

cmd_upload() {
  local path=""
  local from=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      --from) from="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown upload option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] upload requires --path" >&2; exit 1; }
  require_token

  local payload
  if [[ -n "$from" ]]; then
    [[ -f "$from" ]] || { echo "[ERR ] local file not found: $from" >&2; exit 1; }
    local filename
    local content_b64
    filename="$(basename "$from")"
    content_b64="$(base64 -w0 "$from")"
    payload="{\"path\":$(json_escape "$path"),\"filename\":$(json_escape "$filename"),\"contentBase64\":$(json_escape "$content_b64")}"
  else
    payload="{\"path\":$(json_escape "$path")}"
  fi

  call_api POST "/api/v2/files/upload" "$payload" >/dev/null
  if [[ -n "$from" ]]; then
    echo "[ OK ] upload $from -> $path"
  else
    echo "[ OK ] upload path accepted $path"
  fi
}

cmd_download() {
  local path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --path) path="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown download option: $1" >&2; exit 1 ;;
    esac
  done
  [[ -n "$path" ]] || { echo "[ERR ] download requires --path" >&2; exit 1; }
  require_token
  call_api GET "/api/v2/files/download?path=$path"
}

remote_path_exists() {
  local path="$1"

  if [[ $DRY_RUN -eq 1 ]]; then
    return 1
  fi

  local url
  url="$(base_url)/api/v2/files/stat?path=$path"

  local -a curl_args
  curl_args=(--silent --show-error --location --max-time "$TIMEOUT" --request GET "$url" --output /dev/null --write-out "%{http_code}" -H "Content-Type: application/json")
  if [[ -n "$TOKEN" ]]; then
    curl_args+=( -H "Authorization: Bearer $TOKEN" )
  fi

  local code
  code="$(curl "${curl_args[@]}")"
  [[ "$code" -ge 200 && "$code" -lt 300 ]]
}

api_mkdir() {
  local path="$1"
  call_api POST "/api/v2/files/mkdir" "{\"path\":$(json_escape "$path")}" >/dev/null
}

api_delete() {
  local path="$1"
  call_api POST "/api/v2/files/delete" "{\"path\":$(json_escape "$path")}" >/dev/null
}

api_move() {
  local from="$1"
  local to="$2"
  call_api POST "/api/v2/files/move" "{\"from\":$(json_escape "$from"),\"to\":$(json_escape "$to")}" >/dev/null
}

api_upload() {
  local path="$1"
  local from="${2:-}"

  local payload
  if [[ -n "$from" ]]; then
    [[ -f "$from" ]] || { echo "[ERR ] local file not found: $from" >&2; return 1; }
    local filename
    local content_b64
    filename="$(basename "$from")"
    content_b64="$(base64 -w0 "$from")"
    payload="{\"path\":$(json_escape "$path"),\"filename\":$(json_escape "$filename"),\"contentBase64\":$(json_escape "$content_b64")}"
  else
    payload="{\"path\":$(json_escape "$path")}" 
  fi

  call_api POST "/api/v2/files/upload" "$payload" >/dev/null
}

cmd_txn_apply() {
  local ops_file=""
  local simulate_fail_at=0
  local report_file=""
  local idempotency_key=""
  local idempotency_store="$PROJECT_ROOT/.cache/remote_files_txn_idempotency.tsv"

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --ops-file) ops_file="$2"; shift 2 ;;
      --simulate-fail-at) simulate_fail_at="$2"; shift 2 ;;
      --report-file) report_file="$2"; shift 2 ;;
      --idempotency-key) idempotency_key="$2"; shift 2 ;;
      --idempotency-store) idempotency_store="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown txn-apply option: $1" >&2; exit 1 ;;
    esac
  done

  [[ -n "$ops_file" ]] || { echo "[ERR ] txn-apply requires --ops-file" >&2; exit 1; }
  [[ -f "$ops_file" ]] || { echo "[ERR ] ops file not found: $ops_file" >&2; exit 1; }
  [[ "$simulate_fail_at" =~ ^[0-9]+$ ]] || { echo "[ERR ] --simulate-fail-at must be an integer >= 0" >&2; exit 1; }
  if [[ -n "$idempotency_key" ]]; then
    is_valid_idempotency_key "$idempotency_key" || { echo "[ERR ] invalid --idempotency-key format" >&2; exit 1; }
  fi

  local ops_signature steps_total
  read -r ops_signature steps_total < <(txn_ops_signature "$ops_file")

  if [[ -n "$idempotency_key" ]]; then
    local replay_line=""
    if replay_line="$(idempotency_lookup "$idempotency_store" "$idempotency_key" 2>/dev/null)"; then
      local r_key r_sig r_status r_exit r_steps_total r_steps_applied r_rollback_attempted r_rollback_warnings r_finalize_count _r_ts
      IFS='|' read -r r_key r_sig r_status r_exit r_steps_total r_steps_applied r_rollback_attempted r_rollback_warnings r_finalize_count _r_ts <<< "$replay_line"

      if [[ "$r_sig" != "$ops_signature" ]]; then
        echo "[ERR ] idempotency key reuse with different ops signature key=$idempotency_key" >&2
        write_txn_report "$report_file" "key_conflict" "$ops_signature" "$steps_total" "0" "0" "0" "0"
        return 1
      fi

      echo "[TXN ] summary status=replayed prior_status=${r_status} idempotency_key=${idempotency_key} ops_signature=${ops_signature} steps_total=${r_steps_total} steps_applied=${r_steps_applied} rollback_attempted=${r_rollback_attempted} rollback_warnings=${r_rollback_warnings} finalize_count=${r_finalize_count}"
      write_txn_report "$report_file" "replayed" "$ops_signature" "$r_steps_total" "$r_steps_applied" "$r_rollback_attempted" "$r_rollback_warnings" "$r_finalize_count"
      return 0
    fi
  fi

  require_token

  local txn_id
  txn_id="$(date +%s)_$$"
  local -a rollback_ops=()
  local -a finalize_ops=()
  local apply_failed=0
  local applied_steps=0

  run_encoded_op() {
    local encoded="$1"
    local eop e1 e2
    IFS='|' read -r eop e1 e2 _ <<< "$encoded"
    case "$eop" in
      delete) api_delete "$e1" ;;
      move) api_move "$e1" "$e2" ;;
      *) echo "[ERR ] unknown encoded op: $encoded" >&2; return 1 ;;
    esac
  }

  local line op a1 a2 step_no=0
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%$'\r'}"
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^[[:space:]]*# ]] && continue

    IFS='|' read -r op a1 a2 _ <<< "$line"
    op="${op:-}"
    a1="${a1:-}"
    a2="${a2:-}"
    step_no=$((step_no + 1))

    echo "[TXN ] apply #$step_no $op $a1 ${a2:-}"

    if [[ "$simulate_fail_at" -gt 0 && "$step_no" -eq "$simulate_fail_at" ]]; then
      echo "[ERR ] simulated failure at step $step_no" >&2
      apply_failed=1
      break
    fi

    case "$op" in
      mkdir)
        [[ -n "$a1" ]] || { echo "[ERR ] mkdir requires path" >&2; apply_failed=1; break; }
        if remote_path_exists "$a1"; then
          :
        else
          if ! api_mkdir "$a1"; then
            apply_failed=1
            break
          fi
          rollback_ops+=("delete|$a1")
        fi
        applied_steps=$((applied_steps + 1))
        ;;
      upload)
        [[ -n "$a1" ]] || { echo "[ERR ] upload requires path" >&2; apply_failed=1; break; }
        local backup_path=""
        if remote_path_exists "$a1"; then
          backup_path="${a1}.txn.${txn_id}.bak"
          if ! api_move "$a1" "$backup_path"; then
            apply_failed=1
            break
          fi
          rollback_ops+=("move|$backup_path|$a1")
          finalize_ops+=("delete|$backup_path")
        fi
        if ! api_upload "$a1" "$a2"; then
          apply_failed=1
          break
        fi
        rollback_ops+=("delete|$a1")
        applied_steps=$((applied_steps + 1))
        ;;
      move)
        [[ -n "$a1" && -n "$a2" ]] || { echo "[ERR ] move requires from and to" >&2; apply_failed=1; break; }
        local dst_backup=""
        if remote_path_exists "$a2"; then
          dst_backup="${a2}.txn.${txn_id}.bak"
          if ! api_move "$a2" "$dst_backup"; then
            apply_failed=1
            break
          fi
          rollback_ops+=("move|$dst_backup|$a2")
          finalize_ops+=("delete|$dst_backup")
        fi
        if ! api_move "$a1" "$a2"; then
          apply_failed=1
          break
        fi
        rollback_ops+=("move|$a2|$a1")
        applied_steps=$((applied_steps + 1))
        ;;
      delete)
        [[ -n "$a1" ]] || { echo "[ERR ] delete requires path" >&2; apply_failed=1; break; }
        if remote_path_exists "$a1"; then
          local delete_backup
          delete_backup="${a1}.txn.${txn_id}.bak"
          if ! api_move "$a1" "$delete_backup"; then
            apply_failed=1
            break
          fi
          rollback_ops+=("move|$delete_backup|$a1")
          finalize_ops+=("delete|$delete_backup")
        fi
        applied_steps=$((applied_steps + 1))
        ;;
      *)
        echo "[ERR ] Unsupported txn op '$op' in $ops_file" >&2
        apply_failed=1
        break
        ;;
    esac
  done < "$ops_file"

  if [[ "$apply_failed" -eq 1 ]]; then
    echo "[TXN ] rollback start count=${#rollback_ops[@]}" >&2
    local i rb rollback_warnings=0
    for ((i=${#rollback_ops[@]}-1; i>=0; i--)); do
      rb="${rollback_ops[$i]}"
      if ! run_encoded_op "$rb"; then
        echo "[WARN] rollback op failed: $rb" >&2
        rollback_warnings=$((rollback_warnings + 1))
      fi
    done
    echo "[TXN ] rollback complete" >&2
    echo "[TXN ] summary status=rolled_back ops_signature=${ops_signature} steps_total=${steps_total} steps_applied=${applied_steps} rollback_attempted=${#rollback_ops[@]} rollback_warnings=${rollback_warnings} finalize_count=${#finalize_ops[@]}" >&2
    write_txn_report "$report_file" "rolled_back" "$ops_signature" "$steps_total" "$applied_steps" "${#rollback_ops[@]}" "$rollback_warnings" "${#finalize_ops[@]}"
    if [[ -n "$idempotency_key" ]]; then
      idempotency_record "$idempotency_store" "$idempotency_key" "$ops_signature" "rolled_back" "1" "$steps_total" "$applied_steps" "${#rollback_ops[@]}" "$rollback_warnings" "${#finalize_ops[@]}"
    fi
    return 1
  fi

  echo "[TXN ] commit finalize count=${#finalize_ops[@]}"
  local f_item
  for f_item in "${finalize_ops[@]}"; do
    if ! run_encoded_op "$f_item"; then
      echo "[TXN ] summary status=finalize_failed ops_signature=${ops_signature} steps_total=${steps_total} steps_applied=${applied_steps} rollback_attempted=0 rollback_warnings=0 finalize_count=${#finalize_ops[@]}" >&2
      write_txn_report "$report_file" "finalize_failed" "$ops_signature" "$steps_total" "$applied_steps" "0" "0" "${#finalize_ops[@]}"
      if [[ -n "$idempotency_key" ]]; then
        idempotency_record "$idempotency_store" "$idempotency_key" "$ops_signature" "finalize_failed" "1" "$steps_total" "$applied_steps" "0" "0" "${#finalize_ops[@]}"
      fi
      return 1
    fi
  done
  echo "[TXN ] commit complete steps=$step_no"
  echo "[TXN ] summary status=committed ops_signature=${ops_signature} steps_total=${steps_total} steps_applied=${applied_steps} rollback_attempted=0 rollback_warnings=0 finalize_count=${#finalize_ops[@]}"
  write_txn_report "$report_file" "committed" "$ops_signature" "$steps_total" "$applied_steps" "0" "0" "${#finalize_ops[@]}"
  if [[ -n "$idempotency_key" ]]; then
    idempotency_record "$idempotency_store" "$idempotency_key" "$ops_signature" "committed" "0" "$steps_total" "$applied_steps" "0" "0" "${#finalize_ops[@]}"
  fi
  return 0
}

cmd_setup_engine_v2() {
  local module_id="st.profile.520"
  local versions_csv="1.0.0,1.1.0"

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --module-id) module_id="$2"; shift 2 ;;
      --versions) versions_csv="$2"; shift 2 ;;
      *) echo "[ERR ] Unknown setup-engine-v2 option: $1" >&2; exit 1 ;;
    esac
  done

  local -a versions
  IFS=',' read -r -a versions <<< "$versions_csv"
  [[ ${#versions[@]} -gt 0 ]] || { echo "[ERR ] --versions must not be empty" >&2; exit 1; }

  local v
  for v in "${versions[@]}"; do
    is_valid_semver "$v" || { echo "[ERR ] Invalid semver: $v" >&2; exit 1; }
  done

  require_token

  local -a dirs=(
    "/sdcard/config"
    "/sdcard/config/engine_v2"
    "/sdcard/config/engine_v2/machines"
    "/sdcard/config/engine_v2/machines/atari_st"
    "/sdcard/config/engine_v2/input"
    "/sdcard/config/engine_v2/input/mappings"
    "/sdcard/config/engine_v2/input/mappings/atari_st"
    "/sdcard/ebins"
    "/sdcard/ebins/atari_st"
    "/sdcard/ebins/atari_st/cpu"
    "/sdcard/ebins/atari_st/video"
    "/sdcard/ebins/atari_st/io"
    "/sdcard/ebins/atari_st/storage"
    "/sdcard/ebins/atari_st/audio"
    "/sdcard/ebins/atari_st/machine_profile"
  )

  local dir
  for dir in "${dirs[@]}"; do
    call_api POST "/api/v2/files/mkdir" "{\"path\":$(json_escape "$dir")}" >/dev/null
    echo "[ OK ] mkdir $dir"
  done

  local -a json_files=(
    "/sdcard/config/engine_v2/machines/atari_st/st_520_pal.json"
    "/sdcard/config/engine_v2/machines/atari_st/st_520_pal_wiring_bad.json"
    "/sdcard/config/engine_v2/input/mappings/atari_st/atari_st_default_v1.json"
    "/sdcard/config/engine_v2/rom_catalog.json"
    "/sdcard/config/engine_v2/disk_catalog.json"
    "/sdcard/config/engine_v2/tos_catalog.json"
    "/sdcard/config/engine_v2/catalog_sync_schedules.json"
    "/sdcard/ebins/index.json"
  )

  local p
  for p in "${json_files[@]}"; do
    call_api POST "/api/v2/files/upload" "{\"path\":$(json_escape "$p"),\"source\":\"remote_setup_engine_v2\"}" >/dev/null
    echo "[ OK ] upload placeholder $p"
  done

  for v in "${versions[@]}"; do
    p="/sdcard/ebins/atari_st/machine_profile/${module_id}-${v}.ebin"
    call_api POST "/api/v2/files/upload" "{\"path\":$(json_escape "$p"),\"source\":\"remote_setup_engine_v2_fixture\"}" >/dev/null
    echo "[ OK ] upload fixture $p"
  done

  local winner
  winner="$(pick_winner "${versions[@]}")"
  echo "[INFO] Expected loader winner: ${module_id}@${winner}"
  echo "[INFO] Expected resolved profile: st_520_pal"
}

COMMAND=""
ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --scheme) SCHEME="$2"; shift 2 ;;
    --token) TOKEN="$2"; shift 2 ;;
    --client-id) CLIENT_ID="$2"; shift 2 ;;
    --client-secret) CLIENT_SECRET="$2"; shift 2 ;;
    --scope) SCOPE="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --verbose) VERBOSE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    auth-token|list|stat|mkdir|delete|move|upload|download|setup-engine-v2|txn-apply)
      COMMAND="$1"
      shift
      ARGS=("$@")
      break
      ;;
    *)
      echo "[ERR ] Unknown option/command: $1" >&2
      usage
      exit 1
      ;;
  esac
done

[[ -n "$COMMAND" ]] || { usage; exit 1; }

case "$COMMAND" in
  auth-token)
    mint_token
    ;;
  list)
    cmd_list "${ARGS[@]}"
    ;;
  stat)
    cmd_stat "${ARGS[@]}"
    ;;
  mkdir)
    cmd_mkdir "${ARGS[@]}"
    ;;
  delete)
    cmd_delete "${ARGS[@]}"
    ;;
  move)
    cmd_move "${ARGS[@]}"
    ;;
  upload)
    cmd_upload "${ARGS[@]}"
    ;;
  download)
    cmd_download "${ARGS[@]}"
    ;;
  setup-engine-v2)
    cmd_setup_engine_v2 "${ARGS[@]}"
    ;;
  txn-apply)
    cmd_txn_apply "${ARGS[@]}"
    ;;
  *)
    echo "[ERR ] Unsupported command: $COMMAND" >&2
    exit 1
    ;;
esac
