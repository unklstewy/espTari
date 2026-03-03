#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sqlite3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = ROOT / "TRACKING" / "tracking.db"


def dict_rows(cursor: sqlite3.Cursor) -> list[dict]:
    cols = [d[0] for d in cursor.description]
    return [dict(zip(cols, row)) for row in cursor.fetchall()]


def parse_int(value: str | None, default: int, min_value: int, max_value: int) -> int:
    if value is None:
        return default
    try:
        parsed = int(value)
    except ValueError:
        return default
    return max(min_value, min(max_value, parsed))


class TrackingHandler(BaseHTTPRequestHandler):
    db_path: Path

    def _json(self, payload: dict, code: int = 200) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(body)

    def _open(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def do_OPTIONS(self) -> None:
        self._json({"ok": True})

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        try:
            if path == "/health":
                return self._json({"ok": True, "data": {"service": "tracking-db-api"}})
            if path == "/api/overview":
                return self._overview()
            if path == "/api/tasks":
                return self._tasks(query)
            if path.startswith("/api/tasks/"):
                return self._task_detail(path.split("/api/tasks/", 1)[1])
            if path == "/api/acceptance":
                return self._acceptance(query)
            if path == "/api/kanban":
                return self._kanban()
            if path == "/api/search":
                return self._search(query)
            return self._json({"ok": False, "error": {"code": "NOT_FOUND"}}, 404)
        except sqlite3.Error as err:
            return self._json({"ok": False, "error": {"code": "SQLITE_ERROR", "message": str(err)}}, 500)

    def _overview(self) -> None:
        with self._open() as conn:
            status_rows = dict_rows(conn.execute("SELECT status, COUNT(*) AS count FROM tasks GROUP BY status ORDER BY status"))
            decisions = conn.execute("SELECT COUNT(*) AS c FROM acceptance_decisions").fetchone()["c"]
            cards = conn.execute("SELECT COUNT(*) AS c FROM kanban_cards").fetchone()["c"]

        return self._json(
            {
                "ok": True,
                "data": {
                    "tasks_by_status": status_rows,
                    "acceptance_count": decisions,
                    "kanban_card_count": cards,
                },
            }
        )

    def _tasks(self, query: dict[str, list[str]]) -> None:
        status = query.get("status", [""])[0].strip()
        q = query.get("q", [""])[0].strip()
        limit = parse_int(query.get("limit", ["50"])[0], 50, 1, 500)

        where = []
        params: list[object] = []
        if status:
            where.append("status = ?")
            params.append(status)
        if q:
            where.append("(task_id LIKE ? OR objective LIKE ? OR epic LIKE ?)")
            like = f"%{q}%"
            params.extend([like, like, like])

        sql = "SELECT task_id, epic, objective, priority, size, sprint, status, dependencies_raw FROM tasks"
        if where:
            sql += " WHERE " + " AND ".join(where)
        sql += " ORDER BY task_id LIMIT ?"
        params.append(limit)

        with self._open() as conn:
            rows = dict_rows(conn.execute(sql, params))
        return self._json({"ok": True, "data": rows})

    def _task_detail(self, task_id: str) -> None:
        if not task_id:
            return self._json({"ok": False, "error": {"code": "BAD_REQUEST"}}, 400)

        with self._open() as conn:
            task = conn.execute(
                "SELECT task_id, epic, objective, priority, size, sprint, status, dependencies_raw, source_file, source_line FROM tasks WHERE task_id = ?",
                (task_id,),
            ).fetchone()
            if task is None:
                return self._json({"ok": False, "error": {"code": "NOT_FOUND"}}, 404)

            deps = [row["depends_on"] for row in conn.execute("SELECT depends_on FROM task_dependencies WHERE task_id = ? ORDER BY depends_on", (task_id,))]
            acceptance = dict_rows(
                conn.execute(
                    "SELECT id, decision_date, sprint, task_ref, decision, notes, evidence_link FROM acceptance_decisions WHERE task_ref LIKE ? ORDER BY id DESC LIMIT 20",
                    (f"%{task_id}%",),
                )
            )

        return self._json(
            {
                "ok": True,
                "data": {
                    **dict(task),
                    "dependencies": deps,
                    "acceptance": acceptance,
                },
            }
        )

    def _acceptance(self, query: dict[str, list[str]]) -> None:
        task = query.get("task", [""])[0].strip()
        decision = query.get("decision", [""])[0].strip()
        limit = parse_int(query.get("limit", ["50"])[0], 50, 1, 500)

        where = []
        params: list[object] = []
        if task:
            where.append("task_ref LIKE ?")
            params.append(f"%{task}%")
        if decision:
            where.append("decision = ?")
            params.append(decision)

        sql = "SELECT id, decision_date, sprint, task_ref, decision, notes, evidence_link FROM acceptance_decisions"
        if where:
            sql += " WHERE " + " AND ".join(where)
        sql += " ORDER BY id DESC LIMIT ?"
        params.append(limit)

        with self._open() as conn:
            rows = dict_rows(conn.execute(sql, params))

        return self._json({"ok": True, "data": rows})

    def _kanban(self) -> None:
        with self._open() as conn:
            rows = dict_rows(
                conn.execute(
                    "SELECT column_name, card_text, task_id FROM kanban_cards ORDER BY id"
                )
            )

        grouped: dict[str, list[dict]] = {}
        for row in rows:
            grouped.setdefault(row["column_name"], []).append({"card_text": row["card_text"], "task_id": row["task_id"]})

        return self._json({"ok": True, "data": grouped})

    def _search(self, query: dict[str, list[str]]) -> None:
        q = query.get("q", [""])[0].strip()
        if not q:
            return self._json({"ok": True, "data": []})

        with self._open() as conn:
            rows = dict_rows(
                conn.execute(
                    """
                    SELECT a.id, a.task_ref, a.decision_date, a.decision, a.notes, a.evidence_link
                    FROM acceptance_fts f
                    JOIN acceptance_decisions a ON a.id = f.rowid
                    WHERE acceptance_fts MATCH ?
                    ORDER BY rank
                    LIMIT 50
                    """,
                    (q,),
                )
            )

        return self._json({"ok": True, "data": rows})


def run_server(db_path: Path, host: str, port: int) -> None:
    handler = type("TrackingHandlerWithDB", (TrackingHandler,), {"db_path": db_path})
    server = ThreadingHTTPServer((host, port), handler)
    print(f"Serving tracking DB API on http://{host}:{port}")
    print(f"Using DB: {db_path}")
    server.serve_forever()


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve read-only HTTP API for tracking SQLite database.")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB, help="Path to tracking.db")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    if not args.db.exists():
        raise SystemExit(f"Database not found: {args.db}. Run build_tracking_db.py first.")

    run_server(args.db, args.host, args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
