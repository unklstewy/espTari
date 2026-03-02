# 10.8 Engine status/health event stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 66

## 10.8 Engine status/health event stream

- `GET /api/v2/engine/stream`

Event names:

- `engine_status_update`
- `engine_health_update`
- `engine_stream_delivery`
- `stream_backpressure_telemetry`

Payload contract notes:

- `engine_status_update` carries a `status` object that must follow the canonical status payload defined in section `6.6B`.
- `engine_health_update` carries a `health` object that must follow the canonical health payload defined in section `6.6A`.
- `engine_stream_delivery` communicates stream-level degraded delivery/error signaling and backpressure disclosure.

Ordering and sequence rules:

- Every engine stream event must include `event_seq` (uint64) and `event_timestamp_us` (uint64) from the same monotonic runtime time base as section `4`.
- `event_seq` must be strictly monotonic per stream connection and increase by `1` for each emitted event.
- `event_timestamp_us` must be monotonic non-decreasing for a given stream connection.
- Client ordering source of truth is `event_seq`; `event_timestamp_us` is informational and must not be used to reorder events.
- `engine_status_update.status.snapshot_at_us` and `engine_health_update.health.generated_at_us` must be less than or equal to `event_timestamp_us`.

Backpressure/skip disclosure requirements:

- Every `engine_status_update` and `engine_health_update` event must include a `delivery` object.
- `delivery.dropped_events_since_last` reports count of omitted events since the previous delivered event on the same connection.
- `delivery.coalesced_updates` reports how many intermediate updates were merged into this payload (`0` means none).
- `delivery.degraded` is `true` whenever `dropped_events_since_last > 0`, `coalesced_updates > 0`, or stream throttle is active.
- `delivery.reason` enum: `none`, `queue_overflow`, `rate_limited`, `producer_lag`, `transport_backpressure`.

Stream error and degraded-delivery signaling:

- Server must emit `engine_stream_delivery` whenever `delivery.degraded=true` persists or transitions state.
- `engine_stream_delivery.severity` enum: `info`, `warning`, `error`.
- `engine_stream_delivery.state` enum: `normal`, `degraded`, `recovering`.
- If the stream cannot continue, server must emit `engine_stream_delivery` with `state="degraded"`, `severity="error"`, and non-empty `error_code` before close when possible.

Backpressure telemetry event-stream exposure:

- Server must emit `stream_backpressure_telemetry` whenever backpressure counters change or throttle state transitions.
- `stream_backpressure_telemetry` required fields:
  - `type` (string, must equal `stream_backpressure_telemetry`)
  - `schema_version` (uint32)
  - `session_id` (string)
  - `event_seq` (uint64)
  - `event_timestamp_us` (uint64)
  - `stream` (enum: `video`, `audio`, `engine`, `registers`, `bus`, `memory`)
  - `metrics` (object with the same counter/watermark fields as section `10.7`)
- Event ordering rules:
  - `event_seq` is strictly monotonic by `+1` per engine stream connection.
  - `event_timestamp_us` is monotonic non-decreasing.
  - Metric payload must satisfy `BP-CTR-01..04` invariants at emission time.

Normal delivery example (`engine_status_update`):

```json
{
  "type": "engine_status_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 1042,
  "event_timestamp_us": 1710000010450,
  "status": {
    "session_id": "ses_01H...",
    "machine": "atari_st",
    "profile": "st_520_pal",
    "lifecycle_state": "running",
    "run_mode": "realtime",
    "snapshot_at_us": 1710000010448,
    "uptime_ms": 42503,
    "cycle_counter": 112552001,
    "tick_counter": 450208004,
    "loaded_modules": [],
    "runtime": {
      "scheduler_hz": 8000000,
      "stream_health": {
        "video": {"connected_clients": 1, "dropped_packets": 0},
        "audio": {"connected_clients": 1, "dropped_packets": 0}
      },
      "last_transition_at_us": 1710000009000,
      "last_error": null
    }
  },
  "delivery": {
    "degraded": false,
    "reason": "none",
    "dropped_events_since_last": 0,
    "coalesced_updates": 0,
    "throttle_active": false
  }
}
```

Degraded delivery example (`engine_health_update`):

```json
{
  "type": "engine_health_update",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2051,
  "event_timestamp_us": 1710000011200,
  "health": {
    "session_id": "ses_01H...",
    "overall_status": "degraded",
    "overall_severity": "warning",
    "generated_at_us": 1710000011194,
    "freshness_ttl_ms": 1000,
    "freshness_state": "fresh",
    "components": [
      {
        "component": "video",
        "status": "unavailable",
        "severity": "error",
        "observed_at_us": 1710000011193,
        "last_ok_at_us": 1710000010800,
        "reason": "video stream backend not initialized",
        "error_code": "ENGINE_SUBSYSTEM_UNAVAILABLE"
      }
    ]
  },
  "delivery": {
    "degraded": true,
    "reason": "queue_overflow",
    "dropped_events_since_last": 3,
    "coalesced_updates": 2,
    "throttle_active": true
  }
}
```

Degraded-delivery signal example (`engine_stream_delivery`):

```json
{
  "type": "engine_stream_delivery",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2052,
  "event_timestamp_us": 1710000011201,
  "state": "degraded",
  "severity": "warning",
  "error_code": "STREAM_BACKPRESSURE_ACTIVE",
  "message": "Engine stream is coalescing updates due to queue overflow",
  "metrics": {
    "queue_depth": 128,
    "queue_capacity": 128,
    "dropped_events_since_last": 3,
    "coalesced_updates": 2,
    "high_watermark_depth": 128,
    "high_watermark_ratio": 1.0,
    "overflow_events_total": 9,
    "throttle_transitions_total": 15
  }
}
```

Backpressure telemetry event example (`stream_backpressure_telemetry`):

```json
{
  "type": "stream_backpressure_telemetry",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "event_seq": 2053,
  "event_timestamp_us": 1710000011202,
  "stream": "video",
  "metrics": {
    "queue_depth": 96,
    "queue_capacity": 128,
    "dropped_events": 12,
    "dropped_events_since_last": 1,
    "throttle_active": true,
    "high_watermark_depth": 128,
    "high_watermark_ratio": 1.0,
    "overflow_events_total": 9,
    "throttle_transitions_total": 15,
    "sample_timestamp_us": 1710000011201
  }
}
```

---
