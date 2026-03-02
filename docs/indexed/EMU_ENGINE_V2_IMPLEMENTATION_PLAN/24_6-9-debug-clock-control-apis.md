# 6.9 Debug clock-control APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 24

## 6.9 Debug clock-control APIs

- `POST /api/v2/debug/clock/mode`
  - Set execution mode: `realtime`, `slow_motion`, `single_step`
- `POST /api/v2/debug/clock/step`
  - Execute one or more bounded debug steps and emit trace markers
- `GET /api/v2/debug/clock/state`
  - Read active debug clock mode, ratio, and last-step counters
- Realtime/slow-motion clock-control model + deterministic bounds contract (`effective_ratio`, bounds checks `CLOCK-BOUND-01..04`, and mode-change guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.9A` and is the canonical source.
- Clock mode API deterministic transition-flow contract (`mode_transition_seq`, checks `CLOCK-TRANS-01..04`, idempotent transition behavior, and transition guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.9B` and is the canonical source.
- Single-step execution-control API + scheduler-hook contract (`steps` bounds, checks `STEP-CTRL-01..04`, `scheduler_hook_stats`, and deterministic guard/error mapping for `/api/v2/debug/clock/step`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10B` and is the canonical source.
- Opcode/bus-error capture path + diagnostic payload contract (`opcode_capture_v1`, `bus_error_capture_v1`, checks `CAP-DIAG-01..04`, and deterministic capture guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10C` and is the canonical source.

---
