# 6.10C Opcode/bus-error capture path and diagnostic payloads

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 30

## 6.10C Opcode/bus-error capture path and diagnostic payloads

Capture path model (`POST /api/v2/debug/clock/step` with `capture`):

- Capture pipeline runs in committed tick order and emits deterministic capture entries per tick.
- `opcode` capture is sourced from executed instruction decode for each committed CPU step.
- `bus_error` capture is sourced from scheduler/arbitration bus-access fault signals for committed ticks.
- Capture pipeline output is serialized into `capture_payloads` in the same order as committed ticks.

Capture checks:

- `CAP-DIAG-01`: `capture_payloads[*].tick_counter` is monotonic non-decreasing.
- `CAP-DIAG-02`: opcode capture entries include `pc`, `opcode_word`, and `instruction_size_bytes`.
- `CAP-DIAG-03`: bus-error capture entries include `fault_address`, `access_type`, `fault_phase`, and `vector`.
- `CAP-DIAG-04`: capture payload ordering is stable across identical scheduler traces.

Diagnostic payload schemas:

- `opcode_capture_v1`:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `pc` (uint32)
  - `opcode_word` (hex string)
  - `instruction_size_bytes` (uint8)
- `bus_error_capture_v1`:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `fault_address` (uint32)
  - `access_type` (`read`|`write`|`instruction_fetch`)
  - `fault_phase` (`address`|`data`|`ack`)
  - `vector` (uint16)

Capture guard failures:

- Unsupported capture selector in `capture[]` -> `DEBUG_STEP_INVALID`.
- Requested capture selector unavailable in active runtime profile -> `INVALID_SESSION_STATE`.
- Capture schema projection failure after committed ticks -> `INTERNAL_ERROR`.

Step response capture example (`capture=["opcode","bus_error"]`):

```json
{
  "session_id": "ses_01H...",
  "run_mode": "single_step",
  "steps_requested": 2,
  "ticks_committed": 2,
  "capture_payloads": [
    {
      "kind": "opcode_capture_v1",
      "tick_counter": 409771739,
      "cycle_counter": 102443012,
      "pc": 16779904,
      "opcode_word": "0x4E71",
      "instruction_size_bytes": 2
    },
    {
      "kind": "bus_error_capture_v1",
      "tick_counter": 409771740,
      "cycle_counter": 102443019,
      "fault_address": 16781312,
      "access_type": "instruction_fetch",
      "fault_phase": "ack",
      "vector": 2
    }
  ]
}
```

Invalid capture selector error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000011022,
  "error": {
    "code": "DEBUG_STEP_INVALID",
    "category": "debug",
    "message": "Unsupported capture selector 'trace_raw'",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/debug/clock/step",
      "guard_id": "CAP-DIAG-03"
    }
  }
}
```
