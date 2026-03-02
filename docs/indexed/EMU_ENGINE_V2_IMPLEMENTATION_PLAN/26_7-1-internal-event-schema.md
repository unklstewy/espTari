# 7.1 Internal event schema

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 26

## 7.1 Internal event schema

All observable events should include:

- monotonic `tick`
- `cycle`
- `component_id`
- `event_type`
- structured payload

Status telemetry alignment:

- Status snapshot fields (`lifecycle_state`, `run_mode`, `snapshot_at_us`, `cycle_counter`, `tick_counter`, `runtime.last_transition_at_us`, `runtime.last_error`) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6B` and are the canonical source for control-plane telemetry payload shape.
- Engine status/health WebSocket telemetry uses monotonic ordering fields (`event_seq`, `event_timestamp_us`) and degraded-delivery disclosure (`delivery.*`) defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.8`; these are the canonical stream observability contracts.
