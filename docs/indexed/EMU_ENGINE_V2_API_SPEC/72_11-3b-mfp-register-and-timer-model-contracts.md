# 11.3B MFP register and timer model contracts

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 72

## 11.3B MFP register and timer model contracts

MFP register model contract:

- `GET /api/v2/inspect/chipset/windows/registers?session_id=...&group=mfp`
- Response exposes normalized MFP register projection validated by schema `mfp_register_window_v1`.
- `mfp_register_window_v1` required fields:
  - `group` (string, must equal `mfp`)
  - `base_address` (string, hex)
  - `window_bytes` (uint32, `>0`)
  - `registers` (array, length `>=1`)
- `registers[]` required fields:
  - `name` (string)
  - `offset` (uint32)
  - `address` (string, hex)
  - `width_bits` (uint32)
  - `access` (enum: `ro`, `wo`, `rw`)

MFP timer model contract:

- `GET /api/v2/inspect/chipset/windows/timers?session_id=...&group=mfp`
- Response exposes one timer projection per logical MFP timer (`A`, `B`, `C`, `D`) validated by schema `mfp_timer_model_v1`.
- `mfp_timer_model_v1` required fields:
  - `timer_id` (enum: `A`, `B`, `C`, `D`)
  - `control_register` (string)
  - `data_register` (string)
  - `prescaler` (uint32)
  - `counter_value` (uint32)
  - `mode` (enum: `stopped`, `delay`, `event_count`, `pulse_width`)
  - `enabled` (bool)

MFP model deterministic guard failures:

- Missing/invalid `session_id` or invalid `group` selector -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Unsupported timer selector or unresolved timer mapping -> `INSPECT_FILTER_INVALID`.

MFP interrupt emission behavior contract:

- MFP interrupt emission is driven by timer/state transitions and must emit deterministic interrupt events when enable/mask gates are satisfied.
- `mfp_interrupt_event_v1` required fields:
  - `source` (string, must equal `mfp`)
  - `interrupt_line` (enum: `irq2`, `irq6`)
  - `vector` (uint16)
  - `timer_id` (enum: `A`, `B`, `C`, `D` or `null`)
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `event_timestamp_us` (uint64)

MFP interrupt conformance checks:

- `MFP-IRQ-01`: interrupts emit only when corresponding enable+mask bits resolve to active delivery.
- `MFP-IRQ-02`: `event_timestamp_us(next) >= event_timestamp_us(prev)` for emitted MFP interrupt events.
- `MFP-IRQ-03`: emitted `vector` matches active interrupt source/vector mapping.
- `MFP-IRQ-04`: repeated emissions for same source/tick require an explicit re-arm condition.

MFP interrupt deterministic failures:

- Interrupt event emitted with unresolved source/vector mapping -> `INTERNAL_ERROR`.
- Interrupt timing monotonicity violation (`MFP-IRQ-02`) -> `INTERNAL_ERROR` with fail-fast diagnostics.

MFP register model example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "window": {
    "group": "mfp",
    "base_address": "0x00FFFA00",
    "window_bytes": 64,
    "registers": [
      {
        "name": "IERA",
        "offset": 7,
        "address": "0x00FFFA07",
        "width_bits": 8,
        "access": "rw"
      }
    ]
  }
}
```

MFP timer model example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "timers": [
    {
      "timer_id": "A",
      "control_register": "TACR",
      "data_register": "TADR",
      "prescaler": 64,
      "counter_value": 192,
      "mode": "delay",
      "enabled": true
    }
  ]
}
```

MFP interrupt conformance trace example:

```json
{
  "session_id": "ses_01H...",
  "checks": {
    "MFP-IRQ-01": "pass",
    "MFP-IRQ-02": "pass",
    "MFP-IRQ-03": "pass",
    "MFP-IRQ-04": "pass"
  },
  "last_interrupt": {
    "source": "mfp",
    "interrupt_line": "irq6",
    "vector": 26,
    "timer_id": "A",
    "tick_counter": 450208244,
    "cycle_counter": 112552991,
    "event_timestamp_us": 1710000028022
  }
}
```
