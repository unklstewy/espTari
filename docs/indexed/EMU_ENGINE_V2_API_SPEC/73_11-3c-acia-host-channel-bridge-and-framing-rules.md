# 11.3C ACIA host channel bridge and framing rules

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 73

## 11.3C ACIA host channel bridge and framing rules

ACIA host channel bridge contract:

- `GET /api/v2/inspect/chipset/acia/bridge?session_id=...`
- Response `data` must expose bridge state validated by `acia_bridge_state_v1`.
- `acia_bridge_state_v1` required fields:
  - `session_id` (string)
  - `bridge_state` (enum: `detached`, `attached`, `error`)
  - `channel_mode` (enum: `rx`, `tx`, `duplex`)
  - `rx_queue_depth` (uint32)
  - `tx_queue_depth` (uint32)
  - `framing_profile` (string)
  - `last_frame_seq` (uint64)
  - `last_timestamp_us` (uint64)

ACIA framing rules contract:

- `GET /api/v2/inspect/chipset/acia/frames?session_id=...&limit=...`
- Frames are validated by `acia_frame_v1` and emitted in sequence order.
- `acia_frame_v1` required fields:
  - `frame_seq` (uint64)
  - `direction` (enum: `host_to_st`, `st_to_host`)
  - `encoding` (enum: `8N1`)
  - `payload_hex` (string)
  - `start_bit` (uint8, must equal `0`)
  - `stop_bits` (uint8, must equal `1`)
  - `parity` (string, must equal `none`)
  - `timestamp_us` (uint64)

ACIA framing conformance checks:

- `ACIA-FRM-01`: `frame_seq(next) == frame_seq(prev) + 1` per direction stream.
- `ACIA-FRM-02`: `timestamp_us(next) >= timestamp_us(prev)` per direction stream.
- `ACIA-FRM-03`: frame bit-shape satisfies `start_bit=0`, `stop_bits=1`, `parity=none` for `encoding=8N1`.
- `ACIA-FRM-04`: frames rejected by bridge validation must not be emitted in inspected frame stream.

ACIA bridge/framing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Bridge attachment unavailable or unresolved framing profile -> `INTERNAL_ERROR`.

ACIA bridge state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "bridge_state": "attached",
  "channel_mode": "duplex",
  "rx_queue_depth": 3,
  "tx_queue_depth": 1,
  "framing_profile": "acia_8n1_default",
  "last_frame_seq": 9182,
  "last_timestamp_us": 1710000029200
}
```

ACIA frame stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "frames": [
    {
      "frame_seq": 9183,
      "direction": "host_to_st",
      "encoding": "8N1",
      "payload_hex": "0xF6",
      "start_bit": 0,
      "stop_bits": 1,
      "parity": "none",
      "timestamp_us": 1710000029203
    }
  ]
}
```
