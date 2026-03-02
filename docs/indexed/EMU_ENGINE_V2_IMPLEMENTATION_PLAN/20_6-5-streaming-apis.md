# 6.5 Streaming APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 20

## Subsections

- Video stream
- Audio stream
- Engine status/health event stream
- Register inspection stream
- Bus/memory trace stream

---

## 6.5 Streaming APIs

### Video stream

- `GET /api/v2/stream/video` (WebSocket)
- frame packet contains:
  - `frame_id`, `timestamp_us`, `width`, `height`, `pixel_format`, `payload`
- Video metadata channel/schema contract (`video.metadata.v1`, `video_frame_meta_v1` required fields, ordering rules, and payload-pairing validation) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.2` and is the canonical source.
- Video payload stream emitter + pacing control contract (emitter checks `VID-EMIT-01..03`, `set_rate_limit` video pacing schema, and deterministic pacing/backpressure guard behavior) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.2` and is the canonical source.
- optional debug mode:
  - scanline phase markers, border timing markers

### Audio stream

- `GET /api/v2/stream/audio` (WebSocket)
- packet fields:
  - `chunk_id`, `sample_rate`, `channels`, `format`, `payload`, `timestamp_us`
- Audio metadata channel/schema contract (`audio.metadata.v1`, `audio_chunk_meta_v1` required fields, ordering rules, and payload-pairing validation) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.3` and is the canonical source.
- Audio payload stream emitter + pacing control contract (emitter checks `AUD-EMIT-01..03`, `set_rate_limit` audio pacing schema, and deterministic pacing/backpressure guard behavior) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.3` and is the canonical source.

### Engine status/health event stream

- `GET /api/v2/engine/stream` (WebSocket)
- event contracts:
  - `engine_status_update` payload shape follows `docs/EMU_ENGINE_V2_API_SPEC.md` sections `6.6B` and `10.8`
  - `engine_health_update` payload shape follows `docs/EMU_ENGINE_V2_API_SPEC.md` sections `6.6A` and `10.8`
  - `engine_stream_delivery` carries degraded-delivery/error signaling and backpressure disclosure per section `10.8`
- Stream backpressure counters + watermark metrics contract (`BP-CTR-01..04`, required stream-health counter fields, and degraded-delivery metrics alignment) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `10.7`, `10.8`, and `13.2` and is the canonical source.
- Backpressure telemetry exposure contract (REST snapshot endpoint `GET /api/v2/stream/telemetry/backpressure` and event `stream_backpressure_telemetry` with deterministic ordering and metric invariants) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `10.7` and `10.8` and is the canonical source.

### Register inspection stream

- `GET /api/v2/inspect/registers/stream`
- subscription filters:
  - component list (`cpu`, `mfp`, `shifter`, etc.)
  - interval or event-driven mode
- packet fields:
  - component, register name, value, old value, cycle stamp, tick stamp
- Register snapshot schema + selective filter-field contract (`register_snapshot_v1`, selector fields `components`/`registers`/`register_prefixes`, `changed_only`, and filter guards) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.4` and is the canonical source.
- Register snapshot stream publisher + validation-check contract (publisher pipeline ordering, required `event_seq`/`event_timestamp_us`, checks `REG-PUB-01..04`, and deterministic failure mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.4` and is the canonical source.

### Bus/memory trace stream

- `GET /api/v2/inspect/bus/stream`
- `GET /api/v2/inspect/memory/stream`
- filters:
  - address ranges
  - access type (`read`, `write`, `dma`, `iack`)
  - component source
  - sampling level (full, reduced)
- Bus/memory filter request model + guard-rule contract (`bus_filter_v1`, `memory_filter_v1`, atomic `subscribe`/`set_filter` updates, and deterministic guard failures to `BAD_REQUEST`/`ENGINE_NOT_RUNNING`/`INSPECT_FILTER_INVALID`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `10.5` and `10.6` and is the canonical source.
- Filtered bus/memory stream publisher + load-validation contract (publisher checks `BUS-FLT-01..04` and `MEM-FLT-01..04`, required `event_seq`/`event_timestamp_us`, and deterministic load-run validation artifact) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `10.5` and `10.6` and is the canonical source.
