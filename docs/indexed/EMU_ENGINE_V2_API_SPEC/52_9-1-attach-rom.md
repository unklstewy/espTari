# 9.1 Attach ROM

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 52

## 9.1 Attach ROM

- `POST /api/v2/media/rom/attach`

```json
{
  "session_id": "ses_01H...",
  "rom_id": "rom.tos.1.04.uk"
}
```

`rom_id` is catalog-backed and may refer to TOS images where configured by machine profile.

ROM attach request validation contract:

- `session_id` is required and must reference an active session.
- `rom_id` is required and must be a non-empty string.
- Attach request is evaluated against active machine/profile compatibility before media mutation.

Catalog binding checks (`POST /api/v2/media/rom/attach`):

- `rom_id` must resolve through ROM catalog binding (`catalog=rom`) before attach.
- Resolved catalog entry must provide a usable local artifact path for runtime attach.
- If machine profile constrains ROM class (for example TOS-compatible image requirement), resolved entry metadata must satisfy that binding policy.
- Binding checks execute before runtime attach side-effects; failures are deterministic and no-op.

ROM attach success `data` example:

```json
{
  "session_id": "ses_01H...",
  "rom": {
    "id": "rom.tos.1.04.uk",
    "catalog": "rom",
    "local_path": "/sdcard/roms/st/tos104uk.rom",
    "sha256": "sha256:...",
    "size": 196608,
    "binding": {
      "machine": "atari_st",
      "profile": "st_520_pal",
      "binding_result": "matched"
    }
  },
  "attached_at_us": 1710003000100
}
```

ROM mount/apply flow (deterministic):

- Attach operation phases are ordered and transactional:
  1. `validated` (request + catalog binding checks passed)
  2. `mounted` (ROM artifact mounted/loaded into runtime media slot)
  3. `applied` (active machine runtime switched to mounted ROM)
- Phase progression must be monotonic and must not skip intermediate states.
- If `mounted` succeeds but `applied` fails, runtime must rollback to prior active ROM and report `result=failed` with `failed_phase="applied"`.

Attach response `data` example (flow projection):

```json
{
  "session_id": "ses_01H...",
  "rom_id": "rom.tos.1.04.uk",
  "result": "applied",
  "phase": "applied",
  "phase_history": ["validated", "mounted", "applied"],
  "mount": {
    "slot": "rom.primary",
    "mounted_path": "/sdcard/roms/st/tos104uk.rom",
    "mounted_at_us": 1710003000102
  },
  "apply": {
    "applied_at_us": 1710003000103,
    "runtime_generation": 42
  }
}
```

ROM attach status events:

- Attach flow emits `media_attach_status` events on engine stream (`GET /api/v2/engine/stream`) for each phase transition and terminal result.
- Required event fields:
  - `type` (must be `media_attach_status`)
  - `schema_version` (uint32)
  - `session_id` (string)
  - `event_seq` (uint64)
  - `event_timestamp_us` (uint64)
  - `media_type` (must be `rom`)
  - `media_id` (string)
  - `phase` (enum: `validated`, `mounted`, `applied`, `failed`)
  - `result` (enum: `in_progress`, `applied`, `failed`)
  - `request_id` (string)

Attach status event example (`mounted`):

```json
{
  "type": "media_attach_status",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2201,
  "event_timestamp_us": 1710003000102,
  "media_type": "rom",
  "media_id": "rom.tos.1.04.uk",
  "phase": "mounted",
  "result": "in_progress",
  "request_id": "req_01H..."
}
```

Attach status event example (`failed`):

```json
{
  "type": "media_attach_status",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2202,
  "event_timestamp_us": 1710003000104,
  "media_type": "rom",
  "media_id": "rom.tos.1.04.uk",
  "phase": "failed",
  "result": "failed",
  "request_id": "req_01H...",
  "error": {
    "code": "MEDIA_ATTACH_FAILED",
    "message": "ROM apply phase failed; previous ROM restored"
  }
}
```

Deterministic ROM-attach blockers:

- Missing/blank `rom_id` -> `BAD_REQUEST`.
- Inactive/unknown session -> `ENGINE_NOT_RUNNING`.
- ROM catalog unavailable or unresolved -> `CATALOG_NOT_FOUND` / `CATALOG_ENTRY_NOT_FOUND`.
- Resolved entry fails ROM catalog binding or machine/profile compatibility checks -> `BAD_REQUEST`.
- Catalog entry exists but local artifact is unavailable and no fetch policy is allowed -> `CONFLICT`.
- Runtime mount/apply phase failure after successful validation -> `MEDIA_ATTACH_FAILED`.

ROM attach blocker example (catalog binding mismatch):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710003000101,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "ROM attach catalog binding check failed",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "rom_id": "rom.tos.1.04.uk",
      "expected_catalog": "rom",
      "actual_catalog": "disk",
      "validation_stage": "rom_attach_catalog_binding"
    }
  }
}
```
