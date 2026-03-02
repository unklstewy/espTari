# 12. State model

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 87

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
