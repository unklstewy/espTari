# 6.11 Session performance metrics

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 32

## 6.11 Session performance metrics

- `GET /api/v2/metrics/performance?session_id=ses_01H...`

Performance SLO collector configuration contract:

- `POST /api/v2/metrics/performance/collectors/config`
- Request payload is validated by `slo_collector_config_v1`.
- `slo_collector_config_v1` required fields:
  - `session_id` (string, required)
  - `sampling_interval_ms` (uint32, required, range `100..10000`)
  - `window_ms` (uint32, required, range `1000..60000`)
  - `collectors` (object, required):
    - `input_latency_ms.enabled` (bool)
    - `jitter_ms.enabled` (bool)
    - `dropped_frame_percent.enabled` (bool)
  - `emit_history` (bool, optional, default `true`)

Performance sampling pipeline contract:

- `GET /api/v2/metrics/performance/samples?session_id=...&limit=...`
- Response `data.samples[]` entries are validated by `slo_sample_v1`.
- `slo_sample_v1` required fields:
  - `sample_seq` (uint64)
  - `window_start_us` (uint64)
  - `window_end_us` (uint64)
  - `input_latency_ms_p95` (number)
  - `jitter_ms_p95` (number)
  - `dropped_frame_percent` (number)
  - `collector_revision` (string)
  - `timestamp_us` (uint64)

SLO collector/pipeline conformance checks:

- `SLO-COL-01`: `sample_seq(next) == sample_seq(prev) + 1` for emitted sample stream.
- `SLO-COL-02`: `window_start_us(next) >= window_end_us(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `SLO-COL-03`: each emitted sample must be derived only from collectors with `enabled=true` in active `slo_collector_config_v1`.
- `SLO-COL-04`: sampling cadence must respect configured `sampling_interval_ms` with bounded jitter (`|actual_interval - configured_interval| <= 1 tick window`).

SLO collector/pipeline deterministic guard failures:

- Invalid payload shape, out-of-range interval/window, or invalid `limit` -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Collector update attempted when lifecycle is not `running`/`paused` -> `INVALID_SESSION_STATE`.
- Sampling pipeline backend unavailable -> `INTERNAL_ERROR`.

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "window_ms": 5000,
  "input_latency_ms": {"p50": 18, "p95": 41, "max": 49, "target_max": 50, "status": "ok"},
  "jitter_ms": {"p50": 7, "p95": 21, "max": 28, "target_max": 30, "status": "ok"},
  "dropped_frame_percent": {"value": 0.4, "target_max": 1.0, "status": "ok"}
}
```

SLO collector configuration request example:

```json
{
  "session_id": "ses_01H...",
  "sampling_interval_ms": 500,
  "window_ms": 5000,
  "collectors": {
    "input_latency_ms": {"enabled": true},
    "jitter_ms": {"enabled": true},
    "dropped_frame_percent": {"enabled": true}
  },
  "emit_history": true
}
```

SLO collector configuration response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "collector_revision": "slo_col_rev_04",
  "sampling_interval_ms": 500,
  "window_ms": 5000,
  "state": "active",
  "applied_at_us": 1710000006200
}
```

SLO sample stream response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "samples": [
    {
      "sample_seq": 120,
      "window_start_us": 1710000001000,
      "window_end_us": 1710000006000,
      "input_latency_ms_p95": 41,
      "jitter_ms_p95": 21,
      "dropped_frame_percent": 0.4,
      "collector_revision": "slo_col_rev_04",
      "timestamp_us": 1710000006001
    }
  ]
}
```

SLO threshold endpoint contract:

- `GET /api/v2/metrics/performance/thresholds?session_id=...`
- Response `data` must satisfy `slo_thresholds_v1`.
- `slo_thresholds_v1` required fields:
  - `session_id` (string)
  - `thresholds` (object):
    - `input_latency_ms_p95_max` (number)
    - `jitter_ms_p95_max` (number)
    - `dropped_frame_percent_max` (number)
  - `evaluation_window_ms` (uint32)
  - `active_revision` (string)

SLO breach alarm event contract:

- `GET /api/v2/metrics/performance/alarms?session_id=...&limit=...`
- Alarm events are validated by `slo_breach_alarm_event_v1`.
- `slo_breach_alarm_event_v1` required fields:
  - `alarm_seq` (uint64)
  - `metric` (enum: `input_latency_ms_p95`, `jitter_ms_p95`, `dropped_frame_percent`)
  - `threshold` (number)
  - `observed` (number)
  - `severity` (enum: `warning`, `critical`)
  - `state` (enum: `breached`, `recovered`)
  - `window_start_us` (uint64)
  - `window_end_us` (uint64)
  - `timestamp_us` (uint64)

SLO endpoint/alarm conformance checks:

- `SLO-ALRM-01`: `alarm_seq(next) == alarm_seq(prev) + 1` for alarm stream.
- `SLO-ALRM-02`: breach event emitted when `observed > threshold`; recovery event emitted when `observed <= threshold` for same metric.
- `SLO-ALRM-03`: `window_start_us(next) >= window_start_us(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `SLO-ALRM-04`: alarm severity deterministically maps to breach ratio (`critical` when `observed >= threshold * 1.2`, else `warning`).

SLO endpoint/alarm deterministic guard failures:

- Invalid query params (`session_id`, `limit`) or malformed threshold payloads -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Alarm stream requested when collector pipeline is not active -> `INVALID_SESSION_STATE`.
- Alarm evaluation backend unavailable -> `INTERNAL_ERROR`.

SLO thresholds response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "thresholds": {
    "input_latency_ms_p95_max": 50,
    "jitter_ms_p95_max": 30,
    "dropped_frame_percent_max": 1.0
  },
  "evaluation_window_ms": 5000,
  "active_revision": "slo_thr_rev_02"
}
```

SLO alarm stream response (`data`) example:

```json
{
  "session_id": "ses_01H...",
  "alarms": [
    {
      "alarm_seq": 11,
      "metric": "jitter_ms_p95",
      "threshold": 30,
      "observed": 37,
      "severity": "critical",
      "state": "breached",
      "window_start_us": 1710000006000,
      "window_end_us": 1710000011000,
      "timestamp_us": 1710000011001
    },
    {
      "alarm_seq": 12,
      "metric": "jitter_ms_p95",
      "threshold": 30,
      "observed": 24,
      "severity": "warning",
      "state": "recovered",
      "window_start_us": 1710000011000,
      "window_end_us": 1710000016000,
      "timestamp_us": 1710000016001
    }
  ]
}
```

---
