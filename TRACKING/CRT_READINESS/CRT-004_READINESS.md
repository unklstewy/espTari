# CRT-004 Readiness Pack

## Task
- Task ID: CRT-004
- Epic: EPIC-06
- Objective: Prepare observability stream/telemetry implementation-readiness pack.
- Status intent: Ready for implementation-phase verification execution.

## Dependency Closure
- CRT-003 (Done): save/restore compatibility readiness completed.
- Observability contract artifacts available:
  - `docs/emu_engine_v2/api_schema_engine_health_v1.json`
  - `docs/emu_engine_v2/api_schema_engine_status_v1.json`
  - `docs/emu_engine_v2/api_schema_engine_stream_status_health_v1.json`
- Runtime stream endpoint anchors available:
  - `components/esptari_web/esptari_web_stream.c`
  - `components/esptari_web/esptari_web_core_status.c`

## Scenario Matrix (Implementation Phase)
| Check ID | Endpoint / Channel | Scenario | Expected result |
|---|---|---|---|
| OBS-001 | `GET /api/v2/engine/health` | Nominal steady-state polling | Payload conforms to health schema fields and enums |
| OBS-002 | `GET /api/v2/engine/status` | Nominal lifecycle transitions | Status payload reflects deterministic lifecycle state/progress |
| OBS-003 | `GET /api/v2/engine/stream` | Status/health event stream while lifecycle changes | Event ordering follows contract, no missing required fields |
| OBS-004 | `GET /api/v2/engine/stream` | Burst update interval near threshold | Backpressure behavior observed without malformed payloads |
| OBS-G01 | `GET /api/v2/engine/stream` | Simulated threshold breach | Alarm chronology includes threshold-crossing + recovery events |
| OBS-G02 | `GET /api/v2/engine/stream` | High-load with delayed consumer | Stream remains contract-conformant or emits deterministic error envelope |

## Payload Conformance Checklist
1. Every event includes required envelope fields (`ok`, event type/code, timestamp/session linkage per schema).
2. Health/status payload enums match declared schema value sets.
3. Optional fields appear only when their gate conditions are met.
4. Error events include stable `code`, `category`, and deterministic detail keys.
5. Sequence/timing metadata is monotonic for ordered stream assertions.

## Threshold and Alarm Chronology Assertions
- Define thresholds per check family (CPU load, queue depth, frame latency, transport backlog) before execution.
- Assert chronology: nominal -> threshold breach -> alarm emitted -> mitigation/recovery -> normal.
- Capture timestamps and ordering IDs for all threshold transitions.

## Traceability Anchors
- Normative API references: `docs/EMU_ENGINE_V2_API_SPEC.md`
- Health/status contracts: `docs/emu_engine_v2/api_schema_engine_health_v1.json`, `docs/emu_engine_v2/api_schema_engine_status_v1.json`
- Stream contracts: `docs/emu_engine_v2/api_schema_engine_stream_status_health_v1.json`
- Runtime implementation anchors: `components/esptari_web/esptari_web_stream.c`, `components/esptari_web/esptari_web_core_status.c`

## Readiness Decision
CRT-004 readiness artifact is complete: stream/telemetry scenario matrix, payload conformance checklist, and threshold-alarm chronology assertions are fully specified and traceable to accepted observability contracts.
