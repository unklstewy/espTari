#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s9_002_postflash_probe_${TS}.txt"
BUNDLE="captures/s9_002_postflash_probe_bundle_${TS}.json"
mkdir -p captures

python - <<'PY' "$BASE_URL" "$OUT" "$BUNDLE"
import json
import subprocess
import sys
import time
import urllib.parse

BASE = sys.argv[1]
OUT = sys.argv[2]
BUNDLE = sys.argv[3]

results = []
health_attempts = 0

def log(line: str):
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)

def call(method, path, body=None, headers=None):
    cmd = ["curl", "--max-time", "12", "-sS", "-w", "\n%{http_code}", "-X", method, BASE + path]
    hdrs = headers or {}
    for k, v in hdrs.items():
        cmd += ["-H", f"{k}: {v}"]
    if body is not None:
        cmd += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    out = subprocess.check_output(cmd, text=True)
    if "\n" in out:
        b, code = out.rsplit("\n", 1)
    else:
        b, code = out, "000"
    try:
        data = json.loads(b) if b.strip() else {}
    except Exception:
        data = {"_raw": b.strip()}
    return int(code), data

def err_code(data):
    return (data.get("error") or {}).get("code")

def rec(cid, desc, ok, detail):
    results.append({
        "control": cid,
        "check": desc,
        "status": "pass" if ok else "fail",
        "detail": detail,
    })
    log(f"{cid} :: {desc} :: {'PASS' if ok else 'FAIL'} :: {detail}")

admin_headers = {"Authorization": "Bearer esptari-admin-token"}

# Health loop at top (boot/network grace period)
log("health_check_loop=start")
max_attempts = 60
for i in range(1, max_attempts + 1):
    health_attempts = i
    try:
        c, b = call("GET", "/api/v2/engine/health")
        if c == 200 and b.get("ok") is True:
            log(f"health_check=ok attempts={i}")
            break
    except Exception:
        pass
    time.sleep(1)
else:
    log(f"health_check=timeout attempts={max_attempts}")
    report = {
        "task": "S9-002",
        "runtime_probe": time.strftime("%Y-%m-%d"),
        "controls": {},
        "checks": [],
        "fatal": "Health check loop timed out",
        "health_attempts": health_attempts,
    }
    with open(BUNDLE, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    sys.exit(2)

# AUTH-01: unauthorized without bearer on protected route
c, b = call("GET", "/api/v2/files/list?path=" + urllib.parse.quote("/sdcard/roms"))
rec("AUTH-01", "missing bearer -> UNAUTHORIZED", c == 401 and err_code(b) == "UNAUTHORIZED", {"http": c, "code": err_code(b)})

# AUTH-02: wrong-scope token on ebin manage route
headers = {"Authorization": "Bearer esptari-files-token"}
c, b = call("POST", "/api/v2/ebins/resolve", {"machine": "atari_st", "components": ["cpu"], "version_policy": "latest_compatible"}, headers)
rec("AUTH-02", "insufficient scope -> FORBIDDEN", c == 403 and err_code(b) == "FORBIDDEN", {"http": c, "code": err_code(b)})

# PATH-01: outside allowlist rejected
c, b = call("GET", "/api/v2/files/stat?path=" + urllib.parse.quote("/etc/passwd"), headers=admin_headers)
rec("PATH-01", "outside SD allowlist rejected", c == 400 and err_code(b) == "PATH_NOT_ALLOWED", {"http": c, "code": err_code(b)})

# PATH-02: traversal/mixed separators rejected
c1, b1 = call("GET", "/api/v2/files/stat?path=" + urllib.parse.quote("/sdcard/roms/../secret"), headers=admin_headers)
ok1 = c1 == 400 and err_code(b1) in ("PATH_NOT_ALLOWED", "BAD_REQUEST")
rec("PATH-02", "traversal segment rejected", ok1, {"http": c1, "code": err_code(b1)})

c2, b2 = call("GET", "/api/v2/files/stat?path=" + urllib.parse.quote("/sdcard/roms\\..\\evil"), headers=admin_headers)
ok2 = c2 == 400 and err_code(b2) in ("BAD_REQUEST", "PATH_NOT_ALLOWED")
rec("PATH-02", "mixed separator rejected", ok2, {"http": c2, "code": err_code(b2)})

# UPLOAD-02: staged upload controls
c, b = call("POST", "/api/v2/files/upload", {"op": "start", "path": "/sdcard/ebins/test.bin", "chunk_timeout_ms": 100, "max_chunk_bytes": 256}, headers=admin_headers)
start_ok = c == 202 and "uploadId" in b
upload_id = b.get("uploadId") if isinstance(b, dict) else None
rec("UPLOAD-02", "start staged upload accepted", start_ok, {"http": c, "code": err_code(b), "uploadId": upload_id})

if upload_id:
    c, b = call("POST", "/api/v2/files/upload", {"op": "chunk", "upload_id": upload_id, "chunk_index": 0, "chunk_size": 999}, headers=admin_headers)
    rec("UPLOAD-02", "oversize chunk rejected", c == 413 and err_code(b) == "CHUNK_TOO_LARGE", {"http": c, "code": err_code(b)})

    c, b = call("POST", "/api/v2/files/upload", {"op": "start", "path": "/sdcard/ebins/test2.bin", "chunk_timeout_ms": 100, "max_chunk_bytes": 256}, headers=admin_headers)
    uid2 = b.get("uploadId") if isinstance(b, dict) else None
    if c == 202 and uid2:
        time.sleep(0.22)
        c3, b3 = call("POST", "/api/v2/files/upload", {"op": "chunk", "upload_id": uid2, "chunk_index": 0, "chunk_size": 64}, headers=admin_headers)
        rec("UPLOAD-02", "chunk timeout enforced", c3 == 408 and err_code(b3) == "CHUNK_TIMEOUT", {"http": c3, "code": err_code(b3)})
    else:
        rec("UPLOAD-02", "chunk timeout enforced", False, {"http_start": c, "start_code": err_code(b)})

# EBIN-02 runtime path proxy checks
c, b = call("POST", "/api/v2/ebins/validate", {"module_type": "cpu"}, headers=admin_headers)
rec("EBIN-02", "validation failure path reachable", c in (400, 409), {"http": c, "code": err_code(b)})

c, b = call("POST", "/api/v2/engine/session/pause", {})
rec("EBIN-02", "session transition path reachable", c in (200, 409), {"http": c, "code": err_code(b)})

summary = {}
for r in results:
    summary.setdefault(r["control"], []).append(r)
control_status = {k: ("pass" if all(x["status"] == "pass" for x in v) else "fail") for k, v in summary.items()}

report = {
    "task": "S9-002",
    "runtime_probe": time.strftime("%Y-%m-%d"),
    "health_attempts": health_attempts,
    "controls": control_status,
    "checks": results,
}

with open(BUNDLE, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)

log("postflash_probe_bundle=" + BUNDLE)
log("controls=" + json.dumps(control_status, sort_keys=True))
PY

echo "Probe output: $OUT"
echo "Probe bundle: $BUNDLE"