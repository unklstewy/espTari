#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = ROOT / "TRACKING" / "tracking.db"
SCHEMA_SQL = Path(__file__).resolve().parent / "schema.sql"

ALLOWED_STATUSES = {
    "Backlog",
    "Ready",
    "In Progress",
    "In Review",
    "Acceptance",
    "Done",
    "Blocked",
}


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def connect(db_path: Path) -> sqlite3.Connection:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.executescript(SCHEMA_SQL.read_text(encoding="utf-8"))
    return conn


def ensure_task_exists(conn: sqlite3.Connection, task_id: str) -> sqlite3.Row:
    row = conn.execute(
        "SELECT task_id, status FROM tasks WHERE task_id = ?",
        (task_id,),
    ).fetchone()
    if row is None:
        raise SystemExit(f"Task not found in DB: {task_id}. Run build_tracking_db.py first to bootstrap.")
    return row


def set_task_status(conn: sqlite3.Connection, task_id: str, status: str, note: str, actor: str) -> None:
    if status not in ALLOWED_STATUSES:
        raise SystemExit(f"Invalid status '{status}'. Allowed: {sorted(ALLOWED_STATUSES)}")

    task = ensure_task_exists(conn, task_id)
    previous = task["status"]
    ts = now_iso()

    with conn:
        conn.execute(
            "UPDATE tasks SET status = ?, updated_at = ? WHERE task_id = ?",
            (status, ts, task_id),
        )
        conn.execute(
            """
            INSERT INTO task_events (task_id, event_type, from_status, to_status, note, actor, created_at)
            VALUES (?, 'status_change', ?, ?, ?, ?, ?)
            """,
            (task_id, previous, status, note, actor, ts),
        )

    print(f"Updated {task_id}: {previous} -> {status}")


def add_acceptance(conn: sqlite3.Connection,
                   task_ref: str,
                   decision: str,
                   notes: str,
                   evidence: str,
                   sprint: str,
                   actor: str) -> None:
    ts = now_iso()

    with conn:
        cur = conn.execute(
            """
            INSERT INTO acceptance_decisions (
              decision_date, sprint, task_ref, decision, notes, evidence_link,
              source_file, source_line, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (ts[:10], sprint, task_ref, decision, notes, evidence, "tracking.db", 0, ts),
        )
        decision_id = int(cur.lastrowid)

        for artifact in [item.strip() for item in evidence.split(",") if item.strip()]:
            conn.execute(
                "INSERT OR IGNORE INTO acceptance_artifacts (decision_id, artifact_path) VALUES (?, ?)",
                (decision_id, artifact),
            )

        conn.execute(
            "INSERT INTO acceptance_fts (rowid, task_ref, notes, evidence_link) VALUES (?, ?, ?, ?)",
            (decision_id, task_ref, notes, evidence),
        )

        task_id = task_ref.split(",")[0].strip()
        if task_id:
            existing = conn.execute("SELECT 1 FROM tasks WHERE task_id = ?", (task_id,)).fetchone()
            if existing is not None:
                conn.execute(
                    """
                    INSERT INTO task_events (task_id, event_type, from_status, to_status, note, actor, created_at)
                    VALUES (?, 'acceptance_decision', NULL, NULL, ?, ?, ?)
                    """,
                    (task_id, f"{decision}: {notes}", actor, ts),
                )

    print(f"Inserted acceptance decision for {task_ref}")


def show_recent(conn: sqlite3.Connection, limit: int) -> None:
    rows = conn.execute(
        """
        SELECT id, task_id, event_type, from_status, to_status, note, actor, created_at
        FROM task_events
        ORDER BY id DESC
        LIMIT ?
        """,
        (limit,),
    ).fetchall()

    if not rows:
        print("No task events yet.")
        return

    for row in rows:
        print(
            f"[{row['id']}] {row['created_at']} {row['task_id']} {row['event_type']} "
            f"{row['from_status'] or '-'} -> {row['to_status'] or '-'} | {row['note'] or ''}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="DB-first tracking updates for development progress.")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB)
    parser.add_argument("--actor", default="copilot")

    sub = parser.add_subparsers(dest="cmd", required=True)

    set_status = sub.add_parser("set-task-status", help="Update a task status and append task event")
    set_status.add_argument("--task", required=True)
    set_status.add_argument("--status", required=True)
    set_status.add_argument("--note", default="")

    add_decision = sub.add_parser("add-acceptance", help="Append acceptance decision")
    add_decision.add_argument("--task-ref", required=True)
    add_decision.add_argument("--decision", required=True)
    add_decision.add_argument("--notes", required=True)
    add_decision.add_argument("--evidence", default="")
    add_decision.add_argument("--sprint", default="S5")

    recent = sub.add_parser("recent-events", help="Show recent task events")
    recent.add_argument("--limit", type=int, default=20)

    args = parser.parse_args()

    conn = connect(args.db)
    try:
        if args.cmd == "set-task-status":
            set_task_status(conn, args.task, args.status, args.note, args.actor)
        elif args.cmd == "add-acceptance":
            add_acceptance(conn, args.task_ref, args.decision, args.notes, args.evidence, args.sprint, args.actor)
        elif args.cmd == "recent-events":
            show_recent(conn, args.limit)
    finally:
        conn.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
