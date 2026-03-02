# 6.9 Set debug clock mode

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 25

## 6.9 Set debug clock mode

- `POST /api/v2/debug/clock/mode`

Request:

```json
{
  "session_id": "ses_01H...",
  "mode": "slow_motion",
  "ratio": 0.25
}
```

`mode` values:

- `realtime`
- `slow_motion`
- `single_step`

Rules:

- `ratio` is required only for `slow_motion` and must be greater than 0 and less than or equal to 1.
- Invalid mode/ratio combinations return `DEBUG_CLOCK_INVALID`.
