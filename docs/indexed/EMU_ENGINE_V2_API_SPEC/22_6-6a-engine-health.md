# 6.6A Engine health

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 22

## 6.6A Engine health

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/health?session_id=ses_01H...&components=cpu,memory,video`

Required `data` fields:

- `session_id` (string)
- `overall_status` (enum)
- `overall_severity` (enum)
- `generated_at_us` (uint64)
- `freshness_ttl_ms` (uint32)
- `freshness_state` (enum)
- `components` (array)

Component fields:

- `component` (string)
- `status` (enum)
- `severity` (enum)
- `observed_at_us` (uint64)
- `last_ok_at_us` (uint64 or `null`)
- `reason` (string, optional)
- `error_code` (string, optional)

Enums:

- `overall_status` and component `status`: `healthy`, `degraded`, `unavailable`
- `overall_severity` and component `severity`: `info`, `warning`, `error`, `critical`
- `freshness_state`: `fresh`, `stale`

Timestamp and freshness semantics:

- `generated_at_us` and all component timestamps are microseconds from the same monotonic runtime time base used for `timestamp_us` in section `4`.
- `freshness_state=fresh` iff `(timestamp_us - generated_at_us) <= freshness_ttl_ms * 1000`.
- `freshness_state=stale` iff `(timestamp_us - generated_at_us) > freshness_ttl_ms * 1000`.

Unavailable-subsystem error handling:

- If one or more requested subsystems are unavailable and `fail_on_unavailable=false` (default), response remains `ok=true` and each unavailable component must set:
  - `status="unavailable"`
  - `severity="error"` or `severity="critical"`
  - `error_code="ENGINE_SUBSYSTEM_UNAVAILABLE"`
- If `fail_on_unavailable=true` and any requested subsystem is unavailable, response must be `ok=false` with `error.code="ENGINE_SUBSYSTEM_UNAVAILABLE"`.

Response `data` (success example):

```json
{
  "session_id": "ses_01H...",
  "overall_status": "degraded",
  "overall_severity": "warning",
  "generated_at_us": 1710000006123,
  "freshness_ttl_ms": 1000,
  "freshness_state": "fresh",
  "components": [
    {
      "component": "cpu",
      "status": "healthy",
      "severity": "info",
      "observed_at_us": 1710000006118,
      "last_ok_at_us": 1710000006118
    },
    {
      "component": "video",
      "status": "unavailable",
      "severity": "error",
      "observed_at_us": 1710000006119,
      "last_ok_at_us": 1710000005900,
      "reason": "video stream backend not initialized",
      "error_code": "ENGINE_SUBSYSTEM_UNAVAILABLE"
    }
  ]
}
```

Error example (`fail_on_unavailable=true`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000006124,
  "error": {
    "code": "ENGINE_SUBSYSTEM_UNAVAILABLE",
    "category": "engine",
    "message": "Requested subsystem video is unavailable",
    "retryable": true,
    "details": {
      "session_id": "ses_01H...",
      "component": "video"
    }
  }
}
```
