# Task Cards: Sprint 06 Phase-4 Hardening (S6-001 through S6-008)

This file decomposes unplanned Sprint 06 into pullable implementation tasks.

Phase intent:
- Execute phase-4 robustness/conformance hardening after S5 unlock closure.
- Keep slices XS/S and evidence-backed.
- Ensure each task emits deterministic artifacts suitable for acceptance review.

## TASK-ID: S6-001

- Epic: EPIC-08
- Objective: Build phase-4 robustness harness baseline and deterministic fault-injection matrix.
- Dependencies: S5-008
- Scope:
  - Define fault classes and deterministic injection points for runtime/control paths.
  - Build repeatable harness script entrypoints for S6 tasks.
  - Define canonical artifact naming and capture bundle structure.

Acceptance criteria:
1. Harness can execute deterministic injection profiles repeatedly.
2. Injection matrix covers critical runtime/control-plane fault classes.
3. Artifact layout is fixed and reusable by all downstream S6 tasks.

Evidence required:
- Harness script(s).
- Fault-injection matrix artifact.
- Baseline harness run capture.

## TASK-ID: S6-002

- Epic: EPIC-08
- Objective: Execute stress/fault recovery matrix and assert rollback/control-plane survivability.
- Dependencies: S6-001
- Scope:
  - Execute bounded stress runs with induced failures.
  - Verify deterministic recovery outcomes and state envelopes.
  - Verify post-fault API operability and deterministic error semantics.

Acceptance criteria:
1. Recovery outcomes are deterministic for each injected fault class.
2. Control-plane endpoints remain operable after fault handling.
3. Telemetry captures include recovery action and terminal state.

Evidence required:
- Stress/fault run capture artifacts.
- Recovery outcome matrix.
- Post-fault API probe evidence.

## TASK-ID: S6-003

- Epic: EPIC-08
- Objective: Execute long-run stability soak suite with deterministic health/safety thresholds.
- Dependencies: S6-001
- Scope:
  - Run long-duration soak profile with periodic health/status probes.
  - Detect crash/hang/reset regressions and classify outcomes.
  - Emit summary metrics for runtime stability envelope.

Acceptance criteria:
1. Soak run completes with deterministic pass/fail thresholds.
2. Health/status trend data is captured throughout run window.
3. Any instability reproduces with clear classification evidence.

Evidence required:
- Soak run log and summary report.
- Health/status trend capture.
- Regression classification notes (if any).

## TASK-ID: S6-004

- Epic: EPIC-08
- Objective: Validate catalog link-probe/dead-link/retry reliability against API 7.10 contracts.
- Dependencies: S6-001
- Scope:
  - Validate probe timeout classification and dead-link threshold behavior.
  - Validate mark-dead transitions and allow-dead-retry gating.
  - Validate deterministic blocker mapping and response envelopes.

Acceptance criteria:
1. Probe/dead/retry transitions match API 7.10 contract semantics.
2. Dead-link retry requires explicit override and remains deterministic.
3. Failure blockers map to canonical deterministic error envelopes.

Evidence required:
- Catalog reliability smoke captures.
- Transition state snapshots.
- Blocker/error mapping verification log.

## TASK-ID: S6-005

- Epic: EPIC-08
- Objective: Validate on-device scraper schedules including restart-recovery/quarantine behavior.
- Dependencies: S6-004
- Scope:
  - Validate schedule create/list/get/patch/delete behavior under load.
  - Validate restart-recovery recomputation and invalid-record quarantine path.
  - Validate deterministic due-order and catch-up behavior.

Acceptance criteria:
1. Schedule CRUD + runtime metadata semantics match API 7.11.
2. Restart-recovery produces deterministic recompute/quarantine results.
3. Due-order and catch-up behavior are deterministic and evidenced.

Evidence required:
- Scheduler CRUD/recovery captures.
- Recovery report artifact.
- Due-order/catch-up verification capture.

## TASK-ID: S6-006

- Epic: EPIC-08
- Objective: Validate hard SLO latency/jitter/dropped-frame targets and breach alarm semantics.
- Dependencies: S6-003
- Scope:
  - Execute SLO sampling windows over representative runtime conditions.
  - Validate threshold evaluation and alarm/event emission semantics.
  - Verify endpoint payload consistency under normal and breach conditions.

Acceptance criteria:
1. SLO collectors produce deterministic metric series and summaries.
2. Breach alarms emit with canonical fields and deterministic timing semantics.
3. API/event payloads remain contract-aligned during threshold crossings.

Evidence required:
- SLO run capture and summary.
- Alarm/event stream captures.
- Endpoint payload conformance samples.

## TASK-ID: S6-007

- Epic: EPIC-08
- Objective: Validate debug clock mode and single-step conformance checks with diagnostic payload integrity.
- Dependencies: S6-003
- Scope:
  - Validate realtime/slow-motion transitions and guard behavior.
  - Validate single-step execution control and deterministic step boundaries.
  - Validate opcode/bus-error diagnostic payload integrity.

Acceptance criteria:
1. Clock mode transitions are deterministic and guard-safe.
2. Single-step behavior is deterministic across repeated runs.
3. Diagnostic payloads include required fields and contract semantics.

Evidence required:
- Clock-mode/step conformance captures.
- Transition guard verification traces.
- Diagnostic payload integrity samples.

## TASK-ID: S6-008

- Epic: EPIC-08
- Objective: Assemble Sprint 06 hardening evidence packet and PO decision handoff.
- Dependencies: S6-002, S6-005, S6-006, S6-007
- Scope:
  - Consolidate S6 evidence into traceable decision packet.
  - Map each phase-4 objective to evidence and outcome status.
  - Publish residual risks and explicit next actions.

Acceptance criteria:
1. Packet provides 1:1 objective-to-evidence traceability.
2. Open risks and follow-up actions are explicit and owner-tagged.
3. Handoff is decision-ready for PO review.

Evidence required:
- Sprint 06 evidence packet.
- Traceability matrix.
- Decision template artifact.
