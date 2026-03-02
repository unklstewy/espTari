# 6.5 Reset session

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 19

## 6.5 Reset session

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `POST /api/v2/engine/session/reset`

Request optional mode:

```json
{
  "session_id": "ses_01H...",
  "mode": "warm",
  "preserve_media": true
}
```

`mode` values:

- `warm`
- `cold`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "reset_mode": "warm",
  "reset_at_us": 1710000005220
}
```
