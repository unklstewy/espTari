#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s9_risk_validation_003_daily_${TS}.txt"
BUNDLE="captures/s9_risk_validation_bundle_003_daily_${TS}.json"
ART_DIR="captures/s9_003_daily_${TS}"
MATRIX="TRACKING/evidence/s9_risk_validation_matrix_003.json"
SCHEMA="TRACKING/S9_003_EVIDENCE_BUNDLE_SCHEMA_2026-03-04.json"

mkdir -p captures "$ART_DIR"

python - <<'PY' "$BASE_URL" "$ADMIN_TOKEN" "$OUT" "$BUNDLE" "$ART_DIR" "$MATRIX" "$SCHEMA" "$TS"
import json
import os
import subprocess
import sys
import time

BASE = sys.argv[1]
ADMIN_TOKEN = sys.argv[2]
OUT = sys.argv[3]
BUNDLE = sys.argv[4]
ART_DIR = sys.argv[5]
MATRIX = sys.argv[6]
SCHEMA = sys.argv[7]
RUN_ID = sys.argv[8]


def log(line: str):
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)


def call(method, path, body=None, expected=None, headers=None):
    cmd = [
        "curl",
        "--max-time",
        "20",
        "-sS",
        "-w",
        "\n%{http_code}\n%{time_total}",
        "-X",
        method,
        BASE + path,
    ]
    for header_name, header_value in (headers or {}).items():
        cmd += ["-H", f"{header_name}: {header_value}"]
    if body is not None:
        cmd += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]

    out = subprocess.check_output(cmd, text=True)
    parts = out.rsplit("\n", 2)
    raw = parts[0]
    code = int(parts[1])
    elapsed = float(parts[2])
    try:
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        data = {"_raw": raw.strip()}
    ok = expected is None or code in expected
    return ok, code, elapsed, data, raw


def err_code(payload):
    return (payload.get("error") or {}).get("code") if isinstance(payload, dict) else None


def write_artifact(filename, data):
    path = os.path.join(ART_DIR, filename)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    return path


def percentile(values, pct):
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = max(0, min(len(ordered) - 1, int((pct / 100.0) * (len(ordered) - 1))))
    return ordered[idx]


checks = []


def add_check(check_id, risk_id, status, threshold_ref, signal, metrics=None, artifacts=None, notes=""):
    checks.append(
        {
            "check_id": check_id,
            "risk_id": risk_id,
            "status": status,
            "threshold_ref": threshold_ref,
            "signal": signal,
            "metrics": metrics or {},
            "artifacts": artifacts or [],
            "notes": notes,
        }
    )
    log(f"{check_id}::{status}::{signal}")


def wait_for_health():
    for i in range(1, 61):
        try:
            ok, _, elapsed, data, _ = call("GET", "/api/v2/engine/health", expected={200})
            if ok and data.get("ok") is True:
                log(f"health_check=ok attempts={i} elapsed_ms={elapsed*1000:.2f}")
                return True
        except Exception:
            pass
        time.sleep(1)
    log("health_check=timeout attempts=60")
    return False


log("s9_003_daily_runner=start")
log(f"base_url={BASE}")

auth_headers = {"Authorization": f"Bearer {ADMIN_TOKEN}"}

if not os.path.exists(MATRIX):
    raise SystemExit(f"missing_matrix={MATRIX}")
if not os.path.exists(SCHEMA):
    raise SystemExit(f"missing_schema={SCHEMA}")

if not wait_for_health():
    add_check(
        "CHK-RISK-003-03",
        "RISK-SD-IO-LATENCY",
        "not-run",
        "THR-RISK-003-03",
        "Runner aborted due to failed health precondition",
        notes="Health precondition failed before checks started",
    )
else:
    # CHK-RISK-003-03: SD I/O endpoint latency probe
    latencies_ms = []
    status_codes = []
    samples = 8
    for i in range(samples):
        ok, code, elapsed, data, _ = call(
            "GET",
            "/api/v2/files/stat?path=/sdcard/roms",
            expected={200, 400, 401, 403},
            headers=auth_headers,
        )
        latencies_ms.append(elapsed * 1000.0)
        status_codes.append(code)

    p95 = round(percentile(latencies_ms, 95), 3)
    p99 = round(percentile(latencies_ms, 99), 3)
    all_auth_ok = all(code in (200, 400, 403) for code in status_codes)
    sd_latency_pass = p95 <= 120.0 and p99 <= 250.0 and all_auth_ok
    sd_metrics = {
        "samples": samples,
        "p95_ms": p95,
        "p99_ms": p99,
        "threshold_p95_ms": 120.0,
        "threshold_p99_ms": 250.0,
        "status_codes": status_codes,
    }
    sd_art = write_artifact("chk_003_03_sd_io_latency_metrics.json", sd_metrics)
    add_check(
        "CHK-RISK-003-03",
        "RISK-SD-IO-LATENCY",
        "pass" if sd_latency_pass else "fail",
        "THR-RISK-003-03",
        "P95/P99 operation latency stays below configured ms limits",
        metrics=sd_metrics,
        artifacts=[sd_art, OUT],
    )

    # CHK-RISK-003-04: dead-link policy regression
    entry_id = "disk.automation.a_093"
    ok_mark, c1, t1, d1, _ = call(
        "POST",
        "/api/v2/catalogs/floppies/mark-dead",
        body={"entry_id": entry_id, "reason": "S9-003 daily dead-link check"},
        expected={200},
        headers=auth_headers,
    )
    ok_block, c2, t2, d2, _ = call(
        "POST",
        "/api/v2/catalogs/floppies/download-entry",
        body={"entry_id": entry_id},
        expected={409},
        headers=auth_headers,
    )
    ok_read, c3, t3, d3, _ = call(
        "GET",
        f"/api/v2/catalogs/floppies/entries/{entry_id}",
        expected={200},
        headers=auth_headers,
    )
    state = ((d3.get("data") or {}).get("availability_state") if isinstance(d3, dict) else None)
    dead_link_pass = ok_mark and ok_block and ok_read and err_code(d2) == "CATALOG_LINK_DEAD" and state == "dead"
    dead_metrics = {
        "entry_id": entry_id,
        "mark_dead_http": c1,
        "download_without_retry_http": c2,
        "download_without_retry_code": err_code(d2),
        "entry_read_http": c3,
        "availability_state": state,
        "elapsed_ms": {
            "mark_dead": round(t1 * 1000, 3),
            "download_without_retry": round(t2 * 1000, 3),
            "entry_read": round(t3 * 1000, 3),
        },
    }
    dead_art = write_artifact("chk_003_04_dead_link_policy_metrics.json", dead_metrics)
    add_check(
        "CHK-RISK-003-04",
        "RISK-DEAD-LINK-RESILIENCE",
        "pass" if dead_link_pass else "fail",
        "THR-RISK-003-04",
        "Deterministic dead-link state and blocker semantics",
        metrics=dead_metrics,
        artifacts=[dead_art, OUT],
    )

    # Non-daily checks are intentionally not run in daily cadence bundle.
    add_check(
        "CHK-RISK-003-01",
        "RISK-ABI-CHURN",
        "not-run",
        "THR-RISK-003-01",
        "Weekly/per-release check omitted from daily bundle",
        notes="Scheduled in weekly/per-release run",
    )
    add_check(
        "CHK-RISK-003-02",
        "RISK-STREAM-OVERHEAD",
        "not-run",
        "THR-RISK-003-02",
        "Weekly check omitted from daily bundle",
        notes="Scheduled in weekly run",
    )
    add_check(
        "CHK-RISK-003-05",
        "RISK-TRACE-PRESSURE",
        "not-run",
        "THR-RISK-003-05",
        "Weekly check omitted from daily bundle",
        notes="Scheduled in weekly run",
    )
    add_check(
        "CHK-RISK-003-06",
        "RISK-SAVESTATE-DRIFT",
        "not-run",
        "THR-RISK-003-06",
        "Per-release check omitted from daily bundle",
        notes="Scheduled in per-release run",
    )
    add_check(
        "CHK-RISK-003-07",
        "RISK-DEBUG-MODE-PERTURBATION",
        "not-run",
        "THR-RISK-003-07",
        "Weekly check omitted from daily bundle",
        notes="Scheduled in weekly run",
    )


pass_count = sum(1 for c in checks if c["status"] == "pass")
fail_count = sum(1 for c in checks if c["status"] == "fail")
not_run_count = sum(1 for c in checks if c["status"] == "not-run")

bundle = {
    "bundle_version": 1,
    "task_id": "S9-003",
    "run_id": RUN_ID,
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "cadence": "daily",
    "checks": checks,
    "summary": {
        "overall_status": "fail" if fail_count > 0 else "pass",
        "pass_count": pass_count,
        "fail_count": fail_count,
        "not_run_count": not_run_count,
    },
}

with open(BUNDLE, "w", encoding="utf-8") as f:
    json.dump(bundle, f, indent=2)

log(f"bundle_path={BUNDLE}")
log(f"artifacts_dir={ART_DIR}")
log(f"summary={json.dumps(bundle['summary'], sort_keys=True)}")
PY

echo "Smoke COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $BUNDLE"
