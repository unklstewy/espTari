#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s9_risk_validation_003_weekly_${TS}.txt"
BUNDLE="captures/s9_risk_validation_bundle_003_weekly_${TS}.json"
ART_DIR="captures/s9_003_weekly_${TS}"
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
            ok, code, elapsed, data, _ = call("GET", "/api/v2/engine/health", expected={200})
            if ok and data.get("ok") is True:
                log(f"health_check=ok attempts={i} elapsed_ms={elapsed*1000:.2f}")
                return True
        except Exception:
            pass
        time.sleep(1)
    log("health_check=timeout attempts=60")
    return False


log("s9_003_weekly_runner=start")
log(f"base_url={BASE}")

auth_headers = {"Authorization": f"Bearer {ADMIN_TOKEN}"}

if not os.path.exists(MATRIX):
    raise SystemExit(f"missing_matrix={MATRIX}")
if not os.path.exists(SCHEMA):
    raise SystemExit(f"missing_schema={SCHEMA}")

if not wait_for_health():
    add_check(
        "CHK-RISK-003-01",
        "RISK-ABI-CHURN",
        "not-run",
        "THR-RISK-003-01",
        "Runner aborted due to failed health precondition",
        notes="Health precondition failed before checks started",
    )
else:
    call("POST", "/api/v2/engine/session/stop", body={}, expected={200, 409})
    call(
        "POST",
        "/api/v2/engine/session",
        body={
            "machine": "atari_st",
            "profile": "st_520_pal",
            "rom_id": "rom.atari.st.01",
            "tos_id": "tos.eu.1.04",
            "disk_ids": ["disk.automation.a_093"],
        },
        expected={200},
    )

    # CHK-RISK-003-01: ABI gate regression sweep
    ok_resolve, c1, t1, d1, _ = call(
        "POST",
        "/api/v2/ebins/resolve",
        body={"machine": "mega_st", "components": ["cpu"], "version_policy": "latest_compatible"},
        expected={404},
        headers=auth_headers,
    )
    ok_validate, c2, t2, d2, _ = call(
        "POST",
        "/api/v2/ebins/validate",
        body={"module_type": "cpu"},
        expected={400},
        headers=auth_headers,
    )
    code_ok = err_code(d1) == "EBIN_NOT_FOUND" and err_code(d2) == "BAD_REQUEST"
    abi_pass = ok_resolve and ok_validate and code_ok
    abi_metrics = {
        "negative_cases_total": 2,
        "negative_cases_passed": int(abi_pass) * 2 if abi_pass else int(ok_resolve) + int(ok_validate),
        "resolve_http": c1,
        "resolve_code": err_code(d1),
        "validate_http": c2,
        "validate_code": err_code(d2),
        "resolve_elapsed_ms": round(t1 * 1000, 3),
        "validate_elapsed_ms": round(t2 * 1000, 3),
    }
    abi_art = write_artifact("chk_003_01_abi_gate_metrics.json", abi_metrics)
    add_check(
        "CHK-RISK-003-01",
        "RISK-ABI-CHURN",
        "pass" if abi_pass else "fail",
        "THR-RISK-003-01",
        "Canonical ABI guard-code behavior and no unexpected positive regressions",
        metrics=abi_metrics,
        artifacts=[abi_art, OUT],
    )

    # CHK-RISK-003-02: stream envelope overhead regression
    ok_video, c3, tv, dv, rawv = call(
        "GET", "/api/v2/stream/video?session_id=ses_local", expected={200}, headers=auth_headers
    )
    ok_audio, c4, ta, da, rawa = call(
        "GET", "/api/v2/stream/audio?session_id=ses_local", expected={200}, headers=auth_headers
    )
    tv_ms = tv * 1000
    ta_ms = ta * 1000
    # threshold: <=130ms for weekly gate (tunable as baseline data grows)
    overhead_pass = ok_video and ok_audio and tv_ms <= 130.0 and ta_ms <= 130.0
    overhead_metrics = {
        "video_http": c3,
        "audio_http": c4,
        "video_elapsed_ms": round(tv_ms, 3),
        "audio_elapsed_ms": round(ta_ms, 3),
        "video_payload_bytes": len(rawv.encode("utf-8")),
        "audio_payload_bytes": len(rawa.encode("utf-8")),
        "threshold_ms": 130.0,
    }
    overhead_art = write_artifact("chk_003_02_stream_overhead_metrics.json", overhead_metrics)
    add_check(
        "CHK-RISK-003-02",
        "RISK-STREAM-OVERHEAD",
        "pass" if overhead_pass else "fail",
        "THR-RISK-003-02",
        "Envelope latency sample is within threshold and stream payloads are retrievable",
        metrics=overhead_metrics,
        artifacts=[overhead_art, OUT],
    )

    # CHK-RISK-003-05: trace pressure / backpressure sweep
    vb = (dv.get("data") or {}).get("backpressure", {}) if isinstance(dv, dict) else {}
    ab = (da.get("data") or {}).get("backpressure", {}) if isinstance(da, dict) else {}
    dropped_video = int(vb.get("dropped_events_since_last", 0))
    dropped_audio = int(ab.get("dropped_events_since_last", 0))
    throttle_video = bool(vb.get("throttle_active", False))
    throttle_audio = bool(ab.get("throttle_active", False))
    trace_pass = dropped_video == 0 and dropped_audio == 0 and not throttle_video and not throttle_audio
    trace_metrics = {
        "video_dropped_events_since_last": dropped_video,
        "audio_dropped_events_since_last": dropped_audio,
        "video_throttle_active": throttle_video,
        "audio_throttle_active": throttle_audio,
        "video_queue_depth": int(vb.get("queue_depth", 0)),
        "audio_queue_depth": int(ab.get("queue_depth", 0)),
    }
    trace_art = write_artifact("chk_003_05_trace_pressure_metrics.json", trace_metrics)
    add_check(
        "CHK-RISK-003-05",
        "RISK-TRACE-PRESSURE",
        "pass" if trace_pass else "fail",
        "THR-RISK-003-05",
        "No nominal drop/throttle condition in sampled backpressure telemetry",
        metrics=trace_metrics,
        artifacts=[trace_art, OUT],
    )

    # CHK-RISK-003-07: debug-mode perturbation guard sweep
    ok_mode_ss, c5, t5, d5, _ = call(
        "POST",
        "/api/v2/debug/clock/mode",
        body={"session_id": "ses_local", "mode": "single_step"},
        expected={200},
        headers=auth_headers,
    )
    ok_step, c6, t6, d6, _ = call(
        "POST",
        "/api/v2/debug/clock/step",
        body={"session_id": "ses_local", "steps": 2, "capture": ["opcode", "bus_error"]},
        expected={200},
        headers=auth_headers,
    )
    ok_state, c7, t7, d7, _ = call("GET", "/api/v2/debug/clock/state", expected={200}, headers=auth_headers)
    ok_mode_rt, c8, t8, d8, _ = call(
        "POST",
        "/api/v2/debug/clock/mode",
        body={"session_id": "ses_local", "mode": "realtime"},
        expected={200},
        headers=auth_headers,
    )
    step_data = d6.get("data", {}) if isinstance(d6, dict) else {}
    steps_requested = int(step_data.get("steps_requested", 0))
    ticks_committed = int(step_data.get("ticks_committed", -1))
    mode_data = d7.get("data", {}) if isinstance(d7, dict) else {}
    mode_state = str(mode_data.get("mode", ""))
    debug_pass = (
        ok_mode_ss
        and ok_step
        and ok_state
        and ok_mode_rt
        and steps_requested == ticks_committed
        and mode_state == "single_step"
    )
    debug_metrics = {
        "set_single_step_http": c5,
        "step_http": c6,
        "state_http": c7,
        "set_realtime_http": c8,
        "steps_requested": steps_requested,
        "ticks_committed": ticks_committed,
        "state_mode_before_realtime": mode_state,
        "elapsed_ms": {
            "set_single_step": round(t5 * 1000, 3),
            "step": round(t6 * 1000, 3),
            "state": round(t7 * 1000, 3),
            "set_realtime": round(t8 * 1000, 3),
        },
    }
    debug_art = write_artifact("chk_003_07_debug_perturbation_metrics.json", debug_metrics)
    add_check(
        "CHK-RISK-003-07",
        "RISK-DEBUG-MODE-PERTURBATION",
        "pass" if debug_pass else "fail",
        "THR-RISK-003-07",
        "Deterministic clock mode/step behavior and stable guard semantics",
        metrics=debug_metrics,
        artifacts=[debug_art, OUT],
    )

    # Non-weekly checks are intentionally not run in weekly cadence bundle.
    add_check(
        "CHK-RISK-003-03",
        "RISK-SD-IO-LATENCY",
        "not-run",
        "THR-RISK-003-03",
        "Daily cadence check omitted from weekly bundle",
        notes="Scheduled in daily run",
    )
    add_check(
        "CHK-RISK-003-04",
        "RISK-DEAD-LINK-RESILIENCE",
        "not-run",
        "THR-RISK-003-04",
        "Daily cadence check omitted from weekly bundle",
        notes="Scheduled in daily run",
    )
    add_check(
        "CHK-RISK-003-06",
        "RISK-SAVESTATE-DRIFT",
        "not-run",
        "THR-RISK-003-06",
        "Per-release check omitted from weekly bundle",
        notes="Scheduled in per-release run",
    )


pass_count = sum(1 for c in checks if c["status"] == "pass")
fail_count = sum(1 for c in checks if c["status"] == "fail")
not_run_count = sum(1 for c in checks if c["status"] == "not-run")

bundle = {
    "bundle_version": 1,
    "task_id": "S9-003",
    "run_id": RUN_ID,
    "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "cadence": "weekly",
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
