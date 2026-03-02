# 10.3 Audio stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 61

## 10.3 Audio stream

- `GET /api/v2/stream/audio`

Audio metadata channel contract:

- Audio stream metadata is emitted on a JSON sideband channel named `audio.metadata.v1`.
- Each metadata message must validate against schema `audio_chunk_meta_v1` before binary payload emission.
- Per stream connection, metadata messages are strictly ordered by `chunk_id` ascending with no duplicates.
- The binary audio payload that follows a metadata message must belong to the same `chunk_id` and `payload_bytes` declared by that metadata message.

`audio_chunk_meta_v1` required fields:

- `type` (string, must equal `audio_chunk_meta`)
- `schema_version` (uint32, must equal `1`)
- `channel` (string, must equal `audio.metadata.v1`)
- `session_id` (string)
- `chunk_id` (uint64)
- `timestamp_us` (uint64)
- `sample_rate` (uint32, `>0`)
- `channels` (uint32, `>0`)
- `format` (enum)
- `frames` (uint32, `>0`)
- `payload_bytes` (uint32, `>0`)

`format` enum values:

- `PCM_S16LE`
- `PCM_F32LE`

Metadata channel validation rules:

- `schema_version` values other than `1` must fail with `UNSUPPORTED_VERSION`.
- Unknown `format` values or non-positive sample/chunk dimensions (`sample_rate`, `channels`, `frames`, `payload_bytes`) must fail with `BAD_REQUEST`.
- If metadata cannot be paired to the immediately following binary payload (`chunk_id` mismatch or byte-length mismatch), stream delivery must fail-fast with `INTERNAL_ERROR`.

Audio payload stream emitter contract:

- Emitter output unit is an ordered pair: one `audio_chunk_meta` JSON message immediately followed by one binary payload chunk.
- For a given stream connection, payload emitter must preserve strict chunk order and emit exactly one payload per accepted metadata chunk.
- Binary payload byte length must equal metadata `payload_bytes`; mismatches are emitter integrity failures.
- Emitter must not block the core emulation loop; under pressure it must use bounded queue behavior and surface degradation through stream health/backpressure signaling (sections `10.7`, `10.8`, `13.2`, `13.3`).

Audio emitter sequencing checks:

- `AUD-EMIT-01`: `chunk_id(next) > chunk_id(prev)` for consecutive emitted metadata chunks.
- `AUD-EMIT-02`: each emitted metadata chunk is followed by exactly one binary payload before any next metadata chunk.
- `AUD-EMIT-03`: emitted binary payload byte length equals the associated metadata `payload_bytes`.

Audio pacing controls:

- Audio stream pacing is configured through stream control message `set_rate_limit` (section `10.7`) with audio-scoped payload fields.
- Audio pacing payload schema (`audio_pacing_v1`) fields:
  - `stream` (string, must equal `audio`)
  - `pacing_mode` (enum: `realtime`, `fixed_hz`)
  - `target_hz` (uint32, required when `pacing_mode=fixed_hz`, range `1..48000`)
  - `max_burst_chunks` (uint32, optional, range `1..16`, default `1`)
- `pacing_mode=realtime` uses runtime/profile audio cadence without fixed-hz throttling.
- `pacing_mode=fixed_hz` enforces ceiling pacing at `target_hz`; when emitter backlog persists, server must mark throttle/backpressure status via stream health.

Audio pacing guard failures:

- Invalid pacing payload shape or out-of-range values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- If sustained transport pressure prevents policy-compliant delivery, stream degradation must surface with stream category backpressure signaling (`STREAM_BACKPRESSURE`).

Metadata envelope:

```json
{
  "type": "audio_chunk_meta",
  "schema_version": 1,
  "channel": "audio.metadata.v1",
  "session_id": "ses_01H...",
  "chunk_id": 99218,
  "timestamp_us": 1710001112225,
  "sample_rate": 48000,
  "channels": 2,
  "format": "PCM_S16LE",
  "frames": 1024,
  "payload_bytes": 4096
}
```

Binary audio payload follows.

Audio pacing control message example (`set_rate_limit`):

```json
{
  "type": "set_rate_limit",
  "stream": "audio",
  "pacing_mode": "fixed_hz",
  "target_hz": 240,
  "max_burst_chunks": 4
}
```
