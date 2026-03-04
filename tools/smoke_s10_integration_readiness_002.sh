#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s10_integration_readiness_002_${TS}.txt"
JSON_OUT="captures/s10_integration_readiness_002_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$ADMIN_TOKEN" "$OUT" "$JSON_OUT" "$TS"
import json
import subprocess
import sys
import time

base = sys.argv[1]
admin_token = sys.argv[2]
out = sys.argv[3]
json_out = sys.argv[4]
run_id = sys.argv[5]


def log(line: str):
    with open(out, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)


def call(method: str, path: str, headers=None, body=None, expected=None):
    cmd = [
        "curl", "--max-time", "20", "-sS", "-w", "\n%{http_code}\n%{time_total}",
        "-X", method, base + path,
    ]
    for k, v in (headers or {}).items():
        cmd += ["-H", f"{k}: {v}"]
    if body is not None:
        cmd += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    out_raw = subprocess.check_output(cmd, text=True)
    raw, code_s, elapsed_s = out_raw.rsplit("\n", 2)
    code = int(code_s)
    elapsed = float(elapsed_s)
    try:
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        data = {"_raw": raw.strip()}
    ok = expected is None or code in expected
    return ok, code, elapsed, data


log("s10_002_smoke_runner=start")
log(f"base_url={base}")

headers = {"Authorization": f"Bearer {admin_token}"}

ok_health, code_health, t_health, data_health = call("GET", "/api/v2/engine/health", expected={200})
health_observed = ok_health and isinstance(data_health, dict) and data_health.get("ok") is True

ok_video, code_video, t_video, _ = call("GET", "/api/v2/stream/video?session_id=ses_local", headers=headers, expected={200, 409, 400})
ok_audio, code_audio, t_audio, _ = call("GET", "/api/v2/stream/audio?session_id=ses_local", headers=headers, expected={200, 409, 400})
stream_observed = ok_video and ok_audio

ok_debug, code_debug, t_debug, _ = call("GET", "/api/v2/debug/clock/state", headers=headers, expected={200, 409, 400})
debug_observed = ok_debug

result = {
    "task_id": "S10-002",
    "run_id": run_id,
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "status_model": ["baseline_observed", "needs_data", "blocked_by_missing_engine"],
    "checks": [
        {
            "check_id": "S10-002-CHK-HEALTH",
            "status": "baseline_observed" if health_observed else "needs_data",
            "signal": "Engine health endpoint reachable and returns deterministic payload",
            "metrics": {"http": code_health, "elapsed_ms": round(t_health * 1000, 3)}
        },
        {
            "check_id": "S10-002-CHK-STREAM",
            "status": "baseline_observed" if stream_observed else "needs_data",
            "signal": "Video/audio stream probe path executes with deterministic response envelope",
            "metrics": {
                "video_http": code_video,
                "audio_http": code_audio,
                "video_elapsed_ms": round(t_video * 1000, 3),
                "audio_elapsed_ms": round(t_audio * 1000, 3)
            }
        },
        {
            "check_id": "S10-002-CHK-DEBUG",
            "status": "baseline_observed" if debug_observed else "needs_data",
            "signal": "Debug clock state probe path executes",
            "metrics": {"http": code_debug, "elapsed_ms": round(t_debug * 1000, 3)}
        },
        {
            "check_id": "S10-002-FX-REG",
            "status": "blocked_by_missing_engine",
            "signal": "Register snapshot/stream parity fixture placeholder",
            "metrics": {"reason": "register stream parity path not yet implemented in emulated-hardware phase"}
        },
        {
            "check_id": "S10-002-FX-BUS",
            "status": "blocked_by_missing_engine",
            "signal": "Bus/memory filter fixture placeholder",
            "metrics": {"reason": "bus/memory filter runtime parity path not yet implemented in emulated-hardware phase"}
        },
        {
            "check_id": "S10-002-FX-SD",
            "status": "blocked_by_missing_engine",
            "signal": "SD-only enforcement fixture placeholder",
            "metrics": {"reason": "full SD-only runtime media resolver path not yet implemented for integration phase"}
        }
    ]
}

with open(json_out, "w", encoding="utf-8") as f:
    json.dump(result, f, indent=2)

counts = {
    "baseline_observed": sum(1 for c in result["checks"] if c["status"] == "baseline_observed"),
    "needs_data": sum(1 for c in result["checks"] if c["status"] == "needs_data"),
    "blocked_by_missing_engine": sum(1 for c in result["checks"] if c["status"] == "blocked_by_missing_engine"),
}

log(f"summary={json.dumps(counts, sort_keys=True)}")
log(f"json_path={json_out}")
PY

echo "S10-002 smoke COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence json: $JSON_OUT"
