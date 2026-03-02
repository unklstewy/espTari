# 10.7 Stream control and health

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 65

## 10.7 Stream control and health

All streams support control messages:

- `pause_stream`
- `resume_stream`
- `set_filter`
- `set_rate_limit`

Backpressure counters and watermark metrics contract:

- Stream health telemetry must expose deterministic backpressure counters and queue watermark metrics per stream connection.
- Required `stream_health` fields:
  - `queue_depth` (uint32)
  - `queue_capacity` (uint32, `>0`)
  - `dropped_events` (uint64, cumulative)
  - `dropped_events_since_last` (uint32)
  - `throttle_active` (bool)
  - `high_watermark_depth` (uint32)
  - `high_watermark_ratio` (number, `0..1`)
  - `overflow_events_total` (uint64)
  - `throttle_transitions_total` (uint64)
  - `sample_timestamp_us` (uint64)
- Counter/metric invariants:
  - `queue_depth <= queue_capacity`
  - `high_watermark_depth >= queue_depth` and `high_watermark_depth <= queue_capacity`
  - `high_watermark_ratio = high_watermark_depth / queue_capacity`
  - cumulative counters (`dropped_events`, `overflow_events_total`, `throttle_transitions_total`) are monotonic non-decreasing for a stream connection.

Backpressure counter checks:

- `BP-CTR-01`: cumulative counters are monotonic non-decreasing.
- `BP-CTR-02`: `0 <= queue_depth <= queue_capacity`.
- `BP-CTR-03`: `high_watermark_depth`/`high_watermark_ratio` remain consistent with `queue_capacity`.
- `BP-CTR-04`: entering or leaving throttle state increments `throttle_transitions_total` exactly once per transition.

Health message:

```json
{
  "type": "stream_health",
  "queue_depth": 42,
  "queue_capacity": 256,
  "dropped_events": 3,
  "dropped_events_since_last": 1,
  "throttle_active": true,
  "high_watermark_depth": 192,
  "high_watermark_ratio": 0.75,
  "overflow_events_total": 1,
  "throttle_transitions_total": 4,
  "sample_timestamp_us": 1710000011300
}
```

Backpressure telemetry API snapshot:

- `GET /api/v2/stream/telemetry/backpressure?session_id=ses_01H...&stream=video`
- Query params:
  - `session_id` (required)
  - `stream` (required, enum: `video`, `audio`, `engine`, `registers`, `bus`, `memory`)
- Response `data` fields:
  - `session_id` (string)
  - `stream` (string)
  - `sample_timestamp_us` (uint64)
  - `queue_depth` (uint32)
  - `queue_capacity` (uint32)
  - `high_watermark_depth` (uint32)
  - `high_watermark_ratio` (number)
  - `dropped_events` (uint64)
  - `dropped_events_since_last` (uint32)
  - `overflow_events_total` (uint64)
  - `throttle_active` (bool)
  - `throttle_transitions_total` (uint64)
- Snapshot guard failures:
  - Missing/invalid query params -> `BAD_REQUEST`.
  - Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
  - Unknown stream selector -> `INSPECT_FILTER_INVALID`.

Backpressure telemetry API example (response `data`):

```json
{
  "session_id": "ses_01H...",
  "stream": "video",
  "sample_timestamp_us": 1710000011350,
  "queue_depth": 31,
  "queue_capacity": 256,
  "high_watermark_depth": 192,
  "high_watermark_ratio": 0.75,
  "dropped_events": 4,
  "dropped_events_since_last": 1,
  "overflow_events_total": 1,
  "throttle_active": true,
  "throttle_transitions_total": 4
}
```
