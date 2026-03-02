# 6. API architecture (backend only)

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 15

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
