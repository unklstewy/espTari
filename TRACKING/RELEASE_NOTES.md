# Release Notes

## 2026-03-04 — Sprint 09 API Contract Delta Governance Baseline (`S9-001`)

Scope:
- Completed `S9-001` by establishing canonical API contract-delta logging discipline and release-note linkage workflow.

Artifacts finalized:
- Delta log: `TRACKING/API_CONTRACT_DELTA_LOG.md`
- Delta template: `TRACKING/API_CONTRACT_DELTA_TEMPLATE.md`
- Delta workflow: `TRACKING/API_CONTRACT_DELTA_WORKFLOW.md`
- Tracking registry update: `TRACKING/README.md`

Verification summary:
- Canonical delta ledger created with initial entry `DELTA-20260304-001` tied to sprint task context.
- Required template fields and workflow gates now enforce release-note and acceptance-traceability linkage for contract-affecting changes.
- Tracking index now explicitly references all contract-delta governance artifacts for operational discoverability.

Release decision:
- **Approved** as Sprint 09 API contract-delta governance baseline increment.

## 2026-03-04 — Sprint 08 Active Profiles Closure (`S8-001` through `S8-008`)

Scope:
- Completed Sprint 08 phase-6 active-profile enablement and validation chain.

Artifacts finalized:
- S8 packet: `TRACKING/S8_PHASE6_ACTIVE_PROFILES_PACKET_2026-03-04.md`
- S8 traceability: `TRACKING/S8_PHASE6_ACTIVE_PROFILES_TRACEABILITY_MATRIX_2026-03-04.md`
- S8 decision template: `TRACKING/S8_PHASE6_ACTIVE_PROFILES_DECISION_TEMPLATE_2026-03-04.md`
- Evidence matrices: `TRACKING/evidence/s8_*_00{1..7}.json`
- Harness scripts: `tools/smoke_s8_*_00{1..7}.sh`
- Runtime captures/bundles: `captures/s8_*_20260304_0244*.{txt,json}`

Verification summary:
- Non-baseline profiles (`mega_st_pal`, `ste_pal`, `mega_ste_pal`) are active and start successfully under `machine=atari_st`.
- Session state projection now reports active machine/profile context for runtime checks.
- Active lifecycle, extension controls, media/catalog parity, pairwise compatibility, and switch-isolation slices all passed with deterministic guards preserved.

Release decision:
- **Approved** as Sprint 08 active-profile closure increment.

## 2026-03-04 — Sprint 07 Multi-Machine Packet + Decision Handoff (`S7-008`)

Scope:
- Completed `S7-008` by assembling Sprint 07 decision-ready packet artifacts.

Artifacts finalized:
- Packet: `TRACKING/S7_PHASE5_MULTI_MACHINE_PACKET_2026-03-04.md`
- Traceability matrix: `TRACKING/S7_PHASE5_MULTI_MACHINE_TRACEABILITY_MATRIX_2026-03-04.md`
- Decision template: `TRACKING/S7_PHASE5_MULTI_MACHINE_DECISION_TEMPLATE_2026-03-04.md`

Verification summary:
- Sprint 07 objective-to-evidence mapping is explicitly published for `S7-001` through `S7-008`.
- Residual risks and owner-tagged follow-up actions are explicit and aligned to remaining profile-enablement work.
- PO decision input is complete with recommendation, selectable decision outcomes, and condition capture fields.

Release decision:
- **Approved** as Sprint 07 packet/handoff closure increment.

## 2026-03-04 — Sprint 07 Profile-Switch Isolation + Fallback Semantics (`S7-007`)

Scope:
- Completed `S7-007` profile-switch isolation and fallback-semantics validation slice.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_profile_switch_isolation_matrix_007.json`
- Harness script: `tools/smoke_s7_profile_switch_isolation_007.sh`
- Smoke capture: `captures/s7_profile_switch_isolation_007_smoke_postfix_20260304_022557.txt`
- Bundle: `captures/s7_profile_switch_isolation_bundle_007_20260304_022557.json`

Verification summary:
- Guarded switch probes are deterministic for unavailable profiles (`ste_pal`, `mega_ste_pal` -> `MACHINE_PROFILE_NOT_FOUND`).
- Backward-compatible mismatch fallback remained canonical (`BAD_REQUEST` with `machine/profile mismatch`).
- Session context isolation held after guarded switch attempts (`state=running`, `machine=atari_st`, `profile=st_520_pal` with required envelope fields preserved).
- Post-switch control-plane probes remained operable and deterministic (`stream/control` success path, stream envelope reads, invalid-control fallback `BAD_REQUEST`, stopped-control fallback `ENGINE_NOT_RUNNING`, and health check pass).

Release decision:
- **Approved** as Sprint 07 profile-switch isolation and fallback-semantics increment.

## 2026-03-04 — Sprint 07 Cross-Profile ABI/Regression Guard Suite (`S7-006`)

Scope:
- Completed `S7-006` cross-profile compatibility matrix and ABI/regression guard validation slice.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_cross_profile_abi_regression_matrix_006.json`
- Harness script: `tools/smoke_s7_cross_profile_abi_regression_006.sh`
- Smoke capture: `captures/s7_cross_profile_abi_regression_006_smoke_postfix_20260304_022401.txt`
- Bundle: `captures/s7_cross_profile_abi_regression_bundle_006_20260304_022401.json`

Verification summary:
- Cross-profile guard matrix is deterministic in current runtime for unsupported profile surfaces (`ste_pal`, `mega_st_pal`, `mega_ste_pal` -> `MACHINE_PROFILE_NOT_FOUND`).
- Machine/profile mismatch and resolver guard semantics remained canonical (`BAD_REQUEST` with `machine/profile mismatch`; `EBIN_NOT_FOUND` with `resolver_machine_not_indexed`).
- ABI compatibility guard semantics remained canonical (`EBIN_ABI_MISMATCH` on incompatible major ABI validate request).
- Baseline regression envelope checks passed for session and stream payload contracts, and invalid metadata schema probes remained deterministic (`UNSUPPORTED_VERSION`).

Release decision:
- **Approved** as Sprint 07 cross-profile compatibility/ABI guard and regression-suite increment.

## 2026-03-04 — Sprint 07 Mega STe Extension Compatibility Deltas (`S7-005`)

Scope:
- Completed `S7-005` Mega STe extension compatibility-delta validation slice versus STe baseline extension contract behavior.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_mega_ste_extension_compatibility_matrix_005.json`
- Harness script: `tools/smoke_s7_mega_ste_extension_compatibility_005.sh`
- Smoke capture: `captures/s7_mega_ste_extension_compatibility_005_smoke_postfix_20260304_022142.txt`
- Bundle: `captures/s7_mega_ste_extension_compatibility_bundle_005_20260304_022142.json`

Verification summary:
- Baseline Atari ST stream extension-control contract checks remained deterministic for `video.fixed_fps` and `audio.fixed_hz` pacing payloads.
- Compatibility delta between `ste_pal` and `mega_ste_pal` profile probes is explicit: both are canonically guarded with `MACHINE_PROFILE_NOT_FOUND`, and manifest detail-path suffixes are distinct (`/ste_pal.json` vs `/mega_ste_pal.json`).
- Unsupported extension requests retained canonical fallback semantics (`BAD_REQUEST` for invalid pacing mode and `UNSUPPORTED_VERSION` for invalid metadata schema version).
- Stopped-state control fallback remains deterministic at `ENGINE_NOT_RUNNING`.

Release decision:
- **Approved** as Sprint 07 Mega STe compatibility-delta and fallback-semantic validation increment.

## 2026-03-04 — Sprint 07 STe Extension Control Validation (`S7-004`)

Scope:
- Completed `S7-004` STe extension-control validation slice for audio/video deltas.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_ste_extension_controls_matrix_004.json`
- Harness script: `tools/smoke_s7_ste_extension_controls_004.sh`
- Smoke capture: `captures/s7_ste_extension_controls_004_smoke_postfix_20260304_021502.txt`
- Bundle: `captures/s7_ste_extension_controls_bundle_004_20260304_021502.json`

Verification summary:
- Baseline Atari ST audio/video control payloads via `POST /api/v2/stream/control` are deterministic and contract-aligned (`video.fixed_fps`, `audio.fixed_hz`).
- Invalid extension usage maps to canonical deterministic guards (`BAD_REQUEST` for invalid pacing mode and `UNSUPPORTED_VERSION` for unsupported metadata schema).
- Profile-aware STe context remains canonically guarded in current runtime (`MACHINE_PROFILE_NOT_FOUND` for `profile=ste_pal`).
- Backward-compatible stream envelopes are preserved on baseline (`video`/`audio` required fields present, extension field `ste_extensions` absent), and stopped control invocation is deterministic (`ENGINE_NOT_RUNNING`).

Release decision:
- **Approved** as Sprint 07 STe extension-control guard/envelope validation increment.

## 2026-03-04 — Sprint 07 Mega ST Media/Catalog Run-Path Parity (`S7-003`)

Scope:
- Completed `S7-003` Mega ST media/catalog/session run-path parity validation slice.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_mega_st_media_catalog_matrix_003.json`
- Harness script: `tools/smoke_s7_mega_st_media_catalog_003.sh`
- Smoke capture: `captures/s7_mega_st_media_catalog_003_smoke_postfix_20260304_021043.txt`
- Bundle: `captures/s7_mega_st_media_catalog_bundle_003_20260304_021043.json`

Verification summary:
- Atari ST baseline run-path continuity passed across catalog lookup, ROM attach, disk attach, and disk eject flows.
- Session continuity checks passed after media operations (`running`, `machine=atari_st`, `profile=st_520_pal`).
- Missing/incompatible media blockers remained deterministic and canonical (`CATALOG_ENTRY_NOT_FOUND`, `MEDIA_ATTACH_FAILED`).
- Mega ST context remains canonically guarded in current runtime (`MACHINE_PROFILE_NOT_FOUND`), and media attach from stopped state correctly returns `INVALID_SESSION_STATE`.

Release decision:
- **Approved** as Sprint 07 media/catalog parity and blocker-mapping increment.

## 2026-03-04 — Sprint 07 Mega ST Lifecycle/API Parity Validation (`S7-002`)

Scope:
- Completed `S7-002` Mega ST bootstrap/lifecycle/API parity validation slice.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_mega_st_lifecycle_matrix_002.json`
- Harness script: `tools/smoke_s7_mega_st_lifecycle_002.sh`
- Smoke capture: `captures/s7_mega_st_lifecycle_002_smoke_postfix_20260304_020733.txt`
- Bundle: `captures/s7_mega_st_lifecycle_bundle_002_20260304_020733.json`

Verification summary:
- Atari ST baseline lifecycle parity passed for `pause`, `resume`, `reset`, and `stop` transitions with state projections verified by `GET /api/v2/engine/session`.
- Core session payload parity fields (`session_id`, `state`, `run_mode`, `machine`, `profile`, `tick_counter`, `cycle_counter`) were validated in running and stopped projections.
- Mega ST bootstrap path currently blocks deterministically via canonical guards (`MACHINE_PROFILE_NOT_FOUND` for `mega_st_pal`, `BAD_REQUEST` for machine/profile mismatch).
- Stopped-state lifecycle guards passed with canonical `INVALID_SESSION_STATE` on `pause`, `resume`, and `reset`.

Release decision:
- **Approved** as Sprint 07 Mega ST lifecycle/guard parity increment.

## 2026-03-04 — Sprint 07 Multi-Machine Baseline Harness (`S7-001`)

Scope:
- Completed `S7-001` multi-machine profile baseline harness and deterministic profile contract matrix.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s7_multi_machine_profile_matrix_001.json`
- Harness script: `tools/smoke_s7_multi_machine_profile_001.sh`
- Smoke capture: `captures/s7_multi_machine_profile_001_smoke_postfix_20260304_020049.txt`
- Bundle: `captures/s7_multi_machine_profile_bundle_001_20260304_020049.json`

Verification summary:
- Baseline `atari_st` session bootstrap and profile wiring checks passed (`machine=atari_st`, `profile=st_520_pal`, `validated_modules=5`).
- Session-state parity checks passed with deterministic running-state projection.
- Unsupported multi-machine surfaces currently block deterministically (`BAD_REQUEST` machine/profile mismatch or `MACHINE_PROFILE_NOT_FOUND` profile missing).
- `ebins/resolve` guard semantics for unsupported machine (`mega_st`) passed with canonical `EBIN_NOT_FOUND` + `resolver_machine_not_indexed` reason.

Release decision:
- **Approved** as Sprint 07 baseline/matrix initialization increment.

## 2026-03-04 — Sprint 06 Hardening Packet Assembly (`S6-008`)

Scope:
- Completed `S6-008` Sprint 06 hardening evidence packet and PO decision handoff assembly.

Artifacts finalized:
- Review packet: `TRACKING/S6_PHASE4_HARDENING_PACKET_2026-03-04.md`
- Traceability matrix: `TRACKING/S6_PHASE4_HARDENING_TRACEABILITY_MATRIX_2026-03-04.md`
- Decision template: `TRACKING/S6_PHASE4_HARDENING_DECISION_TEMPLATE_2026-03-04.md`

Verification summary:
- `S6-001` through `S6-008` objective-to-evidence mapping is published and complete.
- Residual risks and next actions are explicitly documented.
- Handoff package is decision-ready for PO acceptance workflow.

Release decision:
- **Approved** as Sprint 06 packet/handoff closure increment.

## 2026-03-04 — Sprint 06 Debug Clock + Single-Step Conformance (`S6-007`)

Scope:
- Completed `S6-007` debug clock mode and single-step conformance validation.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_debug_step_matrix_007.json`
- Harness script: `tools/smoke_s6_debug_step_007.sh`
- Smoke capture: `captures/s6_debug_step_007_smoke_postfix_20260304_014432.txt`
- Bundle: `captures/s6_debug_step_bundle_007_20260304_014432.json`

Verification summary:
- Deterministic clock mode transitions validated (`realtime`/`slow_motion`/`single_step`) with canonical invalid-ratio guard.
- Single-step determinism validated across step/capture checks and tick counter invariants.
- Opcode and bus-error diagnostic payload schemas validated; forced-failure check-id guards verified.

Release decision:
- **Approved** as Sprint 06 debug conformance increment.

## 2026-03-04 — Sprint 06 SLO + Alarm Semantics Validation (`S6-006`)

Scope:
- Completed `S6-006` SLO latency/jitter/dropped-frame and alarm-semantic hardening checks.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_slo_alarm_matrix_006.json`
- Harness script: `tools/smoke_s6_slo_alarm_006.sh`
- Smoke capture: `captures/s6_slo_alarm_006_smoke_postfix_20260304_014346.txt`
- Bundle: `captures/s6_slo_alarm_bundle_006_20260304_014346.json`

Verification summary:
- Collector activation and revision semantics validated.
- Sample series monotonicity and deterministic breach presence validated for jitter/drop metrics.
- Threshold payload contract and alarm alternation/timestamp semantics validated.

Release decision:
- **Approved** as Sprint 06 SLO/alarm hardening increment.

## 2026-03-04 — Sprint 06 Scheduler Recovery Validation (`S6-005`)

Scope:
- Completed `S6-005` scheduler CRUD/recovery validation aligned to API `7.11` semantics.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_scheduler_recovery_matrix_005.json`
- Harness script: `tools/smoke_s6_scheduler_recovery_005.sh`
- Smoke capture: `captures/s6_scheduler_recovery_005_smoke_postfix_20260304_013415.txt`
- Bundle: `captures/s6_scheduler_recovery_bundle_005_20260304_013415.json`

Verification summary:
- Schedule create/conflict semantics validated with deterministic `CONFLICT` blocker.
- Schedule patch/get metadata semantics validated, including monotonic `updated_at_us` and canonical invalid patch blocker (`SCRAPER_SCHEDULE_INVALID`).
- Deterministic list ordering validated (`next_run_at_us`, then `schedule_id` tie-break).
- Recovery report schema and quarantine canonical error-code checks validated.
- Post-delete control-plane survivability verified via health probe.

Release decision:
- **Approved** as Sprint 06 scheduler recovery hardening increment.

## 2026-03-04 — Sprint 06 Catalog Reliability Validation (`S6-004`)

Scope:
- Completed `S6-004` catalog probe/dead/retry reliability validation aligned to API `7.10`.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_catalog_reliability_matrix_004.json`
- Harness script: `tools/smoke_s6_catalog_reliability_004.sh`
- Smoke capture: `captures/s6_catalog_reliability_004_smoke_postfix_20260304_012443.txt`
- Bundle: `captures/s6_catalog_reliability_bundle_004_20260304_012443.json`

Verification summary:
- Probe timeout policy path executed deterministically with worker metadata.
- Invalid timeout configuration returned canonical `BAD_REQUEST` blocker.
- Dead link blocked download without override (`CATALOG_LINK_DEAD`).
- Dead retry failure path incremented retry-failure telemetry and preserved `dead` state.
- Dead retry success path recovered entry to `online` with retry-success telemetry increment.

Release decision:
- **Approved** as Sprint 06 catalog reliability hardening increment.

## 2026-03-04 — Sprint 06 Long-Run Stability Soak Baseline (`S6-003`)

Scope:
- Completed `S6-003` deterministic soak profile and stability-threshold validation.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_long_run_stability_matrix_003.json`
- Harness script: `tools/smoke_s6_stability_soak_003.sh`
- Smoke capture: `captures/s6_stability_soak_003_smoke_postfix_20260304_012053.txt`
- Bundle: `captures/s6_stability_soak_bundle_003_20260304_012053.json`

Verification summary:
- `24` sampled health/status checks over bounded soak interval completed successfully.
- Thresholds met with no transport or parse failures (`health_pass=24`, `status_pass=24`, `transport_fail=0`, `parse_fail=0`).
- Stability baseline artifacts are now available as inputs for downstream S6 hardening tasks.

Release decision:
- **Approved** as Sprint 06 long-run stability baseline increment.

## 2026-03-04 — Sprint 06 Stress/Fault Recovery Validation (`S6-002`)

Scope:
- Completed `S6-002` stress/fault recovery execution matrix and control-plane survivability validation.

Artifacts finalized:
- Matrix: `TRACKING/evidence/s6_stress_fault_recovery_matrix_002.json`
- Harness script: `tools/smoke_s6_fault_recovery_002.sh`
- Smoke capture: `captures/s6_fault_recovery_002_smoke_postfix_20260304_011725.txt`
- Bundle: `captures/s6_fault_recovery_bundle_002_20260304_011725.json`

Verification summary:
- Deterministic stress run completed for `3` cycles.
- Recovery assertions passed for rollback (`3/3`), fallback (`3/3`), and unload-drain failure recovery (`3/3`).
- Post-fault control-plane survivability probes passed for all cycle checkpoints (`9/9`).

Release decision:
- **Approved** as Sprint 06 stress/fault recovery increment.

## 2026-03-04 — Sprint 06 Phase-4 Harness Baseline (`S6-001`)

Scope:
- Completed `S6-001` to establish phase-4 robustness harness baseline and deterministic fault-injection matrix.

Artifacts finalized:
- Fault matrix: `TRACKING/evidence/s6_fault_injection_matrix_001.json`
- Harness script: `tools/smoke_s6_phase4_harness_001.sh`
- Smoke capture: `captures/s6_phase4_harness_001_smoke_postfix_20260304_011354.txt`
- Bundle: `captures/s6_phase4_harness_bundle_001_20260304_011354.json`

Verification summary:
- Deterministic fault classes executed with expected error outcomes.
- Post-fault control-plane survivability probes passed after each scenario (`GET /api/v2/engine/health`).

Release decision:
- **Approved** as Sprint 06 baseline/harness setup increment.

## 2026-03-04 — S5 Runtime Unlock Closure

Scope:
- Closed and accepted `S5-001` through `S5-008` runtime unlock chain.

Artifacts finalized:
- ABI contract baseline: `docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md`
- Runtime unlock packet: `TRACKING/S5_RUNTIME_UNLOCK_PACKET_2026-03-04.md`
- Traceability matrix: `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`
- Decision template: `TRACKING/S5_RUNTIME_UNLOCK_DECISION_TEMPLATE_2026-03-04.md`
- Harness bundle evidence: `captures/s5_runtime_unlock_bundle_007_20260304_004226.json`

Verification summary:
- Deterministic checks passed for resolver, validator, load safety gates, load/unload transitions, rollback/fallback recovery, and end-to-end harness scenarios.
- Acceptance chain recorded in `TRACKING/tracking.db` and log entry added to `TRACKING/ACCEPTANCE_LOG.md`.

Release decision:
- **Approved for release** as S5 runtime unlock tranche.

Operational note:
- Decision packet is ready for PO signature workflow using the included template.
- Sprint-level closure summary is published at `TRACKING/SPRINT_02_CLOSURE_REPORT_2026-03-04.md`.
- One-page status roll-up is published at `TRACKING/PROGRAM_STATUS_SNAPSHOT_2026-03-04.md`.
- Acceptance log historical CRT rows were normalized to explicit superseded-context `Deferred` records for clearer current-state reading.

## 2026-03-02 — Contracts + Tracking Tranche Closeout

Scope:
- Closed and accepted task tranche `T-054` through `T-121` under contract-first delivery mode.

Artifacts finalized:
- API contract baseline and task-slice extensions: `docs/EMU_ENGINE_V2_API_SPEC.md`
- Implementation architecture cross-references: `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md`
- Task-card closeout state: `TRACKING/TASK_CARDS_T054_T121.md`
- Board reconciliation: `TRACKING/KANBAN_BOARD.md`
- Acceptance decisions: `TRACKING/ACCEPTANCE_LOG.md`

Verification summary:
- Cross-file deterministic reconciliation passed for `T-054..T-121`:
  - all task cards `Status: Done`
  - all Done checklist items checked
  - board lanes aligned to closed range
  - acceptance coverage present via per-task and/or range entries

Release decision:
- **Approved for release** as a closed contract tranche.

Operational note:
- This tranche is accepted on contract coverage + tracking synchronization.
- Next tranche transitions to contract-to-runtime verification (code-path execution evidence).
