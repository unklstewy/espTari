# 6.7 Suspend and save session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 23

## 6.7 Suspend and save session

- `POST /api/v2/engine/session/suspend-save`

Suspend-save request wiring contract:

- Request payload is validated by `suspend_save_request_v1`.
- `suspend_save_request_v1` required fields:
  - `session_id` (string, required)
  - `name` (string, optional)
  - `auto_resume` (boolean, optional, default `false`)
  - `include_stream_state` (boolean, optional, default `true`)
  - `reason` (string, optional)

Suspend-save response wiring contract:

- Response `data` is validated by `suspend_save_response_v1`.
- `suspend_save_response_v1` required fields:
  - `session_id` (string)
  - `state` (must equal `suspended`)
  - `snapshot_id` (string)
  - `saved_at_us` (uint64)
  - `lifecycle_transition` (string, must equal `running->suspended`)

Suspend-save transition checks:

- `SUSP-REQ-01`: request is accepted only when current lifecycle state is `running`.
- `SUSP-REQ-02`: if lifecycle is transitional (`starting`, `stopping`) request must be rejected with `INVALID_SESSION_STATE` and no snapshot is created.
- `SUSP-REQ-03`: when accepted, snapshot persistence completes before lifecycle state commit to `suspended`.
- `SUSP-REQ-04`: successful suspend-save commits `state=suspended` and `saved_at_us == runtime.last_transition_at_us` in status telemetry.

Suspend-save deterministic guard failures:

- Missing/invalid payload fields (`session_id`, invalid booleans, malformed `name`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Lifecycle state outside allowed source state (`running`) -> `INVALID_SESSION_STATE`.
- Snapshot persistence backend unavailable or save operation unresolved -> `INTERNAL_ERROR`.

Request:

```json
{
  "session_id": "ses_01H...",
  "name": "debug_pre_dma",
  "auto_resume": false,
  "include_stream_state": true,
  "reason": "operator_suspend"
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "suspended",
  "snapshot_id": "snap_01H...",
  "saved_at_us": 1710000004321,
  "lifecycle_transition": "running->suspended"
}
```

Suspend-save invalid-state error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000004333,
  "error": {
    "code": "INVALID_SESSION_STATE",
    "category": "engine",
    "message": "suspend-save is allowed only from running state",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/engine/session/suspend-save",
      "guard_id": "SUSP-REQ-01",
      "current_state": "paused"
    }
  }
}
```
