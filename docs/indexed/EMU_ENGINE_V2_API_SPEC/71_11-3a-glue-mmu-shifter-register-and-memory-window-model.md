# 11.3A GLUE/MMU/SHIFTER register and memory window model

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 71

## 11.3A GLUE/MMU/SHIFTER register and memory window model

Register window model contract:

- `GET /api/v2/inspect/chipset/windows/registers?session_id=...&group=glue,mmu,shifter`
- Response exposes one normalized register-window projection per requested chipset group.
- `register_window_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`)
  - `base_address` (string, hex)
  - `window_bytes` (uint32, `>0`)
  - `registers` (array, length `>=1`)
- `registers[]` required fields:
  - `name` (string)
  - `offset` (uint32)
  - `address` (string, hex)
  - `width_bits` (uint32)
  - `access` (enum: `ro`, `wo`, `rw`)

Memory window model contract:

- `GET /api/v2/inspect/chipset/windows/memory?session_id=...&group=glue,mmu,shifter`
- Response exposes one normalized memory-window projection per requested chipset group.
- `memory_window_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`)
  - `window_start` (string, hex)
  - `window_end` (string, hex, inclusive)
  - `mapped_region` (string)
  - `addressing_mode` (enum: `linear`, `banked`)

GLUE/MMU/SHIFTER model deterministic guard failures:

- Missing/invalid `session_id` or invalid `group` selector -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Unresolved chipset model projection for requested group -> `INTERNAL_ERROR`.

GLUE/MMU/SHIFTER arbitration and timing integration contract:

- Arbitration integration follows scheduler/arbitration hook ordering defined in section `6.10A` and binds chipset participation to each committed tick.
- Per committed tick, chipset arbitration order is deterministic: `glue -> mmu -> shifter` unless profile wiring explicitly overrides with a validated order.
- Required timing integration fields emitted by chipset integration diagnostics:
  - `tick_counter` (uint64)
  - `cycle_counter` (uint64)
  - `chipset_order` (array of strings)
  - `bus_owner` (string)
  - `wait_cycles` (uint32)
  - `event_timestamp_us` (uint64)

Chipset arbitration/timing checks:

- `CHIP-TIM-01`: chipset step order matches validated arbitration order for each committed tick.
- `CHIP-TIM-02`: `cycle_counter(next) >= cycle_counter(prev)` across emitted integration events.
- `CHIP-TIM-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)` for chipset integration telemetry.
- `CHIP-TIM-04`: reported `bus_owner` must be one of `{glue, mmu, shifter, cpu, dma}`.

Chipset integration deterministic failures:

- Chipset arbitration order mismatch or unresolved participant in active order -> `INTERNAL_ERROR`.
- Timing monotonicity breach (`CHIP-TIM-02` or `CHIP-TIM-03`) -> `INTERNAL_ERROR` with fail-fast diagnostics.

Register window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "windows": [
    {
      "group": "shifter",
      "base_address": "0x00FF8200",
      "window_bytes": 64,
      "registers": [
        {
          "name": "video_base_high",
          "offset": 1,
          "address": "0x00FF8201",
          "width_bits": 8,
          "access": "rw"
        }
      ]
    }
  ]
}
```

Memory window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "windows": [
    {
      "group": "mmu",
      "window_start": "0x00FF8000",
      "window_end": "0x00FF82FF",
      "mapped_region": "st_chipset_io",
      "addressing_mode": "linear"
    }
  ]
}
```

Chipset arbitration/timing validation trace example:

```json
{
  "session_id": "ses_01H...",
  "checks": {
    "CHIP-TIM-01": "pass",
    "CHIP-TIM-02": "pass",
    "CHIP-TIM-03": "pass",
    "CHIP-TIM-04": "pass"
  },
  "last_integration": {
    "tick_counter": 450208120,
    "cycle_counter": 112552440,
    "chipset_order": ["glue", "mmu", "shifter"],
    "bus_owner": "mmu",
    "wait_cycles": 2,
    "event_timestamp_us": 1710000026400
  }
}
```
