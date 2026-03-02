# 10.5 Bus trace stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 63

## 10.5 Bus trace stream

- `GET /api/v2/inspect/bus/stream`

Bus filter request model:

- Bus stream filter requests (`subscribe` and `set_filter`) must validate against schema `bus_filter_v1`.
- Filter updates apply atomically per stream connection; partial application is not allowed.

`bus_filter_v1` fields:

- `type` (string, must be `subscribe` or `set_filter`)
- `address_ranges` (array of strings, optional, `0x<start>-0x<end>` inclusive; `start<=end`)
- `access_types` (array of enum, optional)
- `components` (array of strings, optional)
- `level` (enum, optional)
- `max_events_per_sec` (uint32, optional, `>0`)

`access_types` enum values:

- `read`
- `write`
- `iack`
- `dma`

`level` enum values:

- `info`
- `debug`

Bus filter guard rules:

- Invalid filter payload shape, malformed/overlapping ranges, unknown enum values, or non-positive `max_events_per_sec` -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Semantically unresolved selectors (for example unknown component source) -> `INSPECT_FILTER_INVALID`.

Filtered bus stream publisher contract:

- Publisher pipeline order per candidate bus event is fixed: `capture -> apply_filter -> validate -> emit`.
- A bus event may be emitted only when it satisfies all active selectors (`address_ranges`, `access_types`, `components`, `level`).
- Emitted bus events must include `event_seq` (uint64) and `event_timestamp_us` (uint64) for deterministic ordering.
- For one stream connection, `event_seq` is strictly monotonic and increases by `+1` for each emitted bus event.
- `event_timestamp_us` is monotonic non-decreasing and uses the same runtime time base as section `4`.

Filtered bus publisher checks:

- `BUS-FLT-01`: every emitted event matches active selector set.
- `BUS-FLT-02`: `event_seq(next) == event_seq(prev) + 1` for emitted events.
- `BUS-FLT-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `BUS-FLT-04`: events rejected by filters must not be emitted.

Filtered bus publisher failures:

- Validation check failure (`BUS-FLT-*`) -> `INTERNAL_ERROR` with deterministic diagnostics.
- Under sustained load, publisher must preserve sequence invariants over emitted events while surfacing degraded delivery via stream-health/event-stream delivery signaling.

Client filter:

```json
{
  "type": "subscribe",
  "address_ranges": ["0x00FF8000-0x00FF82FF"],
  "access_types": ["read", "write", "iack", "dma"],
  "components": ["cpu", "dma", "shifter"],
  "level": "debug"
}
```

Server event:

```json
{
  "type": "bus_event",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 44102,
  "event_timestamp_us": 1710001117740,
  "tick": 297716490,
  "cycle": 74429123,
  "source": "cpu",
  "access": "write",
  "address": "0x00FF820A",
  "data": "0x0003",
  "size": 2,
  "wait_states": 1
}
```
