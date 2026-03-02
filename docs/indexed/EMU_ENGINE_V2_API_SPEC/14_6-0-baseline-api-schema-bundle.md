# 6.0 Baseline API schema bundle

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 14

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
