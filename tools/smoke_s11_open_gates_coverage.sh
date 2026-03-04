#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TARGET_GATE=""

if [[ "${1:-}" == "--gate" ]]; then
  TARGET_GATE="${2:-}"
fi

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_open_gates_coverage_${TS}.txt"
JSON_OUT="captures/s11_open_gates_coverage_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$ADMIN_TOKEN" "$TARGET_GATE" "$OUT" "$JSON_OUT" "$TS"
import json
import subprocess
import sys
import urllib.parse

BASE = sys.argv[1]
TOKEN = sys.argv[2]
TARGET_GATE = sys.argv[3]
OUT = sys.argv[4]
JSON_OUT = sys.argv[5]
RUN_ID = sys.argv[6]


def log(line: str):
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)


def call(method, path, body=None, headers=None):
    cmd = ["curl", "--max-time", "12", "-sS", "-w", "\n%{http_code}", "-X", method, BASE + path]
    for k, v in (headers or {}).items():
        cmd += ["-H", f"{k}: {v}"]
    if body is not None:
        cmd += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    raw = subprocess.check_output(cmd, text=True)
    if "\n" in raw:
        body_raw, code_raw = raw.rsplit("\n", 1)
    else:
        body_raw, code_raw = raw, "000"
    try:
        code = int(code_raw)
    except Exception:
        code = 0
    try:
        data = json.loads(body_raw) if body_raw.strip() else {}
    except Exception:
        data = {"_raw": body_raw.strip()}
    return code, data


def err_code(payload):
    return (payload.get("error") or {}).get("code") if isinstance(payload, dict) else None


def route_exists(code: int) -> bool:
    return code not in (0, 404, 501)


def gate_02():
    checks = []
    auth = {"Authorization": f"Bearer {TOKEN}"}
    c1, b1 = call("GET", "/api/v2/files/stat?path=" + urllib.parse.quote("/etc/passwd"), headers=auth)
    checks.append({"name": "outside_allowlist_rejected", "http": c1, "code": err_code(b1), "ok": c1 == 400 and err_code(b1) == "PATH_NOT_ALLOWED"})

    c2, b2 = call("GET", "/api/v2/files/stat?path=" + urllib.parse.quote("/sdcard/roms"), headers=auth)
    checks.append({"name": "sd_path_probe_reachable", "http": c2, "code": err_code(b2), "ok": route_exists(c2)})

    status = "pass" if all(x["ok"] for x in checks) else "conditional"
    return status, checks


def first_existing(candidates):
    for item in candidates:
        code, body = call(item[0], item[1], item[2] if len(item) > 2 else None, item[3] if len(item) > 3 else None)
        if route_exists(code):
            return {"candidate": item[1], "http": code, "code": err_code(body), "ok": True}
    # return last attempted as failure detail
    item = candidates[-1]
    code, body = call(item[0], item[1], item[2] if len(item) > 2 else None, item[3] if len(item) > 3 else None)
    return {"candidate": item[1], "http": code, "code": err_code(body), "ok": False}


def gate_05():
    auth = {"Authorization": f"Bearer {TOKEN}"}
    snapshot = first_existing([
        ("GET", "/api/v2/inspect/registers/snapshot?session_id=ses_local", None, auth),
        ("GET", "/api/v2/registers/snapshot?session_id=ses_local", None, auth),
    ])
    stream = first_existing([
        ("GET", "/api/v2/inspect/registers/stream?session_id=ses_local", None, auth),
        ("GET", "/api/v2/registers/stream?session_id=ses_local", None, auth),
    ])
    status = "pass" if snapshot["ok"] and stream["ok"] else "fail"
    return status, [snapshot, stream]


def gate_06():
    auth = {"Authorization": f"Bearer {TOKEN}"}
    bus = first_existing([
        ("GET", "/api/v2/inspect/bus/stream?session_id=ses_local", None, auth),
        ("GET", "/api/v2/inspect/trace/bus?session_id=ses_local", None, auth),
    ])
    mem = first_existing([
        ("GET", "/api/v2/inspect/memory/stream?session_id=ses_local", None, auth),
        ("GET", "/api/v2/inspect/trace/memory?session_id=ses_local", None, auth),
    ])
    status = "pass" if bus["ok"] and mem["ok"] else "fail"
    return status, [bus, mem]


def gate_07():
    auth = {"Authorization": f"Bearer {TOKEN}"}
    mappings = first_existing([
        ("GET", "/api/v2/input/mappings", None, auth),
        ("GET", "/api/v2/input/mappings/list", None, auth),
    ])
    translate = first_existing([
        ("POST", "/api/v2/input/translate", {"session_id": "ses_local", "device": "keyboard", "event": {"key": "A", "state": "down"}}, auth),
        ("POST", "/api/v2/input/events", {"session_id": "ses_local", "device": "keyboard", "event": {"key": "A", "state": "down"}}, auth),
    ])
    status = "pass" if mappings["ok"] and translate["ok"] else "conditional"
    return status, [mappings, translate]


def gate_09():
    auth = {"Authorization": f"Bearer {TOKEN}"}
    capture_get = first_existing([
        ("GET", "/api/v2/input/capture/policy", None, auth),
        ("GET", "/api/v2/input/capture", None, auth),
    ])
    capture_set = first_existing([
        ("POST", "/api/v2/input/capture/policy", {"enabled": True, "mode": "click_to_capture"}, auth),
        ("POST", "/api/v2/input/capture", {"enabled": True, "mode": "click_to_capture"}, auth),
    ])
    status = "pass" if capture_get["ok"] and capture_set["ok"] else "conditional"
    return status, [capture_get, capture_set]


def gate_10():
    list_probe = first_existing([
        ("GET", "/api/v2/catalogs/list"),
    ])
    missing_probe = first_existing([
        ("GET", "/api/v2/catalogs/floppy_catalog/missing-report"),
        ("GET", "/api/v2/catalogs/rom_catalog/missing-report"),
    ])
    status = "pass" if list_probe["ok"] and missing_probe["ok"] else "conditional"
    return status, [list_probe, missing_probe]


def gate_12():
    suspend = first_existing([
        ("POST", "/api/v2/engine/session/suspend-save", {"session_id": "ses_local", "snapshot_id": "s11_probe"}),
        ("POST", "/api/v2/snapshots/suspend-save", {"session_id": "ses_local", "snapshot_id": "s11_probe"}),
    ])
    validate = first_existing([
        ("POST", "/api/v2/engine/session/restore/validate", {"snapshot_id": "s11_probe", "strict": True}),
        ("POST", "/api/v2/snapshots/restore/validate", {"snapshot_id": "s11_probe", "strict": True}),
    ])
    status = "pass" if suspend["ok"] and validate["ok"] else "conditional"
    return status, [suspend, validate]


def gate_13():
    perf = first_existing([
        ("GET", "/api/v2/metrics/performance"),
    ])
    samples = first_existing([
        ("GET", "/api/v2/metrics/performance/samples?limit=20"),
    ])
    thresholds = first_existing([
        ("GET", "/api/v2/metrics/performance/thresholds"),
    ])
    # S11 requires sustained-window hard-threshold validation; route reachability alone is insufficient.
    status = "conditional"
    if perf["ok"] and samples["ok"] and thresholds["ok"]:
        status = "conditional"
    return status, [perf, samples, thresholds]


gates = {
    "GATE-S11-02": gate_02,
    "GATE-S11-05": gate_05,
    "GATE-S11-06": gate_06,
    "GATE-S11-07": gate_07,
    "GATE-S11-09": gate_09,
    "GATE-S11-10": gate_10,
    "GATE-S11-12": gate_12,
    "GATE-S11-13": gate_13,
}

selected = [TARGET_GATE] if TARGET_GATE else list(gates.keys())
results = []
for gate in selected:
    if gate not in gates:
        continue
    status, checks = gates[gate]()
    results.append({"gate_id": gate, "status": status, "checks": checks})
    log(f"{gate}::{status}")

summary = {
    "pass_count": sum(1 for x in results if x["status"] == "pass"),
    "conditional_count": sum(1 for x in results if x["status"] == "conditional"),
    "fail_count": sum(1 for x in results if x["status"] == "fail"),
}

bundle = {
    "task": "S11-open-gates-test-set",
    "run_id": RUN_ID,
    "base_url": BASE,
    "target_gate": TARGET_GATE or "all",
    "results": results,
    "summary": summary,
}

with open(JSON_OUT, "w", encoding="utf-8") as f:
    json.dump(bundle, f, indent=2)

log("summary=" + json.dumps(summary, sort_keys=True))
log("json_path=" + JSON_OUT)
PY

echo "S11 open gates coverage COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
