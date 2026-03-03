# CRT-003 Readiness Pack

## Task
- Task ID: CRT-003
- Epic: EPIC-06
- Objective: Prepare save/restore compatibility implementation-readiness pack.
- Status intent: Ready for implementation-phase verification execution.

## Dependency Closure
- CRT-002 (Done): input mapping readiness pack completed.
- Save/restore lifecycle contract and implementation anchors available:
  - `docs/emu_engine_v2/api_schema_lifecycle_v1.json`
  - `docs/emu_engine_v2/api_schema_lifecycle_transition_matrix_v1.json`
  - `components/esptari_web/esptari_web_lifecycle_state.c`
- Guard/error envelope evidence available from lifecycle smoke:
  - `captures/lifecycle_guard_055_smoke_postflash_20260303_153045.txt`

## Save/Restore Compatibility Matrix (Implementation Phase)
| Check ID | Endpoint | Scenario | Expected result |
|---|---|---|---|
| SR-001 | `POST /api/v2/engine/session/suspend-save` | Valid save from running with `snapshot_id` | `ok=true`, snapshot metadata returned |
| SR-002 | `POST /api/v2/engine/session/restore-resume` | Valid restore from suspended, `resume_mode=running` | `ok=true`, lifecycle resumes to running |
| SR-003 | `POST /api/v2/engine/session/restore-validate` | Validate compatible snapshot + profile | `ok=true`, compatibility report indicates pass |
| SR-G01 | `POST /api/v2/engine/session/suspend-save` | Save when lifecycle not running | `409 INVALID_SESSION_STATE`, `guard_id=G-SUSPEND-01` |
| SR-G02 | `POST /api/v2/engine/session/restore-resume` | Invalid `resume_mode` | `400 BAD_REQUEST`, `guard_id=G-RESUME-02` |
| SR-G03 | `POST /api/v2/engine/session/restore-resume` | Snapshot missing | `404 SNAPSHOT_NOT_FOUND`, `guard_id=G-RESTORE-01` |
| SR-G04 | `POST /api/v2/engine/session/restore-resume` | Snapshot/profile incompatibility | `409 SNAPSHOT_INCOMPATIBLE`, includes `details.rule_id` |

## Restored-State Integrity Checklist
1. CPU and timing domain state present and valid in restore metadata.
2. Memory map baseline fields align with selected machine/profile contract.
3. Media attachment references (ROM/TOS/disk identifiers) are preserved or rejected per compatibility rule.
4. Input baseline configuration required fields are present post-restore.
5. Lifecycle state after restore matches requested `resume_mode` and emitted status envelope.

## Procedure Templates
- Save/restore cycle log template: pre-state, snapshot ID, restore mode, post-state, verdict.
- Compatibility triage template: failing rule ID, expected contract clause, observed payload, remediation note.
- Integrity audit template: checklist item, evidence pointer, pass/fail, blocking severity.

## Traceability Anchors
- Normative API references: `docs/EMU_ENGINE_V2_API_SPEC.md`
- Lifecycle contracts: `docs/emu_engine_v2/api_schema_lifecycle_v1.json`, `docs/emu_engine_v2/api_schema_lifecycle_transition_matrix_v1.json`
- Runtime implementation anchor: `components/esptari_web/esptari_web_lifecycle_state.c`

## Readiness Decision
CRT-003 readiness artifact is complete: compatibility scenarios, guard/error mappings, and restored-state integrity checks are specified and traceable to accepted lifecycle contracts.
