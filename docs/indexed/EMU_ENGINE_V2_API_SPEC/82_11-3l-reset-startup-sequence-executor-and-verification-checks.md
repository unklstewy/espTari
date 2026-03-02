# 11.3L Reset/startup sequence executor and verification checks

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 82

## 11.3L Reset/startup sequence executor and verification checks

Reset/startup sequence executor contract:

- `GET /api/v2/inspect/chipset/startup/sequence?session_id=...`
- Response `data` must expose executor state validated by `startup_sequence_state_v1`.
- `startup_sequence_state_v1` required fields:
  - `session_id` (string)
  - `sequence_id` (string)
  - `phase` (enum: `assert_reset`, `clock_stabilize`, `register_seed`, `interrupt_enable`, `ready`)
  - `step_seq` (uint64)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)
  - `verification_status` (enum: `pending`, `pass`, `fail`)

Reset/startup verification-check contract:

- `GET /api/v2/inspect/chipset/startup/verification?session_id=...&limit=...`
- Verification events are validated by `startup_verification_event_v1` and emitted in sequence order.
- `startup_verification_event_v1` required fields:
  - `event_seq` (uint64)
  - `step_seq` (uint64)
  - `check_id` (string)
  - `component` (enum: `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`)
  - `result` (enum: `pass`, `fail`)
  - `expected` (string)
  - `observed` (string)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Reset/startup conformance checks:

- `RST-SEQ-01`: sequence phases must follow strict order `assert_reset -> clock_stabilize -> register_seed -> interrupt_enable -> ready`.
- `RST-SEQ-02`: `step_seq(next) == step_seq(prev) + 1` and `event_seq(next) == event_seq(prev) + 1` for emitted verification stream.
- `RST-SEQ-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `RST-SEQ-04`: `phase=ready` is allowed only when all required verification checks emit `result=pass`.

Reset/startup deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Sequence executor unavailable or unresolved verification dependency -> `INTERNAL_ERROR`.

Startup sequence state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "sequence_id": "boot_seq_0007",
  "phase": "register_seed",
  "step_seq": 3,
  "tick_counter": 14,
  "timestamp_us": 1710000035710,
  "verification_status": "pending"
}
```

Startup verification stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 1,
      "step_seq": 3,
      "check_id": "BOOT-MFP-RESET",
      "component": "mfp",
      "result": "pass",
      "expected": "IERA=0x00",
      "observed": "IERA=0x00",
      "tick_counter": 15,
      "timestamp_us": 1710000035712
    },
    {
      "event_seq": 2,
      "step_seq": 4,
      "check_id": "BOOT-INT-MAP",
      "component": "glue",
      "result": "pass",
      "expected": "irq_order=7..1",
      "observed": "irq_order=7..1",
      "tick_counter": 16,
      "timestamp_us": 1710000035718
    }
  ]
}
```
