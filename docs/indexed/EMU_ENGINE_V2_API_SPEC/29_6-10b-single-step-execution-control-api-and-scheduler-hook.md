# 6.10B Single-step execution control API and scheduler hook

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 29

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
