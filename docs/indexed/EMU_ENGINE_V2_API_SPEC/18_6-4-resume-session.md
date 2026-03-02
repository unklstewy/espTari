# 6.4 Resume session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 18

## 6.4 Resume session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/resume`

Request:

```json
{
  "session_id": "ses_01H...",
  "resume_mode": "running"
}
```

`resume_mode` values:

- `running` (default)
- `paused`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "resumed_at_us": 1710000005077
}
```
