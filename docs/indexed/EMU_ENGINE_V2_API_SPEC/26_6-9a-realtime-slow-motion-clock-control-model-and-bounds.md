# 6.9A Realtime/slow-motion clock-control model and bounds

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 26

## 6.9A Realtime/slow-motion clock-control model and bounds

Clock-control model:

- `realtime` is the baseline pacing mode and always runs with `effective_ratio=1.0`.
- `slow_motion` runs the same deterministic tick/cycle execution path as `realtime` and changes only wall-clock pacing via `ratio`.
- Clock pacing must not change component ordering, arbitration hook order, or deterministic scheduler invariants from section `6.10A`.

Deterministic bounds contract:

- `CLOCK-BOUND-01`: accepted `slow_motion.ratio` is in `(0, 1]`.
- `CLOCK-BOUND-02`: `realtime` requests must not include `ratio`; if supplied, request is rejected as `DEBUG_CLOCK_INVALID`.
- `CLOCK-BOUND-03`: accepted mode change commits atomically and records `last_transition_at_us` from canonical runtime clock.
- `CLOCK-BOUND-04`: mode-change response must include normalized `effective_ratio` (`1.0` for `realtime`, requested `ratio` for `slow_motion`).

Mode-change response excerpt (`slow_motion`, `ratio=0.25`):

```json
{
  "session_id": "ses_01H...",
  "mode": "slow_motion",
  "effective_ratio": 0.25,
  "clock_bounds": {
    "ratio_min_exclusive": 0.0,
    "ratio_max_inclusive": 1.0,
    "realtime_effective_ratio": 1.0
  },
  "last_transition_at_us": 1710000008124
}
```

Realtime invalid-ratio error example:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000008128,
  "error": {
    "code": "DEBUG_CLOCK_INVALID",
    "category": "debug",
    "message": "ratio is only allowed for slow_motion mode",
    "retryable": false,
    "details": {
      "session_id": "ses_01H...",
      "endpoint": "/api/v2/debug/clock/mode",
      "guard_id": "CLOCK-BOUND-02"
    }
  }
}
```
