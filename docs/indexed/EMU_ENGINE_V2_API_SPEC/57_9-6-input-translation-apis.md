# 9.6 Input translation APIs

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 57

## Subsections

- 9.6.1 Enumerate input devices
- 9.6.2 Load mapping profile
- 9.6.3 Read active mapping
- 9.6.3A Apply active mapping profile
- 9.6.4 Update mapping entries
- 9.6.5 Inject normalized host events
- 9.6.6 Browser capture state and controls
- 9.6.7 Input stream (translated events + diagnostics)

---

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
