# 7.10 Catalog asset retrieval and health APIs

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 43

## Subsections

- Download one entry from hosted URL
- Download missing assets in batch
- Probe hosted links and update states
- Mark link dead (manual override)
- Rescan local storage against catalog
- Missing-asset diff report projection

---

## 7.10 Catalog asset retrieval and health APIs

### Download one entry from hosted URL

- `POST /api/v2/catalogs/{catalog}/download-entry`

```json
{
  "entry_id": "disk.automation.a_093",
  "overwrite": false,
  "verify_sha256": true
}
```

Hosted download request validation (pre-enqueue):

- `entry_id` must resolve to an existing catalog entry.
- Entry must have a non-empty `hosted_url` and must not be marked `availability_state=dead` unless `allow_dead_retry=true` is explicitly provided.
- `overwrite=false` rejects when a verified local file already exists.
- `verify_sha256=true` requires catalog entry to include checksum metadata.

Enqueue path contract:

- On successful validation, server must enqueue a download job and return queue projection metadata.
- Enqueue response `data` fields:
  - `catalog` (string)
  - `entry_id` (string)
  - `job_id` (string)
  - `queue_state` (enum: `queued`, `already_queued`)
  - `priority` (enum: `normal`, `high`)
  - `enqueued_at_us` (uint64)

Enqueue success example:

```json
{
  "catalog": "floppies",
  "entry_id": "disk.automation.a_093",
  "job_id": "dl_01H...",
  "queue_state": "queued",
  "priority": "normal",
  "enqueued_at_us": 1710004000100
}
```

Staged download commit and integrity verification:

- Download worker stages payload to `/sdcard/.staging/catalog_downloads/<job_id>.part`.
- Commit is a deterministic 3-phase sequence:
  1. `staged`: transfer completed to staging file.
  2. `verified`: integrity checks pass (`size` and optional `sha256` when `verify_sha256=true`).
  3. `committed`: atomic move to catalog `local_path` and presence-index update.
- Commit success response projection fields (on completion/status query paths):
  - `job_id` (string)
  - `stage_state` (enum: `staged`, `verified`, `committed`)
  - `staging_path` (string)
  - `final_path` (string)
  - `bytes_downloaded` (uint64)
  - `sha256_expected` (string or `null`)
  - `sha256_actual` (string or `null`)
  - `verified` (bool)
  - `committed_at_us` (uint64)

Commit completion example:

```json
{
  "catalog": "floppies",
  "entry_id": "disk.automation.a_093",
  "job_id": "dl_01H...",
  "stage_state": "committed",
  "staging_path": "/sdcard/.staging/catalog_downloads/dl_01H....part",
  "final_path": "/sdcard/disks/st/AUTOMATION/A_093.ST",
  "bytes_downloaded": 737280,
  "sha256_expected": "sha256:abcd...",
  "sha256_actual": "sha256:abcd...",
  "verified": true,
  "committed_at_us": 1710004000420
}
```

Deterministic validation blockers:

- Unknown catalog or entry ID -> `CATALOG_NOT_FOUND` / `CATALOG_ENTRY_NOT_FOUND`.
- Entry has no usable hosted source (`hosted_url` missing) -> `BAD_REQUEST`.
- Entry marked dead and retry not explicitly allowed -> `CATALOG_LINK_DEAD`.
- Local file already exists and `overwrite=false` -> `CONFLICT`.

Deterministic staged-commit blockers:

- Staging artifact missing/truncated before verification -> `UPLOAD_INCOMPLETE`.
- Integrity/hash mismatch at verify phase -> `CATALOG_SYNC_FAILED`.
- Atomic move or index update failure during commit -> `CATALOG_SYNC_FAILED`.

Validation blocker example (dead link):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710004000101,
  "error": {
    "code": "CATALOG_LINK_DEAD",
    "category": "catalog",
    "message": "Entry disk.automation.a_093 is marked dead and cannot be enqueued",
    "retryable": false,
    "details": {
      "catalog": "floppies",
      "entry_id": "disk.automation.a_093",
      "availability_state": "dead",
      "hint": "Set allow_dead_retry=true to override"
    }
  }
}
```

Commit blocker example (hash mismatch):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710004000421,
  "error": {
    "code": "CATALOG_SYNC_FAILED",
    "category": "catalog",
    "message": "Integrity verification failed for staged download",
    "retryable": false,
    "details": {
      "job_id": "dl_01H...",
      "entry_id": "disk.automation.a_093",
      "stage_state": "verified",
      "sha256_expected": "sha256:abcd...",
      "sha256_actual": "sha256:ef01..."
    }
  }
}
```

### Download missing assets in batch

- `POST /api/v2/catalogs/{catalog}/download-missing`

```json
{
  "limit": 100,
  "state_filter": ["unknown", "online", "offline"],
  "skip_dead": true
}
```

### Probe hosted links and update states

- `POST /api/v2/catalogs/{catalog}/probe-links`

```json
{
  "limit": 500,
  "timeout_ms": 8000,
  "mark_dead_after_failures": 3
}
```

Dead-link probe worker contract:

- Request dispatches a bounded probe worker batch over eligible catalog entries.
- Worker identity and execution controls:
  - `worker_id` (generated for batch run)
  - `concurrency` (uint32, defaults to implementation baseline)
  - `timeout_ms` per probe attempt
  - `max_retries_per_entry` (uint32, default `0` for this endpoint)
- Probe result states per entry:
  - `online` (success)
  - `offline` (transport/connectivity failure)
  - `dead` (terminal/HTTP hard-failure classification)
  - `unknown` (probe skipped or inconclusive)

Timeout policy:

- A probe attempt exceeding `timeout_ms` is classified as `offline` for that attempt.
- Timeout events must increment per-entry `download_fail_count` and emit timeout telemetry in response summary.
- Entry transitions to `dead` when `download_fail_count >= mark_dead_after_failures` and failure class is dead-link eligible.

Probe worker response `data`:

```json
{
  "catalog": "floppies",
  "worker_id": "probe_01H...",
  "started_at_us": 1710005000100,
  "completed_at_us": 1710005008200,
  "policy": {
    "timeout_ms": 8000,
    "mark_dead_after_failures": 3,
    "concurrency": 16
  },
  "summary": {
    "probed": 500,
    "online": 462,
    "offline": 29,
    "dead": 9,
    "timed_out": 17
  },
  "results": [
    {
      "entry_id": "disk.automation.a_093",
      "state_before": "online",
      "state_after": "offline",
      "attempts": 1,
      "timed_out": true,
      "latency_ms": 8000
    }
  ]
}
```

Deterministic probe blockers:

- Unknown catalog name -> `CATALOG_NOT_FOUND`.
- Invalid timeout configuration (`timeout_ms<=0`, `mark_dead_after_failures<1`) -> `BAD_REQUEST`.
- Probe worker capacity unavailable/saturated -> `CONFLICT`.

Probe blocker example (invalid timeout):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710005000101,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "timeout_ms must be greater than 0",
    "retryable": false,
    "details": {
      "catalog": "floppies",
      "field": "timeout_ms",
      "actual": 0,
      "minimum": 1
    }
  }
}
```

### Mark link dead (manual override)

- `POST /api/v2/catalogs/{catalog}/mark-dead`

```json
{
  "entry_id": "disk.automation.a_093",
  "reason": "HTTP 404 repeated 3 times"
}
```

Dead-link mark/retry state machine:

- `POST /mark-dead` transition: `unknown|online|offline -> dead`.
- `POST /mark-dead` on already-dead entry is idempotent: state remains `dead`, telemetry updates `last_dead_reason`/`last_dead_marked_at_us`.
- Dead-retry path is only via `POST /download-entry` with `allow_dead_retry=true`.
- Retry transition contract:
  - enqueue accepted: `dead` state remains sticky while retry is pending.
  - retry commit success: `dead -> online`, reset `download_fail_count=0`.
  - retry commit failure/timeout: state remains `dead`, increment retry-failure telemetry.

Dead-link telemetry fields (entry projection):

- `dead_marked` (bool)
- `dead_source` (enum: `manual`, `probe_threshold`)
- `last_dead_reason` (string)
- `last_dead_marked_at_us` (uint64)
- `dead_retry_attempts` (uint32)
- `dead_retry_successes` (uint32)
- `dead_retry_failures` (uint32)
- `last_dead_retry_at_us` (uint64 or `null`)
- `last_dead_retry_result` (enum: `success`, `failure`, `timeout`, `blocked`, `none`)

Mark-dead success response `data` example:

```json
{
  "catalog": "floppies",
  "entry_id": "disk.automation.a_093",
  "state_before": "offline",
  "state_after": "dead",
  "dead_marked": true,
  "dead_source": "manual",
  "last_dead_reason": "HTTP 404 repeated 3 times",
  "last_dead_marked_at_us": 1710005010200,
  "dead_retry_attempts": 0,
  "dead_retry_successes": 0,
  "dead_retry_failures": 0,
  "last_dead_retry_at_us": null,
  "last_dead_retry_result": "none"
}
```

Retry telemetry projection example (after `allow_dead_retry=true` download attempt):

```json
{
  "catalog": "floppies",
  "entry_id": "disk.automation.a_093",
  "availability_state": "dead",
  "dead_marked": true,
  "dead_retry_attempts": 3,
  "dead_retry_successes": 1,
  "dead_retry_failures": 2,
  "last_dead_retry_at_us": 1710005018200,
  "last_dead_retry_result": "timeout"
}
```

Deterministic mark/retry blockers:

- Unknown catalog or entry ID -> `CATALOG_NOT_FOUND` / `CATALOG_ENTRY_NOT_FOUND`.
- Empty/invalid mark reason (`reason` blank after trim) -> `BAD_REQUEST`.
- Dead-retry requested while `hosted_url` is missing -> `BAD_REQUEST`.
- Dead-retry not explicitly allowed for dead entry -> `CATALOG_LINK_DEAD`.

### Rescan local storage against catalog

- `POST /api/v2/catalogs/{catalog}/rescan-local`

Request:

```json
{
  "scan_roots": ["/sdcard/roms/st", "/sdcard/disks/st"],
  "follow_symlinks": false,
  "limit": 5000,
  "hash_mode": "metadata_only"
}
```

Canonical presence index model:

- Scan operation materializes a canonical per-catalog presence index snapshot.
- Presence index key: `catalog + entry_id`.
- Presence index fields:
  - `entry_id` (string)
  - `catalog` (string)
  - `local_present` (bool)
  - `local_path` (string or `null`)
  - `file_size` (uint64 or `null`)
  - `mtime_us` (uint64 or `null`)
  - `sha256` (string or `null`, available when hashing enabled)
  - `indexed_at_us` (uint64)

Response `data`:

```json
{
  "catalog": "floppies",
  "scan_id": "scan_01H...",
  "indexed_at_us": 1710003000100,
  "stats": {
    "entries_total": 1319,
    "entries_present": 841,
    "entries_missing": 478,
    "entries_changed": 27,
    "entries_unchanged": 1292
  },
  "presence_index": [
    {
      "entry_id": "disk.automation.a_093",
      "catalog": "floppies",
      "local_present": true,
      "local_path": "/sdcard/disks/st/AUTOMATION/A_093.ST",
      "file_size": 737280,
      "mtime_us": 1710002500000,
      "sha256": null,
      "indexed_at_us": 1710003000100
    },
    {
      "entry_id": "disk.game.x",
      "catalog": "floppies",
      "local_present": false,
      "local_path": null,
      "file_size": null,
      "mtime_us": null,
      "sha256": null,
      "indexed_at_us": 1710003000100
    }
  ]
}
```

Behavior:

1. Reconcile catalog entries with SD-card presence.
2. Update local existence flags and availability-state transitions.
3. Preserve audit timestamps for state changes.
4. Presence index snapshot returned by this API is the canonical source for local presence determination used by downstream missing-asset reports.

Deterministic scan blockers:

- Unknown catalog name in path parameter -> `CATALOG_NOT_FOUND`.
- Non-allowlisted scan root or traversal attempt -> `PATH_NOT_ALLOWED`.
- Invalid request shape (for example unsupported `hash_mode`) -> `BAD_REQUEST`.

Scan blocker example (invalid root):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710003000101,
  "error": {
    "code": "PATH_NOT_ALLOWED",
    "category": "path",
    "message": "Scan root /tmp is outside allowed SD-card roots",
    "retryable": false,
    "details": {
      "catalog": "floppies",
      "scan_root": "/tmp",
      "allowed_roots": ["/sdcard/roms", "/sdcard/disks", "/sdcard/cartridges"]
    }
  }
}
```

### Missing-asset diff report projection

- `GET /api/v2/catalogs/{catalog}/missing-report?limit=200&state=missing&since_scan_id=scan_01H...`

Projection source and semantics:

- This endpoint projects only from the canonical presence index produced by `POST /api/v2/catalogs/{catalog}/rescan-local`.
- `state=missing` returns entries where `local_present=false`.
- Optional `since_scan_id` computes a delta versus the referenced prior scan snapshot.
- Returned list is deterministically ordered by `entry_id` ascending unless an explicit supported sort is added in a future schema version.

Response `data`:

```json
{
  "catalog": "floppies",
  "scan_id": "scan_01H_current",
  "base_scan_id": "scan_01H_prev",
  "summary": {
    "missing_total": 478,
    "new_missing": 12,
    "resolved_since_base": 5,
    "unchanged_missing": 466
  },
  "missing_assets": [
    {
      "entry_id": "disk.game.x",
      "catalog": "floppies",
      "local_present": false,
      "expected_path": "/sdcard/disks/st/GAME_X.ST",
      "availability_state": "online",
      "first_missing_at_us": 1710002000000,
      "last_seen_scan_id": "scan_01H_prev"
    },
    {
      "entry_id": "disk.demo.y",
      "catalog": "floppies",
      "local_present": false,
      "expected_path": "/sdcard/disks/st/DEMO_Y.ST",
      "availability_state": "offline",
      "first_missing_at_us": 1710002100000,
      "last_seen_scan_id": "scan_01H_prev"
    }
  ]
}
```

Deterministic projection blockers:

- Unknown catalog name -> `CATALOG_NOT_FOUND`.
- Missing or stale canonical presence index snapshot for requested scan scope -> `CONFLICT`.
- Invalid query params (`limit<=0`, unsupported `state`) -> `BAD_REQUEST`.

Projection blocker example (missing base scan):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710003000201,
  "error": {
    "code": "CONFLICT",
    "category": "catalog",
    "message": "Base scan snapshot scan_01H_prev is unavailable for diff projection",
    "retryable": true,
    "details": {
      "catalog": "floppies",
      "required_operation": "POST /api/v2/catalogs/floppies/rescan-local",
      "since_scan_id": "scan_01H_prev"
    }
  }
}
```
