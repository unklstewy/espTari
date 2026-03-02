# 6.10 Step debug execution

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 28

## 6.10 Step debug execution

- `POST /api/v2/debug/clock/step`

Request:

```json
{
  "session_id": "ses_01H...",
  "steps": 1,
  "capture": ["opcode", "bus_error", "register_delta"]
}
```

Single-step request contract:

- `steps` is required and must be an integer in `[1, 1024]`.
- `capture` is optional; allowed values are `opcode`, `bus_error`, `register_delta`.
- Single-step requests are accepted only when active `run_mode=single_step`.
