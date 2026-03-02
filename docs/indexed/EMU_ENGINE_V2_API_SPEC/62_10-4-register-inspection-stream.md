# 10.4 Register inspection stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 62

## 10.4 Register inspection stream

- `GET /api/v2/inspect/registers/stream`

Register snapshot schema contract:

- Register inspection stream events must validate against schema `register_snapshot_v1`.
- Snapshot events are emitted as `type=register_update` and carry one deterministic register observation per event.
- For a stream connection, events are ordered by `(tick, cycle)` ascending; if `(tick, cycle)` are equal, ordering source of truth is emission order.

`register_snapshot_v1` required fields:

- `type` (string, must equal `register_update`)
- `schema_version` (uint32, must equal `1`)
- `session_id` (string)
- `tick` (uint64)
- `cycle` (uint64)
- `component` (string)
- `register` (string)
- `old_value` (string)
- `new_value` (string)
- `value_encoding` (enum)
- `value_bits` (uint32, `>0`)

`value_encoding` enum values:

- `hex`
- `signed`
- `unsigned`

Selective filter fields (`subscribe` / `set_filter`):

- `components` (array of strings, optional): include only listed components.
- `registers` (array of strings, optional): exact register-name allowlist.
- `register_prefixes` (array of strings, optional): include registers whose names start with one of the prefixes.
- `changed_only` (bool, optional, default `true`): when `true`, events with `old_value == new_value` must be suppressed.
- `mode` (enum: `event`, `interval`) and `interval_us` (uint32, required when `mode=interval`, must be `>0`).

Filter and schema guard failures:

- Invalid filter payload shape, unsupported `mode`, or invalid `interval_us` values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Filter semantics that cannot be resolved by inspection backend (unknown component/register selectors) -> `INSPECT_FILTER_INVALID`.

Register snapshot stream publisher contract:

- Publisher pipeline order per candidate snapshot is fixed: `collect -> apply_filters -> schema_validate -> emit`.
- Publisher output event type is `register_update`; each emitted event must include publisher ordering fields `event_seq` (uint64) and `event_timestamp_us` (uint64).
- `event_seq` is strictly monotonic per stream connection and increments by exactly `+1` for each emitted register event.
- `event_timestamp_us` is monotonic non-decreasing per stream connection and must use the same runtime time base as section `4`.
- If `changed_only=true`, publisher must suppress events where `old_value == new_value`.

Register publisher validation checks:

- `REG-PUB-01`: `event_seq(next) == event_seq(prev) + 1`.
- `REG-PUB-02`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `REG-PUB-03`: emitted event matches active selector constraints (`components`, `registers`, `register_prefixes`).
- `REG-PUB-04`: when `changed_only=true`, all emitted events satisfy `old_value != new_value`.

Register publisher failure mapping:

- Runtime schema validation failure for an internally-produced register event -> `INTERNAL_ERROR` with fail-fast diagnostics.
- Validation-check failure (`REG-PUB-*`) -> `INTERNAL_ERROR` and degraded-delivery signaling via stream health/event-stream delivery semantics.
- Backpressure-induced drops must increment `stream_health.dropped_events` and preserve publisher sequencing invariants over emitted (non-dropped) events.

Client subscribe message:

```json
{
  "type": "subscribe",
  "components": ["cpu", "mfp", "shifter"],
  "registers": ["PC", "SR"],
  "register_prefixes": ["D", "A"],
  "changed_only": true,
  "mode": "event",
  "interval_us": 0
}
```

Server event:

```json
{
  "type": "register_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 8841,
  "event_timestamp_us": 1710001113331,
  "tick": 297716488,
  "cycle": 74429122,
  "component": "cpu",
  "register": "PC",
  "old_value": "0x00FC1234",
  "new_value": "0x00FC1236",
  "value_encoding": "hex",
  "value_bits": 32
}
```

Register publisher validation trace example:

```json
{
  "stream": "registers",
  "session_id": "ses_01H...",
  "checks": {
    "REG-PUB-01": "pass",
    "REG-PUB-02": "pass",
    "REG-PUB-03": "pass",
    "REG-PUB-04": "pass"
  },
  "events_emitted": 128,
  "events_suppressed_changed_only": 34,
  "last_event_seq": 8968,
  "last_event_timestamp_us": 1710001115560
}
```
