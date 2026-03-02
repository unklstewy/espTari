# 10.1 Common stream handshake

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 59

## 10.1 Common stream handshake

Each WebSocket stream supports optional query params:

- `session_id`
- `schema_version` (default `1`)
- channel-specific filter params

Server sends first message:

```json
{
  "type": "hello",
  "schema_version": 1,
  "session_id": "ses_01H...",
  "stream": "video"
}
```
