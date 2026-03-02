# 6.10A Deterministic tick-loop scheduler core

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 31

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
