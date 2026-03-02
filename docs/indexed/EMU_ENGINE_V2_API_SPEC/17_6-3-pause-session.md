# 6.3 Pause session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 17

## 6.3 Pause session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/pause`

Request:

```json
{
  "session_id": "ses_01H...",
  "reason": "user_pause"
}
```

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "paused",
  "paused_at_us": 1710000004988
}
```
