# 6.6 Session state

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 20

## 6.6 Session state

Envelope binding:

- Success uses the canonical envelope in section `4` with this section defining only `data`.
- Errors use the canonical error envelope and taxonomy in sections `4` and `5`.

- `GET /api/v2/engine/session?session_id=ses_01H...`

Response `data`:

```json
{
  "session_id": "ses_01H...",
  "state": "running",
  "machine": "atari_st",
  "profile": "st_520_pal",
  "uptime_ms": 28342,
  "cycle_counter": 74429122,
  "tick_counter": 297716488,
  "loaded_modules": [],
  "stream_health": {
    "video": {"connected_clients": 1, "dropped_packets": 0},
    "audio": {"connected_clients": 1, "dropped_packets": 0}
  }
}
```
