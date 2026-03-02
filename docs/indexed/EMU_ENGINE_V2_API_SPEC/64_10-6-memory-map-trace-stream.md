# 10.6 Memory map trace stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 64

## 10.6 Memory map trace stream

- `GET /api/v2/inspect/memory/stream`

Memory filter request model:

- Memory stream filter requests (`subscribe` and `set_filter`) must validate against schema `memory_filter_v1`.
- Filter updates apply atomically per stream connection; partial application is not allowed.

`memory_filter_v1` fields:

- `type` (string, must be `subscribe` or `set_filter`)
- `regions` (array of strings, optional)
- `address_ranges` (array of strings, optional, `0x<start>-0x<end>` inclusive; `start<=end`)
- `access_types` (array of enum, optional)
- `components` (array of strings, optional)
- `mapped_targets` (array of strings, optional)
- `level` (enum, optional)
- `max_events_per_sec` (uint32, optional, `>0`)

`access_types` enum values:

- `read`
- `write`

`level` enum values:

- `info`
- `debug`

Memory filter guard rules:

- Invalid filter payload shape, malformed ranges, unknown enum values, or non-positive `max_events_per_sec` -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- Semantically unresolved selectors (for example unknown region or mapped target selector) -> `INSPECT_FILTER_INVALID`.

Filtered memory stream publisher contract:

- Publisher pipeline order per candidate memory-map event is fixed: `capture -> apply_filter -> validate -> emit`.
- A memory event may be emitted only when it satisfies all active selectors (`regions`, `address_ranges`, `access_types`, `components`, `mapped_targets`, `level`).
- Emitted memory events must include `event_seq` (uint64) and `event_timestamp_us` (uint64) for deterministic ordering.
- For one stream connection, `event_seq` is strictly monotonic and increases by `+1` for each emitted memory event.
- `event_timestamp_us` is monotonic non-decreasing and uses the same runtime time base as section `4`.

Filtered memory publisher checks:

- `MEM-FLT-01`: every emitted event matches active selector set.
- `MEM-FLT-02`: `event_seq(next) == event_seq(prev) + 1` for emitted events.
- `MEM-FLT-03`: `event_timestamp_us(next) >= event_timestamp_us(prev)`.
- `MEM-FLT-04`: events rejected by filters must not be emitted.

Filtered memory publisher failures:

- Validation check failure (`MEM-FLT-*`) -> `INTERNAL_ERROR` with deterministic diagnostics.
- Under sustained load, publisher must preserve sequence invariants over emitted events while surfacing degraded delivery via stream-health/event-stream delivery signaling.

Client filter:

```json
{
  "type": "subscribe",
  "regions": ["io.video", "chip_ram"],
  "address_ranges": ["0x00FF8000-0x00FF82FF"],
  "access_types": ["read", "write"],
  "components": ["cpu", "blitter"],
  "mapped_targets": ["shifter.mode_register"],
  "level": "debug"
}
```

Server event:

```json
{
  "type": "memory_event",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 52210,
  "event_timestamp_us": 1710001117742,
  "tick": 297716491,
  "cycle": 74429123,
  "region": "io.video",
  "address": "0x00FF820A",
  "access": "write",
  "component": "cpu",
  "mapped_target": "shifter.mode_register"
}
```

Filtered stream load validation run example:

```json
{
  "run_id": "load_01H...",
  "session_id": "ses_01H...",
  "duration_s": 60,
  "bus_stream": {
    "filters_applied": true,
    "events_emitted": 14220,
    "events_rejected": 98740,
    "checks": {
      "BUS-FLT-01": "pass",
      "BUS-FLT-02": "pass",
      "BUS-FLT-03": "pass",
      "BUS-FLT-04": "pass"
    }
  },
  "memory_stream": {
    "filters_applied": true,
    "events_emitted": 9310,
    "events_rejected": 120443,
    "checks": {
      "MEM-FLT-01": "pass",
      "MEM-FLT-02": "pass",
      "MEM-FLT-03": "pass",
      "MEM-FLT-04": "pass"
    }
  },
  "result": "pass"
}
```
