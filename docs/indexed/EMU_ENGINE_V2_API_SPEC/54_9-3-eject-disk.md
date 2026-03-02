# 9.3 Eject disk

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 54

## 9.3 Eject disk

- `POST /api/v2/media/disk/eject`

```json
{
  "session_id": "ses_01H...",
  "drive": "A"
}
```

Disk eject request validation contract:

- `session_id` is required and must reference an active session.
- `drive` is required and must be one of `A` or `B`.
- Eject request for a drive with no mounted media is deterministic no-op success.

Disk eject success `data` example:

```json
{
  "session_id": "ses_01H...",
  "drive": "A",
  "result": "ejected",
  "ejected_disk_id": "disk.game.x",
  "ejected_at_us": 1710003100200
}
```

Disk eject no-op `data` example:

```json
{
  "session_id": "ses_01H...",
  "drive": "B",
  "result": "no_op",
  "ejected_disk_id": null,
  "ejected_at_us": 1710003100201
}
```

Disk eject runtime flow (deterministic):

- Eject runtime phases are ordered:
  1. `detached` (drive mapping removed)
  2. `ejected` (runtime state committed + drive reported empty)
- Eject no-op follows deterministic terminal phase `ejected` with `result=no_op`.

Disk eject flow `data` example (`ejected`):

```json
{
  "session_id": "ses_01H...",
  "drive": "A",
  "result": "ejected",
  "phase_history": ["detached", "ejected"],
  "ejected_disk_id": "disk.game.x",
  "ejected_at_us": 1710003100200
}
```

Disk state event example (`ejected`):

```json
{
  "type": "media_disk_state",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2402,
  "event_timestamp_us": 1710003100200,
  "drive": "A",
  "state": "ejected",
  "disk_id": null,
  "request_id": "req_01H..."
}
```

Deterministic disk-eject blockers:

- Invalid/missing `drive` -> `BAD_REQUEST`.
- Inactive/unknown session -> `ENGINE_NOT_RUNNING`.

Disk eject blocker example (invalid drive):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710003100202,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "Drive must be one of A or B",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "field": "drive",
      "actual": "0",
      "allowed": ["A", "B"]
    }
  }
}
```
