# 6.2 Stop session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 16

## 6.2 Stop session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/stop`

Request:

```json
{
  "session_id": "ses_01H...",
  "reason": "user_stop"
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "stopped",
  "stopped_at_us": 1710000005123
}
```
