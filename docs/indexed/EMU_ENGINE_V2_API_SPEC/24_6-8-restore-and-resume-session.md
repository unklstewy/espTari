# 6.8 Restore and resume session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 24

## 6.8 Restore and resume session

- `POST /api/v2/engine/session/restore-resume`

Restore-resume request wiring contract:

- Request payload is validated by `restore_resume_request_v1`.
- `restore_resume_request_v1` required fields:
  - `session_id` (string, required)
  - `snapshot_id` (string, required)
  - `resume_mode` (enum: `running`, `paused`, required)
  - `reason` (string, optional)

Restore-resume response wiring contract:

- Response `data` is validated by `restore_resume_response_v1`.
- `restore_resume_response_v1` required fields:
  - `session_id` (string)
  - `snapshot_id` (string)
  - `state` (enum: `running`, `paused`)
  - `restored_at_us` (uint64)
  - `lifecycle_transition` (enum: `suspended->running`, `suspended->paused`)

Restore-resume transition checks:

- `REST-RES-01`: request is accepted only when current lifecycle state is `suspended`.
- `REST-RES-02`: `resume_mode=running` must produce `state=running` and transition `suspended->running`; `resume_mode=paused` must produce `state=paused` and transition `suspended->paused`.
- `REST-RES-03`: snapshot compatibility validation (profile/schema/ABI) must complete before lifecycle state commit.
- `REST-RES-04`: successful restore-resume sets `restored_at_us == runtime.last_transition_at_us` in status telemetry.

Restore-resume deterministic guard failures:

- Missing/invalid payload fields (`session_id`, `snapshot_id`, `resume_mode`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Session not currently suspended -> `ENGINE_NOT_SUSPENDED`.
- Snapshot lookup/compatibility failures -> `SNAPSHOT_NOT_FOUND` or `SNAPSHOT_INCOMPATIBLE`.
- Restore operation backend failure after accepted guards -> `SNAPSHOT_RESTORE_FAILED`.

Request:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "resume_mode": "running",
  "reason": "operator_resume"
}
```

`resume_mode` values:

- `running`
- `paused`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "state": "running",
  "restored_at_us": 1710000004988,
  "lifecycle_transition": "suspended->running"
}
```

Restore-resume incompatible snapshot error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000004992,
  "error": {
    "code": "SNAPSHOT_INCOMPATIBLE",
    "category": "snapshot",
    "message": "Snapshot profile is incompatible with current session",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "snapshot_id": "snap_01H...",
      "endpoint": "/api/v2/engine/session/restore-resume",
      "guard_id": "REST-RES-03"
    }
  }
}
```
