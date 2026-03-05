# Tracking DB

SQLite-backed tracking system for development progress with DB-first workflow.

## What this provides

- `schema.sql`: normalized tracking schema + FTS index
- `build_tracking_db.py`: legacy bootstrap utility (used before DB-only migration)
- `serve_tracking_db.py`: read-only API for web UI
- `update_tracking_db.py`: DB-first status and acceptance updates with event history

## Legacy bootstrap (optional)

Markdown tracking was archived during DB-only migration. In normal operation, do not rebuild from markdown.

Archive location:

- `TRACKING/_archive/tracking_markdown_migration_2026-03-04.tar.gz`

If you intentionally restore markdown and need to rebuild:

```bash
python tools/tracking_db/build_tracking_db.py
```

Optional output path:

```bash
python tools/tracking_db/build_tracking_db.py --db /tmp/tracking.db
```

## Run API server

```bash
python tools/tracking_db/serve_tracking_db.py
```

Custom port:

```bash
python tools/tracking_db/serve_tracking_db.py --port 8765
```

## API endpoints

- `GET /health`
- `GET /api/overview`
- `GET /api/tasks?status=Done&q=T-090&limit=50`
- `GET /api/tasks/{task_id}`
- `GET /api/acceptance?task=T-090&decision=Accepted&limit=50`
- `GET /api/kanban`
- `GET /api/search?q=backpressure`

All endpoints are read-only and CORS-enabled for local frontend integration.

## DB-first progress workflow

Use the DB as the active progress source while developing:

```bash
python tools/tracking_db/update_tracking_db.py set-task-status --task T-090 --status "Done" --note "Implemented and validated"
python tools/tracking_db/update_tracking_db.py add-acceptance --task-ref T-090 --decision Accepted --notes "Runtime validation complete" --evidence "components/esptari_web/esptari_web_stream.c"
python tools/tracking_db/update_tracking_db.py recent-events --limit 20
```

Notes:

- Tracking is DB-only; use `update_tracking_db.py` for task status and acceptance updates.
- Avoid re-running `build_tracking_db.py` unless you explicitly restore markdown from archive for a one-time migration task.
