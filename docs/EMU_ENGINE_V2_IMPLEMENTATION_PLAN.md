# espTari Emulation Engine v2 — Implementation Plan (Atari ST First)

Version: 1.0  
Date: 2026-03-01  
Target runtime: ESP32-P4-NANO (Waveshare), ESP-IDF v5.5.2  
Scope: Backend engine architecture + APIs (no frontend work)

Companion API contract: `EMU_ENGINE_V2_API_SPEC.md`

---

## 1. Purpose and scope

This document defines the implementation architecture and delivery plan for **Emulation Engine v2** for espTari, using the `docs/emu_engine_v2` hardware specification as the source of truth for machine behavior.

Initial machine target is:

- **Atari ST baseline** (520ST/1040ST behavior profile)

The architecture is intentionally designed to scale to:

- Mega ST
- STe
- Mega STe

without rebuilding the entire engine, by using dynamic machine/component loading through SD-card-resident EBIN modules.

---

## 2. Non-negotiable architecture constraints

1. **No local display pipeline dependency**
   - Do not use MIPI DSI as the default display path.
   - Video output is produced by the emulation core and exposed via streaming APIs for browser clients.

2. **No frontend coupling in this phase**
   - Deliver backend APIs, control plane, and streaming plane only.

3. **SD-card as media and module storage authority**
   - Disk images, ROM images, cartridge images, and EBIN component binaries are stored and loaded from SD-card paths.

4. **Dynamic EBIN component loading/unloading**
   - Machine components are deployable as custom EBIN binaries.
   - EBINs are loaded into engine runtime memory and can be unloaded/reloaded to switch machine profiles or component revisions.

5. **Remote file management required**
   - Browser-accessible upload/download/list/delete/move APIs for SD-card assets and EBINs.

6. **Full runtime introspection APIs required**
   - Bus and memory-map call tracing APIs.
   - Register inspection APIs with streamed updates.

7. **Input translation APIs required**
  - Engine must accept physical keyboard/mouse/game-controller input events.
  - Engine must translate host input events into machine-accurate virtual inputs for the active machine profile.
  - Input ingress source for this phase is browser session clients.
  - Engine must support input enable/disable and mouse-capture policy controls.

8. **Machine state save/restore lifecycle required**
  - Engine must support machine state saving and snapshot persistence.
  - Engine must support suspend-and-save and restore-and-resume workflows.
  - Saved state must include profile and ABI compatibility metadata.

9. **Hard runtime performance metrics required**
  - Input-device end-to-end latency target: 50 ms or less.
  - Runtime jitter target: less than 30 ms.
  - Dropped-frame rate target: less than 1 percent.

10. **Debug clock-control required**
  - Engine must support realtime, slow-motion, and single-step execution modes for debugging.
  - Debug execution must remain observable through opcode/bus/register tracing APIs.

---

## 3. High-level v2 architecture

## 3.1 Planes

### A) Control plane

Responsible for lifecycle and orchestration:

- Engine boot/shutdown/reset
- Machine profile selection
- EBIN dependency resolution and load order
- Session state transitions (`stopped`, `starting`, `running`, `paused`, `suspended`, `faulted`)

### B) Emulation plane

Responsible for deterministic emulation execution:

- Global master-tick scheduler
- Bus arbitration and component stepping
- Memory map dispatch
- Interrupt/event ordering
- Deterministic replay checkpoints (optional but recommended)

### C) I/O and media plane

Responsible for machine media and external interfaces:

- SD-card media mounts and path policy
- Disk/cartridge/ROM attach-detach
- Remote file manager operations
- Host input ingress and normalization (keyboard/mouse/controller)
- Input-to-virtual-machine mapping profiles

### D) Observability plane

Responsible for debugging and inspection:

- Register snapshots + streaming changes
- Bus transaction stream
- Memory map access stream
- Timing/cycle metrics

### E) Streaming plane

Responsible for browser-consumable outputs:

- Video frame stream (and optional scanline debug stream)
- Audio PCM stream
- Metadata/event channels (state changes, dropped frame counters, sync status)

---

## 4. Runtime component model (EBIN)

## 4.1 EBIN concept

An **EBIN** is a versioned, signed (recommended) binary module stored on SD-card and loaded by the v2 runtime.

EBIN module categories (Atari ST first profile):

- CPU module (68000 core contract)
- GLUE/MMU/SHIFTER module group
- MFP module
- ACIA/IKBD module group
- DMA/FDC module group
- PSG module
- Machine profile module (wiring + model defaults)

## 4.2 EBIN package layout (proposed)

SD-card path convention:

- `/sdcard/ebins/<machine>/<component>/<name>-<version>.ebin`
- `/sdcard/ebins/index.json` (module catalog)

EBIN header metadata (minimum):

- magic/version
- module type (`cpu`, `video`, `io`, `storage`, `machine_profile`, etc.)
- target machine(s) compatibility tags (`st`, `mega_st`, `ste`, `mega_ste`)
- API ABI version
- required exports
- dependency list (module IDs + ABI ranges)
- checksum/hash
- signature block (recommended)

## 4.3 Loader lifecycle

1. Read machine profile request
2. Resolve module set from catalog
3. Validate ABI compatibility and dependencies
4. Validate integrity/signature
5. Allocate runtime region(s)
6. Load + relocate EBIN(s)
7. Bind exported interfaces to engine contracts
8. Run module init hooks
9. Transition engine to `running`

Unload path:

1. Pause emulation
2. Drain stream outputs
3. Detach dependent modules
4. Run module deinit hooks
5. Release allocated regions
6. Update runtime catalog state

## 4.4 Safety and failure handling

- Reject EBINs with ABI mismatch or unresolved dependencies.
- Support rollback to previous known-good module set.
- Keep “safe mode” boot with built-in minimal fallback profile metadata (without embedding full machine logic).

---

## 5. SD-card storage architecture

## 5.1 Canonical SD-card tree

- `/sdcard/roms/st/`
- `/sdcard/disks/st/`
- `/sdcard/cartridges/st/`
- `/sdcard/ebins/`
- `/sdcard/saves/states/`
- `/sdcard/saves/nvram/` (future models)
- `/sdcard/config/engine_v2/`

## 5.2 Catalog metadata files

- `rom_catalog.json`
- `disk_catalog.json`
- `cartridge_catalog.json`
- `ebin_catalog.json`
- `tos_catalog.json`

Catalog entry fields (minimum):

- `id`
- `path`
- `sha256`
- `size`
- `machine_tags`
- `format`
- `created_at` / `updated_at`
- `source_url`
- `hosted_url`
- `availability_state` (`unknown`, `online`, `offline`, `dead`, `local_only`)
- `availability_checked_at`
- `download_fail_count`
- optional `dead_link_reason`

Catalog-backed media ID resolution (required):

- `rom_id`, `disk_id`, and `tos_id` resolve through catalog indexes rather than direct file-path assumptions.
- If a catalog entry exists but the file is missing on SD-card, runtime can request immediate or deferred download from the entry hosted URL.
- Canonical loader contract for these IDs (`rom_catalog.json`, `disk_catalog.json`, `tos_catalog.json`), normalized resolved fields, and deterministic blocker mapping (`CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CONFLICT`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1`.
- Catalog-backed ID resolution API path matrix across session start and media attach endpoints, including stable failure-stage mapping, is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for endpoint behavior.

## 5.3 Catalog sync and link-health subsystem

Responsibilities:

- Keep local catalogs current against upstream hosted sources.
- Detect assets present in catalogs but missing on SD-card.
- Download missing assets on demand or via scheduled prefetch jobs.
- Probe and classify hosted URLs as online/offline/dead.
- Mark dead links in catalog metadata with timestamp and reason.
- SD-card rescan must produce a canonical presence index snapshot (`catalog + entry_id` keyed records with local presence metadata) as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10` (`POST /api/v2/catalogs/{catalog}/rescan-local`).
- Missing-asset diff API projection must derive from that canonical presence index (`GET /api/v2/catalogs/{catalog}/missing-report`) and follow deterministic blocker mapping defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Hosted download request validation + enqueue contract (`POST /api/v2/catalogs/{catalog}/download-entry`) including deterministic blocker mapping and queue projection fields is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Staged download commit + integrity/hash verification contract (staging path, `staged` -> `verified` -> `committed` phases, and deterministic commit blockers) for that same endpoint is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Dead-link probe worker and timeout policy contract (`POST /api/v2/catalogs/{catalog}/probe-links`) including timeout classification, dead-link transition thresholds, and deterministic probe blockers is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Dead-link mark/retry state machine and telemetry fields contract (`POST /api/v2/catalogs/{catalog}/mark-dead` + `allow_dead_retry=true` retry path on `POST /api/v2/catalogs/{catalog}/download-entry`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Catalog-sync scheduler core and persisted schedules contract (authoritative scheduler clock, deterministic due-order selection, persisted schedule store, and deterministic scheduler blockers across `POST/GET/DELETE /api/v2/catalog-sync/schedules`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.11`.
- Schedule CRUD API and reboot-recovery validation contract (`POST/GET/PATCH/DELETE /api/v2/catalog-sync/schedules*`, startup schedule-store validation/quarantine, and deterministic recovery blocker behavior) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.11`.

On-device scraper/sync jobs (aligned with host scraper flow):

- `floppy_catalog_sync` (CSV shard ingestion + catalog update)
- `rom_catalog_sync` (JSON ingestion + catalog update)
- `tos_catalog_sync` (page parse + catalog update)

Job modes:

- `catalog_only`
- `catalog_and_probe_links`
- `catalog_probe_and_prefetch_missing`

## 5.4 File manager policy

- All user uploads enter staging path first: `/sdcard/.staging/`
- Validate format + hash + optional signature
- Move atomically to canonical path
- Update catalog index transactionally

---

## 6. API architecture (backend only)

Protocol split:

- REST for control and file operations
- WebSocket (or SSE where appropriate) for live streaming telemetry

Canonical REST contract:

- All REST endpoints must use one canonical success envelope and one canonical error envelope as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `4` and `5`.
- Control lifecycle and session state contracts must not redefine top-level envelope fields and should define only `data` payload semantics.
- Lifecycle control contracts (`6.1` through `6.8`) and state/status semantics (`6.6`, `12`) must remain cross-referenced to the canonical envelope/error model.
- Lifecycle transition matrix and deterministic guard predicates are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `12` and are the normative source for transition acceptance/rejection behavior.
- Lifecycle guard validator response/error mapping contract (guard-to-code matrix, canonical envelope usage, and required `details.guard_id` / `details.endpoint`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `12` and is the normative source for guard rejection payloads.
- Baseline schema bundle for downstream tasks is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.0 Baseline API schema bundle`.
- Suspend-save request wiring + transition checks contract (`suspend_save_request_v1`, `suspend_save_response_v1`, checks `SUSP-REQ-01..04`, and deterministic suspend-save guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.7` and is the canonical source.
- Restore-resume transition guards + error semantics contract (`restore_resume_request_v1`, `restore_resume_response_v1`, checks `REST-RES-01..04`, and deterministic restore/snapshot guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.8` and is the canonical source.
- Restore compatibility rule matrix contract (`restore_compatibility_matrix_v1`, rules `RCOMP-01..04`, deterministic evaluation order, and compatibility failure mapping to `SNAPSHOT_NOT_FOUND`/`SNAPSHOT_INCOMPATIBLE`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.7` and is the canonical source.
- Restore compatibility validator + error-mapping contract (`restore_compatibility_validate_request_v1`, `restore_compatibility_validate_result_v1`, checks `RCOMP-VAL-01..04`, and deterministic error mapping across request/engine/snapshot domains) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.7` and is the canonical source.

## 6.1 Control APIs

- `POST /api/v2/engine/session`
  - Start machine session with profile + module set
- `POST /api/v2/engine/session/stop`
- `POST /api/v2/engine/session/pause`
- `POST /api/v2/engine/session/resume`
- `POST /api/v2/engine/session/reset`
- `POST /api/v2/engine/session/suspend-save`
- `POST /api/v2/engine/session/restore-resume`
- `GET /api/v2/engine/session`
  - Current state, loaded modules, cycle counters, stream health
- `GET /api/v2/engine/status`
  - Session/runtime status snapshot with lifecycle state semantics (see `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6B`)
- `GET /api/v2/engine/health`
  - Engine health summary and component-level health states (see `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6A`)

Request payload (start):

- `machine`: `"atari_st"`
- `profile`: e.g. `"st_520_pal"`, `"st_1040_ntsc"`
- `rom_id`
- optional `disk_ids[]`
- optional `cartridge_id`
- optional `module_overrides{}`

## 6.2 SD-card remote file manager APIs

- `GET /api/v2/files/list?path=...`
- `POST /api/v2/files/upload` (multipart/chunked)
- `POST /api/v2/files/move`
- `POST /api/v2/files/delete`
- `GET /api/v2/files/download?path=...`
- `POST /api/v2/files/mkdir`
- `GET /api/v2/files/stat?path=...`

Catalog/asset sync endpoints:

- `GET /api/v2/catalogs/list`
- `GET /api/v2/catalogs/{catalog}/entries`
- `GET /api/v2/catalogs/{catalog}/entries/{entry_id}`
- `POST /api/v2/catalogs/{catalog}/download-missing`
- `POST /api/v2/catalogs/{catalog}/download-entry`
- `POST /api/v2/catalogs/{catalog}/probe-links`
- `POST /api/v2/catalogs/{catalog}/mark-dead`
- `POST /api/v2/catalogs/{catalog}/rescan-local`

Scraper/scheduler endpoints:

- `POST /api/v2/catalog-sync/jobs/run`
- `GET /api/v2/catalog-sync/jobs`
- `GET /api/v2/catalog-sync/jobs/{job_id}`
- `POST /api/v2/catalog-sync/schedules`
- `GET /api/v2/catalog-sync/schedules`
- `DELETE /api/v2/catalog-sync/schedules/{schedule_id}`

EBIN-specific endpoints:

- `GET /api/v2/ebins/catalog`
- `POST /api/v2/ebins/rescan`
- `POST /api/v2/ebins/validate`
- `POST /api/v2/ebins/load`
- `POST /api/v2/ebins/unload`

## 6.3 Media APIs

- `POST /api/v2/media/rom/attach`
- `POST /api/v2/media/disk/attach`
- `POST /api/v2/media/disk/eject`
- `POST /api/v2/media/cartridge/attach`
- `POST /api/v2/media/cartridge/eject`
- ROM attach request validation + catalog binding check contract (`session_id`/`rom_id` validation, ROM-catalog binding enforcement, profile-compatibility checks, and deterministic blocker mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.1`.
- ROM mount/apply flow and attach-status event contract (phase progression `validated -> mounted -> applied`, rollback-on-apply-failure behavior, and `media_attach_status` stream events with deterministic sequencing fields) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.1`.
- Disk attach/eject request validation and binding-check contracts (`drive`/`disk_id` request validation, disk-catalog binding/format checks, deterministic no-op eject semantics, and deterministic blocker mapping) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.2` and `9.3`.
- Disk mount/eject runtime flow and disk state event contract (attach phase progression, rollback-on-activation-failure behavior, deterministic eject phase semantics, and `media_disk_state` stream events with sequence/timestamp fields) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.2` and `9.3`.

## 6.4 Input APIs

- `GET /api/v2/input/devices`
  - Enumerate currently available host input devices
- `POST /api/v2/input/events/inject`
  - Submit normalized host input events to translation pipeline
- `GET /api/v2/input/capture/state`
  - Inspect current browser input capture state for the session
- `POST /api/v2/input/policy/enabled`
  - Idempotent enable/disable policy control (`result=applied|no_op`) aligned to policy states in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6`
- `POST /api/v2/input/capture/config`
  - Enable/disable input and set capture mode (`mouse_over` or `click_to_capture`)
- `POST /api/v2/input/capture/release`
  - Force release of mouse capture for the requesting browser session
- `POST /api/v2/input/mappings/load`
  - Load input mapping profile for machine/session
- `GET /api/v2/input/mappings/active`
  - Inspect active mapping profile
- `POST /api/v2/input/mappings/update`
  - Patch mapping entries at runtime
- `GET /api/v2/input/stream` (WebSocket)
  - Stream translated virtual input events, input-pipeline diagnostics, and input-policy transition events

Mapping responsibilities:

- Keyboard host keycodes -> Atari ST key matrix/IKBD semantics
- Mouse host deltas/buttons -> IKBD mouse packet semantics
- Game controller host axes/buttons -> virtual joystick semantics
- Canonical input mapping schema and persistence model (`mapping_profile` fields, per-entry schema, persistence path, revision semantics, and deterministic blocker mapping) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.2` through `9.6.4`.
- Mapping profile CRUD contract (create/list/get/patch/delete semantics, persistence behavior, and deterministic CRUD blockers) plus active-profile apply path contract (`POST /api/v2/input/mappings/apply`, atomic cutover semantics, apply no-op rules, and revision-conflict handling) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.2` through `9.6.4`.

Browser input capture policy:

- Policy state model for endpoint/event contracts is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` (`disabled`, `enabled_idle`, `enabled_captured`) with deterministic transitions and policy metadata (`policy.state`, `policy.source`, `policy.reason`, `policy.changed_at_us`).
- `enabled=false`: input events are ignored except capture-config and diagnostics requests.
- `capture_mode=mouse_over`: browser sends input only while pointer is over canvas; leaving canvas releases capture implicitly.
- `mouse_over` capture state machine contract (state variables, deterministic transition matrix, and guards `MO-GUARD-01`..`MO-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` and is the canonical source for pointer-driven capture activation.
- `mouse_over` enter/leave event hooks and release behavior contract (deterministic `pointer_enter_hook` / `pointer_leave_hook` processing, hook-driven policy events, and explicit release no-op semantics in mouse-over mode) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.6` and `9.6.7`.
- `capture_mode=click_to_capture`: capture starts on canvas click and remains active until explicit release.
- `click_to_capture` acquisition state machine contract (deterministic click-acquire transitions and guards `CT-GUARD-01`..`CT-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6` and is the canonical source for click-driven capture acquisition behavior.
- Escape release sequence is supported in `click_to_capture` mode; default sequence is double-escape (`Escape`, `Escape`) within configurable timeout.
- Escape-release and focus-recovery behavior contract for `click_to_capture` mode (escape-sequence release transition, focus-loss forced release, no auto-reacquire on focus regain, and deterministic guards `ER-GUARD-01`..`ER-GUARD-03`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.6.6` and `9.6.7`.
- `click_to_capture` mode also supports explicit release via `/api/v2/input/capture/release`.
- Policy-related invalid-state and violation errors use canonical input error envelopes (`INPUT_POLICY_INVALID_STATE`, `INPUT_POLICY_VIOLATION`) from `docs/EMU_ENGINE_V2_API_SPEC.md` sections `5` and `9.6.6`.
- Policy enable/disable endpoint guards use deterministic input errors (`INPUT_POLICY_MODE_INVALID`, `INPUT_POLICY_SESSION_INVALID`) and idempotent `no_op` responses as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.6`.
- Input stream policy-change event contract (`input_policy_changed`) including required fields (`source`, `prior_state`, `new_state`, `reason`, `event_timestamp_us`, `event_seq`, `transition_result`), strict event ordering, and no-op observability is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.
- Input translation event payload + ordering contract (`input_translated` required fields including mapping snapshot identifiers, monotonic `event_seq` / `event_timestamp_us`, and `(tick, cycle)` ordering semantics) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.
- Input translation stream emitter + sequencing-check contract (deterministic emission pipeline, checks `SEQ-CHECK-01..03`, and diagnostics surfacing of sequence anomalies) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.6.7`.

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

## 6.6 Snapshot APIs

- `GET /api/v2/inspect/registers/snapshot`
- `GET /api/v2/inspect/bus/snapshot`
- `GET /api/v2/inspect/memory/snapshot?range=...`
- `POST /api/v2/engine/checkpoint/create`
- `POST /api/v2/engine/checkpoint/load`

## 6.7 Machine state persistence APIs

- `POST /api/v2/engine/state/save`
  - Persist complete machine state snapshot and return `snapshot_id`
- `POST /api/v2/engine/state/restore`
  - Restore machine state from `snapshot_id` with compatibility validation
- `GET /api/v2/engine/state/list`
  - Enumerate available machine state snapshots

## 6.8 Performance metrics APIs

- `GET /api/v2/metrics/performance`
  - Expose input latency, runtime jitter, dropped-frame percentage, and measurement windows
- `GET /api/v2/metrics/performance/history`
  - Time-windowed SLO trends for acceptance and regressions
- Performance SLO collector + sampling pipeline contract (`slo_collector_config_v1`, `slo_sample_v1`, checks `SLO-COL-01..04`, and deterministic collector/pipeline guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.11` and is the canonical source.
- SLO endpoints + threshold-breach alarm contract (`slo_thresholds_v1`, `slo_breach_alarm_event_v1`, checks `SLO-ALRM-01..04`, and deterministic alarm-state guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.11` and is the canonical source.

## 6.9 Debug clock-control APIs

- `POST /api/v2/debug/clock/mode`
  - Set execution mode: `realtime`, `slow_motion`, `single_step`
- `POST /api/v2/debug/clock/step`
  - Execute one or more bounded debug steps and emit trace markers
- `GET /api/v2/debug/clock/state`
  - Read active debug clock mode, ratio, and last-step counters
- Realtime/slow-motion clock-control model + deterministic bounds contract (`effective_ratio`, bounds checks `CLOCK-BOUND-01..04`, and mode-change guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.9A` and is the canonical source.
- Clock mode API deterministic transition-flow contract (`mode_transition_seq`, checks `CLOCK-TRANS-01..04`, idempotent transition behavior, and transition guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.9B` and is the canonical source.
- Single-step execution-control API + scheduler-hook contract (`steps` bounds, checks `STEP-CTRL-01..04`, `scheduler_hook_stats`, and deterministic guard/error mapping for `/api/v2/debug/clock/step`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10B` and is the canonical source.
- Opcode/bus-error capture path + diagnostic payload contract (`opcode_capture_v1`, `bus_error_capture_v1`, checks `CAP-DIAG-01..04`, and deterministic capture guard/error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10C` and is the canonical source.

---

## 7. Introspection and observability design

## 7.1 Internal event schema

All observable events should include:

- monotonic `tick`
- `cycle`
- `component_id`
- `event_type`
- structured payload

Status telemetry alignment:

- Status snapshot fields (`lifecycle_state`, `run_mode`, `snapshot_at_us`, `cycle_counter`, `tick_counter`, `runtime.last_transition_at_us`, `runtime.last_error`) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6B` and are the canonical source for control-plane telemetry payload shape.
- Engine status/health WebSocket telemetry uses monotonic ordering fields (`event_seq`, `event_timestamp_us`) and degraded-delivery disclosure (`delivery.*`) defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.8`; these are the canonical stream observability contracts.

## 7.2 Trace levels

- `off`: no trace generation
- `errors`: fault events only
- `summary`: sampled aggregate counters
- `debug`: filtered event stream
- `full`: every eligible event (development only)

## 7.3 Backpressure policy

- Ring buffers per stream channel.
- Drop policy must be explicit and counted (`dropped_events`).
- Optional throttle knobs per stream endpoint.
- Engine status/health stream must disclose skip/coalescing state per event (`delivery.dropped_events_since_last`, `delivery.coalesced_updates`, `delivery.reason`, `delivery.degraded`) as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `10.8`.

## 7.4 Performance SLO gates

Runtime SLO targets:

- Input-device end-to-end latency: <= 50 ms
- Runtime jitter: < 30 ms
- Dropped-frame rate: < 1 percent

Measurement requirements:

- Report p50, p95, and max in rolling windows.
- Expose per-session and aggregate runtime views.
- Mark SLO status as `ok`, `degraded`, or `violating`.

---

## 8. Atari ST-first machine architecture (v2)

## 8.1 First machine profile target

Phase-1 machine profile:

- `atari_st_520_1040_baseline`

Includes:

- 68000-class CPU behavior contract
- ST chipset group behavior contract (GLUE/MMU/SHIFTER)
- GLUE/MMU/SHIFTER register + memory window model contract (register windows `register_window_v1`, memory windows `memory_window_v1`, and deterministic selector/session guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3A` and is the canonical source.
- GLUE/MMU/SHIFTER arbitration + timing integration checks (`CHIP-TIM-01..04`, deterministic chipset order, and timing monotonicity guards) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3A` and are the canonical source.
- MFP register + timer model contracts (`mfp_register_window_v1`, `mfp_timer_model_v1`, and deterministic session/selector guard failures) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3B` and are the canonical source.
- MFP interrupt emission behaviors + conformance checks (`MFP-IRQ-01..04`, deterministic interrupt vector mapping, and interrupt timing monotonicity guards) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3B` and are the canonical source.
- ACIA host channel bridge + framing rules contract (`acia_bridge_state_v1`, `acia_frame_v1`, checks `ACIA-FRM-01..04`, and deterministic bridge/framing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3C` and is the canonical source.
- IKBD parser bridge + keyboard/mouse packet timing contract (`ikbd_bridge_state_v1`, `ikbd_packet_timing_v1`, checks `IKBD-PKT-01..04`, and deterministic bridge/timing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3D` and is the canonical source.
- DMA request pacing model + arbitration hooks contract (`dma_pacing_state_v1`, `dma_arbitration_hook_v1`, checks `DMA-ARB-01..04`, and deterministic pacing/arbitration guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3E` and is the canonical source.
- FDC command/status FSM bridge + terminal-condition contract (`fdc_fsm_state_v1`, `fdc_terminal_event_v1`, checks `FDC-FSM-01..04`, and deterministic FSM/terminal guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3F` and is the canonical source.
- PSG register/audio behavior contract (`psg_register_window_v1`, `psg_audio_state_v1`, checks `PSG-AUD-01..04`, and deterministic register/audio guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3G` and is the canonical source.
- PSG GPIO behavior contract + validation checks (`psg_gpio_state_v1`, `psg_gpio_event_v1`, checks `PSG-GPIO-01..04`, and deterministic GPIO guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3H` and is the canonical source.
- Interrupt hierarchy map + vector routing contract (`interrupt_hierarchy_v1`, `interrupt_route_event_v1`, checks `INT-MAP-01..04`, and deterministic hierarchy/routing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3I` and is the canonical source.
- Interrupt wiring integration-check contract (`interrupt_wiring_state_v1`, `interrupt_wiring_check_v1`, checks `INT-WIRE-01..04`, and deterministic wiring/linkage guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3J` and is the canonical source.
- Startup defaults + power-on register baseline contract (`startup_defaults_v1`, `power_on_register_baseline_v1`, checks `PWR-BASE-01..04`, and deterministic startup/baseline guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3K` and is the canonical source.
- Reset/startup sequence executor + verification checks contract (`startup_sequence_state_v1`, `startup_verification_event_v1`, checks `RST-SEQ-01..04`, and deterministic startup-sequence guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3L` and is the canonical source.
- MFP timing and interrupt behavior
- ACIA + IKBD protocol behavior
- DMA + FDC + floppy media behavior
- PSG audio behavior

## 8.2 Profile configuration artifact

Store profile manifests under:

- `/sdcard/config/engine_v2/machines/atari_st/*.json`

Profile fields:

- region/video standard (`pal`/`ntsc`)
- RAM size profile
- expected module IDs and ABI requirements
- default ROM recommendation set
- enabled stream defaults
- ST profile manifest parser + schema validation contract (canonical manifest path, required schema fields, normalized parse output, and deterministic blockers for missing/invalid manifests) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for session bootstrap.
- Profile wiring validation + fail-fast diagnostics contract (module-resolution checks, `scheduler.step_order` validation, startup abort-before-run guarantees, and deterministic wiring blocker mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for bootstrap safety.

## 8.3 Deterministic scheduler contract

- Single authoritative master tick.
- Component step order from machine profile wiring.
- Shared bus arbitration resolved through profile-defined policy.
- API-visible timestamps derive from this scheduler only.
- Deterministic tick-loop scheduler core contract (tick/cycle invariants, mode-specific execution rules for realtime/slow-motion/single-step, scheduler sequencing checks, and fail-fast guards) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10A` and is normative for runtime timing behavior.
- Arbitration hook layer + deterministic timestamp emitter contract (hook dispatch order, arbitration metadata fields, timestamp monotonicity/determinism checks, and fail-fast internal error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10A` and is normative for observability-grade timing consistency.

---

## 9. Security and integrity model

1. Restrict file manager to SD-card root allowlist.
2. Normalize and validate paths (prevent traversal).
3. Enforce upload size limits and chunk timeouts.
4. Validate EBIN integrity before load.
5. Maintain audit log for:
   - uploads
   - loads/unloads
   - module validation failures
   - session transitions

---

## 10. Delivery roadmap (implementation phases)

## Phase 0 — Foundation

- Engine v2 service skeleton
- Session state machine
- SD-card path policy and catalog infra
- Basic REST control endpoints

## Phase 1 — Atari ST baseline runtime

- Implement machine profile manifest loading
- EBIN loader v1 (single machine profile)
- Core execution loop and deterministic scheduler
- Minimal video/audio stream endpoints
- Minimal register snapshot endpoint

## Phase 2 — Introspection expansion

- Register stream subscriptions
- Bus/memory stream with filters
- Snapshot/checkpoint endpoints
- Machine save-state snapshot and restore endpoints
- Trace level controls and backpressure metrics

## Phase 3 — Remote EBIN/file operations

- Chunked remote upload
- EBIN validation + catalog rescan
- Hot unload/reload with rollback
- Media attach/eject control completeness

## Phase 4 — Robustness and conformance

- Stress/fault recovery tests
- Long-run stability tests
- Atari ST conformance suites aligned to `docs/emu_engine_v2/10_validation_plan.md`
- Catalog sync reliability tests (link-probe accuracy, dead-link marking, retry policy)
- On-device scraper schedule tests (manual/periodic runs, restart recovery)
- Hard performance SLO validation (latency/jitter/dropped-frame)
- Debug clock mode and single-step conformance tests
- Conformance harness scaffold + test manifest loader contract (harness session bootstrap, `conformance_manifest_v1` schema, deterministic loader/session guard failures, and initialization evidence) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Evidence artifact collection + report packaging flow contract (collection endpoint, package endpoint, deterministic state guards, and artifact/report evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Acceptance checklist execution runner contract (run/status endpoints, runner state progression, selection semantics, and deterministic runner guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Review pack generation + signoff bundle assembly contract (review-pack endpoint, signoff-bundle endpoint, deterministic precondition guards, and signed bundle evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Subsystem conformance scaffold + fixture model contract (`conformance_fixture_model_v1`, checks `CONF-FIX-01..04`, deterministic scaffold/fixture guard failures, and scaffold/model evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.2` and is the canonical source.
- Per-subsystem acceptance suite + reporting-output contract (`subsystem_suite_report_v1`, checks `SUB-SUITE-01..04`, deterministic suite/report guard failures, and suite/report evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.3` and is the canonical source.

## Phase 5 — Multi-machine enablement

- Add Mega ST profile using same contracts
- Add STe/Mega STe profile extensions (extra audio/video controls)
- Maintain backward ABI compatibility where possible

---

## 11. Acceptance criteria for v2 Atari ST milestone

A milestone is complete when all are true:

1. Engine can start/pause/resume/reset/stop Atari ST session through v2 APIs.
2. ROM/disk/cartridge assets are loaded from SD-card only.
3. EBIN modules for Atari ST profile can be remotely uploaded, validated, loaded, and unloaded.
4. Video and audio are available via streaming APIs (browser-consumable transport).
5. Register inspection is available as both snapshot and stream.
6. Bus and memory-map access traces are available via filtered stream endpoints.
7. Input API can translate keyboard, mouse, and game-controller events into Atari ST virtual inputs with profile-driven mappings.
8. Trace system remains stable under backpressure with explicit dropped-event metrics.
9. Browser-session input capture supports enable/disable, mouse-over capture mode, click-to-capture mode, and escape-sequence release.
10. Catalog-backed media resolution works for ROM/floppy/TOS IDs and supports download of missing local assets from hosted URLs.
11. Dead hosted links are marked in catalogs with status/timestamps, and scheduler-driven sync jobs can refresh catalogs on-device.
12. Machine state saving supports suspend-save and restore-resume with compatibility checks.
13. Hard performance SLO targets are measured and exposed: input latency <= 50 ms, jitter < 30 ms, dropped-frame rate < 1 percent.
14. Debug clock controls support slow-motion and single-step execution while preserving observability correctness.

---

## 12. Risks and mitigations

- **Risk:** EBIN ABI churn breaks compatibility.  
  **Mitigation:** Versioned ABI contracts + compatibility matrix in catalog.

- **Risk:** Streaming overhead perturbs deterministic timing.  
  **Mitigation:** async ring-buffered export path, deterministic core never waits on network I/O.

- **Risk:** SD-card I/O latency stalls runtime.  
  **Mitigation:** staged preload caches for active media/module pages.

- **Risk:** Upstream media URLs become unavailable or stale.  
  **Mitigation:** periodic link probing, dead-link marking, and alternate-mirror policy hooks.

- **Risk:** Full trace mode overwhelms memory/bandwidth.  
  **Mitigation:** strict trace levels, filters, and capped ring buffers.

- **Risk:** Save-state schema drift breaks restore compatibility.  
  **Mitigation:** schema versioning + ABI/profile compatibility checks + restore rejection codes.

- **Risk:** Debug slow-motion or step mode perturbs runtime assumptions.  
  **Mitigation:** explicit debug mode isolation + conformance checks for debug transitions.

---

## 13. Documentation integration points

This implementation plan maps directly to the existing hardware specification set in:

- `emu_engine_v2/README.md`
- `emu_engine_v2/02_system_block_diagram.md`
- `emu_engine_v2/03_component_spec_table.md`
- `emu_engine_v2/04_memory_io_map.md`
- `emu_engine_v2/05_timing.md`
- `emu_engine_v2/10_validation_plan.md`
- `EMU_ENGINE_V2_API_SPEC.md`

---

## 14. Immediate next actions

1. Freeze API contract draft (`/api/v2/*`) and event schemas.
2. Define EBIN ABI v1 and module manifest schema.
3. Implement SD-card catalog service + remote file manager staging flow.
4. Implement Atari ST baseline machine profile loader.
5. Implement first end-to-end run path (ROM load -> session start -> video/audio stream + register stream).
6. Keep API implementation aligned with `EMU_ENGINE_V2_API_SPEC.md` and publish changelog deltas on contract updates.
7. Implement input translation pipeline and default Atari ST mapping profiles for keyboard, mouse, and controller.
8. Implement catalog sync service with hosted-link probing, dead-link marking, and missing-asset downloader.
9. Implement machine save-state persistence with suspend-save and restore-resume lifecycle APIs.
10. Implement performance SLO metrics collection and reporting endpoints.
11. Implement debug clock mode and single-step execution controls for low-speed trace capture.
9. Implement on-device periodic scraper jobs for floppy/ROM/TOS catalog refresh.
