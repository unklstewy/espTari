# 7.11 On-device scraper job and schedule APIs

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 44

## Subsections

- Run scraper/sync job now
- List jobs
- Get job status
- Create periodic schedule
- List schedules
- Get schedule
- Update schedule
- Delete schedule

---

## 7.11 On-device scraper job and schedule APIs

Scraper job types:

- `floppy_catalog_sync`
- `rom_catalog_sync`
- `tos_catalog_sync`

Job modes:

- `catalog_only`
- `catalog_and_probe_links`
- `catalog_probe_and_prefetch_missing`

Catalog-sync scheduler core contract:

- Scheduler has one authoritative clock source (`scheduler_now_us`) and evaluates all enabled schedules on deterministic tick boundaries.
- Due-time selection order is stable: `(next_run_at_us asc, schedule_id asc)`.
- A schedule dispatch creates exactly one job instance with `trigger="schedule"` and updates schedule runtime metadata atomically.
- Missed-window behavior after reboot/offline gap:
  - if `catch_up=false`, next due is advanced to next cron boundary in the future.
  - if `catch_up=true`, one immediate catch-up dispatch is allowed, then cadence resumes from cron boundaries.
- Runtime saturation must not drop schedules silently; scheduler records skipped dispatch with blocker metadata.

Persisted schedules contract:

- Schedules are persisted in canonical store: `/sdcard/config/engine_v2/catalog_sync_schedules.json`.
- Persisted record minimum fields:
  - `schedule_id` (string, stable)
  - `job_type` (enum)
  - `mode` (enum)
  - `cron` (string)
  - `enabled` (bool)
  - `catch_up` (bool, default `false`)
  - `created_at_us` (uint64)
  - `updated_at_us` (uint64)
  - `last_run_at_us` (uint64 or `null`)
  - `next_run_at_us` (uint64)
  - `last_result` (enum: `success`, `failure`, `skipped`, `none`)
  - `last_error_code` (string or `null`)

### Run scraper/sync job now

- `POST /api/v2/catalog-sync/jobs/run`

```json
{
  "job_type": "floppy_catalog_sync",
  "mode": "catalog_and_probe_links",
  "limit": 1000
}
```

### List jobs

- `GET /api/v2/catalog-sync/jobs?state=running`

### Get job status

- `GET /api/v2/catalog-sync/jobs/{job_id}`

### Create periodic schedule

- `POST /api/v2/catalog-sync/schedules`

```json
{
  "job_type": "rom_catalog_sync",
  "mode": "catalog_and_probe_links",
  "cron": "0 */6 * * *",
  "enabled": true,
  "catch_up": false
}
```

Create schedule success `data` example:

```json
{
  "schedule_id": "sch_01H...",
  "job_type": "rom_catalog_sync",
  "mode": "catalog_and_probe_links",
  "cron": "0 */6 * * *",
  "enabled": true,
  "catch_up": false,
  "created_at_us": 1710006000000,
  "updated_at_us": 1710006000000,
  "last_run_at_us": null,
  "next_run_at_us": 1710007200000,
  "last_result": "none",
  "last_error_code": null
}
```

### List schedules

- `GET /api/v2/catalog-sync/schedules`

### Get schedule

- `GET /api/v2/catalog-sync/schedules/{schedule_id}`

Get schedule response `data` example:

```json
{
  "scheduler_now_us": 1710006600000,
  "schedule": {
    "schedule_id": "sch_01H...",
    "job_type": "rom_catalog_sync",
    "mode": "catalog_and_probe_links",
    "cron": "0 */6 * * *",
    "enabled": true,
    "catch_up": false,
    "created_at_us": 1710006000000,
    "updated_at_us": 1710006000000,
    "last_run_at_us": 1710003600000,
    "next_run_at_us": 1710007200000,
    "last_result": "success",
    "last_error_code": null
  }
}
```

### Update schedule

- `PATCH /api/v2/catalog-sync/schedules/{schedule_id}`

```json
{
  "mode": "catalog_probe_and_prefetch_missing",
  "cron": "0 */12 * * *",
  "enabled": true,
  "catch_up": true
}
```

Update semantics:

- Partial update (`PATCH`) only mutates provided fields; unspecified fields remain unchanged.
- `updated_at_us` must advance monotonically for every successful update.
- If `cron` is changed, `next_run_at_us` is recomputed from `scheduler_now_us` and persisted atomically with updated fields.
- `enabled` transitions:
  - `true -> false`: scheduler must cancel pending due-dispatch for that schedule.
  - `false -> true`: scheduler recomputes and persists next due boundary before re-activation.

Update schedule success `data` example:

```json
{
  "schedule_id": "sch_01H...",
  "job_type": "rom_catalog_sync",
  "mode": "catalog_probe_and_prefetch_missing",
  "cron": "0 */12 * * *",
  "enabled": true,
  "catch_up": true,
  "updated_at_us": 1710006900000,
  "next_run_at_us": 1710010800000
}
```

List schedules response `data` example:

```json
{
  "scheduler_now_us": 1710006600000,
  "schedules": [
    {
      "schedule_id": "sch_01H...",
      "job_type": "rom_catalog_sync",
      "mode": "catalog_and_probe_links",
      "cron": "0 */6 * * *",
      "enabled": true,
      "catch_up": false,
      "last_run_at_us": 1710003600000,
      "next_run_at_us": 1710007200000,
      "last_result": "success",
      "last_error_code": null
    }
  ]
}
```

### Delete schedule

- `DELETE /api/v2/catalog-sync/schedules/{schedule_id}`

Reboot-recovery validation contract:

- On startup, scheduler must load `/sdcard/config/engine_v2/catalog_sync_schedules.json` before accepting schedule mutations.
- Recovery validation pass for each persisted schedule must verify:
  - required fields present and type-valid,
  - `job_type` and `mode` are recognized enums,
  - `cron` parses successfully,
  - `next_run_at_us` is recomputed when stale (`next_run_at_us < scheduler_now_us`).
- Invalid persisted records must be quarantined from active dispatch and exposed in recovery report telemetry.
- Startup recovery response/report fields:
  - `recovery_run_id` (string)
  - `loaded` (uint32)
  - `validated` (uint32)
  - `recomputed_next_run` (uint32)
  - `quarantined` (uint32)

Reboot recovery report `data` example:

```json
{
  "recovery_run_id": "sched_recover_01H...",
  "scheduler_now_us": 1710009000000,
  "loaded": 4,
  "validated": 3,
  "recomputed_next_run": 2,
  "quarantined": 1,
  "quarantine": [
    {
      "schedule_id": "sch_bad_01H...",
      "error_code": "SCRAPER_SCHEDULE_INVALID",
      "reason": "cron parse failed"
    }
  ]
}
```

Deterministic scheduler blockers:

- Invalid cron expression, unknown `job_type`, or unknown `mode` -> `SCRAPER_SCHEDULE_INVALID`.
- Persisted schedule store read/write failure -> `CATALOG_SYNC_FAILED`.
- Duplicate schedule identity (same `job_type` + `mode` + `cron`) -> `CONFLICT`.
- Schedule ID not found on delete/read/update paths -> `SCRAPER_JOB_NOT_FOUND`.
- Recovery validation failure for persisted schedule record -> `SCRAPER_SCHEDULE_INVALID` (record quarantined) and recovery proceeds for valid records.

Scheduler blocker example (invalid cron):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710006000001,
  "error": {
    "code": "SCRAPER_SCHEDULE_INVALID",
    "category": "scheduler",
    "message": "Invalid cron expression",
    "retryable": false,
    "details": {
      "field": "cron",
      "value": "61 * * * *",
      "reason": "minute value out of range"
    }
  }
}
```

---
