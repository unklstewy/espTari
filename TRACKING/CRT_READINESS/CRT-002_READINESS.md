# CRT-002 Readiness Pack

## Task
- Task ID: CRT-002
- Epic: EPIC-06
- Objective: Prepare input mapping CRUD/apply implementation-readiness pack.
- Status intent: Ready for implementation-phase verification execution.

## Dependency Closure
- CRT-001 (Done): lifecycle transition/guard readiness complete.
- Input contract artifacts available:
  - `docs/emu_engine_v2/api_schema_input_policy_v1.json`
  - `docs/emu_engine_v2/api_schema_input_policy_enabled_v1.json`
  - `docs/emu_engine_v2/api_schema_input_policy_events_v1.json`
  - `docs/emu_engine_v2/api_schema_input_mappings_persistence_v1.json`
- Runtime mapping persistence implementation and smoke evidence available:
  - `components/esptari_web/esptari_web_input.c`
  - `tools/smoke_input_mapping_070.sh`
  - `captures/input_mapping_070_smoke_postflash_20260303_152234.txt`

## Planned CRUD/Apply Conformance Probes (Implementation Phase)
| Check ID | Endpoint | Scenario | Expected result |
|---|---|---|---|
| MAP-001 | `POST /api/v2/input/mappings/load` | Load valid profile with `replace=true` | `ok=true`, `applied=true`, deterministic profile metadata |
| MAP-002 | `POST /api/v2/input/mappings/update` | Update one valid entry | `ok=true`, `result=applied`, revision increments by 1 |
| MAP-003 | `POST /api/v2/input/mappings/update` | Update with `entries=[]` | `ok=true`, `result=no_op`, revision unchanged |
| MAP-004 | `POST /api/v2/input/mappings/load` | Reload profile after updates | Applied mapping set matches persisted profile/load semantics |
| MAP-G01 | `POST /api/v2/input/mappings/load` | Missing/invalid required fields | `400 BAD_REQUEST`, deterministic field-level message |
| MAP-G02 | `POST /api/v2/input/mappings/update` | Invalid host/virtual binding payload | `400 BAD_REQUEST`, mapping validation failure details |

## Revision / Cutover / No-op Assertions
1. Revisions are monotonic and increment only on applied mutations.
2. `result=no_op` must not alter persisted mapping payload or revision.
3. Profile load cutover is atomic from the API perspective (`ok=true` response implies active mapping state replaced or merged per request semantics).
4. Response payload always includes enough metadata to correlate with expected mapping state (`revision`, `result`, profile/session identifiers where applicable).

## Conflict-Path Capture Templates
- Conflict note template: operation ID, pre-revision, post-revision, conflicting key(s), resolution rule.
- Reproducibility template: input payload hash, deterministic seed/context, expected normalized mapping record.
- Diff template: expected vs observed map entries (host binding and virtual target/value tuple).

## Traceability Anchors
- Normative API references: `docs/EMU_ENGINE_V2_API_SPEC.md`
- Input policy contracts: `docs/emu_engine_v2/api_schema_input_policy_v1.json`, `docs/emu_engine_v2/api_schema_input_policy_enabled_v1.json`, `docs/emu_engine_v2/api_schema_input_policy_events_v1.json`
- Mapping persistence contract: `docs/emu_engine_v2/api_schema_input_mappings_persistence_v1.json`
- Runtime implementation anchor: `components/esptari_web/esptari_web_input.c`

## Readiness Decision
CRT-002 readiness artifact is complete: dependency CRT-001 is closed, CRUD/apply probes and revision/cutover/no-op assertions are specified, and runtime-phase verification can execute with deterministic evidence templates.
