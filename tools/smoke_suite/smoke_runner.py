#!/usr/bin/env python3
import argparse
import datetime as dt
import glob
import json
import os
import re
import sqlite3
import subprocess
import sys
import time
import tomllib
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any


FILENAME_PATTERN = re.compile(r"^test_[A-Za-z0-9_]+_[A-Za-z0-9-]+\.test$")
DEFAULT_DB = "TRACKING/smoke_results.db"
DEFAULT_DESCRIPTORS_DIR = "smoke/descriptors"


@dataclass
class Descriptor:
    path: Path
    content: dict[str, Any]


@dataclass
class StepOutcome:
    passed: bool
    status_code: int
    duration_ms: int
    response_text: str
    response_json: Any
    error_message: str | None


def run_command_step(step: dict[str, Any], timeout_s: int) -> StepOutcome:
    command = str(step.get("command", "")).strip()
    if not command:
        return StepOutcome(
            passed=False,
            status_code=-1,
            duration_ms=0,
            response_text="",
            response_json=None,
            error_message="missing command",
        )

    cwd = step.get("cwd")
    started = time.perf_counter()
    try:
        result = subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            cwd=str(cwd) if cwd else None,
        )
        duration_ms = int((time.perf_counter() - started) * 1000)
        output = (result.stdout or "") + (result.stderr or "")
        expected_exit = step.get("expect_exit", [0])
        if isinstance(expected_exit, int):
            expected_exit = [expected_exit]

        passed = result.returncode in expected_exit

        contains = step.get("expect_output_contains")
        if contains:
            needles = contains if isinstance(contains, list) else [contains]
            for needle in needles:
                if str(needle) not in output:
                    passed = False

        return StepOutcome(
            passed=passed,
            status_code=result.returncode,
            duration_ms=duration_ms,
            response_text=output,
            response_json=try_parse_json(result.stdout or ""),
            error_message=None,
        )
    except subprocess.TimeoutExpired:
        duration_ms = int((time.perf_counter() - started) * 1000)
        return StepOutcome(
            passed=False,
            status_code=-1,
            duration_ms=duration_ms,
            response_text="",
            response_json=None,
            error_message="command timeout",
        )


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def init_db(db_path: Path) -> sqlite3.Connection:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS smoke_test_runs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            test_id TEXT NOT NULL,
            task_id TEXT,
            descriptor_path TEXT NOT NULL,
            started_at TEXT NOT NULL,
            ended_at TEXT,
            status TEXT,
            base_url TEXT,
            auth_scope TEXT,
            passed_steps INTEGER DEFAULT 0,
            failed_steps INTEGER DEFAULT 0,
            summary_json TEXT
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS smoke_test_step_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL,
            step_index INTEGER NOT NULL,
            step_name TEXT NOT NULL,
            method TEXT NOT NULL,
            path TEXT NOT NULL,
            expected_statuses TEXT,
            expected_error_code TEXT,
            status_code INTEGER,
            passed INTEGER NOT NULL,
            duration_ms INTEGER,
            response_json TEXT,
            error_message TEXT,
            recorded_at TEXT NOT NULL,
            FOREIGN KEY (run_id) REFERENCES smoke_test_runs(id)
        )
        """
    )
    conn.commit()
    return conn


def parse_descriptor(path: Path) -> Descriptor:
    with path.open("rb") as f:
        content = tomllib.load(f)
    return Descriptor(path=path, content=content)


def discover_descriptors(descriptor_dir: Path) -> list[Descriptor]:
    paths = sorted(Path(p) for p in glob.glob(str(descriptor_dir / "**" / "*.test"), recursive=True))
    descriptors: list[Descriptor] = []
    for path in paths:
        if not FILENAME_PATTERN.match(path.name):
            print(f"[WARN] Skipping invalid descriptor filename: {path.name}")
            continue
        descriptors.append(parse_descriptor(path))
    return descriptors


def apply_template(value: str, variables: dict[str, Any]) -> str:
    def replacer(match: re.Match[str]) -> str:
        key = match.group(1).strip()
        if key in variables:
            return str(variables[key])
        return match.group(0)

    return re.sub(r"\{\{\s*([^{}]+?)\s*\}\}", replacer, value)


def render_value(value: Any, variables: dict[str, Any]) -> Any:
    if isinstance(value, str):
        return apply_template(value, variables)
    if isinstance(value, list):
        return [render_value(item, variables) for item in value]
    if isinstance(value, dict):
        return {k: render_value(v, variables) for k, v in value.items()}
    return value


def resolve_descriptor_selection(descriptors: list[Descriptor], requested: list[str]) -> list[Descriptor]:
    if not requested:
        return descriptors

    selected: list[Descriptor] = []
    wanted = set(requested)
    for descriptor in descriptors:
        test_id = descriptor.content.get("meta", {}).get("test_id", "")
        if (
            descriptor.path.name in wanted
            or str(descriptor.path) in wanted
            or test_id in wanted
        ):
            selected.append(descriptor)

    unresolved = wanted - {
        item
        for descriptor in selected
        for item in (descriptor.path.name, str(descriptor.path), descriptor.content.get("meta", {}).get("test_id", ""))
    }
    if unresolved:
        missing = ", ".join(sorted(unresolved))
        raise ValueError(f"Requested test(s) not found: {missing}")

    return selected


def mint_token(base_url: str, scope: str) -> str | None:
    client_id = os.environ.get("AUTH_CLIENT_ID", "esptari-smoke")
    client_secret = os.environ.get("AUTH_CLIENT_SECRET", "esptari-smoke-secret")
    payload = {
        "grant_type": "client_credentials",
        "client_id": client_id,
        "client_secret": client_secret,
        "scope": scope,
    }
    req = urllib.request.Request(
        f"{base_url}/api/v2/auth/token",
        data=json.dumps(payload).encode("utf-8"),
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            body = json.loads(resp.read().decode("utf-8"))
        if body.get("ok"):
            return body.get("data", {}).get("access_token")
    except Exception:
        return None
    return None


def try_parse_json(text: str) -> Any:
    try:
        return json.loads(text)
    except Exception:
        return None


def json_path_get(payload: Any, path: str) -> Any:
    current = payload
    for part in path.split("."):
        if isinstance(current, dict) and part in current:
            current = current[part]
        else:
            raise KeyError(path)
    return current


def run_step(base_url: str, step: dict[str, Any], token: str | None, timeout_s: int) -> StepOutcome:
    kind = str(step.get("kind", "http")).lower()
    if kind == "command":
        return run_command_step(step, timeout_s)

    method = str(step["method"]).upper()
    path = str(step["path"])
    url = f"{base_url}{path}"

    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"

    extra_headers = step.get("headers", {})
    if isinstance(extra_headers, dict):
        for key, value in extra_headers.items():
            headers[str(key)] = str(value)

    body = None
    if "body_json" in step and step["body_json"]:
        body = str(step["body_json"]).encode("utf-8")

    request = urllib.request.Request(url, data=body, method=method, headers=headers)

    started = time.perf_counter()
    status_code = 0
    response_text = ""
    error_message = None
    try:
        with urllib.request.urlopen(request, timeout=timeout_s) as response:
            status_code = response.getcode()
            response_text = response.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        status_code = exc.code
        response_text = exc.read().decode("utf-8", errors="replace")
    except Exception as exc:
        error_message = str(exc)
    duration_ms = int((time.perf_counter() - started) * 1000)

    parsed = try_parse_json(response_text) if response_text else None
    passed = True

    expected_statuses = step.get("expect_status", [200])
    if status_code not in expected_statuses:
        passed = False

    expected_error_code = step.get("expect_error_code")
    if expected_error_code is not None:
        actual_error_code = None
        if isinstance(parsed, dict):
            actual_error_code = parsed.get("error", {}).get("code")
        if actual_error_code != expected_error_code:
            passed = False

    expected_ok = step.get("expect_ok")
    if expected_ok is not None:
        actual_ok = parsed.get("ok") if isinstance(parsed, dict) else None
        if actual_ok is not expected_ok:
            passed = False

    expected_json = step.get("expect_json", {})
    if isinstance(expected_json, dict) and expected_json:
        for jpath, expected in expected_json.items():
            try:
                actual = json_path_get(parsed, str(jpath))
            except Exception:
                passed = False
                continue
            if actual != expected:
                passed = False

    return StepOutcome(
        passed=passed,
        status_code=status_code,
        duration_ms=duration_ms,
        response_text=response_text,
        response_json=parsed,
        error_message=error_message,
    )


def run_descriptor(conn: sqlite3.Connection, descriptor: Descriptor, base_url_override: str | None = None) -> bool:
    meta = descriptor.content.get("meta", {})
    config = descriptor.content.get("config", {})
    steps = descriptor.content.get("step", [])

    test_id = meta.get("test_id", descriptor.path.stem)
    task_id = meta.get("task_id")
    base_url = base_url_override or config.get("base_url", "http://esptari.local")
    auth_scope = config.get("auth_scope", "engine:control inspect:read input:write ebin:manage")
    timeout_s = int(config.get("timeout_s", 20))
    continue_on_failure = bool(config.get("continue_on_failure", False))
    initial_vars = config.get("vars", {}) if isinstance(config.get("vars", {}), dict) else {}

    token = mint_token(base_url, auth_scope)
    started_at = now_iso()
    run_row = conn.execute(
        """
        INSERT INTO smoke_test_runs (test_id, task_id, descriptor_path, started_at, status, base_url, auth_scope)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        """,
        (test_id, task_id, str(descriptor.path), started_at, "running", base_url, auth_scope),
    )
    run_id = int(run_row.lastrowid)
    conn.commit()

    print(f"\n[RUN] {test_id} ({task_id or 'NO_TASK'})")
    print(f"      descriptor={descriptor.path}")
    print(f"      started_at={started_at}")

    passed_steps = 0
    failed_steps = 0
    variables: dict[str, Any] = {
        "run_id": run_id,
        "started_at": started_at,
        "test_id": test_id,
        "task_id": task_id or "",
    }
    variables.update(initial_vars)

    for index, step in enumerate(steps, start=1):
        rendered_step = render_value(step, variables)
        step_name = rendered_step.get("name", f"step_{index}")
        kind = str(rendered_step.get("kind", "http")).lower()
        method = str(rendered_step.get("method", "GET")).upper() if kind == "http" else "CMD"
        path = str(rendered_step.get("path", "/")) if kind == "http" else str(rendered_step.get("command", ""))
        expected_statuses = rendered_step.get("expect_status", [200])
        expected_error_code = rendered_step.get("expect_error_code")

        outcome = run_step(base_url, rendered_step, token, timeout_s)
        status_word = "PASS" if outcome.passed else "FAIL"
        print(f"  [{status_word}] {index:02d} {step_name} {method} {path} -> {outcome.status_code} ({outcome.duration_ms}ms)")

        if outcome.passed:
            passed_steps += 1
        else:
            failed_steps += 1

        conn.execute(
            """
            INSERT INTO smoke_test_step_results (
                run_id, step_index, step_name, method, path,
                expected_statuses, expected_error_code, status_code, passed,
                duration_ms, response_json, error_message, recorded_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                run_id,
                index,
                step_name,
                method,
                path,
                json.dumps(expected_statuses),
                expected_error_code,
                outcome.status_code,
                1 if outcome.passed else 0,
                outcome.duration_ms,
                json.dumps(outcome.response_json) if outcome.response_json is not None else None,
                outcome.error_message,
                now_iso(),
            ),
        )
        conn.commit()

        save_json = rendered_step.get("save_json", {})
        if isinstance(save_json, dict) and outcome.response_json is not None:
            for variable_name, json_path in save_json.items():
                try:
                    variables[str(variable_name)] = json_path_get(outcome.response_json, str(json_path))
                except Exception:
                    pass

        if not outcome.passed and not continue_on_failure:
            print("      stopping early due to failure (continue_on_failure=false)")
            break

    status = "passed" if failed_steps == 0 else "failed"
    ended_at = now_iso()

    summary = {
        "started_at": started_at,
        "ended_at": ended_at,
        "passed_steps": passed_steps,
        "failed_steps": failed_steps,
        "total_steps_executed": passed_steps + failed_steps,
        "auth_token_used": bool(token),
    }

    conn.execute(
        """
        UPDATE smoke_test_runs
        SET ended_at = ?, status = ?, passed_steps = ?, failed_steps = ?, summary_json = ?
        WHERE id = ?
        """,
        (ended_at, status, passed_steps, failed_steps, json.dumps(summary), run_id),
    )
    conn.commit()

    print(f"      ended_at={ended_at} status={status} passed_steps={passed_steps} failed_steps={failed_steps}")
    return failed_steps == 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Descriptor-driven Python smoke test runner")
    parser.add_argument("--descriptors-dir", default=DEFAULT_DESCRIPTORS_DIR, help="Directory containing .test descriptor files")
    parser.add_argument("--db", default=DEFAULT_DB, help="SQLite database path for smoke results")
    parser.add_argument("--all", action="store_true", help="Run all discovered tests (default behavior)")
    parser.add_argument("--test", action="append", default=[], help="Specific test by filename, path, or meta.test_id (repeatable)")
    parser.add_argument("--list", action="store_true", help="List discovered tests and exit")
    parser.add_argument("--base-url", default=None, help="Override base URL for all selected tests")

    args = parser.parse_args()

    descriptors_dir = Path(args.descriptors_dir)
    descriptors = discover_descriptors(descriptors_dir)

    if args.list:
        if not descriptors:
            print("No smoke descriptors found.")
            return 0
        print(f"Discovered {len(descriptors)} smoke descriptors:")
        for descriptor in descriptors:
            meta = descriptor.content.get("meta", {})
            print(
                f"- {descriptor.path.name} | test_id={meta.get('test_id', descriptor.path.stem)} | task_id={meta.get('task_id', 'UNSPECIFIED')}"
            )
        return 0

    if not descriptors:
        print("No smoke descriptors found to run.")
        return 1

    try:
        selected = resolve_descriptor_selection(descriptors, args.test)
    except ValueError as exc:
        print(str(exc))
        return 2

    conn = init_db(Path(args.db))
    all_passed = True

    for descriptor in selected:
        passed = run_descriptor(conn, descriptor, base_url_override=args.base_url)
        all_passed = all_passed and passed

    conn.close()
    return 0 if all_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
