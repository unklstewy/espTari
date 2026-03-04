#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_FILES_SH="$SCRIPT_DIR/remote_files.sh"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VSCODE_SETTINGS_PATH="$PROJECT_ROOT/.vscode/settings.json"
IDF_SETUP="${ESPTARI_IDF_SETUP:-}"
IDF_EXPORT_SH="${ESPTARI_IDF_EXPORT_SH:-}"
IDF_PYTHON_BIN="${ESPTARI_IDF_PYTHON_BIN:-}"
IDF_TOOLS_PATH="${ESPTARI_IDF_TOOLS_PATH:-}"
IDF_PYTHON_ENV_PATH="${ESPTARI_IDF_PYTHON_ENV_PATH:-}"
IDF_PY_SCRIPT=""
IDF_PORT="${ESPTARI_IDF_PORT:-}"
IDF_TOOLCHAIN_PATHS="${ESPTARI_IDF_TOOLCHAIN_PATHS:-}"

SCHEME="http"
HOST="esptari.local"
PORT="80"
TOKEN="${ESPTARI_API_TOKEN:-}"
CLIENT_ID="${ESPTARI_API_CLIENT_ID:-}"
CLIENT_SECRET="${ESPTARI_API_CLIENT_SECRET:-}"
SCOPE="engine:control inspect:read files:write files:read"

VERSIONS="1.0.0,1.1.0"
MODULE_ID="st.profile.520"
SKIP_SETUP=0

RESET_MODE="warm"
PRESERVE_MEDIA="true"

WAIT_TIMEOUT=90
WAIT_INTERVAL=2
VERBOSE=0
DRY_RUN=0
MONITOR=0
MONITOR_CMD="${ESPTARI_MONITOR_CMD:-idf.py monitor}"
FLASH_BEFORE_MONITOR=0
FLASH_CMD="${ESPTARI_FLASH_CMD:-idf.py build flash}"
MONITOR_CMD_CUSTOM=0
FLASH_CMD_CUSTOM=0

if [[ -n "${ESPTARI_MONITOR_CMD:-}" ]]; then
  MONITOR_CMD_CUSTOM=1
fi
if [[ -n "${ESPTARI_FLASH_CMD:-}" ]]; then
  FLASH_CMD_CUSTOM=1
fi

usage() {
  cat <<'EOF'
Usage: remote_dev_cycle.sh [options]

One-command remote dev loop:
  1) remote setup-engine-v2 prep (optional)
  2) issue engine reset endpoint
  3) wait for API readiness (health + protected status)

Options:
  --host HOST                    Device host (default: esptari.local)
  --port PORT                    Device port (default: 80)
  --scheme http|https            URL scheme (default: http)
  --token TOKEN                  Bearer token (or env ESPTARI_API_TOKEN)
  --client-id ID                 Mint token client id
  --client-secret SECRET         Mint token client secret
  --scope "a b c"                Scope for minted token (default includes files+control+inspect)
  --idf-export-sh PATH            Explicit ESP-IDF export.sh path override
  --idf-port PORT                 Explicit serial port override (e.g. /dev/ttyACM0)

  --versions CSV                 Semver fixtures for setup (default: 1.0.0,1.1.0)
  --module-id ID                 Fixture module id (default: st.profile.520)
  --skip-setup                   Skip setup-engine-v2 step
  --quick                        Convenience alias: --skip-setup --flash-before-monitor --monitor

  --reset-mode warm|cold         Reset mode body field (default: warm)
  --preserve-media true|false    preserve_media body field (default: true)

  --wait-timeout SEC             Readiness wait timeout (default: 90)
  --wait-interval SEC            Poll interval (default: 2)
  --flash-before-monitor         Run flash command before monitor launch
  --flash-cmd CMD                Flash command (default: "idf.py build flash")
  --monitor                      Launch monitor command after readiness
  --monitor-cmd CMD              Monitor command (default: "idf.py monitor")
  --dry-run                      Print actions without calling endpoints
  --verbose                      Extra diagnostics
  -h, --help                     Show this help

Examples:
  ./tools/remote_dev_cycle.sh --host esptari.local --client-id dev --client-secret dev
  ./tools/remote_dev_cycle.sh --host esptari.local --token "$ESPTARI_API_TOKEN" --versions 1.0.0,1.1.0,2.0.0
  ./tools/remote_dev_cycle.sh --host esptari.local --token "$ESPTARI_API_TOKEN" --skip-setup --reset-mode cold
  ./tools/remote_dev_cycle.sh --host esptari.local --token "$ESPTARI_API_TOKEN" --flash-before-monitor --monitor
  ./tools/remote_dev_cycle.sh --host esptari.local --token "$ESPTARI_API_TOKEN" --monitor --monitor-cmd "idf.py monitor"
  ./tools/remote_dev_cycle.sh --host esptari.local --client-id esptari-smoke --client-secret esptari-smoke-secret --quick

Resolution order for ESP-IDF environment:
  1) --idf-export-sh / ESPTARI_IDF_EXPORT_SH
  2) .vscode/settings.json -> idf.currentSetup
  3) fallback: /home/sannis/.espressif/v5.5.2/esp-idf/export.sh
EOF
}

extract_vscode_setting() {
  local key="$1"
  local file="$2"
  [[ -f "$file" ]] || return 1
  sed -nE "s/^[[:space:]]*\"$key\"[[:space:]]*:[[:space:]]*\"([^\"]+)\".*/\1/p" "$file" | head -n 1
}

resolve_idf_paths() {
  local fallback_export="/home/sannis/.espressif/v5.5.2/esp-idf/export.sh"
  local vscode_setup=""
  local vscode_python=""
  local vscode_port=""

  vscode_setup="$(extract_vscode_setting 'idf.currentSetup' "$VSCODE_SETTINGS_PATH" || true)"
  vscode_python="$(extract_vscode_setting 'idf.pythonBinPath' "$VSCODE_SETTINGS_PATH" || true)"
  vscode_port="$(extract_vscode_setting 'idf.port' "$VSCODE_SETTINGS_PATH" || true)"

  if [[ -z "$IDF_SETUP" && -n "$vscode_setup" ]]; then
    IDF_SETUP="$vscode_setup"
  fi

  if [[ -z "$IDF_PORT" && -n "$vscode_port" ]]; then
    IDF_PORT="$vscode_port"
  fi

  if [[ -z "$IDF_EXPORT_SH" ]]; then
    if [[ -n "$IDF_SETUP" ]]; then
      IDF_EXPORT_SH="$IDF_SETUP/export.sh"
    else
      IDF_EXPORT_SH="$fallback_export"
    fi
  fi

  if [[ -z "$IDF_PYTHON_BIN" && -n "$vscode_python" ]]; then
    IDF_PYTHON_BIN="$vscode_python"
  fi

  if [[ -n "$IDF_PYTHON_BIN" ]]; then
    local py_dir
    py_dir="$(dirname "$IDF_PYTHON_BIN")"
    if [[ -x "$py_dir/python" ]]; then
      IDF_PYTHON_BIN="$py_dir/python"
    fi
  fi

  if [[ -n "$IDF_SETUP" ]]; then
    IDF_PY_SCRIPT="$IDF_SETUP/tools/idf.py"
  fi

  if [[ -z "$IDF_PYTHON_BIN" && -n "$IDF_SETUP" ]]; then
    local espressif_root
    local version_dir
    espressif_root="$(dirname "$(dirname "$IDF_SETUP")")"
    version_dir="$(basename "$(dirname "$IDF_SETUP")")"
    local candidate="$espressif_root/tools/python/$version_dir/venv/bin/python3"
    local candidate_py="$espressif_root/tools/python/$version_dir/venv/bin/python"
    if [[ -x "$candidate_py" ]]; then
      IDF_PYTHON_BIN="$candidate_py"
    elif [[ -x "$candidate" ]]; then
      IDF_PYTHON_BIN="$candidate"
    fi
  fi

  if [[ -z "$IDF_TOOLS_PATH" && -n "$IDF_SETUP" ]]; then
    local espressif_root
    espressif_root="$(dirname "$(dirname "$IDF_SETUP")")"
    local tools_candidate="$espressif_root/tools"
    if [[ -d "$tools_candidate" ]]; then
      IDF_TOOLS_PATH="$tools_candidate"
    fi
  fi

  if [[ -z "$IDF_PYTHON_ENV_PATH" && -n "$IDF_PYTHON_BIN" ]]; then
    IDF_PYTHON_ENV_PATH="$(dirname "$(dirname "$IDF_PYTHON_BIN")")"
  fi

  if [[ $VERBOSE -eq 1 ]]; then
    echo "[DBG ] VSCode settings: $VSCODE_SETTINGS_PATH"
    echo "[DBG ] Resolved IDF_SETUP=$IDF_SETUP"
    echo "[DBG ] Resolved IDF_EXPORT_SH=$IDF_EXPORT_SH"
    echo "[DBG ] Resolved IDF_PYTHON_BIN=$IDF_PYTHON_BIN"
    echo "[DBG ] Resolved IDF_TOOLS_PATH=$IDF_TOOLS_PATH"
    echo "[DBG ] Resolved IDF_PYTHON_ENV_PATH=$IDF_PYTHON_ENV_PATH"
    echo "[DBG ] Resolved IDF_PY_SCRIPT=$IDF_PY_SCRIPT"
    echo "[DBG ] Resolved IDF_PORT=$IDF_PORT"
  fi

  if [[ -z "$IDF_TOOLCHAIN_PATHS" && -n "$IDF_TOOLS_PATH" && -d "$IDF_TOOLS_PATH" ]]; then
    local bins
    bins="$(find "$IDF_TOOLS_PATH" -type f -name '*-addr2line' -printf '%h\n' 2>/dev/null | sort -u | tr '\n' ':' | sed 's/:$//')"
    if [[ -n "$bins" ]]; then
      IDF_TOOLCHAIN_PATHS="$bins"
    fi
  fi

  if [[ $VERBOSE -eq 1 ]]; then
    echo "[DBG ] Resolved IDF_TOOLCHAIN_PATHS=$IDF_TOOLCHAIN_PATHS"
  fi

  if [[ -n "$IDF_PORT" ]]; then
    if [[ $FLASH_CMD_CUSTOM -eq 0 ]]; then
      FLASH_CMD="idf.py -p $IDF_PORT build flash"
    fi
    if [[ $MONITOR_CMD_CUSTOM -eq 0 ]]; then
      MONITOR_CMD="idf.py -p $IDF_PORT monitor"
    fi
  fi
}

base_url() {
  printf '%s://%s:%s' "$SCHEME" "$HOST" "$PORT"
}

curl_code() {
  local method="$1"
  local url="$2"
  local data="${3:-}"
  local -a args
  args=(--silent --show-error --location --max-time 10 --request "$method" "$url" --output /dev/null --write-out "%{http_code}")
  if [[ -n "$TOKEN" ]]; then
    args+=( -H "Authorization: Bearer $TOKEN" )
  fi
  if [[ -n "$data" ]]; then
    args+=( -H "Content-Type: application/json" --data "$data" )
  fi
  curl "${args[@]}"
}

mint_token_if_needed() {
  if [[ -n "$TOKEN" || $DRY_RUN -eq 1 ]]; then
    return
  fi
  if [[ -z "$CLIENT_ID" || -z "$CLIENT_SECRET" ]]; then
    echo "[ERR ] Missing auth: provide --token or --client-id/--client-secret" >&2
    exit 1
  fi

  local out
  out="$($REMOTE_FILES_SH --scheme "$SCHEME" --host "$HOST" --port "$PORT" --client-id "$CLIENT_ID" --client-secret "$CLIENT_SECRET" --scope "$SCOPE" auth-token)"
  TOKEN="$(printf '%s\n' "$out" | tail -n 1)"
  [[ -n "$TOKEN" ]] || { echo "[ERR ] Failed to mint token" >&2; exit 1; }
  echo "[INFO] Minted token with scope: $SCOPE"
}

run_setup_if_enabled() {
  if [[ $SKIP_SETUP -eq 1 ]]; then
    echo "[INFO] Skipping setup-engine-v2"
    return
  fi

  local -a cmd
  cmd=("$REMOTE_FILES_SH" --scheme "$SCHEME" --host "$HOST" --port "$PORT")
  if [[ -n "$TOKEN" ]]; then
    cmd+=(--token "$TOKEN")
  else
    cmd+=(--client-id "$CLIENT_ID" --client-secret "$CLIENT_SECRET" --scope "$SCOPE")
  fi
  if [[ $DRY_RUN -eq 1 ]]; then
    cmd+=(--dry-run)
  fi
  if [[ $VERBOSE -eq 1 ]]; then
    cmd+=(--verbose)
  fi
  cmd+=(setup-engine-v2 --module-id "$MODULE_ID" --versions "$VERSIONS")

  echo "[STEP] Remote setup-engine-v2"
  "${cmd[@]}"
}

issue_reset() {
  local payload
  payload="{\"mode\":\"$RESET_MODE\",\"preserve_media\":$PRESERVE_MEDIA}"

  if [[ $DRY_RUN -eq 1 ]]; then
    echo "[DRY] POST $(base_url)/api/v2/engine/session/reset body=$payload"
    return
  fi

  local code
  code="$(curl_code POST "$(base_url)/api/v2/engine/session/reset" "$payload")"
  if [[ "$code" == "409" ]]; then
    echo "[WARN] Reset returned HTTP 409 (INVALID_SESSION_STATE); continuing"
    return
  fi
  if [[ "$code" -lt 200 || "$code" -ge 300 ]]; then
    echo "[ERR ] Reset request failed with HTTP $code" >&2
    exit 1
  fi
  echo "[STEP] Reset requested (mode=$RESET_MODE preserve_media=$PRESERVE_MEDIA)"
}

wait_ready() {
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "[DRY] wait for $(base_url)/api/v2/engine/health and /api/v2/engine/status"
    return
  fi

  local start now elapsed health_code status_code
  start="$(date +%s)"

  while true; do
    health_code="$(curl_code GET "$(base_url)/api/v2/engine/health" || true)"
    status_code="$(curl_code GET "$(base_url)/api/v2/engine/status" || true)"

    if [[ "$health_code" == "200" && "$status_code" == "200" ]]; then
      echo "[STEP] API ready (health=200 status=200)"
      return
    fi

    now="$(date +%s)"
    elapsed=$((now - start))
    if (( elapsed >= WAIT_TIMEOUT )); then
      echo "[ERR ] Timed out waiting for readiness after ${WAIT_TIMEOUT}s (last health=$health_code status=$status_code)" >&2
      exit 1
    fi

    if [[ $VERBOSE -eq 1 ]]; then
      echo "[DBG ] waiting... health=$health_code status=$status_code elapsed=${elapsed}s"
    fi
    sleep "$WAIT_INTERVAL"
  done
}

run_project_cmd() {
  local cmd="$1"

  if [[ "$cmd" == *"idf.py"* && "$cmd" != *"export.sh"* ]]; then
    if [[ -f "$IDF_EXPORT_SH" ]]; then
      local export_err
      export_err="$(mktemp)"
      if (
        cd "$PROJECT_ROOT"
        PATH="$IDF_TOOLCHAIN_PATHS:$PATH" bash -lc "source \"$IDF_EXPORT_SH\" >/dev/null 2>&1 && $cmd"
      ) 2>"$export_err"; then
        rm -f "$export_err"
        return
      fi

      echo "[WARN] export.sh-based idf.py invocation failed; fallback is active"

      if [[ $VERBOSE -eq 1 ]]; then
        echo "[DBG ] trying python+idf.py fallback"
      fi
      rm -f "$export_err"
    fi

    if [[ -x "$IDF_PYTHON_BIN" && -f "$IDF_PY_SCRIPT" ]]; then
      local rewritten
      rewritten="${cmd//idf.py/\"$IDF_PYTHON_BIN\" \"$IDF_PY_SCRIPT\"}"
      (
        cd "$PROJECT_ROOT"
        PATH="$IDF_TOOLCHAIN_PATHS:$PATH" \
        IDF_PATH="$IDF_SETUP" \
        IDF_TOOLS_PATH="$IDF_TOOLS_PATH" \
        IDF_PYTHON_ENV_PATH="$IDF_PYTHON_ENV_PATH" \
        bash -lc "$rewritten"
      )
      return
    fi

    echo "[ERR ] Unable to run idf.py command. Checked export and fallback paths:" >&2
    echo "[ERR ]   IDF_EXPORT_SH=$IDF_EXPORT_SH" >&2
    echo "[ERR ]   IDF_PYTHON_BIN=$IDF_PYTHON_BIN" >&2
    echo "[ERR ]   IDF_TOOLS_PATH=$IDF_TOOLS_PATH" >&2
    echo "[ERR ]   IDF_PYTHON_ENV_PATH=$IDF_PYTHON_ENV_PATH" >&2
    echo "[ERR ]   IDF_TOOLCHAIN_PATHS=$IDF_TOOLCHAIN_PATHS" >&2
    echo "[ERR ]   IDF_PY_SCRIPT=$IDF_PY_SCRIPT" >&2
    exit 1
  fi

  (
    cd "$PROJECT_ROOT"
    bash -lc "$cmd"
  )
}

flash_before_monitor_if_enabled() {
  if [[ $FLASH_BEFORE_MONITOR -eq 0 ]]; then
    return
  fi

  if [[ $DRY_RUN -eq 1 ]]; then
    if [[ "$FLASH_CMD" == *"idf.py"* && "$FLASH_CMD" != *"export.sh"* ]]; then
      if [[ -f "$IDF_EXPORT_SH" ]]; then
        echo "[DRY] run flash in $PROJECT_ROOT: source $IDF_EXPORT_SH && $FLASH_CMD"
      elif [[ -x "$IDF_PYTHON_BIN" && -f "$IDF_PY_SCRIPT" ]]; then
        echo "[DRY] run flash in $PROJECT_ROOT: ${FLASH_CMD//idf.py/$IDF_PYTHON_BIN $IDF_PY_SCRIPT}"
      else
        echo "[DRY] run flash in $PROJECT_ROOT: $FLASH_CMD"
      fi
    else
      echo "[DRY] run flash in $PROJECT_ROOT: $FLASH_CMD"
    fi
    return
  fi

  echo "[STEP] Flashing firmware: $FLASH_CMD"
  run_project_cmd "$FLASH_CMD"
}

launch_monitor_if_enabled() {
  if [[ $MONITOR -eq 0 ]]; then
    return
  fi

  if [[ $DRY_RUN -eq 1 ]]; then
    if [[ "$MONITOR_CMD" == *"idf.py"* && "$MONITOR_CMD" != *"export.sh"* ]]; then
      if [[ -f "$IDF_EXPORT_SH" ]]; then
        echo "[DRY] launch monitor in $PROJECT_ROOT: source $IDF_EXPORT_SH && $MONITOR_CMD"
      elif [[ -x "$IDF_PYTHON_BIN" && -f "$IDF_PY_SCRIPT" ]]; then
        echo "[DRY] launch monitor in $PROJECT_ROOT: ${MONITOR_CMD//idf.py/$IDF_PYTHON_BIN $IDF_PY_SCRIPT}"
      else
        echo "[DRY] launch monitor in $PROJECT_ROOT: $MONITOR_CMD"
      fi
    else
      echo "[DRY] launch monitor in $PROJECT_ROOT: $MONITOR_CMD"
    fi
    return
  fi

  echo "[STEP] Launching monitor: $MONITOR_CMD"
  run_project_cmd "$MONITOR_CMD"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --scheme) SCHEME="$2"; shift 2 ;;
    --token) TOKEN="$2"; shift 2 ;;
    --client-id) CLIENT_ID="$2"; shift 2 ;;
    --client-secret) CLIENT_SECRET="$2"; shift 2 ;;
    --scope) SCOPE="$2"; shift 2 ;;
    --idf-export-sh) IDF_EXPORT_SH="$2"; shift 2 ;;
    --idf-port) IDF_PORT="$2"; shift 2 ;;

    --versions) VERSIONS="$2"; shift 2 ;;
    --module-id) MODULE_ID="$2"; shift 2 ;;
    --skip-setup) SKIP_SETUP=1; shift ;;
    --quick)
      SKIP_SETUP=1
      FLASH_BEFORE_MONITOR=1
      MONITOR=1
      shift
      ;;

    --reset-mode)
      [[ "$2" == "warm" || "$2" == "cold" ]] || { echo "[ERR ] --reset-mode must be warm|cold" >&2; exit 1; }
      RESET_MODE="$2"
      shift 2
      ;;
    --preserve-media)
      [[ "$2" == "true" || "$2" == "false" ]] || { echo "[ERR ] --preserve-media must be true|false" >&2; exit 1; }
      PRESERVE_MEDIA="$2"
      shift 2
      ;;
    --wait-timeout) WAIT_TIMEOUT="$2"; shift 2 ;;
    --wait-interval) WAIT_INTERVAL="$2"; shift 2 ;;
    --flash-before-monitor) FLASH_BEFORE_MONITOR=1; shift ;;
    --flash-cmd) FLASH_CMD="$2"; FLASH_CMD_CUSTOM=1; shift 2 ;;
    --monitor) MONITOR=1; shift ;;
    --monitor-cmd) MONITOR_CMD="$2"; MONITOR_CMD_CUSTOM=1; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --verbose) VERBOSE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "[ERR ] Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

[[ -x "$REMOTE_FILES_SH" ]] || {
  echo "[ERR ] Missing executable dependency: $REMOTE_FILES_SH" >&2
  exit 1
}

resolve_idf_paths

echo "[INFO] Target: $(base_url)"
mint_token_if_needed
run_setup_if_enabled
issue_reset
wait_ready
flash_before_monitor_if_enabled
launch_monitor_if_enabled
echo "[DONE] Remote prep/reset cycle complete. You can run monitor/API tests now."
