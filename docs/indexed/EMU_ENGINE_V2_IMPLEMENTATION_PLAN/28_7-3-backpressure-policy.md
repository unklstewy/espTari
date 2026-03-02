# 7.3 Backpressure policy

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 28

## 7.3 Backpressure policy

- Ring buffers per stream channel.
- Drop policy must be explicit and counted (`dropped_events`).
- Optional throttle knobs per stream endpoint.
- Engine status/health stream must disclose skip/coalescing state per event (`delivery.dropped_events_since_last`, `delivery.coalesced_updates`, `delivery.reason`, `delivery.degraded`) as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.8`.
