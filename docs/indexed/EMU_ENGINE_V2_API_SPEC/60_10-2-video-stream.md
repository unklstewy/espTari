# 10.2 Video stream

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 60

## 10.2 Video stream

- `GET /api/v2/stream/video`

Modes:

- `mode=frame` (default)
- `mode=debug_scanline`

Video metadata channel contract:

- Video stream metadata is emitted on a JSON sideband channel named `video.metadata.v1`.
- Each metadata message must validate against schema `video_frame_meta_v1` before binary payload emission.
- Per stream connection, metadata messages are strictly ordered by `frame_id` ascending with no duplicates.
- The binary frame payload that follows a metadata message must belong to the same `frame_id` and `payload_bytes` declared by that metadata message.

`video_frame_meta_v1` required fields:

- `type` (string, must equal `video_frame_meta`)
- `schema_version` (uint32, must equal `1`)
- `channel` (string, must equal `video.metadata.v1`)
- `session_id` (string)
- `frame_id` (uint64)
- `timestamp_us` (uint64)
- `width` (uint32, `>0`)
- `height` (uint32, `>0`)
- `pixel_format` (enum)
- `payload_bytes` (uint32, `>0`)

`pixel_format` enum values:

- `RGB565`
- `XRGB8888`
- `RGB888`

Metadata channel validation rules:

- `schema_version` values other than `1` must fail with `UNSUPPORTED_VERSION`.
- Unknown `pixel_format` values or non-positive dimensions/payload sizes must fail with `BAD_REQUEST`.
- If metadata cannot be paired to the immediately following binary payload (`frame_id` mismatch or byte-length mismatch), stream delivery must fail-fast with `INTERNAL_ERROR`.

Video payload stream emitter contract:

- Emitter output unit is an ordered pair: one `video_frame_meta` JSON message immediately followed by one binary payload frame.
- For a given stream connection, payload emitter must preserve strict frame order and emit exactly one payload per accepted metadata frame.
- Binary payload byte length must equal metadata `payload_bytes`; mismatches are emitter integrity failures.
- Emitter must not block the core emulation loop; under pressure it must use bounded queue behavior and surface degradation through stream health/backpressure signaling (sections `10.7`, `10.8`, `13.2`, `13.3`).

Video emitter sequencing checks:

- `VID-EMIT-01`: `frame_id(next) > frame_id(prev)` for consecutive emitted metadata frames.
- `VID-EMIT-02`: each emitted metadata frame is followed by exactly one binary payload before any next metadata frame.
- `VID-EMIT-03`: emitted binary payload byte length equals the associated metadata `payload_bytes`.

Video pacing controls:

- Video stream pacing is configured through stream control message `set_rate_limit` (section `10.7`) with video-scoped payload fields.
- Video pacing payload schema (`video_pacing_v1`) fields:
  - `stream` (string, must equal `video`)
  - `pacing_mode` (enum: `realtime`, `fixed_fps`)
  - `target_fps` (uint32, required when `pacing_mode=fixed_fps`, range `1..240`)
  - `max_burst_frames` (uint32, optional, range `1..8`, default `1`)
- `pacing_mode=realtime` uses runtime/profile cadence without fixed-FPS throttling.
- `pacing_mode=fixed_fps` enforces ceiling pacing at `target_fps`; when emitter backlog persists, server must mark throttle/backpressure status via stream health.

Video pacing guard failures:

- Invalid pacing payload shape or out-of-range values -> `BAD_REQUEST`.
- Unknown/inactive session for the stream -> `ENGINE_NOT_RUNNING`.
- If sustained transport pressure prevents policy-compliant delivery, stream degradation must surface with stream category backpressure signaling (`STREAM_BACKPRESSURE`).

Metadata envelope (JSON sideband):

```json
{
  "type": "video_frame_meta",
  "schema_version": 1,
  "channel": "video.metadata.v1",
  "session_id": "ses_01H...",
  "frame_id": 8821,
  "timestamp_us": 1710001112223,
  "width": 640,
  "height": 400,
  "pixel_format": "RGB565",
  "payload_bytes": 512000
}
```

Binary frame payload follows.

Video pacing control message example (`set_rate_limit`):

```json
{
  "type": "set_rate_limit",
  "stream": "video",
  "pacing_mode": "fixed_fps",
  "target_fps": 50,
  "max_burst_frames": 2
}
```
