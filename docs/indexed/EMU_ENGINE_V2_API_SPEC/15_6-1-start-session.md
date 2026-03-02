# 6.1 Start session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 15

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
