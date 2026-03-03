#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sqlite3
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import re
from typing import Iterable

ROOT = Path(__file__).resolve().parents[2]
TRACKING_DIR = ROOT / "TRACKING"
SCHEMA_SQL = Path(__file__).resolve().parent / "schema.sql"
DEFAULT_DB = TRACKING_DIR / "tracking.db"

TASK_ID_RE = re.compile(r"^[A-Z]+-\d+$")


def resolve_tracking_file(filename: str) -> Path:
    direct = TRACKING_DIR / filename
    if direct.exists():
        return direct

    archive_root = TRACKING_DIR / "_archive"
    if archive_root.exists():
        candidates = sorted([p for p in archive_root.glob("legacy_markdown_*") if p.is_dir()])
        for folder in reversed(candidates):
            archived = folder / filename
            if archived.exists():
                return archived

    raise FileNotFoundError(f"Unable to locate tracking file: {filename}")


BACKLOG_MD = resolve_tracking_file("BACKLOG.md")
KANBAN_MD = resolve_tracking_file("KANBAN_BOARD.md")
ACCEPTANCE_MD = resolve_tracking_file("ACCEPTANCE_LOG.md")


@dataclass
class TaskRow:
    task_id: str
    epic: str
    objective: str
    priority: str
    size: str
    sprint: str
    status: str
    dependencies_raw: str
    source_line: int


@dataclass
class AcceptanceRow:
    decision_date: str
    sprint: str
    task_ref: str
    decision: str
    notes: str
    evidence_link: str
    source_line: int


@dataclass
class KanbanCard:
    column_name: str
    card_text: str
    task_id: str | None
    source_line: int


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def split_table_cells(line: str) -> list[str]:
    if not line.strip().startswith("|"):
        return []
    parts = [cell.strip() for cell in line.strip().strip("|").split("|")]
    return parts


def is_separator_row(cells: list[str]) -> bool:
    if not cells:
        return False
    return all(set(c) <= {"-", ":"} and len(c) > 0 for c in cells)


def parse_backlog(path: Path) -> list[TaskRow]:
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[TaskRow] = []
    for line_no, line in enumerate(lines, start=1):
        cells = split_table_cells(line)
        if len(cells) != 8:
            continue
        if cells[0] == "Task ID" or is_separator_row(cells):
            continue
        task_id = cells[0]
        if not TASK_ID_RE.match(task_id):
            continue
        out.append(
            TaskRow(
                task_id=task_id,
                epic=cells[1],
                objective=cells[2],
                priority=cells[3],
                size=cells[4],
                sprint=cells[5],
                status=cells[6],
                dependencies_raw=cells[7],
                source_line=line_no,
            )
        )
    return out


def extract_task_id(text: str) -> str | None:
    m = re.search(r"\b([A-Z]+-\d+)\b", text)
    return m.group(1) if m else None


def parse_kanban(path: Path) -> list[KanbanCard]:
    lines = path.read_text(encoding="utf-8").splitlines()
    cards: list[KanbanCard] = []
    current_column = ""
    for line_no, line in enumerate(lines, start=1):
        if line.startswith("### "):
            current_column = line[4:].strip()
            continue
        if not current_column:
            continue
        if not line.startswith("- "):
            continue
        text = line[2:].strip()
        if not text:
            continue
        cards.append(
            KanbanCard(
                column_name=current_column,
                card_text=text,
                task_id=extract_task_id(text),
                source_line=line_no,
            )
        )
    return cards


def parse_acceptance(path: Path) -> list[AcceptanceRow]:
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[AcceptanceRow] = []
    in_table = False
    for line_no, line in enumerate(lines, start=1):
        cells = split_table_cells(line)
        if len(cells) != 6:
            continue
        if cells[0] == "Date":
            in_table = True
            continue
        if in_table and is_separator_row(cells):
            continue
        if not in_table:
            continue
        if cells[0] == "YYYY-MM-DD":
            continue
        out.append(
            AcceptanceRow(
                decision_date=cells[0],
                sprint=cells[1],
                task_ref=cells[2],
                decision=cells[3],
                notes=cells[4],
                evidence_link=cells[5],
                source_line=line_no,
            )
        )
    return out


def split_dependencies(raw: str) -> Iterable[str]:
    raw = raw.strip()
    if not raw:
        return []
    return [part.strip() for part in raw.split(",") if part.strip()]


def split_artifacts(raw: str) -> Iterable[str]:
    raw = raw.strip()
    if not raw:
        return []
    return [part.strip() for part in raw.split(",") if part.strip()]


def init_db(conn: sqlite3.Connection) -> None:
    schema = SCHEMA_SQL.read_text(encoding="utf-8")
    conn.executescript(schema)


def rebuild_db(conn: sqlite3.Connection, tasks: list[TaskRow], acceptance: list[AcceptanceRow], cards: list[KanbanCard]) -> None:
    ts = now_iso()
    with conn:
        conn.execute("DELETE FROM task_dependencies")
        conn.execute("DELETE FROM tasks")
        conn.execute("DELETE FROM acceptance_artifacts")
        conn.execute("DELETE FROM acceptance_decisions")
        conn.execute("DELETE FROM kanban_cards")
        conn.execute("DELETE FROM acceptance_fts")

        for task in tasks:
            conn.execute(
                """
                INSERT INTO tasks (
                  task_id, epic, objective, priority, size, sprint, status,
                  dependencies_raw, source_file, source_line, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    task.task_id,
                    task.epic,
                    task.objective,
                    task.priority,
                    task.size,
                    task.sprint,
                    task.status,
                    task.dependencies_raw,
                    str(BACKLOG_MD.relative_to(ROOT)),
                    task.source_line,
                    ts,
                ),
            )
            for dep in split_dependencies(task.dependencies_raw):
                conn.execute(
                    "INSERT OR IGNORE INTO task_dependencies (task_id, depends_on) VALUES (?, ?)",
                    (task.task_id, dep),
                )

        for row in acceptance:
            cur = conn.execute(
                """
                INSERT INTO acceptance_decisions (
                  decision_date, sprint, task_ref, decision, notes, evidence_link,
                  source_file, source_line, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    row.decision_date,
                    row.sprint,
                    row.task_ref,
                    row.decision,
                    row.notes,
                    row.evidence_link,
                    str(ACCEPTANCE_MD.relative_to(ROOT)),
                    row.source_line,
                    ts,
                ),
            )
            decision_id = int(cur.lastrowid)
            for artifact in split_artifacts(row.evidence_link):
                conn.execute(
                    "INSERT OR IGNORE INTO acceptance_artifacts (decision_id, artifact_path) VALUES (?, ?)",
                    (decision_id, artifact),
                )
            conn.execute(
                "INSERT INTO acceptance_fts (rowid, task_ref, notes, evidence_link) VALUES (?, ?, ?, ?)",
                (decision_id, row.task_ref, row.notes, row.evidence_link),
            )

        for card in cards:
            conn.execute(
                """
                INSERT INTO kanban_cards (column_name, card_text, task_id, source_file, source_line, updated_at)
                VALUES (?, ?, ?, ?, ?, ?)
                """,
                (
                    card.column_name,
                    card.card_text,
                    card.task_id,
                    str(KANBAN_MD.relative_to(ROOT)),
                    card.source_line,
                    ts,
                ),
            )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build SQLite tracking index from Markdown tracking files.")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB, help="Output SQLite database path")
    args = parser.parse_args()

    if not BACKLOG_MD.exists() or not KANBAN_MD.exists() or not ACCEPTANCE_MD.exists():
        raise SystemExit("Missing one or more tracking Markdown files.")

    args.db.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(args.db)
    try:
        init_db(conn)
        tasks = parse_backlog(BACKLOG_MD)
        acceptance = parse_acceptance(ACCEPTANCE_MD)
        cards = parse_kanban(KANBAN_MD)
        rebuild_db(conn, tasks, acceptance, cards)
        print(f"Built tracking DB: {args.db}")
        print(f"  tasks={len(tasks)} acceptance={len(acceptance)} kanban_cards={len(cards)}")
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
