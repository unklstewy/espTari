# 11.3J Interrupt wiring integration checks across subsystems

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 80

## 11.3J Interrupt wiring integration checks across subsystems

Interrupt wiring integration contract:

- `GET /api/v2/inspect/chipset/interrupts/wiring?session_id=...`
- Response `data` must expose subsystem wiring state validated by `interrupt_wiring_state_v1`.
- `interrupt_wiring_state_v1` required fields:
  - `session_id` (string)
  - `subsystems` (array of `interrupt_subsystem_wiring_v1`)
  - `global_route_seq` (uint64)
  - `last_timestamp_us` (uint64)
- `interrupt_subsystem_wiring_v1` required fields:
  - `subsystem_id` (enum: `mfp`, `acia`, `fdc`, `blitter`, `vbl`)
  - `source_line` (string)
  - `cpu_interrupt_line` (enum: `irq1`, `irq2`, `irq3`, `irq4`, `irq5`, `irq6`, `irq7`)
  - `vector` (uint16)
  - `enabled` (boolean)

Interrupt wiring integration-check event contract:

- `GET /api/v2/inspect/chipset/interrupts/wiring/checks?session_id=...&limit=...`
- Integration-check events are validated by `interrupt_wiring_check_v1` and emitted in sequence order.
- `interrupt_wiring_check_v1` required fields:
  - `check_seq` (uint64)
  - `subsystem_id` (string)
  - `expected_cpu_line` (string)
  - `observed_cpu_line` (string)
  - `expected_vector` (uint16)
  - `observed_vector` (uint16)
  - `result` (enum: `pass`, `fail`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Interrupt wiring integration conformance checks:

- `INT-WIRE-01`: `check_seq(next) == check_seq(prev) + 1` for integration-check stream.
- `INT-WIRE-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `INT-WIRE-03`: when `result=pass`, `observed_cpu_line == expected_cpu_line` and `observed_vector == expected_vector`.
- `INT-WIRE-04`: failed checks must not mutate active routing map; next delivered interrupt must still follow current `interrupt_hierarchy_v1` order.

Interrupt wiring deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Wiring map unavailable or unresolved subsystem route linkage -> `INTERNAL_ERROR`.

Interrupt wiring state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "subsystems": [
    {
      "subsystem_id": "mfp",
      "source_line": "mfp_irq",
      "cpu_interrupt_line": "irq6",
      "vector": 38,
      "enabled": true
    }
  ],
  "global_route_seq": 10022,
  "last_timestamp_us": 1710000034620
}
```

Interrupt wiring check stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "checks": [
    {
      "check_seq": 410,
      "subsystem_id": "mfp",
      "expected_cpu_line": "irq6",
      "observed_cpu_line": "irq6",
      "expected_vector": 38,
      "observed_vector": 38,
      "result": "pass",
      "tick_counter": 913901,
      "timestamp_us": 1710000034623
    },
    {
      "check_seq": 411,
      "subsystem_id": "fdc",
      "expected_cpu_line": "irq3",
      "observed_cpu_line": "irq5",
      "expected_vector": 54,
      "observed_vector": 54,
      "result": "fail",
      "tick_counter": 913907,
      "timestamp_us": 1710000034630
    }
  ]
}
```
