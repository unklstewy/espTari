# 6.4 Input APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 19

## 6.4 Input APIs

- `GET /api/v2/input/devices`
  - Enumerate currently available host input devices
- `POST /api/v2/input/events/inject`
  - Submit normalized host input events to translation pipeline
- `GET /api/v2/input/capture/state`
  - Inspect current browser input capture state for the session
- `POST /api/v2/input/policy/enabled`
  - Idempotent enable/disable policy control (`result=applied|no_op`) aligned to policy states in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6`
- `POST /api/v2/input/capture/config`
  - Enable/disable input and set capture mode (`mouse_over` or `click_to_capture`)
- `POST /api/v2/input/capture/release`
  - Force release of mouse capture for the requesting browser session
- `POST /api/v2/input/mappings/load`
  - Load input mapping profile for machine/session
- `GET /api/v2/input/mappings/active`
  - Inspect active mapping profile
- `POST /api/v2/input/mappings/update`
  - Patch mapping entries at runtime
- `GET /api/v2/input/stream` (WebSocket)
  - Stream translated virtual input events, input-pipeline diagnostics, and input-policy transition events

Mapping responsibilities:

- Keyboard host keycodes -> Atari ST key matrix/IKBD semantics
- Mouse host deltas/buttons -> IKBD mouse packet semantics
- Game controller host axes/buttons -> virtual joystick semantics
- Canonical input mapping schema and persistence model (`mapping_profile` fields, per-entry schema, persistence path, revision semantics, and deterministic blocker mapping) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.2` through `9.6.4`.
- Mapping profile CRUD contract (create/list/get/patch/delete semantics, persistence behavior, and deterministic CRUD blockers) plus active-profile apply path contract (`POST /api/v2/input/mappings/apply`, atomic cutover semantics, apply no-op rules, and revision-conflict handling) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.2` through `9.6.4`.

Browser input capture policy:

- Policy state model for endpoint/event contracts is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` (`disabled`, `enabled_idle`, `enabled_captured`) with deterministic transitions and policy metadata (`policy.state`, `policy.source`, `policy.reason`, `policy.changed_at_us`).
- `enabled=false`: input events are ignored except capture-config and diagnostics requests.
- `capture_mode=mouse_over`: browser sends input only while pointer is over canvas; leaving canvas releases capture implicitly.
- `mouse_over` capture state machine contract (state variables, deterministic transition matrix, and guards `MO-GUARD-01`..`MO-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` and is the canonical source for pointer-driven capture activation.
- `mouse_over` enter/leave event hooks and release behavior contract (deterministic `pointer_enter_hook` / `pointer_leave_hook` processing, hook-driven policy events, and explicit release no-op semantics in mouse-over mode) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.6` and `9.6.7`.
- `capture_mode=click_to_capture`: capture starts on canvas click and remains active until explicit release.
- `click_to_capture` acquisition state machine contract (deterministic click-acquire transitions and guards `CT-GUARD-01`..`CT-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` and is the canonical source for click-driven capture acquisition behavior.
- Escape release sequence is supported in `click_to_capture` mode; default sequence is double-escape (`Escape`, `Escape`) within configurable timeout.
- Escape-release and focus-recovery behavior contract for `click_to_capture` mode (escape-sequence release transition, focus-loss forced release, no auto-reacquire on focus regain, and deterministic guards `ER-GUARD-01`..`ER-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.6` and `9.6.7`.
- `click_to_capture` mode also supports explicit release via `/api/v2/input/capture/release`.
- Policy-related invalid-state and violation errors use canonical input error envelopes (`INPUT_POLICY_INVALID_STATE`, `INPUT_POLICY_VIOLATION`) from `docs/EMU_ENGINE_V2_API_SPEC.md` sections `5` and `9.6.6`.
- Policy enable/disable endpoint guards use deterministic input errors (`INPUT_POLICY_MODE_INVALID`, `INPUT_POLICY_SESSION_INVALID`) and idempotent `no_op` responses as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6`.
- Input stream policy-change event contract (`input_policy_changed`) including required fields (`source`, `prior_state`, `new_state`, `reason`, `event_timestamp_us`, `event_seq`, `transition_result`), strict event ordering, and no-op observability is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.
- Input translation event payload + ordering contract (`input_translated` required fields including mapping snapshot identifiers, monotonic `event_seq` / `event_timestamp_us`, and `(tick, cycle)` ordering semantics) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.
- Input translation stream emitter + sequencing-check contract (deterministic emission pipeline, checks `SEQ-CHECK-01..03`, and diagnostics surfacing of sequence anomalies) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.
