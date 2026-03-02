# 11.3G PSG register/audio behavior contract

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 77

## 11.3G PSG register/audio behavior contract

PSG register behavior contract:

- `GET /api/v2/inspect/chipset/psg/registers?session_id=...`
- Response `data` must expose register-window state validated by `psg_register_window_v1`.
- `psg_register_window_v1` required fields:
  - `session_id` (string)
  - `base_address` (string, hex)
  - `window_bytes` (uint32)
  - `registers` (array of `psg_register_entry_v1`)
- `psg_register_entry_v1` required fields:
  - `name` (string)
  - `index` (uint8)
  - `value` (uint8)
  - `latched_tick` (uint64)

PSG audio behavior contract:

- `GET /api/v2/inspect/chipset/psg/audio?session_id=...&limit=...`
- Audio entries are validated by `psg_audio_state_v1` and emitted in sequence order.
- `psg_audio_state_v1` required fields:
  - `frame_seq` (uint64)
  - `mix_mode` (enum: `mono`)
  - `sample_rate_hz` (uint32)
  - `channel_a_level` (uint8)
  - `channel_b_level` (uint8)
  - `channel_c_level` (uint8)
  - `noise_enable` (boolean)
  - `envelope_shape` (uint8)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

PSG register/audio conformance checks:

- `PSG-AUD-01`: register writes to tone/noise/envelope control must be reflected in subsequent emitted audio states.
- `PSG-AUD-02`: `frame_seq(next) == frame_seq(prev) + 1` for emitted audio states.
- `PSG-AUD-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `PSG-AUD-04`: `channel_*_level` values remain within `0..15` and are deterministic for identical register input traces.

PSG register/audio deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- PSG register window unavailable or audio state derivation unresolved -> `INTERNAL_ERROR`.

PSG register window example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "base_address": "0x00FF8800",
  "window_bytes": 16,
  "registers": [
    {
      "name": "CHANNEL_A_FINE",
      "index": 0,
      "value": 34,
      "latched_tick": 913002
    }
  ]
}
```

PSG audio state stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "states": [
    {
      "frame_seq": 7810,
      "mix_mode": "mono",
      "sample_rate_hz": 50066,
      "channel_a_level": 10,
      "channel_b_level": 4,
      "channel_c_level": 0,
      "noise_enable": true,
      "envelope_shape": 9,
      "tick_counter": 913010,
      "timestamp_us": 1710000032522
    }
  ]
}
```
