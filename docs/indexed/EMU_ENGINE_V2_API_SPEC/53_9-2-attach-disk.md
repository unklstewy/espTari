# 9.2 Attach disk

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 53

## 9.2 Attach disk

- `POST /api/v2/media/disk/attach`

```json
{
  "session_id": "ses_01H...",
  "drive": "A",
  "disk_id": "disk.game.x",
  "write_protect": true
}
```

If `disk_id` exists in catalog but local file is missing, implementation may return `CONFLICT` with remediation hint to call `/api/v2/catalogs/floppies/download-entry`.

Disk attach request validation contract:

- `session_id` is required and must reference an active session.
- `drive` is required and must be one of `A` or `B`.
- `disk_id` is required and must be a non-empty string.
- `write_protect` is optional boolean (defaults to profile/runtime policy when omitted).

Disk catalog binding checks (`POST /api/v2/media/disk/attach`):

- `disk_id` must resolve through disk catalog binding (`catalog=disk`) before attach.
- Resolved entry must provide a usable local artifact path and supported disk image format.
- Binding/format checks must complete before runtime media mutation; failures are deterministic no-op.

Disk attach success `data` example:

```json
{
  "session_id": "ses_01H...",
  "drive": "A",
  "disk": {
    "id": "disk.game.x",
    "catalog": "disk",
    "local_path": "/sdcard/disks/st/GAME_X.ST",
    "format": "st",
    "write_protect": true,
    "binding_result": "matched"
  },
  "attached_at_us": 1710003100100
}
```

Disk mount runtime flow (deterministic):

- Attach runtime phases are ordered and transactional:
  1. `validated`
  2. `mounted`
  3. `active`
- `mounted` indicates artifact bound to drive slot; `active` indicates runtime media map committed for I/O path.
- Phase progression must be monotonic with no skipped phases.
- If `mounted` succeeds but `active` commit fails, runtime must rollback drive mapping to previous disk state and return terminal failure.

Disk attach flow `data` example:

```json
{
  "session_id": "ses_01H...",
  "drive": "A",
  "disk_id": "disk.game.x",
  "result": "active",
  "phase_history": ["validated", "mounted", "active"],
  "mount": {
    "mounted_at_us": 1710003100102,
    "mounted_path": "/sdcard/disks/st/GAME_X.ST"
  },
  "runtime": {
    "active_at_us": 1710003100103,
    "generation": 73
  }
}
```

Disk state events (attach/eject):

- Runtime emits `media_disk_state` events on `GET /api/v2/engine/stream` for mount/eject state transitions.
- Required event fields:
  - `type` (must be `media_disk_state`)
  - `schema_version` (uint32)
  - `session_id` (string)
  - `event_seq` (uint64)
  - `event_timestamp_us` (uint64)
  - `drive` (enum: `A`, `B`)
  - `state` (enum: `empty`, `mounted`, `active`, `ejected`, `failed`)
  - `disk_id` (string or `null`)
  - `request_id` (string)

Disk state event example (`active`):

```json
{
  "type": "media_disk_state",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2401,
  "event_timestamp_us": 1710003100103,
  "drive": "A",
  "state": "active",
  "disk_id": "disk.game.x",
  "request_id": "req_01H..."
}
```

Deterministic disk-attach blockers:

- Missing/blank `disk_id` or invalid `drive` -> `BAD_REQUEST`.
- Inactive/unknown session -> `ENGINE_NOT_RUNNING`.
- Disk catalog unavailable or unresolved entry -> `CATALOG_NOT_FOUND` / `CATALOG_ENTRY_NOT_FOUND`.
- Catalog binding mismatch (`expected catalog=disk`) -> `BAD_REQUEST`.
- Local artifact unavailable with no fetch policy allowed -> `CONFLICT`.
- Unsupported disk media format for runtime attach -> `UNSUPPORTED_MEDIA_FORMAT`.
- Runtime mount/activation failure after validation -> `MEDIA_ATTACH_FAILED`.

Disk attach blocker example (invalid drive):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710003100101,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "Drive must be one of A or B",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "field": "drive",
      "actual": "C",
      "allowed": ["A", "B"]
    }
  }
}
```
