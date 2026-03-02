# 11.3D IKBD parser bridge and keyboard/mouse packet timing path

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 74

## 11.3D IKBD parser bridge and keyboard/mouse packet timing path

IKBD parser bridge contract:

- `GET /api/v2/inspect/chipset/ikbd/bridge?session_id=...`
- Response `data` must expose parser bridge state validated by `ikbd_bridge_state_v1`.
- `ikbd_bridge_state_v1` required fields:
  - `session_id` (string)
  - `bridge_state` (enum: `detached`, `attached`, `error`)
  - `parser_mode` (enum: `atari_st_ikbd`)
  - `acia_channel_mode` (enum: `rx`, `tx`, `duplex`)
  - `rx_queue_depth` (uint32)
  - `tx_queue_depth` (uint32)
  - `last_packet_seq` (uint64)
  - `last_timestamp_us` (uint64)

IKBD keyboard/mouse packet timing contract:

- `GET /api/v2/inspect/chipset/ikbd/packets?session_id=...&limit=...&packet_type=...`
- Packet entries are validated by `ikbd_packet_timing_v1` and emitted in sequence order.
- `ikbd_packet_timing_v1` required fields:
  - `packet_seq` (uint64)
  - `packet_type` (enum: `keyboard_scancode`, `mouse_packet`)
  - `direction` (enum: `host_to_st`, `st_to_host`)
  - `payload_hex` (string)
  - `acia_frame_seq` (uint64)
  - `inter_packet_gap_us` (uint64)
  - `timestamp_us` (uint64)

IKBD packet timing conformance checks:

- `IKBD-PKT-01`: `packet_seq(next) == packet_seq(prev) + 1` for each `packet_type` stream.
- `IKBD-PKT-02`: `timestamp_us(next) >= timestamp_us(prev)` for each `packet_type` stream.
- `IKBD-PKT-03`: `inter_packet_gap_us == timestamp_us(curr) - timestamp_us(prev)` for adjacent packets in the same `packet_type` stream.
- `IKBD-PKT-04`: `acia_frame_seq` for each emitted IKBD packet must resolve to an existing ACIA frame and preserve bridge order.

IKBD bridge/timing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`, `packet_type`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Parser bridge unavailable or ACIA frame linkage unresolved -> `INTERNAL_ERROR`.

IKBD parser bridge state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "bridge_state": "attached",
  "parser_mode": "atari_st_ikbd",
  "acia_channel_mode": "duplex",
  "rx_queue_depth": 2,
  "tx_queue_depth": 1,
  "last_packet_seq": 12411,
  "last_timestamp_us": 1710000030102
}
```

IKBD packet timing stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "packets": [
    {
      "packet_seq": 12412,
      "packet_type": "keyboard_scancode",
      "direction": "host_to_st",
      "payload_hex": "0x1C",
      "acia_frame_seq": 9184,
      "inter_packet_gap_us": 170,
      "timestamp_us": 1710000030272
    },
    {
      "packet_seq": 12413,
      "packet_type": "mouse_packet",
      "direction": "st_to_host",
      "payload_hex": "0xF80801",
      "acia_frame_seq": 9185,
      "inter_packet_gap_us": 244,
      "timestamp_us": 1710000030516
    }
  ]
}
```
