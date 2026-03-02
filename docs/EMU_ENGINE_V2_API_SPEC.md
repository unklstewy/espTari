# espTari Emulation Engine v2 — API Specification (Atari ST First)

Version: 1.0  
Date: 2026-03-01  
Status: Normative API contract for backend phase  
Scope: REST + WebSocket APIs for engine control, SD-card file management, EBIN management, media control, input translation, streaming output, and runtime inspection

---

## 1. Purpose

This document defines the concrete API contract for Emulation Engine v2 and complements:

- `EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md`
- `emu_engine_v2/README.md`

Initial machine target:

- Atari ST baseline profile (`atari_st`)

This API is intentionally machine-extensible for later Mega ST / STe / Mega STe profiles.

---

## 2. Protocols and versioning

## 2.1 Transports

- REST: HTTP/1.1 JSON endpoints
- Streaming: WebSocket JSON envelope + binary payload frames where noted

## 2.2 Base path

- `/api/v2`

## 2.3 Content types

- Request JSON: `application/json`
- Response JSON: `application/json`
- File upload: `multipart/form-data` (or chunked upload JSON API)
- Video/audio stream packets: WebSocket binary with JSON sideband metadata

## 2.4 Versioning policy

- Backward-compatible additions allowed in minor versions.
- Breaking changes require new major path (`/api/v3`).
- Stream envelope contains `schema_version`.

---

## 3. Security and access model

## 3.1 Authentication modes

Implementations may run with one of:

- `dev-open` (LAN test mode)
- `token` (bearer token)

Recommended production mode:

- Bearer token in `Authorization: Bearer <token>`

## 3.2 Authorization scopes

Minimum scopes:

- `engine:control`
- `files:read`
- `files:write`
- `ebin:manage`
- `inspect:read`
- `stream:read`
- `input:read`
- `input:write`

## 3.3 Path hardening

All file endpoints must enforce:

- SD-card allowlist roots only
- path normalization
- traversal prevention

---

## 4. Common REST response envelope

Canonical contract rules:

- Every REST endpoint returns exactly one envelope shape for success (`ok=true`) and one for error (`ok=false`).
- `request_id` and `timestamp_us` are mandatory on both success and error responses.
- Endpoint sections define only the `data` payload for success and must not redefine top-level envelope fields.
- Lifecycle and status contracts in sections `6.1` through `6.8` and `12` are bound to this canonical envelope.

Success:

```json
{
  "ok": true,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000000,
  "data": {}
}
```

Error:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000000,
  "error": {
    "code": "EBIN_ABI_MISMATCH",
    "category": "ebin",
    "message": "Requested module ABI 2.1 is incompatible with engine ABI 2.0",
    "retryable": false,
    "details": {
      "module_id": "st.cpu.m68k",
      "required": "2.1.x",
      "actual": "2.0.4"
    }
  }
}
```

---

## 5. Common error code catalog

Error taxonomy requirements:

- Error object required fields: `code`, `category`, `message`, `retryable`, `details`.
- Deterministic namespace rule: `code` is stable `UPPER_SNAKE_CASE` and is immutable once published.
- Deterministic category set: `request`, `auth`, `path`, `engine`, `media`, `ebin`, `stream`, `input`, `snapshot`, `debug`, `catalog`, `scheduler`, `internal`.
- Unknown errors must map to `INTERNAL_ERROR` with `category=internal`.

Code to category mapping:

- `request`: `BAD_REQUEST`
- `auth`: `UNAUTHORIZED`, `FORBIDDEN`
- `path`: `NOT_FOUND`, `PATH_NOT_ALLOWED`
- `engine`: `CONFLICT`, `ENGINE_NOT_RUNNING`, `ENGINE_ALREADY_RUNNING`, `INVALID_SESSION_STATE`, `MACHINE_PROFILE_NOT_FOUND`, `ENGINE_NOT_SUSPENDED`
- `media`: `UNSUPPORTED_MEDIA_FORMAT`, `UPLOAD_INCOMPLETE`, `MEDIA_ATTACH_FAILED`
- `ebin`: `EBIN_NOT_FOUND`, `EBIN_INVALID`, `EBIN_SIGNATURE_INVALID`, `EBIN_ABI_MISMATCH`, `EBIN_DEPENDENCY_MISSING`
- `stream`: `STREAM_BACKPRESSURE`, `INSPECT_FILTER_INVALID`
- `input`: `INPUT_DEVICE_UNAVAILABLE`, `INPUT_MAPPING_NOT_FOUND`, `INPUT_EVENT_INVALID`, `INPUT_CAPTURE_DISABLED`, `INPUT_CAPTURE_NOT_ACTIVE`, `INPUT_POLICY_INVALID_STATE`, `INPUT_POLICY_VIOLATION`, `INPUT_POLICY_MODE_INVALID`, `INPUT_POLICY_SESSION_INVALID`
- `snapshot`: `SNAPSHOT_NOT_FOUND`, `SNAPSHOT_SAVE_FAILED`, `SNAPSHOT_RESTORE_FAILED`, `SNAPSHOT_INCOMPATIBLE`
- `debug`: `DEBUG_CLOCK_INVALID`
- `catalog`: `CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CATALOG_LINK_DEAD`, `CATALOG_SYNC_FAILED`
- `scheduler`: `SCRAPER_JOB_NOT_FOUND`, `SCRAPER_SCHEDULE_INVALID`
- `internal`: `INTERNAL_ERROR`

- `BAD_REQUEST`
- `UNAUTHORIZED`
- `FORBIDDEN`
- `NOT_FOUND`
- `CONFLICT`
- `UNSUPPORTED_MEDIA_FORMAT`
- `PATH_NOT_ALLOWED`
- `UPLOAD_INCOMPLETE`
- `ENGINE_NOT_RUNNING`
- `ENGINE_ALREADY_RUNNING`
- `INVALID_SESSION_STATE`
- `MACHINE_PROFILE_NOT_FOUND`
- `MEDIA_ATTACH_FAILED`
- `EBIN_NOT_FOUND`
- `EBIN_INVALID`
- `EBIN_SIGNATURE_INVALID`
- `EBIN_ABI_MISMATCH`
- `EBIN_DEPENDENCY_MISSING`
- `STREAM_BACKPRESSURE`
- `INSPECT_FILTER_INVALID`
- `INPUT_DEVICE_UNAVAILABLE`
- `INPUT_MAPPING_NOT_FOUND`
- `INPUT_EVENT_INVALID`
- `INPUT_CAPTURE_DISABLED`
- `INPUT_CAPTURE_NOT_ACTIVE`
- `INPUT_POLICY_INVALID_STATE`
- `INPUT_POLICY_VIOLATION`
- `INPUT_POLICY_MODE_INVALID`
- `INPUT_POLICY_SESSION_INVALID`
- `SNAPSHOT_NOT_FOUND`
- `SNAPSHOT_SAVE_FAILED`
- `SNAPSHOT_RESTORE_FAILED`
- `SNAPSHOT_INCOMPATIBLE`
- `ENGINE_NOT_SUSPENDED`
- `ENGINE_SUBSYSTEM_UNAVAILABLE`
- `DEBUG_CLOCK_INVALID`
- `CATALOG_NOT_FOUND`
- `CATALOG_ENTRY_NOT_FOUND`
- `CATALOG_LINK_DEAD`
- `CATALOG_SYNC_FAILED`
- `SCRAPER_JOB_NOT_FOUND`
- `SCRAPER_SCHEDULE_INVALID`
- `INTERNAL_ERROR`

---

## 6. Engine control APIs

## 6.0 Baseline API schema bundle

This baseline bundle is the authoritative contract set for downstream control-plane tasks.

Bundle contents:

- Canonical REST success/error envelope: section `4`
- Canonical error taxonomy and deterministic codes: section `5`
- Lifecycle endpoint schemas and guard inputs: sections `6.1` through `6.6B`
- Transition model and invalid-transition constraints: section `12`

Representative baseline endpoint-family examples:

- Start session (`6.1`): request + success response `data`
- Stop session (`6.2`): request + success response `data`
- Pause session (`6.3`): request + success response `data`
- Resume session (`6.4`): request + success response `data`
- Reset session (`6.5`): request + success response `data`
- Session state read (`6.6`): success response `data`
- Session status (`6.6B`): success response `data` + lifecycle-mode semantics
- Invalid transition (`6`): canonical error envelope example with `code=INVALID_SESSION_STATE`

Cross-reference:

- Implementation architecture anchor: `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` section `6` and `6.1`

Control contract binding:

- Sections `6.1` through `6.8` are lifecycle contracts and use the canonical success envelope in section `4` and canonical error envelope/taxonomy in section `5`.
- Lifecycle endpoint sections define only request fields and success `data` payload semantics.

Lifecycle guard input matrix (`6.1` through `6.5`):

| Endpoint | Required fields | Optional fields | Allowed current state(s) | Success target state | Invalid transition error code |
|---|---|---|---|---|---|
| `POST /api/v2/engine/session` | `machine`, `profile`, `rom_id` | `disk_ids`, `cartridge_id`, `module_overrides`, `stream_defaults`, `input_defaults` | `stopped` | `running` | `INVALID_SESSION_STATE` |
| `POST /api/v2/engine/session/stop` | `session_id` | `reason` | `running`, `paused`, `suspended`, `faulted` | `stopped` | `INVALID_SESSION_STATE` |
| `POST /api/v2/engine/session/pause` | `session_id` | `reason` | `running` | `paused` | `INVALID_SESSION_STATE` |
| `POST /api/v2/engine/session/resume` | `session_id` | `resume_mode` | `paused`, `suspended` | `running` or `paused` (if `resume_mode=paused`) | `INVALID_SESSION_STATE` |
| `POST /api/v2/engine/session/reset` | `session_id` | `mode`, `preserve_media` | `running`, `paused` | `running` | `INVALID_SESSION_STATE` |

Invalid transition response shape (canonical error envelope):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000004321,
  "error": {
    "code": "INVALID_SESSION_STATE",
    "category": "engine",
    "message": "Cannot pause session from state stopped",
    "retryable": false,
    "details": {
      "endpoint": "/api/v2/engine/session/pause",
      "current_state": "stopped",
      "allowed_states": ["running"],
      "requested_transition": "stopped->paused"
    }
  }
}
```

## 6.1 Start session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session`

Request:

```json
{
  "machine": "atari_st",
  "profile": "st_520_pal",
  "rom_id": "rom.tos.1.04.uk",
  "tos_id": "tos.1.04.uk",
  "disk_ids": ["disk.game.x"],
  "cartridge_id": null,
  "module_overrides": {
    "cpu": "st.cpu.m68k@2.0.0"
  },
  "stream_defaults": {
    "video": true,
    "audio": true,
    "input_stream": true,
    "inspect_registers": false,
    "inspect_bus": false,
    "inspect_memory": false
  },
  "input_defaults": {
    "input_enabled": true,
    "capture_mode": "click_to_capture",
    "escape_release": {
      "enabled": true,
      "sequence": ["Escape", "Escape"],
      "timeout_ms": 600
    }
  }
}
```

ST profile manifest parser contract (`machine=atari_st`):

- Start-session profile resolution must parse manifest from canonical path:
  - `/sdcard/config/engine_v2/machines/atari_st/<profile>.json`.
- Parser output is a normalized `profile_manifest` object bound to session bootstrap.
- Parser must be deterministic for identical file bytes and requested profile.

Profile manifest schema (minimum required fields):

- `manifest_version` (uint32)
- `machine` (must be `atari_st` for this profile family)
- `profile` (string; must equal requested `profile`)
- `region` (enum: `pal`, `ntsc`)
- `ram_kb` (uint32)
- `modules` (object)
  - required keys: `cpu`, `video`, `io`, `storage`, `machine_profile`
  - values: module selectors (`<module_id>@<version>`)
- `scheduler` (object)
  - required keys: `tick_hz`, `step_order`

Manifest parse/validation result projection (`data.profile_manifest_validation`):

- `path` (string)
- `manifest_version` (uint32)
- `schema_valid` (bool)
- `normalized_profile` (string)
- `validated_at_us` (uint64)

Manifest validation success excerpt:

```json
{
  "session_id": "ses_01H...",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "profile_manifest_validation": {
    "path": "/sdcard/config/engine_v2/machines/atari_st/st_520_pal.json",
    "manifest_version": 1,
    "schema_valid": true,
    "normalized_profile": "st_520_pal",
    "validated_at_us": 1710000000003
  }
}
```

Deterministic manifest blockers:

- Requested profile manifest path not found -> `MACHINE_PROFILE_NOT_FOUND`.
- Manifest payload cannot be parsed as JSON or fails required schema fields -> `BAD_REQUEST`.
- Manifest machine/profile mismatch against start request -> `BAD_REQUEST`.

Profile wiring validation contract (post-schema, pre-runtime bootstrap):

- Wiring validator must execute after manifest schema validation and before any runtime component initialization.
- Validator input: normalized `profile_manifest.modules` + `profile_manifest.scheduler.step_order`.
- Required wiring checks:
  - `step_order` references only declared module keys.
  - No duplicate module keys in resolved wiring plan.
  - Module selectors resolve to loadable EBIN/module artifacts.
  - Declared module ABI compatibility satisfies machine-profile requirements.
- Validation output projection (`data.profile_wiring_validation`):
  - `wiring_valid` (bool)
  - `validated_modules` (uint32)
  - `step_order_length` (uint32)
  - `validated_at_us` (uint64)

Fail-fast diagnostics behavior:

- Any wiring validation failure must abort session start before transition to `running`.
- Fail-fast responses must include `details.validation_stage="profile_wiring"` and deterministic reason tokens.
- No partial runtime bootstrap side effects are permitted when fail-fast wiring validation triggers.

Wiring validation success excerpt:

```json
{
  "session_id": "ses_01H...",
  "profile_wiring_validation": {
    "wiring_valid": true,
    "validated_modules": 5,
    "step_order_length": 5,
    "validated_at_us": 1710000000005
  }
}
```

Deterministic wiring blockers:

- Wiring references unresolved module selector -> `EBIN_NOT_FOUND`.
- Wiring module ABI/version incompatible with profile requirements -> `EBIN_ABI_MISMATCH`.
- Wiring dependency missing for referenced module chain -> `EBIN_DEPENDENCY_MISSING`.
- Invalid/duplicate `scheduler.step_order` entries -> `BAD_REQUEST`.

Wiring blocker example (ABI mismatch fail-fast):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000006,
  "error": {
    "code": "EBIN_ABI_MISMATCH",
    "category": "ebin",
    "message": "Profile wiring validation failed for module ABI compatibility",
    "retryable": false,
    "details": {
      "validation_stage": "profile_wiring",
      "profile": "st_520_pal",
      "module_key": "cpu",
      "module_selector": "st.cpu.m68k@1.0.0",
      "required_abi": "2.0",
      "actual_abi": "1.0"
    }
  }
}
```

Manifest blocker example (schema invalid):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000004,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "Profile manifest failed schema validation",
    "retryable": false,
    "details": {
      "machine": "atari_st",
      "profile": "st_520_pal",
      "path": "/sdcard/config/engine_v2/machines/atari_st/st_520_pal.json",
      "missing_fields": ["scheduler.step_order"],
      "validator": "profile_manifest_schema_v1"
    }
  }
}
```

Media ID resolution rule:

- `rom_id`, `disk_ids`, `tos_id`, and `cartridge_id` resolve via catalog indexes (rom/floppy/tos/cartridge catalogs).
- If an ID exists in a catalog but file is missing locally, server may require prefetch (`CONFLICT`) or auto-fetch according to session policy.

Catalog index loader contract (`rom_id` / `disk_id` / `tos_id`):

- Loader inputs:
  - `rom_id` (required for start flow)
  - `disk_ids[]` (optional multi-resolution)
  - `tos_id` (optional; machine-profile override for TOS image selection)
- Loader index sources:
  - `rom_catalog.json` for `rom_id`
  - `disk_catalog.json` for `disk_id` values
  - `tos_catalog.json` for `tos_id`
- Loader normalized output per resolved ID:
  - `id` (catalog ID)
  - `catalog` (`rom`, `disk`, `tos`)
  - `local_path`
  - `sha256`
  - `size`
  - `availability_state`

Deterministic dependency blockers:

- Missing required catalog index file (`rom_catalog.json`, `disk_catalog.json`, `tos_catalog.json`) -> `CATALOG_NOT_FOUND`.
- Requested ID absent from loaded index -> `CATALOG_ENTRY_NOT_FOUND`.
- Catalog entry exists but local asset missing and no auto-fetch permitted -> `CONFLICT` with remediation hint to relevant `/api/v2/catalogs/{catalog}/download-entry`.

Catalog-backed ID resolution API path (deterministic):

| Endpoint | ID field(s) | Catalog source | Resolution stage order | Success projection | Deterministic failure mapping |
|---|---|---|---|---|---|
| `POST /api/v2/engine/session` | `rom_id`, `disk_ids[]`, `tos_id`, `cartridge_id` | `rom`, `disk`, `tos`, `cartridge` catalogs | index load -> entry lookup -> local presence check -> optional prefetch policy | `resolved_media` in start response | `CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CONFLICT` |
| `POST /api/v2/media/rom/attach` | `rom_id` | `rom` catalog | index load -> entry lookup -> local presence check -> optional prefetch policy | attached ROM media descriptor | `CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CONFLICT` |
| `POST /api/v2/media/disk/attach` | `disk_id` | `disk` catalog | index load -> entry lookup -> local presence check -> optional prefetch policy | attached disk media descriptor | `CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CONFLICT` |

Resolution-stage contract rules:

- ID resolution must always execute through catalog indexes before any direct path access.
- Optional prefetch policy may convert a local-missing condition from immediate failure into deferred/synchronous download flow; if policy disallows prefetch, response must be `CONFLICT`.
- Failure mapping must be stable across all catalog-backed endpoints listed above.

Resolution evidence (start-session data excerpt):

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "resolved_media": {
    "rom": {
      "id": "rom.tos.1.04.uk",
      "catalog": "rom",
      "local_path": "/sdcard/roms/st/tos104uk.rom",
      "sha256": "sha256:...",
      "size": 196608,
      "availability_state": "local_only"
    },
    "tos": {
      "id": "tos.1.04.uk",
      "catalog": "tos",
      "local_path": "/sdcard/roms/st/tos104uk.rom",
      "sha256": "sha256:...",
      "size": 196608,
      "availability_state": "local_only"
    },
    "disks": [
      {
        "id": "disk.game.x",
        "catalog": "disk",
        "local_path": "/sdcard/disks/st/GAME_X.ST",
        "sha256": "sha256:...",
        "size": 737280,
        "availability_state": "online"
      }
    ]
  }
}
```

Resolution blocker example (missing local file with prefetch disabled):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000002,
  "error": {
    "code": "CONFLICT",
    "category": "engine",
    "message": "Catalog entry disk.game.x exists but local asset is missing",
    "retryable": true,
    "details": {
      "catalog": "disk",
      "entry_id": "disk.game.x",
      "request_field": "disk_id",
      "remediation": "POST /api/v2/catalogs/floppies/download-entry",
      "prefetch_policy": "disabled"
    }
  }
}
```

Resolution blocker example (missing entry):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000001,
  "error": {
    "code": "CATALOG_ENTRY_NOT_FOUND",
    "category": "catalog",
    "message": "Catalog entry tos.9.99.test not found in tos catalog index",
    "retryable": false,
    "details": {
      "catalog": "tos",
      "entry_id": "tos.9.99.test",
      "request_field": "tos_id"
    }
  }
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "loaded_modules": [
    {"component": "cpu", "module_id": "st.cpu.m68k", "version": "2.0.0"}
  ],
  "timing": {"master_hz": 32000000, "cpu_hz": 8000000}
}
```

## 6.2 Stop session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/stop`

Request:

```json
{
  "session_id": "ses_01H...",
  "reason": "user_stop"
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "stopped",
  "stopped_at_us": 1710000005123
}
```

## 6.3 Pause session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/pause`

Request:

```json
{
  "session_id": "ses_01H...",
  "reason": "user_pause"
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "paused",
  "paused_at_us": 1710000004988
}
```

## 6.4 Resume session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/resume`

Request:

```json
{
  "session_id": "ses_01H...",
  "resume_mode": "running"
}
```

`resume_mode` values:

- `running` (default)
- `paused`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "resumed_at_us": 1710000005077
}
```

## 6.5 Reset session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/reset`

Request optional mode:

```json
{
  "session_id": "ses_01H...",
  "mode": "warm",
  "preserve_media": true
}
```

`mode` values:

- `warm`
- `cold`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "reset_mode": "warm",
  "reset_at_us": 1710000005220
}
```

## 6.6 Session state

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/session?session_id=ses_01H...`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "uptime_ms": 28342,
  "cycle_counter": 74429122,
  "tick_counter": 297716488,
  "loaded_modules": [],
  "stream_health": {
    "video": {"connected_clients": 1, "dropped_packets": 0},
    "audio": {"connected_clients": 1, "dropped_packets": 0}
  }
}
```

## 6.6B Engine status

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/status?session_id=ses_01H...`

Required `data` fields:

- `session_id` (string)
- `machine` (string)
- `profile` (string)
- `lifecycle_state` (enum: `stopped`, `starting`, `running`, `paused`, `suspended`, `faulted`, `stopping`)
- `run_mode` (enum: `realtime`, `slow_motion`, `single_step`)
- `snapshot_at_us` (uint64)
- `uptime_ms` (uint64)
- `cycle_counter` (uint64)
- `tick_counter` (uint64)
- `loaded_modules` (array)
- `runtime` (object)

Required `runtime` fields:

- `scheduler_hz` (uint32)
- `stream_health` (object)
- `last_transition_at_us` (uint64)
- `last_error` (object or `null`)

Lifecycle-mode semantics:

- `running`: `uptime_ms > 0`, counters increase between snapshots, `last_error` is `null` unless non-fatal fault recorded.
- `paused`: counters are stable between snapshots, `run_mode` remains unchanged, `last_transition_at_us` is pause timestamp.
- `suspended`: counters are stable, `runtime.stream_health` may report disconnected outputs, `last_transition_at_us` is suspend timestamp.
- `faulted`: counters may stop, `last_error` must be non-null with `code`, `message`, and `at_us`.
- `starting` and `stopping`: counters may be transient; `lifecycle_state` transitions must follow section `12`.
- `stopped`: `uptime_ms = 0`, counters may reset to baseline for the next start sequence.

Consistency with lifecycle API rules:

- `lifecycle_state` must always be one of the states defined in section `12`.
- Returned state must be reachable through allowed transitions in section `12`.
- Unknown or inactive session identifiers must return canonical engine errors (`ENGINE_NOT_RUNNING` or `NOT_FOUND`) and must not return synthetic lifecycle states.

Response `data` (success example):

```json
{
  "session_id": "ses_01H...",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "lifecycle_state": "running",
  "run_mode": "realtime",
  "snapshot_at_us": 1710000007123,
  "uptime_ms": 39284,
  "cycle_counter": 102442933,
  "tick_counter": 409771732,
  "loaded_modules": [
    {"component": "cpu", "module_id": "st.cpu.m68k", "version": "2.0.0"}
  ],
  "runtime": {
    "scheduler_hz": 8000000,
    "stream_health": {
      "video": {"connected_clients": 1, "dropped_packets": 0},
      "audio": {"connected_clients": 1, "dropped_packets": 0}
    },
    "last_transition_at_us": 1710000004000,
    "last_error": null
  }
}
```

Error example (inactive session):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000007124,
  "error": {
    "code": "ENGINE_NOT_RUNNING",
    "category": "engine",
    "message": "No active session for session_id ses_01H...",
    "retryable": true,
    "details": {
      "session_id": "ses_01H..."
    }
  }
}
```

## 6.6A Engine health

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/health?session_id=ses_01H...&components=cpu,memory,video`

Required `data` fields:

- `session_id` (string)
- `overall_status` (enum)
- `overall_severity` (enum)
- `generated_at_us` (uint64)
- `freshness_ttl_ms` (uint32)
- `freshness_state` (enum)
- `components` (array)

Component fields:

- `component` (string)
- `status` (enum)
- `severity` (enum)
- `observed_at_us` (uint64)
- `last_ok_at_us` (uint64 or `null`)
- `reason` (string, optional)
- `error_code` (string, optional)

Enums:

- `overall_status` and component `status`: `healthy`, `degraded`, `unavailable`
- `overall_severity` and component `severity`: `info`, `warning`, `error`, `critical`
- `freshness_state`: `fresh`, `stale`

Timestamp and freshness semantics:

- `generated_at_us` and all component timestamps are microseconds from the same monotonic runtime time base used for `timestamp_us` in section `4`.
- `freshness_state=fresh` iff `(timestamp_us - generated_at_us) <= freshness_ttl_ms * 1000`.
- `freshness_state=stale` iff `(timestamp_us - generated_at_us) > freshness_ttl_ms * 1000`.

Unavailable-subsystem error handling:

- If one or more requested subsystems are unavailable and `fail_on_unavailable=false` (default), response remains `ok=true` and each unavailable component must set:
  - `status="unavailable"`
  - `severity="error"` or `severity="critical"`
  - `error_code="ENGINE_SUBSYSTEM_UNAVAILABLE"`
- If `fail_on_unavailable=true` and any requested subsystem is unavailable, response must be `ok=false` with `error.code="ENGINE_SUBSYSTEM_UNAVAILABLE"`.

Response `data` (success example):

```json
{
  "session_id": "ses_01H...",
  "overall_status": "degraded",
  "overall_severity": "warning",
  "generated_at_us": 1710000006123,
  "freshness_ttl_ms": 1000,
  "freshness_state": "fresh",
  "components": [
    {
      "component": "cpu",
      "status": "healthy",
      "severity": "info",
      "observed_at_us": 1710000006118,
      "last_ok_at_us": 1710000006118
    },
    {
      "component": "video",
      "status": "unavailable",
      "severity": "error",
      "observed_at_us": 1710000006119,
      "last_ok_at_us": 1710000005900,
      "reason": "video stream backend not initialized",
      "error_code": "ENGINE_SUBSYSTEM_UNAVAILABLE"
    }
  ]
}
```

Error example (`fail_on_unavailable=true`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000006124,
  "error": {
    "code": "ENGINE_SUBSYSTEM_UNAVAILABLE",
    "category": "engine",
    "message": "Requested subsystem video is unavailable",
    "retryable": true,
    "details": {
      "session_id": "ses_01H...",
      "component": "video"
    }
  }
}
```

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

## 6.9 Set debug clock mode

- `POST /api/v2/debug/clock/mode`

Request:

```json
{
  "session_id": "ses_01H...",
  "mode": "slow_motion",
  "ratio": 0.25
}
```

`mode` values:

- `realtime`
- `slow_motion`
- `single_step`

Rules:

- `ratio` is required only for `slow_motion` and must be greater than 0 and less than or equal to 1.
- Invalid mode/ratio combinations return `DEBUG_CLOCK_INVALID`.

## 6.9A Realtime/slow-motion clock-control model and bounds

Clock-control model:

- `realtime` is the baseline pacing mode and always runs with `effective_ratio=1.0`.
- `slow_motion` runs the same deterministic tick/cycle execution path as `realtime` and changes only wall-clock pacing via `ratio`.
- Clock pacing must not change component ordering, arbitration hook order, or deterministic scheduler invariants from section `6.10A`.

Deterministic bounds contract:

- `CLOCK-BOUND-01`: accepted `slow_motion.ratio` is in `(0, 1]`.
- `CLOCK-BOUND-02`: `realtime` requests must not include `ratio`; if supplied, request is rejected as `DEBUG_CLOCK_INVALID`.
- `CLOCK-BOUND-03`: accepted mode change commits atomically and records `last_transition_at_us` from canonical runtime clock.
- `CLOCK-BOUND-04`: mode-change response must include normalized `effective_ratio` (`1.0` for `realtime`, requested `ratio` for `slow_motion`).

Mode-change response excerpt (`slow_motion`, `ratio=0.25`):

```json
{
  "session_id": "ses_01H...",
  "mode": "slow_motion",
  "effective_ratio": 0.25,
  "clock_bounds": {
    "ratio_min_exclusive": 0.0,
    "ratio_max_inclusive": 1.0,
    "realtime_effective_ratio": 1.0
  },
  "last_transition_at_us": 1710000008124
}
```

Realtime invalid-ratio error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000008128,
  "error": {
    "code": "DEBUG_CLOCK_INVALID",
    "category": "debug",
    "message": "ratio is only allowed for slow_motion mode",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/debug/clock/mode",
      "guard_id": "CLOCK-BOUND-02"
    }
  }
}
```

## 6.9B Clock mode API deterministic transition flow

Transition model (`POST /api/v2/debug/clock/mode`):

1. Validate payload and bounds (`mode`, `ratio`) using section `6.9A` guards.
2. Resolve target configuration (`target_mode`, `effective_ratio`).
3. Compare with active configuration for idempotency.
4. Commit accepted transition at the next scheduler tick boundary.
5. Persist transition metadata and return deterministic transition result.

Deterministic transition checks:

- `CLOCK-TRANS-01`: accepted transition commits atomically at one tick boundary and never partially applies.
- `CLOCK-TRANS-02`: each applied transition increments `mode_transition_seq` by exactly `+1`.
- `CLOCK-TRANS-03`: idempotent request (`target_mode/effective_ratio` equals active config) returns success with `transition_applied=false` and must not increment `mode_transition_seq`.
- `CLOCK-TRANS-04`: transition response includes both `from_mode` and `to_mode` plus `effective_ratio` and `last_transition_at_us`.

Mode transition matrix:

- `realtime -> slow_motion`: allowed only with `ratio in (0,1]`.
- `slow_motion -> realtime`: allowed and normalizes to `effective_ratio=1.0`.
- `realtime|slow_motion -> single_step`: allowed; runtime enters deterministic step-gated execution path.
- `single_step -> realtime|slow_motion`: allowed after transition guard checks and atomic boundary commit.

Transition guard failures:

- Missing/invalid request fields (`session_id`, `mode`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Invalid mode/ratio pairing or out-of-bounds ratio -> `DEBUG_CLOCK_INVALID`.
- Session lifecycle state not eligible for transition commit -> `INVALID_SESSION_STATE`.
- Scheduler transition commit failure after accepted guards -> `INTERNAL_ERROR`.

Mode-transition response example (`realtime -> slow_motion`):

```json
{
  "session_id": "ses_01H...",
  "transition_applied": true,
  "mode_transition_seq": 42,
  "from_mode": "realtime",
  "to_mode": "slow_motion",
  "effective_ratio": 0.5,
  "last_transition_at_us": 1710000009104
}
```

Idempotent transition response example:

```json
{
  "session_id": "ses_01H...",
  "transition_applied": false,
  "mode_transition_seq": 42,
  "from_mode": "slow_motion",
  "to_mode": "slow_motion",
  "effective_ratio": 0.5,
  "reason": "already_in_target_mode"
}
```

## 6.10 Step debug execution

- `POST /api/v2/debug/clock/step`

Request:

```json
{
  "session_id": "ses_01H...",
  "steps": 1,
  "capture": ["opcode", "bus_error", "register_delta"]
}
```

Single-step request contract:

- `steps` is required and must be an integer in `[1, 1024]`.
- `capture` is optional; allowed values are `opcode`, `bus_error`, `register_delta`.
- Single-step requests are accepted only when active `run_mode=single_step`.

## 6.10B Single-step execution control API and scheduler hook

Deterministic single-step control flow:

1. Validate step request fields and capture options.
2. Confirm session is active and clock mode is `single_step`.
3. Execute exactly `N=steps` committed scheduler ticks through deterministic hook sequence.
4. Emit aggregated step result with before/after counters and capture payload metadata.

Single-step checks:

- `STEP-CTRL-01`: accepted `steps=N` commits exactly `N` ticks.
- `STEP-CTRL-02`: response must satisfy `tick_counter_after = tick_counter_before + ticks_committed`.
- `STEP-CTRL-03`: per committed tick hook order is `arb_pre_tick -> arb_component_step* -> arb_post_tick`.
- `STEP-CTRL-04`: capture payload ordering is deterministic and aligned to committed tick order.

Scheduler hook contract for step API:

- Hook fields required per committed tick: `tick_counter`, `cycle_counter`, `hook_phase`, `slot_index`, `component_id`.
- Step API diagnostics include `scheduler_hook_stats` with:
  - `ticks_with_hooks`
  - `hook_order_violations`
  - `component_step_mismatches`
- Any non-zero `hook_order_violations` or `component_step_mismatches` fails request as `INTERNAL_ERROR`.

Step API guard failures:

- Missing/invalid request fields (`session_id`, `steps`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Step request while `run_mode != single_step` -> `INVALID_SESSION_STATE`.
- Invalid capture selector or bounds violation (`steps<1` or `steps>1024`) -> `DEBUG_STEP_INVALID`.
- Scheduler hook integrity failure (`STEP-CTRL-03`/`STEP-CTRL-04`) -> `INTERNAL_ERROR`.

Single-step response example (`steps=2`):

```json
{
  "session_id": "ses_01H...",
  "run_mode": "single_step",
  "steps_requested": 2,
  "ticks_committed": 2,
  "tick_counter_before": 409771736,
  "tick_counter_after": 409771738,
  "cycle_counter_before": 102442977,
  "cycle_counter_after": 102443001,
  "scheduler_hook_stats": {
    "ticks_with_hooks": 2,
    "hook_order_violations": 0,
    "component_step_mismatches": 0
  }
}
```

Single-step invalid-state error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000010012,
  "error": {
    "code": "INVALID_SESSION_STATE",
    "category": "session",
    "message": "Clock mode must be single_step for step execution",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/debug/clock/step",
      "guard_id": "STEP-CTRL-03"
    }
  }
}
```

## 6.10C Opcode/bus-error capture path and diagnostic payloads

Capture path model (`POST /api/v2/debug/clock/step` with `capture`):

- Capture pipeline runs in committed tick order and emits deterministic capture entries per tick.
- `opcode` capture is sourced from executed instruction decode for each committed CPU step.
- `bus_error` capture is sourced from scheduler/arbitration bus-access fault signals for committed ticks.
- Capture pipeline output is serialized into `capture_payloads` in the same order as committed ticks.

Capture checks:

- `CAP-DIAG-01`: `capture_payloads[*].tick_counter` is monotonic non-decreasing.
- `CAP-DIAG-02`: opcode capture entries include `pc`, `opcode_word`, and `instruction_size_bytes`.
- `CAP-DIAG-03`: bus-error capture entries include `fault_address`, `access_type`, `fault_phase`, and `vector`.
- `CAP-DIAG-04`: capture payload ordering is stable across identical scheduler traces.

Diagnostic payload schemas:

- `opcode_capture_v1`:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `pc` (uint32)
  - `opcode_word` (hex string)
  - `instruction_size_bytes` (uint8)
- `bus_error_capture_v1`:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `fault_address` (uint32)
  - `access_type` (`read`|`write`|`instruction_fetch`)
  - `fault_phase` (`address`|`data`|`ack`)
  - `vector` (uint16)

Capture guard failures:

- Unsupported capture selector in `capture[]` -> `DEBUG_STEP_INVALID`.
- Requested capture selector unavailable in active runtime profile -> `INVALID_SESSION_STATE`.
- Capture schema projection failure after committed ticks -> `INTERNAL_ERROR`.

Step response capture example (`capture=["opcode","bus_error"]`):

```json
{
  "session_id": "ses_01H...",
  "run_mode": "single_step",
  "steps_requested": 2,
  "ticks_committed": 2,
  "capture_payloads": [
    {
      "kind": "opcode_capture_v1",
      "tick_counter": 409771739,
      "cycle_counter": 102443012,
      "pc": 16779904,
      "opcode_word": "0x4E71",
      "instruction_size_bytes": 2
    },
    {
      "kind": "bus_error_capture_v1",
      "tick_counter": 409771740,
      "cycle_counter": 102443019,
      "fault_address": 16781312,
      "access_type": "instruction_fetch",
      "fault_phase": "ack",
      "vector": 2
    }
  ]
}
```

Invalid capture selector error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000011022,
  "error": {
    "code": "DEBUG_STEP_INVALID",
    "category": "debug",
    "message": "Unsupported capture selector 'trace_raw'",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/debug/clock/step",
      "guard_id": "CAP-DIAG-03"
    }
  }
}
```

## 6.10A Deterministic tick-loop scheduler core

Core scheduler model:

- Runtime uses one authoritative tick-loop scheduler for all component execution.
- Tick-loop input parameters:
  - `scheduler_hz` (from active profile/runtime)
  - `run_mode` (`realtime`, `slow_motion`, `single_step`)
  - component `step_order` from validated profile wiring.
- Tick-loop output invariants:
  - `tick_counter` increments by exactly `+1` per committed scheduler tick.
  - `cycle_counter` is monotonic non-decreasing and advances according to executed work in that tick.
  - API-visible timestamps/counters (`snapshot_at_us`, `tick_counter`, `cycle_counter`) derive only from this loop.

Mode-specific execution rules:

- `realtime`: loop advances continuously with deterministic component step order each tick.
- `slow_motion`: same deterministic order/invariants as realtime, but pacing ratio throttles wall-clock progression only.
- `single_step`: loop advances only via `POST /api/v2/debug/clock/step`; each accepted `steps=N` commits exactly `N` ticks.

Sequencing checks for scheduler loop:

- `TICK-CHECK-01`: `tick_counter(next) == tick_counter(prev) + committed_ticks`.
- `TICK-CHECK-02`: `cycle_counter(next) >= cycle_counter(prev)`.
- `TICK-CHECK-03`: component execution follows profile `step_order` exactly for each committed tick.

Arbitration hook layer contract:

- Scheduler loop must expose deterministic arbitration hooks for each committed tick:
  - `arb_pre_tick` (before first component step)
  - `arb_component_step` (per component in `step_order`)
  - `arb_post_tick` (after last component step)
- Hook dispatch order must be stable and exactly aligned to validated `step_order`.
- Required hook metadata fields:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `arbitration_round` (uint32)
  - `slot_index` (uint32)
  - `component_id` (string)
  - `bus_owner` (string)
  - `wait_cycles` (uint32)

Deterministic timestamp emitter contract:

- Runtime timestamp emitter is fed only by scheduler/arbitration hook outputs and is the canonical source for emitted `event_timestamp_us` values.
- Emitter must produce monotonic non-decreasing timestamps within a stream and stable mapping from `(tick_counter, cycle_counter, slot_index)` to timestamp.
- Timestamp emission must be deterministic across identical scheduler traces (same tick/cycle/slot sequence).
- Emitter metadata projection fields (runtime/diagnostics):
  - `timestamp_origin_us` (uint64)
  - `timestamp_last_emitted_us` (uint64)
  - `timestamp_regressions` (uint64)

Additional runtime checks:

- `ARB-CHECK-01`: `slot_index` increments deterministically within a tick and resets at next tick boundary.
- `ARB-CHECK-02`: `component_id` order exactly matches validated `step_order` for the active profile.
- `TS-CHECK-01`: emitted `event_timestamp_us(next) >= event_timestamp_us(prev)`.

Arbitration/timestamp guard failures:

- Hook order mismatch or unresolved `component_id` in arbitration path -> `INTERNAL_ERROR`.
- Timestamp regression detected by emitter (`TS-CHECK-01`) -> `INTERNAL_ERROR` with fail-fast diagnostics emission.

Fail-fast scheduler guards:

- Missing/invalid scheduler configuration (`scheduler_hz<=0`, empty `step_order`) -> `BAD_REQUEST`.
- Step request while session is not in valid debug state -> `INVALID_SESSION_STATE`.
- Invalid debug clock configuration (`mode`/`ratio`) -> `DEBUG_CLOCK_INVALID`.

Tick-loop status excerpt (deterministic counters):

```json
{
  "session_id": "ses_01H...",
  "run_mode": "single_step",
  "snapshot_at_us": 1710000008200,
  "tick_counter": 409771733,
  "cycle_counter": 102442944,
  "runtime": {
    "scheduler_hz": 8000000,
    "timestamp_origin_us": 1710000000000,
    "timestamp_last_emitted_us": 1710000008200,
    "timestamp_regressions": 0,
    "last_transition_at_us": 1710000008100,
    "last_error": null
  }
}
```

Step response excerpt (`steps=3`):

```json
{
  "session_id": "ses_01H...",
  "run_mode": "single_step",
  "steps_requested": 3,
  "ticks_committed": 3,
  "tick_counter_before": 409771733,
  "tick_counter_after": 409771736,
  "cycle_counter_before": 102442944,
  "cycle_counter_after": 102442977,
  "arbitration": {
    "arbitration_round": 133,
    "slots_executed": 15,
    "last_bus_owner": "cpu"
  }
}
```

Arbitration/timestamp blocker example (timestamp regression):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000008202,
  "error": {
    "code": "INTERNAL_ERROR",
    "category": "internal",
    "message": "Deterministic timestamp emitter regression detected",
    "retryable": false,
    "details": {
      "check_id": "TS-CHECK-01",
      "tick_counter": 409771736,
      "previous_event_timestamp_us": 1710000008202,
      "current_event_timestamp_us": 1710000008201,
      "arbitration_round": 133
    }
  }
}
```

Scheduler guard blocker example (invalid step state):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000008201,
  "error": {
    "code": "INVALID_SESSION_STATE",
    "category": "engine",
    "message": "Debug step is not allowed while run_mode is realtime",
    "retryable": true,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "POST /api/v2/debug/clock/step",
      "run_mode": "realtime",
      "required_mode": "single_step"
    }
  }
}
```

## 6.11 Session performance metrics

- `GET /api/v2/metrics/performance?session_id=ses_01H...`

Performance SLO collector configuration contract:

- `POST /api/v2/metrics/performance/collectors/config`
- Request payload is validated by `slo_collector_config_v1`.
- `slo_collector_config_v1` required fields:
  - `session_id` (string, required)
  - `sampling_interval_ms` (uint32, required, range `100..10000`)
  - `window_ms` (uint32, required, range `1000..60000`)
  - `collectors` (object, required):
    - `input_latency_ms.enabled` (bool)
    - `jitter_ms.enabled` (bool)
    - `dropped_frame_percent.enabled` (bool)
  - `emit_history` (bool, optional, default `true`)

Performance sampling pipeline contract:

- `GET /api/v2/metrics/performance/samples?session_id=...&limit=...`
- Response `data.samples[]` entries are validated by `slo_sample_v1`.
- `slo_sample_v1` required fields:
  - `sample_seq` (uint64)
  - `window_start_us` (uint64)
  - `window_end_us` (uint64)
  - `input_latency_ms_p95` (number)
  - `jitter_ms_p95` (number)
  - `dropped_frame_percent` (number)
  - `collector_revision` (string)
  - `timestamp_us` (uint64)

SLO collector/pipeline conformance checks:

- `SLO-COL-01`: `sample_seq(next) == sample_seq(prev) + 1` for emitted sample stream.
- `SLO-COL-02`: `window_start_us(next) >= window_end_us(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `SLO-COL-03`: each emitted sample must be derived only from collectors with `enabled=true` in active `slo_collector_config_v1`.
- `SLO-COL-04`: sampling cadence must respect configured `sampling_interval_ms` with bounded jitter (`|actual_interval - configured_interval| <= 1 tick window`).

SLO collector/pipeline deterministic guard failures:

- Invalid payload shape, out-of-range interval/window, or invalid `limit` -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Collector update attempted when lifecycle is not `running`/`paused` -> `INVALID_SESSION_STATE`.
- Sampling pipeline backend unavailable -> `INTERNAL_ERROR`.

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "window_ms": 5000,
  "input_latency_ms": {"p50": 18, "p95": 41, "max": 49, "target_max": 50, "status": "ok"},
  "jitter_ms": {"p50": 7, "p95": 21, "max": 28, "target_max": 30, "status": "ok"},
  "dropped_frame_percent": {"value": 0.4, "target_max": 1.0, "status": "ok"}
}
```

SLO collector configuration request example:

```json
{
  "session_id": "ses_01H...",
  "sampling_interval_ms": 500,
  "window_ms": 5000,
  "collectors": {
    "input_latency_ms": {"enabled": true},
    "jitter_ms": {"enabled": true},
    "dropped_frame_percent": {"enabled": true}
  },
  "emit_history": true
}
```

SLO collector configuration response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "collector_revision": "slo_col_rev_04",
  "sampling_interval_ms": 500,
  "window_ms": 5000,
  "state": "active",
  "applied_at_us": 1710000006200
}
```

SLO sample stream response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "samples": [
    {
      "sample_seq": 120,
      "window_start_us": 1710000001000,
      "window_end_us": 1710000006000,
      "input_latency_ms_p95": 41,
      "jitter_ms_p95": 21,
      "dropped_frame_percent": 0.4,
      "collector_revision": "slo_col_rev_04",
      "timestamp_us": 1710000006001
    }
  ]
}
```

SLO threshold endpoint contract:

- `GET /api/v2/metrics/performance/thresholds?session_id=...`
- Response `data` must satisfy `slo_thresholds_v1`.
- `slo_thresholds_v1` required fields:
  - `session_id` (string)
  - `thresholds` (object):
    - `input_latency_ms_p95_max` (number)
    - `jitter_ms_p95_max` (number)
    - `dropped_frame_percent_max` (number)
  - `evaluation_window_ms` (uint32)
  - `active_revision` (string)

SLO breach alarm event contract:

- `GET /api/v2/metrics/performance/alarms?session_id=...&limit=...`
- Alarm events are validated by `slo_breach_alarm_event_v1`.
- `slo_breach_alarm_event_v1` required fields:
  - `alarm_seq` (uint64)
  - `metric` (enum: `input_latency_ms_p95`, `jitter_ms_p95`, `dropped_frame_percent`)
  - `threshold` (number)
  - `observed` (number)
  - `severity` (enum: `warning`, `critical`)
  - `state` (enum: `breached`, `recovered`)
  - `window_start_us` (uint64)
  - `window_end_us` (uint64)
  - `timestamp_us` (uint64)

SLO endpoint/alarm conformance checks:

- `SLO-ALRM-01`: `alarm_seq(next) == alarm_seq(prev) + 1` for alarm stream.
- `SLO-ALRM-02`: breach event emitted when `observed > threshold`; recovery event emitted when `observed <= threshold` for same metric.
- `SLO-ALRM-03`: `window_start_us(next) >= window_start_us(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `SLO-ALRM-04`: alarm severity deterministically maps to breach ratio (`critical` when `observed >= threshold * 1.2`, else `warning`).

SLO endpoint/alarm deterministic guard failures:

- Invalid query params (`session_id`, `limit`) or malformed threshold payloads -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Alarm stream requested when collector pipeline is not active -> `INVALID_SESSION_STATE`.
- Alarm evaluation backend unavailable -> `INTERNAL_ERROR`.

SLO thresholds response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "thresholds": {
    "input_latency_ms_p95_max": 50,
    "jitter_ms_p95_max": 30,
    "dropped_frame_percent_max": 1.0
  },
  "evaluation_window_ms": 5000,
  "active_revision": "slo_thr_rev_02"
}
```

SLO alarm stream response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "alarms": [
    {
      "alarm_seq": 11,
      "metric": "jitter_ms_p95",
      "threshold": 30,
      "observed": 37,
      "severity": "critical",
      "state": "breached",
      "window_start_us": 1710000006000,
      "window_end_us": 1710000011000,
      "timestamp_us": 1710000011001
    },
    {
      "alarm_seq": 12,
      "metric": "jitter_ms_p95",
      "threshold": 30,
      "observed": 24,
      "severity": "warning",
      "state": "recovered",
      "window_start_us": 1710000011000,
      "window_end_us": 1710000016000,
      "timestamp_us": 1710000016001
    }
  ]
}
```

---

## 7. SD-card file manager APIs

## 7.1 List directory

- `GET /api/v2/files/list?path=/sdcard/disks/st`

Response `data`:

```json
{
  "path": "/sdcard/disks/st",
  "entries": [
    {"name": "game.st", "type": "file", "size": 901120, "mtime": 1710000001}
  ]
}
```

## 7.2 Stat path

- `GET /api/v2/files/stat?path=/sdcard/roms/st/tos104.img`

## 7.3 Create directory

- `POST /api/v2/files/mkdir`

```json
{
  "path": "/sdcard/ebins/st/cpu"
}
```

## 7.4 Move/rename

- `POST /api/v2/files/move`

```json
{
  "from": "/sdcard/.staging/new.ebin",
  "to": "/sdcard/ebins/st/cpu/new.ebin",
  "overwrite": false
}
```

## 7.5 Delete

- `POST /api/v2/files/delete`

```json
{
  "path": "/sdcard/disks/st/old.st",
  "recursive": false
}
```

## 7.6 Download

- `GET /api/v2/files/download?path=/sdcard/disks/st/game.st`

Returns file stream.

## 7.7 Upload (multipart)

- `POST /api/v2/files/upload`

Multipart fields:

- `path` (target canonical path)
- `file` (binary data)
- optional `sha256`
- optional `machine_tags`

Behavior:

1. Write to staging path
2. Verify integrity/format
3. Atomic move to target path
4. Update corresponding catalog

## 7.8 Upload (chunked alternative)

- `POST /api/v2/files/upload/init`
- `POST /api/v2/files/upload/chunk`
- `POST /api/v2/files/upload/complete`
- `POST /api/v2/files/upload/cancel`

## 7.9 Catalog list and entry APIs

Supported catalogs:

- `roms`
- `floppies`
- `tos`

### List catalogs

- `GET /api/v2/catalogs/list`

Response `data`:

```json
{
  "catalogs": [
    {"name": "roms", "path": "/sdcard/config/engine_v2/rom_catalog.json", "entries": 46},
    {"name": "floppies", "path": "/sdcard/config/engine_v2/disk_catalog.json", "entries": 1319},
    {"name": "tos", "path": "/sdcard/config/engine_v2/tos_catalog.json", "entries": 1498}
  ]
}
```

### List catalog entries

- `GET /api/v2/catalogs/{catalog}/entries?query=...&missing_only=false&state=online`

Entry state values:

- `unknown`
- `online`
- `offline`
- `dead`
- `local_only`

### Get one entry

- `GET /api/v2/catalogs/{catalog}/entries/{entry_id}`

Response `data` example:

```json
{
  "id": "disk.automation.a_093",
  "local_path": "/sdcard/disks/st/AUTOMATION/A_093.ST",
  "hosted_url": "http://ataristdb.sidecartridge.com/AUTOMATION/A_093.ST",
  "availability_state": "online",
  "availability_checked_at": "2026-03-01T15:22:01Z",
  "download_fail_count": 0
}
```

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

## 8. EBIN management APIs

## 8.1 Catalog

- `GET /api/v2/ebins/catalog`

Response includes:

- module ID
- version
- component type
- machine tags
- ABI version
- dependencies
- integrity status

## 8.2 Rescan SD-card

- `POST /api/v2/ebins/rescan`

Request optional path scope:

```json
{
  "path": "/sdcard/ebins"
}
```

## 8.3 Validate module

- `POST /api/v2/ebins/validate`

```json
{
  "path": "/sdcard/ebins/st/cpu/st.cpu.m68k-2.0.0.ebin"
}
```

Response `data`:

```json
{
  "valid": true,
  "module_id": "st.cpu.m68k",
  "version": "2.0.0",
  "abi": "2.0",
  "integrity": {"sha256": "...", "signature": "ok"},
  "dependencies": []
}
```

## 8.4 Load module

- `POST /api/v2/ebins/load`

```json
{
  "session_id": "ses_01H...",
  "component": "cpu",
  "module_id": "st.cpu.m68k",
  "version": "2.0.0",
  "policy": "pause_swap_resume"
}
```

## 8.5 Unload module

- `POST /api/v2/ebins/unload`

```json
{
  "session_id": "ses_01H...",
  "component": "cpu",
  "policy": "pause_unload"
}
```

---

## 9. Media attach/eject APIs

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

## 9.4 Attach cartridge

- `POST /api/v2/media/cartridge/attach`

## 9.5 Eject cartridge

- `POST /api/v2/media/cartridge/eject`

## 9.6 Input translation APIs

The engine is responsible for translating host physical inputs into virtual-machine input semantics.

Supported host classes:

- keyboard
- mouse
- game_controller

Translated virtual classes for Atari ST baseline:

- IKBD keyboard events
- IKBD mouse packets
- joystick port events

### 9.6.1 Enumerate input devices

- `GET /api/v2/input/devices?session_id=...`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "devices": [
    {
      "device_id": "kbd_001",
      "type": "keyboard",
      "name": "USB Keyboard",
      "connected": true,
      "capabilities": ["keys", "modifiers"]
    },
    {
      "device_id": "mouse_001",
      "type": "mouse",
      "name": "USB Mouse",
      "connected": true,
      "capabilities": ["delta", "buttons", "wheel"]
    }
  ]
}
```

### 9.6.2 Load mapping profile

- `POST /api/v2/input/mappings/load`

```json
{
  "session_id": "ses_01H...",
  "mapping_profile_id": "atari_st_default_v1",
  "replace": true
}
```

Canonical mapping schema (`mapping_profile`):

- `mapping_profile_id` (string, stable identifier)
- `schema_version` (uint32)
- `machine` (string, e.g. `atari_st`)
- `profile` (string machine profile affinity)
- `revision` (uint32, monotonic per profile)
- `updated_at_us` (uint64, monotonic runtime/storage time base)
- `entries` (array)

Entry schema:

- `entry_id` (string, unique within profile)
- `host` (object)
  - `device_type` (enum: `keyboard`, `mouse`, `game_controller`)
  - `code` (string)
  - optional `modifiers` (array of strings)
- `virtual` (object)
  - `target` (string, virtual endpoint path)
  - `value` (string | number | boolean)
  - optional `phase` (enum: `press`, `release`, `repeat`)
- optional `flags` (array; e.g. `invert_axis`, `turbo`)

Persistence model:

- Profiles persist under `/sdcard/config/engine_v2/input/mappings/<machine>/<mapping_profile_id>.json`.
- Persisted file must include top-level `mapping_profile` object matching schema above.
- `revision` increments by `1` for each successful persisted update.
- `updated_at_us` and persisted metadata must reflect the committed profile snapshot.

Mapping profile CRUD endpoints:

- `POST /api/v2/input/mappings`
- `GET /api/v2/input/mappings?machine=...`
- `GET /api/v2/input/mappings/{mapping_profile_id}`
- `PATCH /api/v2/input/mappings/{mapping_profile_id}`
- `DELETE /api/v2/input/mappings/{mapping_profile_id}`

Create mapping request example:

```json
{
  "machine": "atari_st",
  "profile": "st_520_pal",
  "mapping_profile_id": "atari_st_custom_gamepad_v1",
  "entries": [
    {
      "entry_id": "pad.south",
      "host": {"device_type": "game_controller", "code": "BTN_SOUTH"},
      "virtual": {"target": "joystick.port0.button", "value": 1}
    }
  ]
}
```

Create mapping success `data` example:

```json
{
  "mapping_profile_id": "atari_st_custom_gamepad_v1",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "revision": 1,
  "updated_at_us": 1710002224050,
  "persistence": {
    "path": "/sdcard/config/engine_v2/input/mappings/atari_st/atari_st_custom_gamepad_v1.json",
    "saved": true
  }
}
```

CRUD semantics:

- `POST` creates new profile; duplicate `mapping_profile_id` for same machine must return `CONFLICT`.
- `GET /mappings` returns profile summaries (`mapping_profile_id`, `machine`, `profile`, `revision`, `updated_at_us`).
- `GET /mappings/{mapping_profile_id}` returns full canonical `mapping_profile` object.
- `PATCH /mappings/{mapping_profile_id}` is partial update and increments `revision` only if effective mapping changes.
- `DELETE /mappings/{mapping_profile_id}` removes persisted profile unless it is currently active for a running session.

Load response `data` (persistence-aware):

```json
{
  "session_id": "ses_01H...",
  "mapping_profile_id": "atari_st_default_v1",
  "applied": true,
  "mapping_profile": {
    "mapping_profile_id": "atari_st_default_v1",
    "schema_version": 1,
    "machine": "atari_st",
    "profile": "st_520_pal",
    "revision": 7,
    "updated_at_us": 1710002224000,
    "entries": [
      {
        "entry_id": "kbd.keya",
        "host": {"device_type": "keyboard", "code": "KeyA"},
        "virtual": {"target": "ikbd.key", "value": "ST_SC_A", "phase": "press"}
      }
    ]
  },
  "persistence": {
    "path": "/sdcard/config/engine_v2/input/mappings/atari_st/atari_st_default_v1.json",
    "revision": 7,
    "saved": true
  }
}
```

### 9.6.3 Read active mapping

- `GET /api/v2/input/mappings/active?session_id=...`

Read response `data` must include `mapping_profile` and `persistence` metadata as defined in section `9.6.2`.

### 9.6.3A Apply active mapping profile

- `POST /api/v2/input/mappings/apply`

```json
{
  "session_id": "ses_01H...",
  "mapping_profile_id": "atari_st_custom_gamepad_v1",
  "expected_revision": 1
}
```

Apply-path semantics:

- Profile apply is session-scoped and must validate session is running before mutation.
- Apply must resolve requested profile from persisted store and verify optional `expected_revision` if provided.
- Active-profile switch is atomic at input-translation boundary:
  - all events before cutover use prior profile,
  - all events after cutover use new profile.
- Re-applying currently active profile with same revision is deterministic no-op (`result=no_op`).

Apply success `data` example (`applied`):

```json
{
  "session_id": "ses_01H...",
  "result": "applied",
  "previous_mapping_profile_id": "atari_st_default_v1",
  "active_mapping_profile_id": "atari_st_custom_gamepad_v1",
  "active_mapping_revision": 1,
  "applied_at_us": 1710002224065,
  "cutover_tick": 297716640
}
```

Apply success `data` example (`no_op`):

```json
{
  "session_id": "ses_01H...",
  "result": "no_op",
  "previous_mapping_profile_id": "atari_st_custom_gamepad_v1",
  "active_mapping_profile_id": "atari_st_custom_gamepad_v1",
  "active_mapping_revision": 1,
  "applied_at_us": 1710002224065,
  "cutover_tick": 297716640
}
```

### 9.6.4 Update mapping entries

- `POST /api/v2/input/mappings/update`

```json
{
  "session_id": "ses_01H...",
  "patch": [
    {
      "host": {"device_type": "keyboard", "code": "KeyA"},
      "virtual": {"target": "ikbd.key", "value": "ST_SC_A"}
    },
    {
      "host": {"device_type": "game_controller", "code": "BTN_SOUTH"},
      "virtual": {"target": "joystick.port0.button", "value": 1}
    }
  ]
}
```

Update persistence semantics:

- Patch application must be deterministic and transactional per request.
- If at least one patch operation mutates effective mapping, `revision` increments and persisted file is rewritten atomically.
- If patch is semantically no-op, persisted content is unchanged and revision does not increment.

Update response `data` (mutating example):

```json
{
  "session_id": "ses_01H...",
  "mapping_profile_id": "atari_st_default_v1",
  "result": "applied",
  "updated_entries": 2,
  "revision_before": 7,
  "revision_after": 8,
  "persistence": {
    "path": "/sdcard/config/engine_v2/input/mappings/atari_st/atari_st_default_v1.json",
    "saved": true,
    "saved_at_us": 1710002224100
  }
}
```

Deterministic dependency blockers:

- Unknown `mapping_profile_id` for load/read/update -> `INPUT_MAPPING_NOT_FOUND`.
- Invalid mapping payload shape (missing required schema fields, unsupported `device_type`, invalid `virtual.target`) -> `BAD_REQUEST`.
- Session not running/invalid for mapping operations -> `ENGINE_NOT_RUNNING`.
- Duplicate profile create (`mapping_profile_id` already exists) -> `CONFLICT`.
- Delete requested for currently active mapping profile in running session -> `CONFLICT`.
- Apply requested with revision mismatch (`expected_revision` != stored `revision`) -> `CONFLICT`.

Apply blocker example (revision mismatch):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710002224066,
  "error": {
    "code": "CONFLICT",
    "category": "engine",
    "message": "Requested mapping profile revision does not match persisted revision",
    "retryable": true,
    "details": {
      "session_id": "ses_01H...",
      "mapping_profile_id": "atari_st_custom_gamepad_v1",
      "expected_revision": 2,
      "actual_revision": 1
    }
  }
}
```

### 9.6.5 Inject normalized host events

- `POST /api/v2/input/events/inject`

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "events": [
    {
      "event_id": "evt_1001",
      "timestamp_us": 1710002223334,
      "device_id": "kbd_001",
      "type": "key_down",
      "code": "KeyA",
      "modifiers": ["Shift"]
    },
    {
      "event_id": "evt_1002",
      "timestamp_us": 1710002223335,
      "device_id": "mouse_001",
      "type": "mouse_move",
      "dx": 4,
      "dy": -1,
      "buttons": ["left"]
    }
  ]
}
```

Behavior requirements:

1. Events are accepted in host-normalized format from browser session clients.
2. Translation uses active machine + profile + mapping table.
3. Resulting virtual events are injected into emulated device pipelines in deterministic tick order.
4. If input is disabled or capture is not active, server returns `INPUT_CAPTURE_DISABLED` or `INPUT_CAPTURE_NOT_ACTIVE` for real-time event injection.

### 9.6.6 Browser capture state and controls

The input pipeline is browser-session aware and exposes capture controls.

#### Read capture state

- `GET /api/v2/input/capture/state?session_id=...&browser_session_id=...`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": true,
  "policy": {
    "state": "enabled_captured",
    "source": "user_request",
    "reason": "capture_config_applied",
    "changed_at_us": 1710002223300
  },
  "escape_release": {
    "enabled": true,
    "sequence": ["Escape", "Escape"],
    "timeout_ms": 600
  }
}
```

#### Input policy state model

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only policy payload semantics.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

Policy states:

- `disabled`
- `enabled_idle`
- `enabled_captured`

Policy metadata fields:

- `policy.state` (enum above)
- `policy.source` (enum: `user_request`, `system_guard`, `lifecycle_transition`)
- `policy.reason` (string, stable reason token)
- `policy.changed_at_us` (uint64, monotonic runtime time base)

Deterministic transition rules:

| Current | Trigger | Next | Deterministic result |
|---|---|---|---|
| `disabled` | enable input policy | `enabled_idle` | success |
| `enabled_idle` | disable input policy | `disabled` | success |
| `enabled_idle` | capture activated by configured mode | `enabled_captured` | success |
| `enabled_captured` | capture released or pointer-exit rule | `enabled_idle` | success |
| `enabled_captured` | disable input policy | `disabled` | success |
| any | request resulting in same state | unchanged | success (no-op) |

Invalid policy transitions:

- A request that targets an unreachable policy state for the current context must return `INPUT_POLICY_INVALID_STATE`.
- A request that violates session/browser policy constraints (for example cross-browser control attempt) must return `INPUT_POLICY_VIOLATION`.

Policy invalid-state error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710002223340,
  "error": {
    "code": "INPUT_POLICY_INVALID_STATE",
    "category": "input",
    "message": "Cannot release capture when policy state is enabled_idle",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "browser_session_id": "web_01H...",
      "current_policy_state": "enabled_idle",
      "requested_action": "capture_release",
      "allowed_states": ["enabled_captured"]
    }
  }
}
```

Policy violation error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710002223341,
  "error": {
    "code": "INPUT_POLICY_VIOLATION",
    "category": "input",
    "message": "Browser session does not own capture policy",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "browser_session_id": "web_02H...",
      "policy_owner_browser_session_id": "web_01H...",
      "requested_action": "capture_config_update"
    }
  }
}
```

#### Configure capture policy

- `POST /api/v2/input/capture/config`

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "mouse_over",
  "escape_release": {
    "enabled": true,
    "sequence": ["Escape", "Escape"],
    "timeout_ms": 600
  }
}
```

`capture_mode` values:

- `mouse_over`
- `click_to_capture`

#### Enable/disable input policy (idempotent)

- `POST /api/v2/input/policy/enabled`

Request:

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "enabled": true,
  "reason": "user_toggle"
}
```

Request contract:

- `enabled` is required boolean.
- `reason` is optional stable reason token.
- Endpoint must only change `policy.state` and enable/disable behavior; capture-mode fields are out of scope for this endpoint.

Response `data` fields:

- `session_id` (string)
- `browser_session_id` (string)
- `requested_enabled` (bool)
- `result` (enum: `applied`, `no_op`)
- `previous_policy_state` (enum from section `9.6.6`)
- `policy` (object with `state`, `source`, `reason`, `changed_at_us`)

Idempotency rules:

- `enabled=true` from `disabled` -> `policy.state=enabled_idle`, `result=applied`.
- `enabled=true` from `enabled_idle` or `enabled_captured` -> unchanged state, `result=no_op`.
- `enabled=false` from `enabled_idle` or `enabled_captured` -> `policy.state=disabled`, `result=applied`.
- `enabled=false` from `disabled` -> unchanged state, `result=no_op`.
- `result=no_op` responses must still return full `policy` object and current state snapshot.

Success example (`applied`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "requested_enabled": true,
  "result": "applied",
  "previous_policy_state": "disabled",
  "policy": {
    "state": "enabled_idle",
    "source": "user_request",
    "reason": "user_toggle",
    "changed_at_us": 1710002223400
  }
}
```

No-op example (`idempotent`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "requested_enabled": true,
  "result": "no_op",
  "previous_policy_state": "enabled_idle",
  "policy": {
    "state": "enabled_idle",
    "source": "user_request",
    "reason": "idempotent_enable",
    "changed_at_us": 1710002223400
  }
}
```

Transition guard failures:

- Invalid `enabled` mode/type or unsupported action form must return `INPUT_POLICY_MODE_INVALID`.
- Invalid session/browser-session conditions (missing/inactive session, unknown browser session, mismatched ownership) must return `INPUT_POLICY_SESSION_INVALID` or `INPUT_POLICY_VIOLATION` as applicable.

Failure example (invalid mode):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710002223401,
  "error": {
    "code": "INPUT_POLICY_MODE_INVALID",
    "category": "input",
    "message": "Field enabled must be boolean",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "browser_session_id": "web_01H...",
      "field": "enabled",
      "expected": "boolean",
      "actual": "string"
    }
  }
}
```

Failure example (invalid session condition):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710002223402,
  "error": {
    "code": "INPUT_POLICY_SESSION_INVALID",
    "category": "input",
    "message": "Browser session is not active for requested engine session",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "browser_session_id": "web_09H...",
      "requested_action": "set_enabled"
    }
  }
}
```

Behavior:

1. `mouse_over`: capture active only while pointer is over the display canvas.
2. `click_to_capture`: capture begins on canvas click and persists until released.
3. Escape release sequence is evaluated only in `click_to_capture` mode.
4. `input_enabled=false` disables translation and event injection processing.

#### `mouse_over` capture state machine

State variables:

- `capture_mode` (must be `mouse_over` for this state machine)
- `input_enabled` (boolean)
- `pointer_over_canvas` (boolean runtime signal)
- `capture_active` (derived boolean output)
- `policy.state` (from section `9.6.6`: `disabled`, `enabled_idle`, `enabled_captured`)

Deterministic transition matrix (`capture_mode=mouse_over`):

| Current policy/capture | Trigger | Next policy/capture | Result |
|---|---|---|---|
| `disabled` / `capture_active=false` | pointer enters/leaves canvas | unchanged | no-op |
| `enabled_idle` / `capture_active=false` | `pointer_over_canvas=true` | `enabled_captured` / `capture_active=true` | applied |
| `enabled_captured` / `capture_active=true` | `pointer_over_canvas=false` | `enabled_idle` / `capture_active=false` | applied |
| `enabled_idle` / `capture_active=false` | `pointer_over_canvas=false` | unchanged | no-op |
| `enabled_captured` / `capture_active=true` | `pointer_over_canvas=true` | unchanged | no-op |

Guard predicates:

- `MO-GUARD-01` (mode guard): this state machine is valid only when `capture_mode=mouse_over`; otherwise transitions must be rejected for mouse-over specific actions with `INPUT_POLICY_MODE_INVALID`.
- `MO-GUARD-02` (session guard): missing/inactive `session_id` or `browser_session_id` for capture-state operations must return `INPUT_POLICY_SESSION_INVALID`.
- `MO-GUARD-03` (input disable guard): if `input_enabled=false`, `capture_active` must be forced `false` and pointer transitions become deterministic no-op.

Determinism rules:

- `capture_active` is fully derived: `capture_active = (input_enabled && capture_mode == "mouse_over" && pointer_over_canvas)`.
- On every applied transition, `policy.changed_at_us` must advance monotonically and `policy.source` should be `system_guard` when transition is pointer-driven.
- Repeated identical pointer signals must not oscillate policy state and must produce no-op outcomes.

Enter/leave hook contract (`mouse_over`):

- Host/browser integration must surface two deterministic hook triggers for this mode:
  - `pointer_enter_hook` (canvas pointer-enter)
  - `pointer_leave_hook` (canvas pointer-leave)
- Hook-to-state mapping:
  - `pointer_enter_hook` sets `pointer_over_canvas=true`.
  - `pointer_leave_hook` sets `pointer_over_canvas=false`.
- Hook processing must evaluate guards `MO-GUARD-01..03` before state mutation.
- Hook dispatch must be idempotent: repeated enter while already over-canvas or repeated leave while already out-of-canvas is deterministic no-op.
- Applied hook transitions must emit `input_policy_changed` with `source=system_guard`, `request_action` equal to hook action name, and deterministic `transition_result` (`applied` or `no_op`).

Release behavior (`mouse_over`):

- Implicit release is mandatory on `pointer_leave_hook` when state is captured (`enabled_captured -> enabled_idle`, `capture_active=true -> false`).
- `POST /api/v2/input/capture/release` while `capture_mode=mouse_over` is accepted but must be deterministic no-op; release authority is pointer-leave in this mode.
- Explicit release no-op in `mouse_over` mode must be observable through `input_policy_changed` with:
  - `request_action="explicit_release"`
  - `reason="mouse_over_release_no_op"`
  - `transition_result="no_op"`

State snapshot example (`pointer_over_canvas=true`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "mouse_over",
  "capture_active": true,
  "policy": {
    "state": "enabled_captured",
    "source": "system_guard",
    "reason": "mouse_over_pointer_in",
    "changed_at_us": 1710002223500
  }
}
```

State snapshot example (`pointer_over_canvas=false`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "mouse_over",
  "capture_active": false,
  "policy": {
    "state": "enabled_idle",
    "source": "system_guard",
    "reason": "mouse_over_pointer_out",
    "changed_at_us": 1710002223520
  }
}
```

#### `click_to_capture` acquisition state machine

State variables:

- `capture_mode` (must be `click_to_capture` for this state machine)
- `input_enabled` (boolean)
- `capture_active` (boolean)
- `policy.state` (from section `9.6.6`: `disabled`, `enabled_idle`, `enabled_captured`)

Deterministic acquisition transition matrix (`capture_mode=click_to_capture`):

| Current policy/capture | Trigger | Next policy/capture | Result |
|---|---|---|---|
| `disabled` / `capture_active=false` | canvas click acquire request | unchanged | no-op |
| `enabled_idle` / `capture_active=false` | canvas click acquire request | `enabled_captured` / `capture_active=true` | applied |
| `enabled_captured` / `capture_active=true` | canvas click acquire request | unchanged | no-op |
| `enabled_idle` / `capture_active=false` | repeated non-acquire host input | unchanged | no-op |
| `enabled_captured` / `capture_active=true` | repeated acquire request | unchanged | no-op |

Guard predicates:

- `CT-GUARD-01` (mode guard): click-to-capture acquisition transitions are valid only when `capture_mode=click_to_capture`; otherwise reject click-acquire specific actions with `INPUT_POLICY_MODE_INVALID`.
- `CT-GUARD-02` (session guard): missing/inactive `session_id` or `browser_session_id` for acquisition actions must return `INPUT_POLICY_SESSION_INVALID`.
- `CT-GUARD-03` (input disable guard): if `input_enabled=false`, acquisition requests must be deterministic no-op with `capture_active=false`.

Determinism rules:

- `capture_active` may transition from `false` to `true` only through an explicit canvas click acquire trigger while `capture_mode=click_to_capture`.
- Applied acquisition must set `policy.state=enabled_captured`, `policy.source=system_guard`, and monotonically advance `policy.changed_at_us`.
- Duplicate acquisition requests while already captured must not change state and must be observable as no-op.

Acquisition snapshot example (`applied`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": true,
  "policy": {
    "state": "enabled_captured",
    "source": "system_guard",
    "reason": "click_to_capture_acquired",
    "changed_at_us": 1710002223600
  }
}
```

Acquisition snapshot example (`no_op` duplicate acquire):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": true,
  "policy": {
    "state": "enabled_captured",
    "source": "system_guard",
    "reason": "click_to_capture_already_active",
    "changed_at_us": 1710002223600
  }
}
```

#### Explicit capture release

- `POST /api/v2/input/capture/release`

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "reason": "user_escape_sequence"
}
```

#### Escape-release and focus-recovery (`click_to_capture`)

State variables:

- `capture_mode` (must be `click_to_capture`)
- `input_enabled` (boolean)
- `capture_active` (boolean)
- `browser_focus` (boolean runtime signal)
- `escape_release.enabled` (boolean)
- `escape_release.sequence` (array, default `["Escape", "Escape"]`)
- `escape_release.timeout_ms` (uint32)

Deterministic transition matrix (`capture_mode=click_to_capture`):

| Current policy/capture | Trigger | Next policy/capture | Result |
|---|---|---|---|
| `enabled_captured` / `capture_active=true` | configured escape sequence completed within timeout | `enabled_idle` / `capture_active=false` | applied |
| `enabled_captured` / `capture_active=true` | browser focus lost | `enabled_idle` / `capture_active=false` | applied |
| `enabled_idle` / `capture_active=false` | browser focus regained | unchanged | no-op |
| `enabled_idle` / `capture_active=false` | partial/expired escape sequence | unchanged | no-op |
| `disabled` / `capture_active=false` | escape sequence or focus transitions | unchanged | no-op |

Guard predicates:

- `ER-GUARD-01` (mode guard): escape-release transitions are valid only when `capture_mode=click_to_capture`; otherwise release-by-escape action must return `INPUT_POLICY_MODE_INVALID`.
- `ER-GUARD-02` (session guard): missing/inactive `session_id` or `browser_session_id` for escape/focus transitions must return `INPUT_POLICY_SESSION_INVALID`.
- `ER-GUARD-03` (sequence guard): escape sequence must complete exactly as configured within `timeout_ms`, otherwise transition is deterministic no-op.

Determinism rules:

- Focus loss while captured always forces release to idle; auto re-acquire on focus regain is forbidden.
- Focus regain restores eligibility for capture but does not mutate policy state unless an acquire trigger occurs.
- Escape-release and focus-loss release must emit `input_policy_changed` with `source=system_guard` and monotonic `event_seq`/`event_timestamp_us`.

Focus-recovery snapshot example (`focus lost -> released`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": false,
  "policy": {
    "state": "enabled_idle",
    "source": "system_guard",
    "reason": "focus_lost_release",
    "changed_at_us": 1710002223650
  }
}
```

Focus-recovery snapshot example (`focus regained no auto-acquire`):

```json
{
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": false,
  "policy": {
    "state": "enabled_idle",
    "source": "system_guard",
    "reason": "focus_regained_idle",
    "changed_at_us": 1710002223650
  }
}
```

### 9.6.7 Input stream (translated events + diagnostics)

- `GET /api/v2/input/stream`

Client subscribe message:

```json
{
  "type": "subscribe",
  "session_id": "ses_01H...",
  "include_host_events": false,
  "include_virtual_events": true,
  "include_diagnostics": true,
  "include_policy_events": true
}
```

Translated event:

```json
{
  "type": "input_translated",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 1201,
  "event_timestamp_us": 1710002223468,
  "host_event_id": "evt_1001",
  "tick": 297716600,
  "cycle": 74429500,
  "mapping_profile_id": "atari_st_custom_gamepad_v1",
  "mapping_revision": 1,
  "host_event": {
    "device_id": "kbd_001",
    "type": "key_down",
    "code": "KeyA"
  },
  "virtual_event": {
    "target": "ikbd.key",
    "value": "ST_SC_A",
    "phase": "press"
  }
}
```

Translated event payload contract:

- Required top-level fields:
  - `type` (must be `input_translated`)
  - `schema_version` (uint32)
  - `session_id` (string)
  - `browser_session_id` (string)
  - `event_seq` (uint64)
  - `event_timestamp_us` (uint64)
  - `host_event_id` (string)
  - `tick` (uint64)
  - `cycle` (uint64)
  - `mapping_profile_id` (string)
  - `mapping_revision` (uint32)
  - `host_event` (object)
  - `virtual_event` (object)
- `host_event` echoes normalized input used for translation; `virtual_event` is the deterministic mapped output for device pipeline injection.
- `mapping_profile_id` + `mapping_revision` identify the exact mapping snapshot used for translation.

Translated event ordering contract:

- `event_seq` is strictly monotonic per `(session_id, browser_session_id)` input stream and increments by exactly `1` for each emitted `input_translated` event.
- `event_timestamp_us` is monotonic non-decreasing and must correspond to server emission time base.
- `(tick, cycle)` ordering must be monotonic lexicographic for emitted translated events in a stream.
- Ordering source of truth for clients is `event_seq`; `event_timestamp_us`, `tick`, and `cycle` are observability fields and must not be used to reorder across sequence gaps.
- Sequence gaps indicate stream degradation/loss and must be reflected by diagnostics counters (`dropped_events`) in subsequent `input_diagnostics` events.

Translated stream emitter contract:

- Emitter is single-writer per `(session_id, browser_session_id)` stream and is the sole allocator of translated-event `event_seq`.
- Emission path order is deterministic:
  1. Validate capture/policy eligibility.
  2. Translate host event using active mapping snapshot.
  3. Allocate next `event_seq`.
  4. Stamp `event_timestamp_us`.
  5. Publish `input_translated`.
- Failed translation or validation must not allocate/publish `input_translated`; failure impact is observable through diagnostics counters.
- Emitter must publish diagnostics snapshots at bounded cadence (implementation-defined period) and on detected sequencing anomalies.

Sequencing checks (runtime validation):

- Check `SEQ-CHECK-01` (strict sequence): expected next `event_seq = prev_event_seq + 1`; deviations increment `sequencing_violations`.
- Check `SEQ-CHECK-02` (timestamp monotonicity): `event_timestamp_us` must be non-decreasing; regression increments `sequencing_violations`.
- Check `SEQ-CHECK-03` (tick/cycle monotonicity): `(tick, cycle)` must be monotonic lexicographic; regression increments `sequencing_violations`.
- On any sequencing check failure, emitter continues stream operation and reports anomaly in diagnostics (`last_sequence_error`, `last_sequence_error_at_us`) without rewriting previously emitted events.

Diagnostics event:

```json
{
  "type": "input_diagnostics",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "last_emitted_event_seq": 1201,
  "emitted_events": 4521,
  "queue_depth": 3,
  "queue_capacity": 128,
  "dropped_events": 0,
  "mapping_profile_id": "atari_st_default_v1",
  "sequencing_violations": 0,
  "last_sequence_error": null,
  "last_sequence_error_at_us": null
}
```

Diagnostics event example (sequencing anomaly observed):

```json
{
  "type": "input_diagnostics",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "last_emitted_event_seq": 1210,
  "emitted_events": 4530,
  "queue_depth": 6,
  "queue_capacity": 128,
  "dropped_events": 1,
  "mapping_profile_id": "atari_st_default_v1",
  "sequencing_violations": 1,
  "last_sequence_error": "SEQ-CHECK-02:event_timestamp_us_regression",
  "last_sequence_error_at_us": 1710002223480
}
```

Capture state event:

```json
{
  "type": "input_capture_state",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "input_enabled": true,
  "capture_mode": "click_to_capture",
  "capture_active": false,
  "policy": {
    "state": "enabled_idle",
    "source": "user_request",
    "reason": "escape_sequence",
    "changed_at_us": 1710002223342
  }
}
```

Policy-change event:

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 901,
  "event_timestamp_us": 1710002223450,
  "source": "user_request",
  "prior_state": "disabled",
  "new_state": "enabled_idle",
  "reason": "user_toggle",
  "request_action": "set_enabled",
  "requested_enabled": true,
  "transition_result": "applied"
}
```

Policy-change no-op event:

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 902,
  "event_timestamp_us": 1710002223452,
  "source": "user_request",
  "prior_state": "enabled_idle",
  "new_state": "enabled_idle",
  "reason": "idempotent_enable",
  "request_action": "set_enabled",
  "requested_enabled": true,
  "transition_result": "no_op"
}
```

Policy-change event example (`mouse_over` enter hook applied):

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 903,
  "event_timestamp_us": 1710002223460,
  "source": "system_guard",
  "prior_state": "enabled_idle",
  "new_state": "enabled_captured",
  "reason": "mouse_over_pointer_in",
  "request_action": "pointer_enter_hook",
  "requested_enabled": true,
  "transition_result": "applied"
}
```

Policy-change event example (`mouse_over` explicit release no-op):

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 904,
  "event_timestamp_us": 1710002223462,
  "source": "system_guard",
  "prior_state": "enabled_idle",
  "new_state": "enabled_idle",
  "reason": "mouse_over_release_no_op",
  "request_action": "explicit_release",
  "requested_enabled": true,
  "transition_result": "no_op"
}
```

Policy-change event example (`click_to_capture` escape release applied):

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 905,
  "event_timestamp_us": 1710002223660,
  "source": "system_guard",
  "prior_state": "enabled_captured",
  "new_state": "enabled_idle",
  "reason": "escape_sequence",
  "request_action": "escape_release",
  "requested_enabled": true,
  "transition_result": "applied"
}
```

Policy-change event example (`click_to_capture` focus regained no-op):

```json
{
  "type": "input_policy_changed",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "browser_session_id": "web_01H...",
  "event_seq": 906,
  "event_timestamp_us": 1710002223662,
  "source": "system_guard",
  "prior_state": "enabled_idle",
  "new_state": "enabled_idle",
  "reason": "focus_regained_idle",
  "request_action": "focus_regain",
  "requested_enabled": true,
  "transition_result": "no_op"
}
```

Policy-change event contract and ordering rules:

- `input_policy_changed` is emitted for every accepted policy mutation request, including idempotent no-op requests.
- Required fields: `source`, `prior_state`, `new_state`, `reason`, `event_timestamp_us`, `event_seq`, `transition_result`.
- `source` must align with the policy metadata source model in section `9.6.6`.
- `transition_result` enum: `applied`, `no_op`.
- `event_seq` is uint64, strictly monotonic per `(session_id, browser_session_id)` input stream connection, incrementing by `1` for each policy-change event.
- `event_timestamp_us` is uint64 and monotonic non-decreasing per stream connection.
- Client ordering source of truth is `event_seq`; `event_timestamp_us` is informational and must not be used for reordering.
- `transition_result=no_op` must satisfy `prior_state == new_state`.
- `transition_result=applied` must satisfy `(prior_state, new_state)` as an allowed pair from the deterministic transition rules in section `9.6.6`.
- Duplicate/no-op requests are therefore observable via deterministic event shape (`transition_result=no_op`, same prior/new states, unique next `event_seq`).

---

## 10. Streaming APIs

## 10.1 Common stream handshake

Each WebSocket stream supports optional query params:

- `session_id`
- `schema_version` (default `1`)
- channel-specific filter params

Server sends first message:

```json
{
  "type": "hello",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "stream": "video"
}
```

## 10.2 Video stream

- `GET /api/v2/stream/video`

Modes:

- `mode=frame` (default)
- `mode=debug_scanline`

Video metadata channel contract:

- Video stream metadata is emitted on a JSON sideband channel named `video.metadata.v1`.
- Each metadata message must validate against schema `video_frame_meta_v1` before binary payload emission.
- Per stream connection, metadata messages are strictly ordered by `frame_id` ascending with no duplicates.
- The binary frame payload that follows a metadata message must belong to the same `frame_id` and `payload_bytes` declared by that metadata message.

`video_frame_meta_v1` required fields:

- `type` (string, must equal `video_frame_meta`)
- `schema_version` (uint32, must equal `1`)
- `channel` (string, must equal `video.metadata.v1`)
- `session_id` (string)
- `frame_id` (uint64)
- `timestamp_us` (uint64)
- `width` (uint32, `>0`)
- `height` (uint32, `>0`)
- `pixel_format` (enum)
- `payload_bytes` (uint32, `>0`)

`pixel_format` enum values:

- `RGB565`
- `XRGB8888`
- `RGB888`

Metadata channel validation rules:

- `schema_version` values other than `1` must fail with `UNSUPPORTED_VERSION`.
- Unknown `pixel_format` values or non-positive dimensions/payload sizes must fail with `BAD_REQUEST`.
- If metadata cannot be paired to the immediately following binary payload (`frame_id` mismatch or byte-length mismatch), stream delivery must fail-fast with `INTERNAL_ERROR`.

Video payload stream emitter contract:

- Emitter output unit is an ordered pair: one `video_frame_meta` JSON message immediately followed by one binary payload frame.
- For a given stream connection, payload emitter must preserve strict frame order and emit exactly one payload per accepted metadata frame.
- Binary payload byte length must equal metadata `payload_bytes`; mismatches are emitter integrity failures.
- Emitter must not block the core emulation loop; under pressure it must use bounded queue behavior and surface degradation through stream health/backpressure signaling (sections `10.7`, `10.8`, `13.2`, `13.3`).

Video emitter sequencing checks:

- `VID-EMIT-01`: `frame_id(next) > frame_id(prev)` for consecutive emitted metadata frames.
- `VID-EMIT-02`: each emitted metadata frame is followed by exactly one binary payload before any next metadata frame.
- `VID-EMIT-03`: emitted binary payload byte length equals the associated metadata `payload_bytes`.

Video pacing controls:

- Video stream pacing is configured through stream control message `set_rate_limit` (section `10.7`) with video-scoped payload fields.
- Video pacing payload schema (`video_pacing_v1`) fields:
  - `stream` (string, must equal `video`)
  - `pacing_mode` (enum: `realtime`, `fixed_fps`)
  - `target_fps` (uint32, required when `pacing_mode=fixed_fps`, range `1..240`)
  - `max_burst_frames` (uint32, optional, range `1..8`, default `1`)
- `pacing_mode=realtime` uses runtime/profile cadence without fixed-FPS throttling.
- `pacing_mode=fixed_fps` enforces ceiling pacing at `target_fps`; when emitter backlog persists, server must mark throttle/backpressure status via stream health.

Video pacing guard failures:

- Invalid pacing payload shape or out-of-range values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- If sustained transport pressure prevents policy-compliant delivery, stream degradation must surface with stream category backpressure signaling (`STREAM_BACKPRESSURE`).

Metadata envelope (JSON sideband):

```json
{
  "type": "video_frame_meta",
  "schema_version": 1,
  "channel": "video.metadata.v1",
  "session_id": "ses_01H...",
  "frame_id": 8821,
  "timestamp_us": 1710001112223,
  "width": 640,
  "height": 400,
  "pixel_format": "RGB565",
  "payload_bytes": 512000
}
```

Binary frame payload follows.

Video pacing control message example (`set_rate_limit`):

```json
{
  "type": "set_rate_limit",
  "stream": "video",
  "pacing_mode": "fixed_fps",
  "target_fps": 50,
  "max_burst_frames": 2
}
```

## 10.3 Audio stream

- `GET /api/v2/stream/audio`

Audio metadata channel contract:

- Audio stream metadata is emitted on a JSON sideband channel named `audio.metadata.v1`.
- Each metadata message must validate against schema `audio_chunk_meta_v1` before binary payload emission.
- Per stream connection, metadata messages are strictly ordered by `chunk_id` ascending with no duplicates.
- The binary audio payload that follows a metadata message must belong to the same `chunk_id` and `payload_bytes` declared by that metadata message.

`audio_chunk_meta_v1` required fields:

- `type` (string, must equal `audio_chunk_meta`)
- `schema_version` (uint32, must equal `1`)
- `channel` (string, must equal `audio.metadata.v1`)
- `session_id` (string)
- `chunk_id` (uint64)
- `timestamp_us` (uint64)
- `sample_rate` (uint32, `>0`)
- `channels` (uint32, `>0`)
- `format` (enum)
- `frames` (uint32, `>0`)
- `payload_bytes` (uint32, `>0`)

`format` enum values:

- `PCM_S16LE`
- `PCM_F32LE`

Metadata channel validation rules:

- `schema_version` values other than `1` must fail with `UNSUPPORTED_VERSION`.
- Unknown `format` values or non-positive sample/chunk dimensions (`sample_rate`, `channels`, `frames`, `payload_bytes`) must fail with `BAD_REQUEST`.
- If metadata cannot be paired to the immediately following binary payload (`chunk_id` mismatch or byte-length mismatch), stream delivery must fail-fast with `INTERNAL_ERROR`.

Audio payload stream emitter contract:

- Emitter output unit is an ordered pair: one `audio_chunk_meta` JSON message immediately followed by one binary payload chunk.
- For a given stream connection, payload emitter must preserve strict chunk order and emit exactly one payload per accepted metadata chunk.
- Binary payload byte length must equal metadata `payload_bytes`; mismatches are emitter integrity failures.
- Emitter must not block the core emulation loop; under pressure it must use bounded queue behavior and surface degradation through stream health/backpressure signaling (sections `10.7`, `10.8`, `13.2`, `13.3`).

Audio emitter sequencing checks:

- `AUD-EMIT-01`: `chunk_id(next) > chunk_id(prev)` for consecutive emitted metadata chunks.
- `AUD-EMIT-02`: each emitted metadata chunk is followed by exactly one binary payload before any next metadata chunk.
- `AUD-EMIT-03`: emitted binary payload byte length equals the associated metadata `payload_bytes`.

Audio pacing controls:

- Audio stream pacing is configured through stream control message `set_rate_limit` (section `10.7`) with audio-scoped payload fields.
- Audio pacing payload schema (`audio_pacing_v1`) fields:
  - `stream` (string, must equal `audio`)
  - `pacing_mode` (enum: `realtime`, `fixed_hz`)
  - `target_hz` (uint32, required when `pacing_mode=fixed_hz`, range `1..48000`)
  - `max_burst_chunks` (uint32, optional, range `1..16`, default `1`)
- `pacing_mode=realtime` uses runtime/profile audio cadence without fixed-hz throttling.
- `pacing_mode=fixed_hz` enforces ceiling pacing at `target_hz`; when emitter backlog persists, server must mark throttle/backpressure status via stream health.

Audio pacing guard failures:

- Invalid pacing payload shape or out-of-range values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- If sustained transport pressure prevents policy-compliant delivery, stream degradation must surface with stream category backpressure signaling (`STREAM_BACKPRESSURE`).

Metadata envelope:

```json
{
  "type": "audio_chunk_meta",
  "schema_version": 1,
  "channel": "audio.metadata.v1",
  "session_id": "ses_01H...",
  "chunk_id": 99218,
  "timestamp_us": 1710001112225,
  "sample_rate": 48000,
  "channels": 2,
  "format": "PCM_S16LE",
  "frames": 1024,
  "payload_bytes": 4096
}
```

Binary audio payload follows.

Audio pacing control message example (`set_rate_limit`):

```json
{
  "type": "set_rate_limit",
  "stream": "audio",
  "pacing_mode": "fixed_hz",
  "target_hz": 240,
  "max_burst_chunks": 4
}
```

## 10.4 Register inspection stream

- `GET /api/v2/inspect/registers/stream`

Register snapshot schema contract:

- Register inspection stream events must validate against schema `register_snapshot_v1`.
- Snapshot events are emitted as `type=register_update` and carry one deterministic register observation per event.
- For a stream connection, events are ordered by `(tick, cycle)` ascending; if `(tick, cycle)` are equal, ordering source of truth is emission order.

`register_snapshot_v1` required fields:

- `type` (string, must equal `register_update`)
- `schema_version` (uint32, must equal `1`)
- `session_id` (string)
- `tick` (uint64)
- `cycle` (uint64)
- `component` (string)
- `register` (string)
- `old_value` (string)
- `new_value` (string)
- `value_encoding` (enum)
- `value_bits` (uint32, `>0`)

`value_encoding` enum values:

- `hex`
- `signed`
- `unsigned`

Selective filter fields (`subscribe` / `set_filter`):

- `components` (array of strings, optional): include only listed components.
- `registers` (array of strings, optional): exact register-name allowlist.
- `register_prefixes` (array of strings, optional): include registers whose names start with one of the prefixes.
- `changed_only` (bool, optional, default `true`): when `true`, events with `old_value == new_value` must be suppressed.
- `mode` (enum: `event`, `interval`) and `interval_us` (uint32, required when `mode=interval`, must be `>0`).

Filter and schema guard failures:

- Invalid filter payload shape, unsupported `mode`, or invalid `interval_us` values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Filter semantics that cannot be resolved by inspection backend (unknown component/register selectors) -> `INSPECT_FILTER_INVALID`.

Register snapshot stream publisher contract:

- Publisher pipeline order per candidate snapshot is fixed: `collect -> apply_filters -> schema_validate -> emit`.
- Publisher output event type is `register_update`; each emitted event must include publisher ordering fields `event_seq` (uint64) and `event_timestamp_us` (uint64).
- `event_seq` is strictly monotonic per stream connection and increments by exactly `+1` for each emitted register event.
- `event_timestamp_us` is monotonic non-decreasing per stream connection and must use the same runtime time base as section `4`.
- If `changed_only=true`, publisher must suppress events where `old_value == new_value`.

Register publisher validation checks:

- `REG-PUB-01`: `event_seq(next) == event_seq(prev) + 1`.
- `REG-PUB-02`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `REG-PUB-03`: emitted event matches active selector constraints (`components`, `registers`, `register_prefixes`).
- `REG-PUB-04`: when `changed_only=true`, all emitted events satisfy `old_value != new_value`.

Register publisher failure mapping:

- Runtime schema validation failure for an internally-produced register event -> `INTERNAL_ERROR` with fail-fast diagnostics.
- Validation-check failure (`REG-PUB-*`) -> `INTERNAL_ERROR` and degraded-delivery signaling via stream health/event-stream delivery semantics.
- Backpressure-induced drops must increment `stream_health.dropped_events` and preserve publisher sequencing invariants over emitted (non-dropped) events.

Client subscribe message:

```json
{
  "type": "subscribe",
  "components": ["cpu", "mfp", "shifter"],
  "registers": ["PC", "SR"],
  "register_prefixes": ["D", "A"],
  "changed_only": true,
  "mode": "event",
  "interval_us": 0
}
```

Server event:

```json
{
  "type": "register_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 8841,
  "event_timestamp_us": 1710001113331,
  "tick": 297716488,
  "cycle": 74429122,
  "component": "cpu",
  "register": "PC",
  "old_value": "0x00FC1234",
  "new_value": "0x00FC1236",
  "value_encoding": "hex",
  "value_bits": 32
}
```

Register publisher validation trace example:

```json
{
  "stream": "registers",
  "session_id": "ses_01H...",
  "checks": {
    "REG-PUB-01": "pass",
    "REG-PUB-02": "pass",
    "REG-PUB-03": "pass",
    "REG-PUB-04": "pass"
  },
  "events_emitted": 128,
  "events_suppressed_changed_only": 34,
  "last_event_seq": 8968,
  "last_event_timestamp_us": 1710001115560
}
```

## 10.5 Bus trace stream

- `GET /api/v2/inspect/bus/stream`

Bus filter request model:

- Bus stream filter requests (`subscribe` and `set_filter`) must validate against schema `bus_filter_v1`.
- Filter updates apply atomically per stream connection; partial application is not allowed.

`bus_filter_v1` fields:

- `type` (string, must be `subscribe` or `set_filter`)
- `address_ranges` (array of strings, optional, `0x<start>-0x<end>` inclusive; `start<=end`)
- `access_types` (array of enum, optional)
- `components` (array of strings, optional)
- `level` (enum, optional)
- `max_events_per_sec` (uint32, optional, `>0`)

`access_types` enum values:

- `read`
- `write`
- `iack`
- `dma`

`level` enum values:

- `info`
- `debug`

Bus filter guard rules:

- Invalid filter payload shape, malformed/overlapping ranges, unknown enum values, or non-positive `max_events_per_sec` -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Semantically unresolved selectors (for example unknown component source) -> `INSPECT_FILTER_INVALID`.

Filtered bus stream publisher contract:

- Publisher pipeline order per candidate bus event is fixed: `capture -> apply_filter -> validate -> emit`.
- A bus event may be emitted only when it satisfies all active selectors (`address_ranges`, `access_types`, `components`, `level`).
- Emitted bus events must include `event_seq` (uint64) and `event_timestamp_us` (uint64) for deterministic ordering.
- For one stream connection, `event_seq` is strictly monotonic and increases by `+1` for each emitted bus event.
- `event_timestamp_us` is monotonic non-decreasing and uses the same runtime time base as section `4`.

Filtered bus publisher checks:

- `BUS-FLT-01`: every emitted event matches active selector set.
- `BUS-FLT-02`: `event_seq(next) == event_seq(prev) + 1` for emitted events.
- `BUS-FLT-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `BUS-FLT-04`: events rejected by filters must not be emitted.

Filtered bus publisher failures:

- Validation check failure (`BUS-FLT-*`) -> `INTERNAL_ERROR` with deterministic diagnostics.
- Under sustained load, publisher must preserve sequence invariants over emitted events while surfacing degraded delivery via stream-health/event-stream delivery signaling.

Client filter:

```json
{
  "type": "subscribe",
  "address_ranges": ["0x00FF8000-0x00FF82FF"],
  "access_types": ["read", "write", "iack", "dma"],
  "components": ["cpu", "dma", "shifter"],
  "level": "debug"
}
```

Server event:

```json
{
  "type": "bus_event",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 44102,
  "event_timestamp_us": 1710001117740,
  "tick": 297716490,
  "cycle": 74429123,
  "source": "cpu",
  "access": "write",
  "address": "0x00FF820A",
  "data": "0x0003",
  "size": 2,
  "wait_states": 1
}
```

## 10.6 Memory map trace stream

- `GET /api/v2/inspect/memory/stream`

Memory filter request model:

- Memory stream filter requests (`subscribe` and `set_filter`) must validate against schema `memory_filter_v1`.
- Filter updates apply atomically per stream connection; partial application is not allowed.

`memory_filter_v1` fields:

- `type` (string, must be `subscribe` or `set_filter`)
- `regions` (array of strings, optional)
- `address_ranges` (array of strings, optional, `0x<start>-0x<end>` inclusive; `start<=end`)
- `access_types` (array of enum, optional)
- `components` (array of strings, optional)
- `mapped_targets` (array of strings, optional)
- `level` (enum, optional)
- `max_events_per_sec` (uint32, optional, `>0`)

`access_types` enum values:

- `read`
- `write`

`level` enum values:

- `info`
- `debug`

Memory filter guard rules:

- Invalid filter payload shape, malformed ranges, unknown enum values, or non-positive `max_events_per_sec` -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Semantically unresolved selectors (for example unknown region or mapped target selector) -> `INSPECT_FILTER_INVALID`.

Filtered memory stream publisher contract:

- Publisher pipeline order per candidate memory-map event is fixed: `capture -> apply_filter -> validate -> emit`.
- A memory event may be emitted only when it satisfies all active selectors (`regions`, `address_ranges`, `access_types`, `components`, `mapped_targets`, `level`).
- Emitted memory events must include `event_seq` (uint64) and `event_timestamp_us` (uint64) for deterministic ordering.
- For one stream connection, `event_seq` is strictly monotonic and increases by `+1` for each emitted memory event.
- `event_timestamp_us` is monotonic non-decreasing and uses the same runtime time base as section `4`.

Filtered memory publisher checks:

- `MEM-FLT-01`: every emitted event matches active selector set.
- `MEM-FLT-02`: `event_seq(next) == event_seq(prev) + 1` for emitted events.
- `MEM-FLT-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `MEM-FLT-04`: events rejected by filters must not be emitted.

Filtered memory publisher failures:

- Validation check failure (`MEM-FLT-*`) -> `INTERNAL_ERROR` with deterministic diagnostics.
- Under sustained load, publisher must preserve sequence invariants over emitted events while surfacing degraded delivery via stream-health/event-stream delivery signaling.

Client filter:

```json
{
  "type": "subscribe",
  "regions": ["io.video", "chip_ram"],
  "address_ranges": ["0x00FF8000-0x00FF82FF"],
  "access_types": ["read", "write"],
  "components": ["cpu", "blitter"],
  "mapped_targets": ["shifter.mode_register"],
  "level": "debug"
}
```

Server event:

```json
{
  "type": "memory_event",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 52210,
  "event_timestamp_us": 1710001117742,
  "tick": 297716491,
  "cycle": 74429123,
  "region": "io.video",
  "address": "0x00FF820A",
  "access": "write",
  "component": "cpu",
  "mapped_target": "shifter.mode_register"
}
```

Filtered stream load validation run example:

```json
{
  "run_id": "load_01H...",
  "session_id": "ses_01H...",
  "duration_s": 60,
  "bus_stream": {
    "filters_applied": true,
    "events_emitted": 14220,
    "events_rejected": 98740,
    "checks": {
      "BUS-FLT-01": "pass",
      "BUS-FLT-02": "pass",
      "BUS-FLT-03": "pass",
      "BUS-FLT-04": "pass"
    }
  },
  "memory_stream": {
    "filters_applied": true,
    "events_emitted": 9310,
    "events_rejected": 120443,
    "checks": {
      "MEM-FLT-01": "pass",
      "MEM-FLT-02": "pass",
      "MEM-FLT-03": "pass",
      "MEM-FLT-04": "pass"
    }
  },
  "result": "pass"
}
```

## 10.7 Stream control and health

All streams support control messages:

- `pause_stream`
- `resume_stream`
- `set_filter`
- `set_rate_limit`

Backpressure counters and watermark metrics contract:

- Stream health telemetry must expose deterministic backpressure counters and queue watermark metrics per stream connection.
- Required `stream_health` fields:
  - `queue_depth` (uint32)
  - `queue_capacity` (uint32, `>0`)
  - `dropped_events` (uint64, cumulative)
  - `dropped_events_since_last` (uint32)
  - `throttle_active` (bool)
  - `high_watermark_depth` (uint32)
  - `high_watermark_ratio` (number, `0..1`)
  - `overflow_events_total` (uint64)
  - `throttle_transitions_total` (uint64)
  - `sample_timestamp_us` (uint64)
- Counter/metric invariants:
  - `queue_depth <= queue_capacity`
  - `high_watermark_depth >= queue_depth` and `high_watermark_depth <= queue_capacity`
  - `high_watermark_ratio = high_watermark_depth / queue_capacity`
  - cumulative counters (`dropped_events`, `overflow_events_total`, `throttle_transitions_total`) are monotonic non-decreasing for a stream connection.

Backpressure counter checks:

- `BP-CTR-01`: cumulative counters are monotonic non-decreasing.
- `BP-CTR-02`: `0 <= queue_depth <= queue_capacity`.
- `BP-CTR-03`: `high_watermark_depth`/`high_watermark_ratio` remain consistent with `queue_capacity`.
- `BP-CTR-04`: entering or leaving throttle state increments `throttle_transitions_total` exactly once per transition.

Health message:

```json
{
  "type": "stream_health",
  "queue_depth": 42,
  "queue_capacity": 256,
  "dropped_events": 3,
  "dropped_events_since_last": 1,
  "throttle_active": true,
  "high_watermark_depth": 192,
  "high_watermark_ratio": 0.75,
  "overflow_events_total": 1,
  "throttle_transitions_total": 4,
  "sample_timestamp_us": 1710000011300
}
```

Backpressure telemetry API snapshot:

- `GET /api/v2/stream/telemetry/backpressure?session_id=ses_01H...&stream=video`
- Query params:
  - `session_id` (required)
  - `stream` (required, enum: `video`, `audio`, `engine`, `registers`, `bus`, `memory`)
- Response `data` fields:
  - `session_id` (string)
  - `stream` (string)
  - `sample_timestamp_us` (uint64)
  - `queue_depth` (uint32)
  - `queue_capacity` (uint32)
  - `high_watermark_depth` (uint32)
  - `high_watermark_ratio` (number)
  - `dropped_events` (uint64)
  - `dropped_events_since_last` (uint32)
  - `overflow_events_total` (uint64)
  - `throttle_active` (bool)
  - `throttle_transitions_total` (uint64)
- Snapshot guard failures:
  - Missing/invalid query params -> `BAD_REQUEST`.
  - Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
  - Unknown stream selector -> `INSPECT_FILTER_INVALID`.

Backpressure telemetry API example (response `data`):

```json
{
  "session_id": "ses_01H...",
  "stream": "video",
  "sample_timestamp_us": 1710000011350,
  "queue_depth": 31,
  "queue_capacity": 256,
  "high_watermark_depth": 192,
  "high_watermark_ratio": 0.75,
  "dropped_events": 4,
  "dropped_events_since_last": 1,
  "overflow_events_total": 1,
  "throttle_active": true,
  "throttle_transitions_total": 4
}
```

## 10.8 Engine status/health event stream

- `GET /api/v2/engine/stream`

Event names:

- `engine_status_update`
- `engine_health_update`
- `engine_stream_delivery`
- `stream_backpressure_telemetry`

Payload contract notes:

- `engine_status_update` carries a `status` object that must follow the canonical status payload defined in section `6.6B`.
- `engine_health_update` carries a `health` object that must follow the canonical health payload defined in section `6.6A`.
- `engine_stream_delivery` communicates stream-level degraded delivery/error signaling and backpressure disclosure.

Ordering and sequence rules:

- Every engine stream event must include `event_seq` (uint64) and `event_timestamp_us` (uint64) from the same monotonic runtime time base as section `4`.
- `event_seq` must be strictly monotonic per stream connection and increase by `1` for each emitted event.
- `event_timestamp_us` must be monotonic non-decreasing for a given stream connection.
- Client ordering source of truth is `event_seq`; `event_timestamp_us` is informational and must not be used to reorder events.
- `engine_status_update.status.snapshot_at_us` and `engine_health_update.health.generated_at_us` must be less than or equal to `event_timestamp_us`.

Backpressure/skip disclosure requirements:

- Every `engine_status_update` and `engine_health_update` event must include a `delivery` object.
- `delivery.dropped_events_since_last` reports count of omitted events since the previous delivered event on the same connection.
- `delivery.coalesced_updates` reports how many intermediate updates were merged into this payload (`0` means none).
- `delivery.degraded` is `true` whenever `dropped_events_since_last > 0`, `coalesced_updates > 0`, or stream throttle is active.
- `delivery.reason` enum: `none`, `queue_overflow`, `rate_limited`, `producer_lag`, `transport_backpressure`.

Stream error and degraded-delivery signaling:

- Server must emit `engine_stream_delivery` whenever `delivery.degraded=true` persists or transitions state.
- `engine_stream_delivery.severity` enum: `info`, `warning`, `error`.
- `engine_stream_delivery.state` enum: `normal`, `degraded`, `recovering`.
- If the stream cannot continue, server must emit `engine_stream_delivery` with `state="degraded"`, `severity="error"`, and non-empty `error_code` before close when possible.

Backpressure telemetry event-stream exposure:

- Server must emit `stream_backpressure_telemetry` whenever backpressure counters change or throttle state transitions.
- `stream_backpressure_telemetry` required fields:
  - `type` (string, must equal `stream_backpressure_telemetry`)
  - `schema_version` (uint32)
  - `session_id` (string)
  - `event_seq` (uint64)
  - `event_timestamp_us` (uint64)
  - `stream` (enum: `video`, `audio`, `engine`, `registers`, `bus`, `memory`)
  - `metrics` (object with the same counter/watermark fields as section `10.7`)
- Event ordering rules:
  - `event_seq` is strictly monotonic by `+1` per engine stream connection.
  - `event_timestamp_us` is monotonic non-decreasing.
  - Metric payload must satisfy `BP-CTR-01..04` invariants at emission time.

Normal delivery example (`engine_status_update`):

```json
{
  "type": "engine_status_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 1042,
  "event_timestamp_us": 1710000010450,
  "status": {
    "session_id": "ses_01H...",
    "machine": "atari_st",
    "profile": "st_520_pal",
    "lifecycle_state": "running",
    "run_mode": "realtime",
    "snapshot_at_us": 1710000010448,
    "uptime_ms": 42503,
    "cycle_counter": 112552001,
    "tick_counter": 450208004,
    "loaded_modules": [],
    "runtime": {
      "scheduler_hz": 8000000,
      "stream_health": {
        "video": {"connected_clients": 1, "dropped_packets": 0},
        "audio": {"connected_clients": 1, "dropped_packets": 0}
      },
      "last_transition_at_us": 1710000009000,
      "last_error": null
    }
  },
  "delivery": {
    "degraded": false,
    "reason": "none",
    "dropped_events_since_last": 0,
    "coalesced_updates": 0,
    "throttle_active": false
  }
}
```

Degraded delivery example (`engine_health_update`):

```json
{
  "type": "engine_health_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2051,
  "event_timestamp_us": 1710000011200,
  "health": {
    "session_id": "ses_01H...",
    "overall_status": "degraded",
    "overall_severity": "warning",
    "generated_at_us": 1710000011194,
    "freshness_ttl_ms": 1000,
    "freshness_state": "fresh",
    "components": [
      {
        "component": "video",
        "status": "unavailable",
        "severity": "error",
        "observed_at_us": 1710000011193,
        "last_ok_at_us": 1710000010800,
        "reason": "video stream backend not initialized",
        "error_code": "ENGINE_SUBSYSTEM_UNAVAILABLE"
      }
    ]
  },
  "delivery": {
    "degraded": true,
    "reason": "queue_overflow",
    "dropped_events_since_last": 3,
    "coalesced_updates": 2,
    "throttle_active": true
  }
}
```

Degraded-delivery signal example (`engine_stream_delivery`):

```json
{
  "type": "engine_stream_delivery",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2052,
  "event_timestamp_us": 1710000011201,
  "state": "degraded",
  "severity": "warning",
  "error_code": "STREAM_BACKPRESSURE_ACTIVE",
  "message": "Engine stream is coalescing updates due to queue overflow",
  "metrics": {
    "queue_depth": 128,
    "queue_capacity": 128,
    "dropped_events_since_last": 3,
    "coalesced_updates": 2,
    "high_watermark_depth": 128,
    "high_watermark_ratio": 1.0,
    "overflow_events_total": 9,
    "throttle_transitions_total": 15
  }
}
```

Backpressure telemetry event example (`stream_backpressure_telemetry`):

```json
{
  "type": "stream_backpressure_telemetry",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2053,
  "event_timestamp_us": 1710000011202,
  "stream": "video",
  "metrics": {
    "queue_depth": 96,
    "queue_capacity": 128,
    "dropped_events": 12,
    "dropped_events_since_last": 1,
    "throttle_active": true,
    "high_watermark_depth": 128,
    "high_watermark_ratio": 1.0,
    "overflow_events_total": 9,
    "throttle_transitions_total": 15,
    "sample_timestamp_us": 1710000011201
  }
}
```

---

## 11. Snapshot and checkpoint APIs

## 11.1 Register snapshot

- `GET /api/v2/inspect/registers/snapshot?session_id=...&components=cpu,mfp`

## 11.2 Bus snapshot

- `GET /api/v2/inspect/bus/snapshot?session_id=...`

## 11.3 Memory snapshot

- `GET /api/v2/inspect/memory/snapshot?session_id=...&range=0x000000-0x0003FF`

## 11.3A GLUE/MMU/SHIFTER register and memory window model

Register window model contract:

- `GET /api/v2/inspect/chipset/windows/registers?session_id=...&group=glue,mmu,shifter`
- Response exposes one normalized register-window projection per requested chipset group.
- `register_window_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`)
  - `base_address` (string, hex)
  - `window_bytes` (uint32, `>0`)
  - `registers` (array, length `>=1`)
- `registers[]` required fields:
  - `name` (string)
  - `offset` (uint32)
  - `address` (string, hex)
  - `width_bits` (uint32)
  - `access` (enum: `ro`, `wo`, `rw`)

Memory window model contract:

- `GET /api/v2/inspect/chipset/windows/memory?session_id=...&group=glue,mmu,shifter`
- Response exposes one normalized memory-window projection per requested chipset group.
- `memory_window_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`)
  - `window_start` (string, hex)
  - `window_end` (string, hex, inclusive)
  - `mapped_region` (string)
  - `addressing_mode` (enum: `linear`, `banked`)

GLUE/MMU/SHIFTER model deterministic guard failures:

- Missing/invalid `session_id` or invalid `group` selector -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Unresolved chipset model projection for requested group -> `INTERNAL_ERROR`.

GLUE/MMU/SHIFTER arbitration and timing integration contract:

- Arbitration integration follows scheduler/arbitration hook ordering defined in section `6.10A` and binds chipset participation to each committed tick.
- Per committed tick, chipset arbitration order is deterministic: `glue -> mmu -> shifter` unless profile wiring explicitly overrides with a validated order.
- Required timing integration fields emitted by chipset integration diagnostics:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `chipset_order` (array of strings)
  - `bus_owner` (string)
  - `wait_cycles` (uint32)
  - `event_timestamp_us` (uint64)

Chipset arbitration/timing checks:

- `CHIP-TIM-01`: chipset step order matches validated arbitration order for each committed tick.
- `CHIP-TIM-02`: `cycle_counter(next) >= cycle_counter(prev)` across emitted integration events.
- `CHIP-TIM-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)` for chipset integration telemetry.
- `CHIP-TIM-04`: reported `bus_owner` must be one of `{glue, mmu, shifter, cpu, dma}`.

Chipset integration deterministic failures:

- Chipset arbitration order mismatch or unresolved participant in active order -> `INTERNAL_ERROR`.
- Timing monotonicity breach (`CHIP-TIM-02` or `CHIP-TIM-03`) -> `INTERNAL_ERROR` with fail-fast diagnostics.

Register window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "windows": [
    {
      "group": "shifter",
      "base_address": "0x00FF8200",
      "window_bytes": 64,
      "registers": [
        {
          "name": "video_base_high",
          "offset": 1,
          "address": "0x00FF8201",
          "width_bits": 8,
          "access": "rw"
        }
      ]
    }
  ]
}
```

Memory window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "windows": [
    {
      "group": "mmu",
      "window_start": "0x00FF8000",
      "window_end": "0x00FF82FF",
      "mapped_region": "st_chipset_io",
      "addressing_mode": "linear"
    }
  ]
}
```

Chipset arbitration/timing validation trace example:

```json
{
  "session_id": "ses_01H...",
  "checks": {
    "CHIP-TIM-01": "pass",
    "CHIP-TIM-02": "pass",
    "CHIP-TIM-03": "pass",
    "CHIP-TIM-04": "pass"
  },
  "last_integration": {
    "tick_counter": 450208120,
    "cycle_counter": 112552440,
    "chipset_order": ["glue", "mmu", "shifter"],
    "bus_owner": "mmu",
    "wait_cycles": 2,
    "event_timestamp_us": 1710000026400
  }
}
```

## 11.3B MFP register and timer model contracts

MFP register model contract:

- `GET /api/v2/inspect/chipset/windows/registers?session_id=...&group=mfp`
- Response exposes normalized MFP register projection validated by schema `mfp_register_window_v1`.
- `mfp_register_window_v1` required fields:
  - `group` (string, must equal `mfp`)
  - `base_address` (string, hex)
  - `window_bytes` (uint32, `>0`)
  - `registers` (array, length `>=1`)
- `registers[]` required fields:
  - `name` (string)
  - `offset` (uint32)
  - `address` (string, hex)
  - `width_bits` (uint32)
  - `access` (enum: `ro`, `wo`, `rw`)

MFP timer model contract:

- `GET /api/v2/inspect/chipset/windows/timers?session_id=...&group=mfp`
- Response exposes one timer projection per logical MFP timer (`A`, `B`, `C`, `D`) validated by schema `mfp_timer_model_v1`.
- `mfp_timer_model_v1` required fields:
  - `timer_id` (enum: `A`, `B`, `C`, `D`)
  - `control_register` (string)
  - `data_register` (string)
  - `prescaler` (uint32)
  - `counter_value` (uint32)
  - `mode` (enum: `stopped`, `delay`, `event_count`, `pulse_width`)
  - `enabled` (bool)

MFP model deterministic guard failures:

- Missing/invalid `session_id` or invalid `group` selector -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Unsupported timer selector or unresolved timer mapping -> `INSPECT_FILTER_INVALID`.

MFP interrupt emission behavior contract:

- MFP interrupt emission is driven by timer/state transitions and must emit deterministic interrupt events when enable/mask gates are satisfied.
- `mfp_interrupt_event_v1` required fields:
  - `source` (string, must equal `mfp`)
  - `interrupt_line` (enum: `irq2`, `irq6`)
  - `vector` (uint16)
  - `timer_id` (enum: `A`, `B`, `C`, `D` or `null`)
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `event_timestamp_us` (uint64)

MFP interrupt conformance checks:

- `MFP-IRQ-01`: interrupts emit only when corresponding enable+mask bits resolve to active delivery.
- `MFP-IRQ-02`: `event_timestamp_us(next) >= event_timestamp_us(prev)` for emitted MFP interrupt events.
- `MFP-IRQ-03`: emitted `vector` matches active interrupt source/vector mapping.
- `MFP-IRQ-04`: repeated emissions for same source/tick require an explicit re-arm condition.

MFP interrupt deterministic failures:

- Interrupt event emitted with unresolved source/vector mapping -> `INTERNAL_ERROR`.
- Interrupt timing monotonicity violation (`MFP-IRQ-02`) -> `INTERNAL_ERROR` with fail-fast diagnostics.

MFP register model example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "window": {
    "group": "mfp",
    "base_address": "0x00FFFA00",
    "window_bytes": 64,
    "registers": [
      {
        "name": "IERA",
        "offset": 7,
        "address": "0x00FFFA07",
        "width_bits": 8,
        "access": "rw"
      }
    ]
  }
}
```

MFP timer model example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "timers": [
    {
      "timer_id": "A",
      "control_register": "TACR",
      "data_register": "TADR",
      "prescaler": 64,
      "counter_value": 192,
      "mode": "delay",
      "enabled": true
    }
  ]
}
```

MFP interrupt conformance trace example:

```json
{
  "session_id": "ses_01H...",
  "checks": {
    "MFP-IRQ-01": "pass",
    "MFP-IRQ-02": "pass",
    "MFP-IRQ-03": "pass",
    "MFP-IRQ-04": "pass"
  },
  "last_interrupt": {
    "source": "mfp",
    "interrupt_line": "irq6",
    "vector": 26,
    "timer_id": "A",
    "tick_counter": 450208244,
    "cycle_counter": 112552991,
    "event_timestamp_us": 1710000028022
  }
}
```

## 11.3C ACIA host channel bridge and framing rules

ACIA host channel bridge contract:

- `GET /api/v2/inspect/chipset/acia/bridge?session_id=...`
- Response `data` must expose bridge state validated by `acia_bridge_state_v1`.
- `acia_bridge_state_v1` required fields:
  - `session_id` (string)
  - `bridge_state` (enum: `detached`, `attached`, `error`)
  - `channel_mode` (enum: `rx`, `tx`, `duplex`)
  - `rx_queue_depth` (uint32)
  - `tx_queue_depth` (uint32)
  - `framing_profile` (string)
  - `last_frame_seq` (uint64)
  - `last_timestamp_us` (uint64)

ACIA framing rules contract:

- `GET /api/v2/inspect/chipset/acia/frames?session_id=...&limit=...`
- Frames are validated by `acia_frame_v1` and emitted in sequence order.
- `acia_frame_v1` required fields:
  - `frame_seq` (uint64)
  - `direction` (enum: `host_to_st`, `st_to_host`)
  - `encoding` (enum: `8N1`)
  - `payload_hex` (string)
  - `start_bit` (uint8, must equal `0`)
  - `stop_bits` (uint8, must equal `1`)
  - `parity` (string, must equal `none`)
  - `timestamp_us` (uint64)

ACIA framing conformance checks:

- `ACIA-FRM-01`: `frame_seq(next) == frame_seq(prev) + 1` per direction stream.
- `ACIA-FRM-02`: `timestamp_us(next) >= timestamp_us(prev)` per direction stream.
- `ACIA-FRM-03`: frame bit-shape satisfies `start_bit=0`, `stop_bits=1`, `parity=none` for `encoding=8N1`.
- `ACIA-FRM-04`: frames rejected by bridge validation must not be emitted in inspected frame stream.

ACIA bridge/framing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Bridge attachment unavailable or unresolved framing profile -> `INTERNAL_ERROR`.

ACIA bridge state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "bridge_state": "attached",
  "channel_mode": "duplex",
  "rx_queue_depth": 3,
  "tx_queue_depth": 1,
  "framing_profile": "acia_8n1_default",
  "last_frame_seq": 9182,
  "last_timestamp_us": 1710000029200
}
```

ACIA frame stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "frames": [
    {
      "frame_seq": 9183,
      "direction": "host_to_st",
      "encoding": "8N1",
      "payload_hex": "0xF6",
      "start_bit": 0,
      "stop_bits": 1,
      "parity": "none",
      "timestamp_us": 1710000029203
    }
  ]
}
```

## 11.3D IKBD parser bridge and keyboard/mouse packet timing path

IKBD parser bridge contract:

- `GET /api/v2/inspect/chipset/ikbd/bridge?session_id=...`
- Response `data` must expose parser bridge state validated by `ikbd_bridge_state_v1`.
- `ikbd_bridge_state_v1` required fields:
  - `session_id` (string)
  - `bridge_state` (enum: `detached`, `attached`, `error`)
  - `parser_mode` (enum: `atari_st_ikbd`)
  - `acia_channel_mode` (enum: `rx`, `tx`, `duplex`)
  - `rx_queue_depth` (uint32)
  - `tx_queue_depth` (uint32)
  - `last_packet_seq` (uint64)
  - `last_timestamp_us` (uint64)

IKBD keyboard/mouse packet timing contract:

- `GET /api/v2/inspect/chipset/ikbd/packets?session_id=...&limit=...&packet_type=...`
- Packet entries are validated by `ikbd_packet_timing_v1` and emitted in sequence order.
- `ikbd_packet_timing_v1` required fields:
  - `packet_seq` (uint64)
  - `packet_type` (enum: `keyboard_scancode`, `mouse_packet`)
  - `direction` (enum: `host_to_st`, `st_to_host`)
  - `payload_hex` (string)
  - `acia_frame_seq` (uint64)
  - `inter_packet_gap_us` (uint64)
  - `timestamp_us` (uint64)

IKBD packet timing conformance checks:

- `IKBD-PKT-01`: `packet_seq(next) == packet_seq(prev) + 1` for each `packet_type` stream.
- `IKBD-PKT-02`: `timestamp_us(next) >= timestamp_us(prev)` for each `packet_type` stream.
- `IKBD-PKT-03`: `inter_packet_gap_us == timestamp_us(curr) - timestamp_us(prev)` for adjacent packets in the same `packet_type` stream.
- `IKBD-PKT-04`: `acia_frame_seq` for each emitted IKBD packet must resolve to an existing ACIA frame and preserve bridge order.

IKBD bridge/timing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`, `packet_type`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Parser bridge unavailable or ACIA frame linkage unresolved -> `INTERNAL_ERROR`.

IKBD parser bridge state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "bridge_state": "attached",
  "parser_mode": "atari_st_ikbd",
  "acia_channel_mode": "duplex",
  "rx_queue_depth": 2,
  "tx_queue_depth": 1,
  "last_packet_seq": 12411,
  "last_timestamp_us": 1710000030102
}
```

IKBD packet timing stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "packets": [
    {
      "packet_seq": 12412,
      "packet_type": "keyboard_scancode",
      "direction": "host_to_st",
      "payload_hex": "0x1C",
      "acia_frame_seq": 9184,
      "inter_packet_gap_us": 170,
      "timestamp_us": 1710000030272
    },
    {
      "packet_seq": 12413,
      "packet_type": "mouse_packet",
      "direction": "st_to_host",
      "payload_hex": "0xF80801",
      "acia_frame_seq": 9185,
      "inter_packet_gap_us": 244,
      "timestamp_us": 1710000030516
    }
  ]
}
```

## 11.3E DMA request pacing model and arbitration hooks

DMA request pacing contract:

- `GET /api/v2/inspect/chipset/dma/pacing?session_id=...`
- Response `data` must expose pacing state validated by `dma_pacing_state_v1`.
- `dma_pacing_state_v1` required fields:
  - `session_id` (string)
  - `pacing_mode` (enum: `deterministic_tick`)
  - `request_window_ticks` (uint32)
  - `max_requests_per_window` (uint32)
  - `window_start_tick` (uint64)
  - `window_end_tick` (uint64)
  - `queued_requests` (uint32)
  - `last_request_seq` (uint64)

DMA arbitration hook contract:

- `GET /api/v2/inspect/chipset/dma/arbitration?session_id=...&limit=...`
- Arbitration entries are validated by `dma_arbitration_hook_v1` and emitted in sequence order.
- `dma_arbitration_hook_v1` required fields:
  - `request_seq` (uint64)
  - `requester` (enum: `fdc`, `blitter`, `memory_refresh`)
  - `arbitration_round` (uint32)
  - `grant_state` (enum: `granted`, `deferred`, `denied`)
  - `scheduled_tick` (uint64)
  - `granted_tick` (uint64, nullable when `grant_state!=granted`)
  - `timestamp_us` (uint64)

DMA pacing/arbitration conformance checks:

- `DMA-ARB-01`: `request_seq(next) == request_seq(prev) + 1` for emitted arbitration entries.
- `DMA-ARB-02`: `scheduled_tick(next) >= scheduled_tick(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `DMA-ARB-03`: when `grant_state=granted`, `granted_tick >= scheduled_tick`; when `grant_state!=granted`, `granted_tick` must be `null`.
- `DMA-ARB-04`: grants in each pacing window must not exceed `max_requests_per_window`; excess requests are emitted as `deferred`.

DMA pacing/arbitration deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Arbitration hook source unavailable or pacing window state unresolved -> `INTERNAL_ERROR`.

DMA pacing state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "pacing_mode": "deterministic_tick",
  "request_window_ticks": 128,
  "max_requests_per_window": 16,
  "window_start_tick": 912640,
  "window_end_tick": 912767,
  "queued_requests": 3,
  "last_request_seq": 55301
}
```

DMA arbitration hook stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "request_seq": 55302,
      "requester": "fdc",
      "arbitration_round": 44,
      "grant_state": "granted",
      "scheduled_tick": 912700,
      "granted_tick": 912700,
      "timestamp_us": 1710000031120
    },
    {
      "request_seq": 55303,
      "requester": "blitter",
      "arbitration_round": 45,
      "grant_state": "deferred",
      "scheduled_tick": 912701,
      "granted_tick": null,
      "timestamp_us": 1710000031124
    }
  ]
}
```

## 11.3F FDC command/status FSM bridge and terminal conditions

FDC command/status bridge contract:

- `GET /api/v2/inspect/chipset/fdc/fsm?session_id=...`
- Response `data` must expose command/status FSM state validated by `fdc_fsm_state_v1`.
- `fdc_fsm_state_v1` required fields:
  - `session_id` (string)
  - `fsm_state` (enum: `idle`, `command_latched`, `executing`, `result_ready`, `error`)
  - `active_command` (string, nullable)
  - `command_seq` (uint64)
  - `status_register` (uint8)
  - `busy` (boolean)
  - `drq` (boolean)
  - `intrq` (boolean)
  - `last_transition_tick` (uint64)
  - `last_transition_us` (uint64)

FDC terminal-condition event contract:

- `GET /api/v2/inspect/chipset/fdc/terminal?session_id=...&limit=...`
- Terminal events are validated by `fdc_terminal_event_v1` and emitted in sequence order.
- `fdc_terminal_event_v1` required fields:
  - `event_seq` (uint64)
  - `command_seq` (uint64)
  - `terminal_condition` (enum: `ok`, `crc_error`, `record_not_found`, `write_protect`, `lost_data`, `timeout`, `aborted`)
  - `status_register` (uint8)
  - `busy` (boolean)
  - `drq` (boolean)
  - `intrq` (boolean)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

FDC FSM/terminal conformance checks:

- `FDC-FSM-01`: `event_seq(next) == event_seq(prev) + 1` for terminal event stream.
- `FDC-FSM-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `FDC-FSM-03`: terminal event must be emitted only from `result_ready` or `error` FSM states.
- `FDC-FSM-04`: when terminal event is emitted, `busy=false` and `intrq=true`; `drq` reflects terminal condition semantics.

FDC FSM/terminal deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- FSM bridge unavailable or unresolved command/status transition -> `INTERNAL_ERROR`.

FDC FSM state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "fsm_state": "executing",
  "active_command": "READ_SECTOR",
  "command_seq": 8012,
  "status_register": 129,
  "busy": true,
  "drq": false,
  "intrq": false,
  "last_transition_tick": 912840,
  "last_transition_us": 1710000031888
}
```

FDC terminal event stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 20331,
      "command_seq": 8012,
      "terminal_condition": "ok",
      "status_register": 0,
      "busy": false,
      "drq": false,
      "intrq": true,
      "tick_counter": 912864,
      "timestamp_us": 1710000031951
    }
  ]
}
```

## 11.3G PSG register/audio behavior contract

PSG register behavior contract:

- `GET /api/v2/inspect/chipset/psg/registers?session_id=...`
- Response `data` must expose register-window state validated by `psg_register_window_v1`.
- `psg_register_window_v1` required fields:
  - `session_id` (string)
  - `base_address` (string, hex)
  - `window_bytes` (uint32)
  - `registers` (array of `psg_register_entry_v1`)
- `psg_register_entry_v1` required fields:
  - `name` (string)
  - `index` (uint8)
  - `value` (uint8)
  - `latched_tick` (uint64)

PSG audio behavior contract:

- `GET /api/v2/inspect/chipset/psg/audio?session_id=...&limit=...`
- Audio entries are validated by `psg_audio_state_v1` and emitted in sequence order.
- `psg_audio_state_v1` required fields:
  - `frame_seq` (uint64)
  - `mix_mode` (enum: `mono`)
  - `sample_rate_hz` (uint32)
  - `channel_a_level` (uint8)
  - `channel_b_level` (uint8)
  - `channel_c_level` (uint8)
  - `noise_enable` (boolean)
  - `envelope_shape` (uint8)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

PSG register/audio conformance checks:

- `PSG-AUD-01`: register writes to tone/noise/envelope control must be reflected in subsequent emitted audio states.
- `PSG-AUD-02`: `frame_seq(next) == frame_seq(prev) + 1` for emitted audio states.
- `PSG-AUD-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `PSG-AUD-04`: `channel_*_level` values remain within `0..15` and are deterministic for identical register input traces.

PSG register/audio deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- PSG register window unavailable or audio state derivation unresolved -> `INTERNAL_ERROR`.

PSG register window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "base_address": "0x00FF8800",
  "window_bytes": 16,
  "registers": [
    {
      "name": "CHANNEL_A_FINE",
      "index": 0,
      "value": 34,
      "latched_tick": 913002
    }
  ]
}
```

PSG audio state stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "states": [
    {
      "frame_seq": 7810,
      "mix_mode": "mono",
      "sample_rate_hz": 50066,
      "channel_a_level": 10,
      "channel_b_level": 4,
      "channel_c_level": 0,
      "noise_enable": true,
      "envelope_shape": 9,
      "tick_counter": 913010,
      "timestamp_us": 1710000032522
    }
  ]
}
```

## 11.3H PSG GPIO behavior contract and validation checks

PSG GPIO behavior contract:

- `GET /api/v2/inspect/chipset/psg/gpio?session_id=...`
- Response `data` must expose GPIO state validated by `psg_gpio_state_v1`.
- `psg_gpio_state_v1` required fields:
  - `session_id` (string)
  - `port_a_direction` (enum: `input`, `output`)
  - `port_b_direction` (enum: `input`, `output`)
  - `port_a_value` (uint8)
  - `port_b_value` (uint8)
  - `latched_tick` (uint64)
  - `timestamp_us` (uint64)

PSG GPIO event contract:

- `GET /api/v2/inspect/chipset/psg/gpio/events?session_id=...&limit=...`
- GPIO events are validated by `psg_gpio_event_v1` and emitted in sequence order.
- `psg_gpio_event_v1` required fields:
  - `event_seq` (uint64)
  - `port` (enum: `A`, `B`)
  - `direction` (enum: `input`, `output`)
  - `value_before` (uint8)
  - `value_after` (uint8)
  - `source` (enum: `cpu_write`, `external_signal`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

PSG GPIO conformance checks:

- `PSG-GPIO-01`: `event_seq(next) == event_seq(prev) + 1` for emitted GPIO events.
- `PSG-GPIO-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `PSG-GPIO-03`: when `direction=output`, `value_after` must equal the last CPU-written value for that port.
- `PSG-GPIO-04`: when `direction=input`, `value_after` must only change due to `source=external_signal`.

PSG GPIO deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- GPIO latch state unavailable or unresolved direction/value mapping -> `INTERNAL_ERROR`.

PSG GPIO state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "port_a_direction": "output",
  "port_b_direction": "input",
  "port_a_value": 127,
  "port_b_value": 16,
  "latched_tick": 913240,
  "timestamp_us": 1710000033180
}
```

PSG GPIO event stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 4201,
      "port": "A",
      "direction": "output",
      "value_before": 95,
      "value_after": 127,
      "source": "cpu_write",
      "tick_counter": 913241,
      "timestamp_us": 1710000033182
    },
    {
      "event_seq": 4202,
      "port": "B",
      "direction": "input",
      "value_before": 16,
      "value_after": 48,
      "source": "external_signal",
      "tick_counter": 913248,
      "timestamp_us": 1710000033194
    }
  ]
}
```

## 11.3I Interrupt hierarchy map and vector routing contract

Interrupt hierarchy map contract:

- `GET /api/v2/inspect/chipset/interrupts/hierarchy?session_id=...`
- Response `data` must expose hierarchy state validated by `interrupt_hierarchy_v1`.
- `interrupt_hierarchy_v1` required fields:
  - `session_id` (string)
  - `cpu_level_order` (array of uint8, expected levels `1..7`)
  - `sources` (array of `interrupt_source_v1`)
  - `default_vector_base` (uint16)
  - `last_route_seq` (uint64)
  - `last_timestamp_us` (uint64)
- `interrupt_source_v1` required fields:
  - `source_id` (enum: `mfp`, `acia`, `fdc`, `blitter`, `vbl`)
  - `priority_level` (uint8)
  - `vector` (uint16)
  - `enabled` (boolean)

Interrupt vector routing contract:

- `GET /api/v2/inspect/chipset/interrupts/routes?session_id=...&limit=...`
- Route entries are validated by `interrupt_route_event_v1` and emitted in sequence order.
- `interrupt_route_event_v1` required fields:
  - `route_seq` (uint64)
  - `source_id` (string)
  - `priority_level` (uint8)
  - `vector` (uint16)
  - `cpu_interrupt_line` (enum: `irq1`, `irq2`, `irq3`, `irq4`, `irq5`, `irq6`, `irq7`)
  - `delivery_state` (enum: `delivered`, `masked`, `deferred`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Interrupt hierarchy/routing conformance checks:

- `INT-MAP-01`: each enabled `source_id` must map to exactly one active (`priority_level`, `vector`) pair.
- `INT-MAP-02`: `route_seq(next) == route_seq(prev) + 1` for route stream entries.
- `INT-MAP-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `INT-MAP-04`: when multiple pending sources share a tick, delivered route order must follow `cpu_level_order` then stable `source_id` tie-break.

Interrupt hierarchy/routing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Interrupt map unavailable or unresolved vector routing state -> `INTERNAL_ERROR`.

Interrupt hierarchy map example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "cpu_level_order": [7, 6, 5, 4, 3, 2, 1],
  "sources": [
    {
      "source_id": "mfp",
      "priority_level": 6,
      "vector": 38,
      "enabled": true
    }
  ],
  "default_vector_base": 24,
  "last_route_seq": 9901,
  "last_timestamp_us": 1710000034028
}
```

Interrupt route stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "routes": [
    {
      "route_seq": 9902,
      "source_id": "mfp",
      "priority_level": 6,
      "vector": 38,
      "cpu_interrupt_line": "irq6",
      "delivery_state": "delivered",
      "tick_counter": 913601,
      "timestamp_us": 1710000034031
    },
    {
      "route_seq": 9903,
      "source_id": "fdc",
      "priority_level": 3,
      "vector": 54,
      "cpu_interrupt_line": "irq3",
      "delivery_state": "deferred",
      "tick_counter": 913601,
      "timestamp_us": 1710000034032
    }
  ]
}
```

## 11.3J Interrupt wiring integration checks across subsystems

Interrupt wiring integration contract:

- `GET /api/v2/inspect/chipset/interrupts/wiring?session_id=...`
- Response `data` must expose subsystem wiring state validated by `interrupt_wiring_state_v1`.
- `interrupt_wiring_state_v1` required fields:
  - `session_id` (string)
  - `subsystems` (array of `interrupt_subsystem_wiring_v1`)
  - `global_route_seq` (uint64)
  - `last_timestamp_us` (uint64)
- `interrupt_subsystem_wiring_v1` required fields:
  - `subsystem_id` (enum: `mfp`, `acia`, `fdc`, `blitter`, `vbl`)
  - `source_line` (string)
  - `cpu_interrupt_line` (enum: `irq1`, `irq2`, `irq3`, `irq4`, `irq5`, `irq6`, `irq7`)
  - `vector` (uint16)
  - `enabled` (boolean)

Interrupt wiring integration-check event contract:

- `GET /api/v2/inspect/chipset/interrupts/wiring/checks?session_id=...&limit=...`
- Integration-check events are validated by `interrupt_wiring_check_v1` and emitted in sequence order.
- `interrupt_wiring_check_v1` required fields:
  - `check_seq` (uint64)
  - `subsystem_id` (string)
  - `expected_cpu_line` (string)
  - `observed_cpu_line` (string)
  - `expected_vector` (uint16)
  - `observed_vector` (uint16)
  - `result` (enum: `pass`, `fail`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Interrupt wiring integration conformance checks:

- `INT-WIRE-01`: `check_seq(next) == check_seq(prev) + 1` for integration-check stream.
- `INT-WIRE-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `INT-WIRE-03`: when `result=pass`, `observed_cpu_line == expected_cpu_line` and `observed_vector == expected_vector`.
- `INT-WIRE-04`: failed checks must not mutate active routing map; next delivered interrupt must still follow current `interrupt_hierarchy_v1` order.

Interrupt wiring deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Wiring map unavailable or unresolved subsystem route linkage -> `INTERNAL_ERROR`.

Interrupt wiring state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "subsystems": [
    {
      "subsystem_id": "mfp",
      "source_line": "mfp_irq",
      "cpu_interrupt_line": "irq6",
      "vector": 38,
      "enabled": true
    }
  ],
  "global_route_seq": 10022,
  "last_timestamp_us": 1710000034620
}
```

Interrupt wiring check stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "checks": [
    {
      "check_seq": 410,
      "subsystem_id": "mfp",
      "expected_cpu_line": "irq6",
      "observed_cpu_line": "irq6",
      "expected_vector": 38,
      "observed_vector": 38,
      "result": "pass",
      "tick_counter": 913901,
      "timestamp_us": 1710000034623
    },
    {
      "check_seq": 411,
      "subsystem_id": "fdc",
      "expected_cpu_line": "irq3",
      "observed_cpu_line": "irq5",
      "expected_vector": 54,
      "observed_vector": 54,
      "result": "fail",
      "tick_counter": 913907,
      "timestamp_us": 1710000034630
    }
  ]
}
```

## 11.3K Startup defaults and power-on register baseline table

Startup defaults contract:

- `GET /api/v2/inspect/chipset/startup/defaults?session_id=...`
- Response `data` must expose startup defaults validated by `startup_defaults_v1`.
- `startup_defaults_v1` required fields:
  - `session_id` (string)
  - `machine_profile` (string)
  - `video_standard` (enum: `pal`, `ntsc`)
  - `ram_size_kib` (uint32)
  - `boot_device` (enum: `floppy`, `harddisk`, `rom`)
  - `defaults_revision` (string)
  - `applied_tick` (uint64)
  - `applied_timestamp_us` (uint64)

Power-on register baseline contract:

- `GET /api/v2/inspect/chipset/startup/baseline?session_id=...&group=...`
- Baseline entries are validated by `power_on_register_baseline_v1`.
- `power_on_register_baseline_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`)
  - `registers` (array of `power_on_register_entry_v1`)
  - `baseline_revision` (string)
  - `source` (enum: `profile_defaults`, `hardware_boot_table`)
- `power_on_register_entry_v1` required fields:
  - `name` (string)
  - `address` (string, hex)
  - `width_bits` (uint8)
  - `reset_value` (string, hex)
  - `mask` (string, hex)

Startup default/baseline conformance checks:

- `PWR-BASE-01`: each required startup default field must be populated before `running` lifecycle state is entered.
- `PWR-BASE-02`: every baseline register entry must have deterministic `reset_value` and `mask` for the selected `machine_profile`.
- `PWR-BASE-03`: duplicate register addresses within a `group` are forbidden.
- `PWR-BASE-04`: `applied_timestamp_us` must be monotonic across restart cycles for the same persisted session timeline.

Startup default/baseline deterministic guard failures:

- Missing/invalid query params (`session_id`, `group`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Baseline table unavailable or unresolved startup-default revision -> `INTERNAL_ERROR`.

Startup defaults example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "machine_profile": "atari_st_520_1040_baseline",
  "video_standard": "pal",
  "ram_size_kib": 1024,
  "boot_device": "floppy",
  "defaults_revision": "startup_rev_03",
  "applied_tick": 12,
  "applied_timestamp_us": 1710000035200
}
```

Power-on register baseline example (`data` excerpt):

```json
{
  "group": "mfp",
  "registers": [
    {
      "name": "IERA",
      "address": "0x00FFFA07",
      "width_bits": 8,
      "reset_value": "0x00",
      "mask": "0xFF"
    }
  ],
  "baseline_revision": "baseline_rev_07",
  "source": "hardware_boot_table"
}
```

## 11.3L Reset/startup sequence executor and verification checks

Reset/startup sequence executor contract:

- `GET /api/v2/inspect/chipset/startup/sequence?session_id=...`
- Response `data` must expose executor state validated by `startup_sequence_state_v1`.
- `startup_sequence_state_v1` required fields:
  - `session_id` (string)
  - `sequence_id` (string)
  - `phase` (enum: `assert_reset`, `clock_stabilize`, `register_seed`, `interrupt_enable`, `ready`)
  - `step_seq` (uint64)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)
  - `verification_status` (enum: `pending`, `pass`, `fail`)

Reset/startup verification-check contract:

- `GET /api/v2/inspect/chipset/startup/verification?session_id=...&limit=...`
- Verification events are validated by `startup_verification_event_v1` and emitted in sequence order.
- `startup_verification_event_v1` required fields:
  - `event_seq` (uint64)
  - `step_seq` (uint64)
  - `check_id` (string)
  - `component` (enum: `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`)
  - `result` (enum: `pass`, `fail`)
  - `expected` (string)
  - `observed` (string)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Reset/startup conformance checks:

- `RST-SEQ-01`: sequence phases must follow strict order `assert_reset -> clock_stabilize -> register_seed -> interrupt_enable -> ready`.
- `RST-SEQ-02`: `step_seq(next) == step_seq(prev) + 1` and `event_seq(next) == event_seq(prev) + 1` for emitted verification stream.
- `RST-SEQ-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `RST-SEQ-04`: `phase=ready` is allowed only when all required verification checks emit `result=pass`.

Reset/startup deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Sequence executor unavailable or unresolved verification dependency -> `INTERNAL_ERROR`.

Startup sequence state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "sequence_id": "boot_seq_0007",
  "phase": "register_seed",
  "step_seq": 3,
  "tick_counter": 14,
  "timestamp_us": 1710000035710,
  "verification_status": "pending"
}
```

Startup verification stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 1,
      "step_seq": 3,
      "check_id": "BOOT-MFP-RESET",
      "component": "mfp",
      "result": "pass",
      "expected": "IERA=0x00",
      "observed": "IERA=0x00",
      "tick_counter": 15,
      "timestamp_us": 1710000035712
    },
    {
      "event_seq": 2,
      "step_seq": 4,
      "check_id": "BOOT-INT-MAP",
      "component": "glue",
      "result": "pass",
      "expected": "irq_order=7..1",
      "observed": "irq_order=7..1",
      "tick_counter": 16,
      "timestamp_us": 1710000035718
    }
  ]
}
```

## 11.4 Create checkpoint

- `POST /api/v2/engine/checkpoint/create`

```json
{
  "session_id": "ses_01H...",
  "name": "post_boot_tos"
}
```

## 11.5 Load checkpoint

- `POST /api/v2/engine/checkpoint/load`

```json
{
  "session_id": "ses_01H...",
  "checkpoint_id": "chk_01H..."
}
```

## 11.6 Save machine state snapshot

- `POST /api/v2/engine/state/save`

Request:

```json
{
  "session_id": "ses_01H...",
  "name": "suspend_after_boot",
  "tags": ["boot", "debug"]
}
```

Response `data`:

```json
{
  "snapshot_id": "snap_01H...",
  "schema_version": 1,
  "profile": "st_520_pal",
  "abi": {"engine": "2.0.0", "modules": {"cpu": "2.0.0"}},
  "hash": "sha256:...",
  "saved_at_us": 1710000004321
}
```

## 11.7 Restore machine state snapshot

- `POST /api/v2/engine/state/restore`

Request:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "resume_mode": "paused"
}
```

Compatibility rules:

- Snapshot profile must match active machine profile or return `SNAPSHOT_INCOMPATIBLE`.
- Snapshot ABI requirements must be satisfied by active engine/module versions.
- Missing snapshots return `SNAPSHOT_NOT_FOUND`.

Restore compatibility rule matrix (`restore_compatibility_matrix_v1`):

| Rule ID | Dimension | Source field(s) | Runtime field(s) | Predicate | Failure code |
|---|---|---|---|---|---|
| `RCOMP-01` | schema | `snapshot.schema_version` | `engine.accepted_snapshot_schema_versions[]` | `snapshot.schema_version` is present in accepted schema list | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-02` | profile | `snapshot.profile` | `session.profile` | exact string equality | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-03` | ABI engine | `snapshot.abi.engine` | `engine.abi.engine` | exact semver match | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-04` | ABI modules | `snapshot.abi.modules{module_id:version}` | `engine.abi.modules{module_id:version}` | every required module exists and version matches exactly | `SNAPSHOT_INCOMPATIBLE` |

Restore compatibility evaluation contract:

- Compatibility checks execute in deterministic order: `RCOMP-01 -> RCOMP-02 -> RCOMP-03 -> RCOMP-04`.
- First failing rule terminates evaluation and becomes `details.rule_id` in error payload.
- Missing snapshot identity must short-circuit compatibility evaluation and return `SNAPSHOT_NOT_FOUND`.

Restore compatibility deterministic guard failures:

- Malformed restore payload (`session_id`, `snapshot_id`, `resume_mode`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Snapshot record absent for `snapshot_id` -> `SNAPSHOT_NOT_FOUND`.
- Any failed compatibility rule (`RCOMP-01..04`) -> `SNAPSHOT_INCOMPATIBLE`.

Restore compatibility validator contract:

- `POST /api/v2/engine/state/restore/validate`
- Request payload is validated by `restore_compatibility_validate_request_v1`.
- `restore_compatibility_validate_request_v1` required fields:
  - `session_id` (string, required)
  - `snapshot_id` (string, required)
  - `strict` (boolean, optional, default `true`)
- Response `data` is validated by `restore_compatibility_validate_result_v1`.
- `restore_compatibility_validate_result_v1` required fields:
  - `snapshot_id` (string)
  - `compatible` (boolean)
  - `evaluated_rules` (array)
  - `failed_rule_id` (string or `null`)
  - `error_code` (string or `null`)
  - `validated_at_us` (uint64)

Restore compatibility validator checks:

- `RCOMP-VAL-01`: validator executes rules in deterministic order `RCOMP-01..04`.
- `RCOMP-VAL-02`: exactly one terminal outcome is emitted: `compatible=true` with `failed_rule_id=null` OR `compatible=false` with non-null `failed_rule_id`.
- `RCOMP-VAL-03`: on first failed rule, validator must stop evaluating subsequent rules.
- `RCOMP-VAL-04`: when `strict=true`, compatibility mismatch must map to terminal `error_code=SNAPSHOT_INCOMPATIBLE`.

Restore compatibility error mapping matrix:

| Failure source | Rule/guard | error.code | error.category | retryable |
|---|---|---|---|---|
| Request payload invalid | validator input guard | `BAD_REQUEST` | `request` | `false` |
| Session identity unresolved | session guard | `ENGINE_NOT_RUNNING` | `engine` | `false` |
| Snapshot identifier unresolved | snapshot lookup guard | `SNAPSHOT_NOT_FOUND` | `snapshot` | `false` |
| Schema/profile/ABI mismatch | `RCOMP-01..04` | `SNAPSHOT_INCOMPATIBLE` | `snapshot` | `false` |

Restore compatibility validator request example:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "strict": true
}
```

Restore compatibility validator response (`data`) example:

```json
{
  "snapshot_id": "snap_01H...",
  "compatible": false,
  "evaluated_rules": [
    {"rule_id": "RCOMP-01", "result": "pass"},
    {"rule_id": "RCOMP-02", "result": "pass"},
    {"rule_id": "RCOMP-03", "result": "fail"}
  ],
  "failed_rule_id": "RCOMP-03",
  "error_code": "SNAPSHOT_INCOMPATIBLE",
  "validated_at_us": 1710000005004
}
```

Restore compatibility matrix pass example (`data` excerpt):

```json
{
  "snapshot_id": "snap_01H...",
  "evaluated_rules": [
    {"rule_id": "RCOMP-01", "result": "pass"},
    {"rule_id": "RCOMP-02", "result": "pass"},
    {"rule_id": "RCOMP-03", "result": "pass"},
    {"rule_id": "RCOMP-04", "result": "pass"}
  ],
  "compatible": true
}
```

Restore compatibility failure example (`SNAPSHOT_INCOMPATIBLE`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000005005,
  "error": {
    "code": "SNAPSHOT_INCOMPATIBLE",
    "category": "snapshot",
    "message": "Snapshot compatibility rule failed",
    "retryable": false,
    "details": {
      "snapshot_id": "snap_01H...",
      "rule_id": "RCOMP-03",
      "source_value": "2.1.0",
      "runtime_value": "2.0.0"
    }
  }
}
```

---

## 12. State model

Contract binding:

- Lifecycle transitions in this section are enforced by control endpoints in sections `6.1` through `6.8`.
- Transition guard inputs for lifecycle endpoints are defined in section `6` and interpreted using this state model.
- Session status payload semantics are defined by section `6.6` and are returned in the canonical success envelope defined in section `4`.
- Invalid transitions must return the canonical error envelope with `code=INVALID_SESSION_STATE` and `category=engine`.

Session states:

- `stopped`
- `starting`
- `running`
- `paused`
- `suspended`
- `faulted`
- `stopping`

Lifecycle transition matrix:

| Current state | Requested transition | Allowed | Intermediate path | Guard predicate(s) |
|---|---|---|---|---|
| `stopped` | `start` | yes | `stopped -> starting -> running` | `G-START-01`, `G-START-02` |
| `starting` | any control transition | no | n/a | `G-COMMON-01` |
| `running` | `pause` | yes | `running -> paused` | `G-PAUSE-01` |
| `running` | `suspend_save` | yes | `running -> suspended` | `G-SUSPEND-01` |
| `running` | `stop` | yes | `running -> stopping -> stopped` | `G-STOP-01` |
| `running` | `reset` | yes | `running -> running` | `G-RESET-01` |
| `paused` | `resume` | yes | `paused -> running` (or `paused` with `resume_mode=paused`) | `G-RESUME-01` |
| `paused` | `reset` | yes | `paused -> running` | `G-RESET-01` |
| `paused` | `stop` | yes | `paused -> stopping -> stopped` | `G-STOP-01` |
| `suspended` | `restore_resume` | yes | `suspended -> running` or `suspended -> paused` | `G-RESTORE-01`, `G-RESUME-02` |
| `suspended` | `stop` | yes | `suspended -> stopping -> stopped` | `G-STOP-01` |
| `faulted` | `stop` | yes | `faulted -> stopped` | `G-STOP-01` |
| `faulted` | `reset` | no | n/a | `G-COMMON-01` |
| `stopping` | any control transition | no | n/a | `G-COMMON-01` |

Guard predicates (deterministic):

- `G-COMMON-01` (transitional-state guard): reject control transitions when current state is `starting` or `stopping`.
- `G-START-01` (singleton session guard): start is allowed only when no active session exists (`state=stopped`).
- `G-START-02` (start-input completeness): start request must include required fields from section `6.1` (`machine`, `profile`, `rom_id`).
- `G-PAUSE-01` (pause-state guard): pause is allowed only from `running`.
- `G-RESUME-01` (resume-state guard): resume is allowed only from `paused` or `suspended`.
- `G-RESUME-02` (resume-mode guard): if `resume_mode=paused`, resulting state must be `paused`; otherwise resulting state must be `running`.
- `G-RESET-01` (reset-state guard): reset is allowed only from `running` or `paused`.
- `G-SUSPEND-01` (suspend-state guard): suspend-save is allowed only from `running`.
- `G-RESTORE-01` (restore-state guard): restore-resume is allowed only from `suspended` and requires valid snapshot compatibility per section `11.7`.
- `G-STOP-01` (stop-state guard): stop is allowed from `running`, `paused`, `suspended`, or `faulted`.

Deterministic guard outcomes:

- Guard failures caused by invalid current lifecycle state must return canonical `INVALID_SESSION_STATE`.
- Guard failures caused by missing/inactive session identity must return canonical `ENGINE_NOT_RUNNING`.
- Guard failures caused by duplicate start while active session exists must return canonical `ENGINE_ALREADY_RUNNING`.
- Guard failures caused by invalid restore prerequisites must return canonical snapshot errors from section `11.7` (`SNAPSHOT_NOT_FOUND`, `SNAPSHOT_INCOMPATIBLE`).

Lifecycle guard validator response contract:

- Every lifecycle guard rejection must return canonical error envelope from section `4`.
- Validator must include `details.guard_id` and `details.endpoint` in every guard failure.
- `error.category` must map deterministically to `engine` for lifecycle state/session failures and `snapshot` for restore prerequisite failures.

Guard predicate to error mapping matrix:

| Guard predicate | Failure condition class | error.code | error.category | retryable |
|---|---|---|---|---|
| `G-COMMON-01` | Transitional state (`starting`/`stopping`) rejects control action | `INVALID_SESSION_STATE` | `engine` | `false` |
| `G-START-01` | Duplicate start while active session exists | `ENGINE_ALREADY_RUNNING` | `engine` | `false` |
| `G-START-02` | Start request missing required fields | `BAD_REQUEST` | `request` | `false` |
| `G-PAUSE-01` | Pause requested outside `running` | `INVALID_SESSION_STATE` | `engine` | `false` |
| `G-RESUME-01` | Resume requested outside `paused`/`suspended` | `INVALID_SESSION_STATE` | `engine` | `false` |
| `G-RESUME-02` | Invalid `resume_mode` value or conflict | `BAD_REQUEST` | `request` | `false` |
| `G-RESET-01` | Reset requested outside `running`/`paused` | `INVALID_SESSION_STATE` | `engine` | `false` |
| `G-SUSPEND-01` | Suspend-save requested outside `running` | `INVALID_SESSION_STATE` | `engine` | `false` |
| `G-RESTORE-01` | Invalid snapshot prerequisite | `SNAPSHOT_NOT_FOUND` or `SNAPSHOT_INCOMPATIBLE` | `snapshot` | `false` |
| `G-STOP-01` | Stop requested outside stoppable states | `INVALID_SESSION_STATE` | `engine` | `false` |

Validator failure example (`G-COMMON-01`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000006200,
  "error": {
    "code": "INVALID_SESSION_STATE",
    "category": "engine",
    "message": "Cannot pause session while lifecycle state is starting",
    "retryable": false,
    "details": {
      "guard_id": "G-COMMON-01",
      "endpoint": "/api/v2/engine/session/pause",
      "session_id": "ses_01H...",
      "current_state": "starting",
      "allowed_states": ["running"],
      "requested_transition": "starting->paused"
    }
  }
}
```

Validator failure example (`G-START-02`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000006201,
  "error": {
    "code": "BAD_REQUEST",
    "category": "request",
    "message": "Missing required field rom_id for start request",
    "retryable": false,
    "details": {
      "guard_id": "G-START-02",
      "endpoint": "/api/v2/engine/session",
      "missing_fields": ["rom_id"]
    }
  }
}
```

Validator failure example (`G-RESTORE-01`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000006202,
  "error": {
    "code": "SNAPSHOT_INCOMPATIBLE",
    "category": "snapshot",
    "message": "Snapshot profile does not match active machine profile",
    "retryable": false,
    "details": {
      "guard_id": "G-RESTORE-01",
      "endpoint": "/api/v2/engine/session/restore-resume",
      "session_id": "ses_01H...",
      "snapshot_id": "snap_01H...",
      "expected_profile": "st_520_pal",
      "actual_profile": "st_1040_ntsc"
    }
  }
}
```

Any transition not explicitly allowed by this matrix must be rejected deterministically by guard predicates above.

---

## 13. Rate limits and backpressure

## 13.1 REST limits (recommended defaults)

- Control endpoints: 20 req/s per client
- File management: 10 req/s per client
- Upload chunk endpoints: 100 req/s per client

## 13.2 Stream backpressure

- Per-stream ring buffers are bounded.
- When full, policy is `drop_oldest` by default.
- Every drop increments `dropped_events` and emits health updates.
- Backpressure counters (`dropped_events`, `overflow_events_total`, `throttle_transitions_total`) are cumulative and monotonic for each stream connection.
- Watermark metrics (`high_watermark_depth`, `high_watermark_ratio`) track the highest observed queue occupancy for the active connection window.
- Counter/metric snapshots exposed in stream health and degraded-delivery telemetry must satisfy checks `BP-CTR-01..04`.

## 13.3 Determinism rule

Core emulation loop must not block on network I/O, stream subscribers, or slow clients.

## 13.4 Hard performance SLO targets

Required runtime targets:

- Input-device end-to-end latency: <= 50 ms
- Runtime jitter: < 30 ms
- Dropped-frame rate: < 1 percent

Measurement policy:

- Metrics are emitted in rolling windows and expose at least p50, p95, and max where applicable.
- Any target breach marks metric status `violating` and emits health notifications.

---

## 14. Machine extensibility contract

Machine IDs:

- `atari_st` (initial)
- `mega_st` (future)
- `atari_ste` (future)
- `mega_ste` (future)

API stability requirement:

- Existing `atari_st` fields remain backward compatible.
- New machine-specific fields are additive under `data.machine_extensions`.

---

## 15. Minimal conformance checklist for API implementation

1. Start/pause/resume/reset/stop endpoints behave with valid state transitions.
2. SD-card file manager enforces path policy and staging upload workflow.
3. EBIN catalog/validate/load/unload lifecycle functions are operational.
4. Video and audio streams emit metadata + binary payload with consistent timestamps.
5. Input APIs translate keyboard/mouse/game-controller host events into virtual machine events deterministically.
6. Browser input capture supports enable/disable, `mouse_over` mode, `click_to_capture` mode, and escape-sequence release.
7. Register/bus/memory streams support filtering and report dropped-event metrics.
8. Snapshot/checkpoint APIs return consistent and restorable state references.
9. Error envelope and code catalog are consistent across all endpoints.
10. Catalog APIs can identify missing local assets and download from hosted URLs.
11. Link-probe APIs update availability state and support dead-link marking with timestamps/reasons.
12. On-device scraper jobs and periodic schedules execute and expose status/history via API.
13. Suspend-save and restore-resume endpoints support state `suspended` with valid transitions and failure codes.
14. Performance metrics endpoint reports latency, jitter, and dropped-frame percentages against hard targets.
15. Debug clock mode and step endpoints support realtime, slow-motion, and single-step execution control.

### 15.1 Conformance harness scaffold and test manifest loader

Conformance harness scaffold contract:

- `POST /api/v2/conformance/harness/session`
- Request fields:
  - `session_id` (string, required)
  - `manifest_id` (string, required)
  - `run_mode` (enum: `dry_run`, `execute`)
  - `evidence_level` (enum: `summary`, `full`; default `summary`)
- Response `data` fields:
  - `harness_session_id` (string)
  - `session_id` (string)
  - `manifest_id` (string)
  - `state` (enum: `initialized`, `ready`, `running`, `completed`, `failed`)
  - `created_at_us` (uint64)
  - `checks_total` (uint32)
  - `checks_enabled` (uint32)

Test manifest loader contract:

- `POST /api/v2/conformance/manifests/load`
- Request fields:
  - `manifest_id` (string, required)
  - `manifest` (object, required)
- Required manifest schema fields (`conformance_manifest_v1`):
  - `manifest_version` (uint32, must equal `1`)
  - `target_machine` (string)
  - `profile` (string)
  - `suite` (string)
  - `cases` (array, length `>=1`)
- Required `cases[]` fields:
  - `case_id` (string)
  - `kind` (enum: `api`, `stream`)
  - `target` (string)
  - `assertions` (array, length `>=1`)

Harness/manifest deterministic guard failures:

- Invalid payload shape, unknown `run_mode`/`evidence_level`, or invalid manifest schema -> `BAD_REQUEST`.
- Unknown/inactive engine session for harness session creation -> `ENGINE_NOT_RUNNING`.
- Missing target profile binding for manifest (`target_machine`/`profile`) -> `MACHINE_PROFILE_NOT_FOUND`.
- Attempt to start harness with unresolved loader errors -> `INVALID_SESSION_STATE`.

Conformance harness create example:

```json
{
  "session_id": "ses_01H...",
  "manifest_id": "st_core_runtime_v1",
  "run_mode": "dry_run",
  "evidence_level": "summary"
}
```

Conformance harness create response (`data`):

```json
{
  "harness_session_id": "chs_01H...",
  "session_id": "ses_01H...",
  "manifest_id": "st_core_runtime_v1",
  "state": "initialized",
  "created_at_us": 1710000020000,
  "checks_total": 48,
  "checks_enabled": 48
}
```

Manifest load example:

```json
{
  "manifest_id": "st_core_runtime_v1",
  "manifest": {
    "manifest_version": 1,
    "target_machine": "atari_st",
    "profile": "st_520_pal",
    "suite": "core_runtime",
    "cases": [
      {
        "case_id": "clock_step_contract",
        "kind": "api",
        "target": "/api/v2/debug/clock/step",
        "assertions": ["status.ok", "tick_counter_monotonic"]
      }
    ]
  }
}
```

Evidence artifact collection contract:

- `POST /api/v2/conformance/harness/evidence/collect`
- Request fields:
  - `harness_session_id` (string, required)
  - `artifact_types` (array of enum: `logs`, `metrics`, `stream_samples`, `snapshots`, required, length `>=1`)
  - `window` (object, optional):
    - `start_event_seq` (uint64)
    - `end_event_seq` (uint64)
- Response `data` fields:
  - `collection_id` (string)
  - `harness_session_id` (string)
  - `state` (enum: `queued`, `collecting`, `completed`, `failed`)
  - `artifacts` (array)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Report packaging flow contract:

- `POST /api/v2/conformance/harness/report/package`
- Request fields:
  - `harness_session_id` (string, required)
  - `collection_id` (string, required)
  - `format` (enum: `json`, `zip`, required)
- Response `data` fields:
  - `package_id` (string)
  - `harness_session_id` (string)
  - `collection_id` (string)
  - `state` (enum: `building`, `ready`, `failed`)
  - `report_uri` (string or `null`)
  - `sha256` (string or `null`)

Evidence/report deterministic guard failures:

- Invalid payload shape, empty `artifact_types`, or invalid window/order (`start_event_seq > end_event_seq`) -> `BAD_REQUEST`.
- Unknown harness session or collection identifiers -> `NOT_FOUND`.
- Artifact collection requested when harness session is not executable/completed (`initialized`, `failed`) -> `INVALID_SESSION_STATE`.
- Report packaging requested before collection reaches `completed` -> `INVALID_SESSION_STATE`.

Evidence collection request example:

```json
{
  "harness_session_id": "chs_01H...",
  "artifact_types": ["logs", "metrics", "stream_samples"],
  "window": {
    "start_event_seq": 1000,
    "end_event_seq": 2053
  }
}
```

Evidence collection response (`data`) example:

```json
{
  "collection_id": "col_01H...",
  "harness_session_id": "chs_01H...",
  "state": "completed",
  "artifacts": [
    {"type": "logs", "uri": "sdcard/conformance/chs_01H/logs.ndjson", "bytes": 120334},
    {"type": "metrics", "uri": "sdcard/conformance/chs_01H/metrics.json", "bytes": 4421}
  ],
  "started_at_us": 1710000021000,
  "completed_at_us": 1710000022333
}
```

Report package request example:

```json
{
  "harness_session_id": "chs_01H...",
  "collection_id": "col_01H...",
  "format": "zip"
}
```

Report package response (`data`) example:

```json
{
  "package_id": "pkg_01H...",
  "harness_session_id": "chs_01H...",
  "collection_id": "col_01H...",
  "state": "ready",
  "report_uri": "sdcard/conformance/chs_01H/report_pkg_01H.zip",
  "sha256": "9f2f52be6f7d11f8f0a6f7bc62bf4b6c6e5ac6f4c91dc2a82bf32f4c0c55f2a1"
}
```

Acceptance checklist execution runner contract:

- `POST /api/v2/conformance/harness/checklist/run`
- Request fields:
  - `harness_session_id` (string, required)
  - `checklist_id` (string, required)
  - `selection` (object, optional):
    - `include_case_ids` (array of strings)
    - `exclude_case_ids` (array of strings)
  - `stop_on_failure` (bool, default `false`)
- Response `data` fields:
  - `runner_id` (string)
  - `harness_session_id` (string)
  - `checklist_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `cases_total` (uint32)
  - `cases_passed` (uint32)
  - `cases_failed` (uint32)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Runner status and progress contract:

- `GET /api/v2/conformance/harness/checklist/run/status?runner_id=run_01H...`
- Response `data` fields:
  - `runner_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `current_case_id` (string or `null`)
  - `progress` (object):
    - `completed_cases` (uint32)
    - `total_cases` (uint32)
  - `last_transition_at_us` (uint64)

Checklist runner deterministic guard failures:

- Invalid payload shape or contradictory selection (`include_case_ids` and `exclude_case_ids` intersect) -> `BAD_REQUEST`.
- Unknown harness session, checklist, or runner identifier -> `NOT_FOUND`.
- Runner start requested before report package state is `ready` -> `INVALID_SESSION_STATE`.
- Concurrent runner already active for same `harness_session_id` + `checklist_id` -> `CONFLICT`.

Checklist runner request example:

```json
{
  "harness_session_id": "chs_01H...",
  "checklist_id": "st_acceptance_v1",
  "selection": {
    "include_case_ids": ["clock_step_contract", "stream_backpressure_contract"],
    "exclude_case_ids": []
  },
  "stop_on_failure": true
}
```

Checklist runner response (`data`) example:

```json
{
  "runner_id": "run_01H...",
  "harness_session_id": "chs_01H...",
  "checklist_id": "st_acceptance_v1",
  "state": "running",
  "cases_total": 2,
  "cases_passed": 0,
  "cases_failed": 0,
  "started_at_us": 1710000023000,
  "completed_at_us": null
}
```

Checklist runner status response (`data`) example:

```json
{
  "runner_id": "run_01H...",
  "state": "completed",
  "current_case_id": null,
  "progress": {
    "completed_cases": 2,
    "total_cases": 2
  },
  "last_transition_at_us": 1710000024555
}
```

Review pack generation contract:

- `POST /api/v2/conformance/harness/review-pack/generate`
- Request fields:
  - `harness_session_id` (string, required)
  - `runner_id` (string, required)
  - `package_id` (string, required)
  - `include_sections` (array of enum: `summary`, `failures`, `artifacts`, `telemetry`, `checklist`; required, length `>=1`)
- Response `data` fields:
  - `review_pack_id` (string)
  - `state` (enum: `building`, `ready`, `failed`)
  - `harness_session_id` (string)
  - `runner_id` (string)
  - `generated_at_us` (uint64 or `null`)
  - `review_pack_uri` (string or `null`)

Signoff bundle assembly contract:

- `POST /api/v2/conformance/harness/signoff-bundle/assemble`
- Request fields:
  - `review_pack_id` (string, required)
  - `signoff` (object, required):
    - `requested_by` (string)
    - `approver` (string)
    - `label` (string)
- Response `data` fields:
  - `bundle_id` (string)
  - `review_pack_id` (string)
  - `state` (enum: `assembling`, `ready`, `failed`)
  - `bundle_uri` (string or `null`)
  - `sha256` (string or `null`)
  - `assembled_at_us` (uint64 or `null`)

Review-pack/signoff deterministic guard failures:

- Invalid payload shape or empty `include_sections` -> `BAD_REQUEST`.
- Unknown `harness_session_id`, `runner_id`, `package_id`, or `review_pack_id` -> `NOT_FOUND`.
- Review-pack generation requested before runner reaches terminal state (`completed`/`failed`/`aborted`) -> `INVALID_SESSION_STATE`.
- Signoff bundle assembly requested before review pack state is `ready` -> `INVALID_SESSION_STATE`.

Review pack request example:

```json
{
  "harness_session_id": "chs_01H...",
  "runner_id": "run_01H...",
  "package_id": "pkg_01H...",
  "include_sections": ["summary", "failures", "artifacts", "telemetry", "checklist"]
}
```

Review pack response (`data`) example:

```json
{
  "review_pack_id": "rvp_01H...",
  "state": "ready",
  "harness_session_id": "chs_01H...",
  "runner_id": "run_01H...",
  "generated_at_us": 1710000025600,
  "review_pack_uri": "sdcard/conformance/chs_01H/review_pack_rvp_01H.json"
}
```

Signoff bundle request example:

```json
{
  "review_pack_id": "rvp_01H...",
  "signoff": {
    "requested_by": "qa_lead",
    "approver": "product_owner",
    "label": "s4_acceptance"
  }
}
```

Signoff bundle response (`data`) example:

```json
{
  "bundle_id": "sgn_01H...",
  "review_pack_id": "rvp_01H...",
  "state": "ready",
  "bundle_uri": "sdcard/conformance/chs_01H/signoff_bundle_sgn_01H.zip",
  "sha256": "6b2ebf4f95df8a59f2ad8a5e622ecf9f53251b1a17e5dca35d43a83a420ff7d9",
  "assembled_at_us": 1710000025900
}
```

### 15.2 Subsystem conformance test scaffold and fixture model

Subsystem conformance scaffold contract:

- `POST /api/v2/conformance/subsystems/scaffold`
- Request fields:
  - `harness_session_id` (string, required)
  - `subsystem` (enum: `cpu`, `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`, required)
  - `fixture_profile` (string, required)
  - `seed_mode` (enum: `baseline`, `snapshot`, default `baseline`)
- Response `data` fields:
  - `scaffold_id` (string)
  - `harness_session_id` (string)
  - `subsystem` (string)
  - `state` (enum: `initialized`, `prepared`, `ready`, `failed`)
  - `fixture_model_id` (string)
  - `created_at_us` (uint64)

Subsystem fixture model contract:

- `GET /api/v2/conformance/subsystems/fixtures/model?scaffold_id=...`
- Response `data` must satisfy `conformance_fixture_model_v1`.
- `conformance_fixture_model_v1` required fields:
  - `fixture_model_id` (string)
  - `subsystem` (string)
  - `inputs` (array)
  - `expected_outputs` (array)
  - `invariants` (array of strings)
  - `tolerances` (object)
  - `schema_version` (uint32, must equal `1`)

Subsystem scaffold/fixture conformance checks:

- `CONF-FIX-01`: scaffold transitions must follow `initialized -> prepared -> ready` unless terminal `failed`.
- `CONF-FIX-02`: `fixture_model_id` must be stable for identical (`harness_session_id`, `subsystem`, `fixture_profile`, `seed_mode`) inputs.
- `CONF-FIX-03`: fixture model must include at least one `input`, one `expected_output`, and one `invariant`.
- `CONF-FIX-04`: scaffold/fixture timestamps are monotonic (`created_at_us <= prepared_at_us <= ready_at_us`) when those timestamps are present.

Subsystem scaffold/fixture deterministic guard failures:

- Invalid payload shape, unknown `subsystem`/`seed_mode`, or missing required fields -> `BAD_REQUEST`.
- Unknown harness session or invalid scaffold identifier -> `NOT_FOUND`.
- Scaffold request submitted before harness session reaches `ready` -> `INVALID_SESSION_STATE`.
- Fixture model fetch requested before scaffold state is `ready` -> `INVALID_SESSION_STATE`.

Subsystem scaffold request example:

```json
{
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "fixture_profile": "mfp_timer_irq_smoke",
  "seed_mode": "baseline"
}
```

Subsystem scaffold response (`data`) example:

```json
{
  "scaffold_id": "scf_01H...",
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "state": "ready",
  "fixture_model_id": "fxm_01H...",
  "created_at_us": 1710000026400,
  "prepared_at_us": 1710000026440,
  "ready_at_us": 1710000026488
}
```

Subsystem fixture model response (`data`) example:

```json
{
  "fixture_model_id": "fxm_01H...",
  "subsystem": "mfp",
  "inputs": [
    {
      "signal": "timer_a_start",
      "value": "0x01"
    }
  ],
  "expected_outputs": [
    {
      "signal": "irq6_assert",
      "within_ticks": 32
    }
  ],
  "invariants": ["event_seq_monotonic", "timestamp_us_monotonic"],
  "tolerances": {
    "tick_jitter_max": 2
  },
  "schema_version": 1
}
```

### 15.3 Per-subsystem acceptance suites and reporting output

Per-subsystem acceptance suite execution contract:

- `POST /api/v2/conformance/subsystems/suites/run`
- Request fields:
  - `harness_session_id` (string, required)
  - `scaffold_id` (string, required)
  - `suite_id` (string, required)
  - `selection` (object, optional):
    - `include_case_ids` (array of strings)
    - `exclude_case_ids` (array of strings)
  - `stop_on_failure` (bool, default `false`)
- Response `data` fields:
  - `suite_run_id` (string)
  - `harness_session_id` (string)
  - `subsystem` (string)
  - `suite_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `cases_total` (uint32)
  - `cases_passed` (uint32)
  - `cases_failed` (uint32)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Per-subsystem reporting output contract:

- `GET /api/v2/conformance/subsystems/suites/report?suite_run_id=...`
- Response `data` must satisfy `subsystem_suite_report_v1`.
- `subsystem_suite_report_v1` required fields:
  - `suite_run_id` (string)
  - `subsystem` (string)
  - `summary` (object):
    - `cases_total` (uint32)
    - `cases_passed` (uint32)
    - `cases_failed` (uint32)
    - `pass_rate` (number, `0..1`)
  - `case_results` (array)
  - `evidence_uris` (array of strings)
  - `generated_at_us` (uint64)

Per-subsystem suite/report conformance checks:

- `SUB-SUITE-01`: `cases_passed + cases_failed == cases_total` for terminal suite states.
- `SUB-SUITE-02`: report `pass_rate == cases_passed / cases_total` when `cases_total > 0`.
- `SUB-SUITE-03`: `generated_at_us >= completed_at_us` for completed/failed/aborted suite runs.
- `SUB-SUITE-04`: `case_results` ordering must be deterministic by (`case_id`, `assertion_id`) stable sort.

Per-subsystem suite/report deterministic guard failures:

- Invalid payload shape, contradictory selection, or invalid identifiers -> `BAD_REQUEST`.
- Unknown harness/scaffold/suite run identifiers -> `NOT_FOUND`.
- Suite run requested before scaffold state is `ready` -> `INVALID_SESSION_STATE`.
- Report requested before suite run reaches terminal state (`completed`, `failed`, `aborted`) -> `INVALID_SESSION_STATE`.

Per-subsystem suite run request example:

```json
{
  "harness_session_id": "chs_01H...",
  "scaffold_id": "scf_01H...",
  "suite_id": "mfp_acceptance_v1",
  "selection": {
    "include_case_ids": ["timer_a_irq", "vector_routing"],
    "exclude_case_ids": []
  },
  "stop_on_failure": true
}
```

Per-subsystem suite run response (`data`) example:

```json
{
  "suite_run_id": "ssr_01H...",
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "suite_id": "mfp_acceptance_v1",
  "state": "completed",
  "cases_total": 2,
  "cases_passed": 2,
  "cases_failed": 0,
  "started_at_us": 1710000027000,
  "completed_at_us": 1710000027240
}
```

Per-subsystem suite report response (`data`) example:

```json
{
  "suite_run_id": "ssr_01H...",
  "subsystem": "mfp",
  "summary": {
    "cases_total": 2,
    "cases_passed": 2,
    "cases_failed": 0,
    "pass_rate": 1.0
  },
  "case_results": [
    {
      "case_id": "timer_a_irq",
      "assertion_id": "irq6_asserted",
      "result": "pass"
    }
  ],
  "evidence_uris": ["sdcard/conformance/chs_01H/suites/ssr_01H/mfp_report.json"],
  "generated_at_us": 1710000027260
}
```

---

## 16. Cross-reference map

- Hardware behavior requirements: `emu_engine_v2/README.md` and linked chapters.
- Engine architecture and phased delivery: `EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md`.
