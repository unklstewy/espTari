# Tracking DB MVP

SQLite-backed tracking system for development progress with optional Markdown import/export workflows.

## What this provides

- `schema.sql`: normalized tracking schema + FTS index
- `build_tracking_db.py`: parses tracking markdown and rebuilds `TRACKING/tracking.db`
- `serve_tracking_db.py`: read-only API for web UI
- `update_tracking_db.py`: DB-first status and acceptance updates with event history

## Build database

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

## DB-first progress workflow (recommended)

Use the DB as the active progress source while developing:

```bash
python tools/tracking_db/update_tracking_db.py set-task-status --task T-090 --status "Done" --note "Implemented and validated"
python tools/tracking_db/update_tracking_db.py add-acceptance --task-ref T-090 --decision Accepted --notes "Runtime validation complete" --evidence "components/esptari_web/esptari_web_stream.c"
python tools/tracking_db/update_tracking_db.py recent-events --limit 20
```

Notes:

- Markdown files are optional publication artifacts in this mode.
- If needed, bootstrap/refresh DB from current Markdown with `build_tracking_db.py`.
- Avoid re-running `build_tracking_db.py` after DB-first updates unless you intentionally want to reset from Markdown.
