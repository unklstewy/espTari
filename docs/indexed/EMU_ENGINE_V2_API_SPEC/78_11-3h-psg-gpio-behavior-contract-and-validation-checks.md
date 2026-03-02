# 11.3H PSG GPIO behavior contract and validation checks

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 78

## 11.3H PSG GPIO behavior contract and validation checks

PSG GPIO behavior contract:

- `GET /api/v2/inspect/chipset/psg/gpio?session_id=...`
- Response `data` must expose GPIO state validated by `psg_gpio_state_v1`.
- `psg_gpio_state_v1` required fields:
  - `session_id` (string)
  - `port_a_direction` (enum: `input`, `output`)
  - `port_b_direction` (enum: `input`, `output`)
  - `port_a_value` (uint8)
  - `port_b_value` (uint8)
  - `latched_tick` (uint64)
  - `timestamp_us` (uint64)

PSG GPIO event contract:

- `GET /api/v2/inspect/chipset/psg/gpio/events?session_id=...&limit=...`
- GPIO events are validated by `psg_gpio_event_v1` and emitted in sequence order.
- `psg_gpio_event_v1` required fields:
  - `event_seq` (uint64)
  - `port` (enum: `A`, `B`)
  - `direction` (enum: `input`, `output`)
  - `value_before` (uint8)
  - `value_after` (uint8)
  - `source` (enum: `cpu_write`, `external_signal`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

PSG GPIO conformance checks:

- `PSG-GPIO-01`: `event_seq(next) == event_seq(prev) + 1` for emitted GPIO events.
- `PSG-GPIO-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `PSG-GPIO-03`: when `direction=output`, `value_after` must equal the last CPU-written value for that port.
- `PSG-GPIO-04`: when `direction=input`, `value_after` must only change due to `source=external_signal`.

PSG GPIO deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- GPIO latch state unavailable or unresolved direction/value mapping -> `INTERNAL_ERROR`.

PSG GPIO state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "port_a_direction": "output",
  "port_b_direction": "input",
  "port_a_value": 127,
  "port_b_value": 16,
  "latched_tick": 913240,
  "timestamp_us": 1710000033180
}
```

PSG GPIO event stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 4201,
      "port": "A",
      "direction": "output",
      "value_before": 95,
      "value_after": 127,
      "source": "cpu_write",
      "tick_counter": 913241,
      "timestamp_us": 1710000033182
    },
    {
      "event_seq": 4202,
      "port": "B",
      "direction": "input",
      "value_before": 16,
      "value_after": 48,
      "source": "external_signal",
      "tick_counter": 913248,
      "timestamp_us": 1710000033194
    }
  ]
}
```
