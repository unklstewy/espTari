# 6.6B Engine status

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 21

## 6.6B Engine status

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/status?session_id=ses_01H...`

Required `data` fields:

- `session_id` (string)
- `machine` (string)
- `profile` (string)
- `lifecycle_state` (enum: `stopped`, `starting`, `running`, `paused`, `suspended`, `faulted`, `stopping`)
- `run_mode` (enum: `realtime`, `slow_motion`, `single_step`)
- `snapshot_at_us` (uint64)
- `uptime_ms` (uint64)
- `cycle_counter` (uint64)
- `tick_counter` (uint64)
- `loaded_modules` (array)
- `runtime` (object)

Required `runtime` fields:

- `scheduler_hz` (uint32)
- `stream_health` (object)
- `last_transition_at_us` (uint64)
- `last_error` (object or `null`)

Lifecycle-mode semantics:

- `running`: `uptime_ms > 0`, counters increase between snapshots, `last_error` is `null` unless non-fatal fault recorded.
- `paused`: counters are stable between snapshots, `run_mode` remains unchanged, `last_transition_at_us` is pause timestamp.
- `suspended`: counters are stable, `runtime.stream_health` may report disconnected outputs, `last_transition_at_us` is suspend timestamp.
- `faulted`: counters may stop, `last_error` must be non-null with `code`, `message`, and `at_us`.
- `starting` and `stopping`: counters may be transient; `lifecycle_state` transitions must follow section `12`.
- `stopped`: `uptime_ms = 0`, counters may reset to baseline for the next start sequence.

Consistency with lifecycle API rules:

- `lifecycle_state` must always be one of the states defined in section `12`.
- Returned state must be reachable through allowed transitions in section `12`.
- Unknown or inactive session identifiers must return canonical engine errors (`ENGINE_NOT_RUNNING` or `NOT_FOUND`) and must not return synthetic lifecycle states.

Response `data` (success example):

```json
{
  "session_id": "ses_01H...",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "lifecycle_state": "running",
  "run_mode": "realtime",
  "snapshot_at_us": 1710000007123,
  "uptime_ms": 39284,
  "cycle_counter": 102442933,
  "tick_counter": 409771732,
  "loaded_modules": [
    {"component": "cpu", "module_id": "st.cpu.m68k", "version": "2.0.0"}
  ],
  "runtime": {
    "scheduler_hz": 8000000,
    "stream_health": {
      "video": {"connected_clients": 1, "dropped_packets": 0},
      "audio": {"connected_clients": 1, "dropped_packets": 0}
    },
    "last_transition_at_us": 1710000004000,
    "last_error": null
  }
}
```

Error example (inactive session):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000007124,
  "error": {
    "code": "ENGINE_NOT_RUNNING",
    "category": "engine",
    "message": "No active session for session_id ses_01H...",
    "retryable": true,
    "details": {
      "session_id": "ses_01H..."
    }
  }
}
```
