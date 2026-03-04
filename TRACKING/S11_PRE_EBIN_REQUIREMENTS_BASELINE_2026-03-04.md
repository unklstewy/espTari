# S11 Pre-EBIN Development Requirements Baseline (2026-03-04)

## Purpose

Define the minimum requirements to reach engineering confidence before active development of emulated EBIN devices for Atari ST.

This baseline is sourced from:
- `TRACKING/S10_005_REVIEW_DECISION_AND_S11_TRANSITION_2026-03-04.md`
- `TRACKING/S9_005_DRY_RUN_GATE_EXECUTION_2026-03-04.md`
- `TRACKING/S9_002_SECURITY_INTEGRITY_CLOSURE_REPORT_2026-03-04.md`
- `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` (Section 11)
- `docs/EMU_ENGINE_V2_API_SPEC.md` (Section 8 + related guard contracts)

## Requirement status model (S11)

- `required_before_ebin_device_dev`: must be complete before coding new emulated EBIN devices.
- `required_for_first_ebin_integration`: must be complete before first EBIN device is integrated into runtime path.
- `required_for_release_gate`: required for milestone hard-validation/release decision.

## Requirements

| Req ID | Requirement | Source anchor | Required level |
|---|---|---|---|
| PRE-EBIN-01 | Register snapshot + stream parity runtime path is executable with deterministic evidence | S11-CAND-01, GATE-S11-05 | required_before_ebin_device_dev |
| PRE-EBIN-02 | Bus/memory filtered trace runtime path is executable with deterministic evidence | S11-CAND-01, GATE-S11-06 | required_before_ebin_device_dev |
| PRE-EBIN-03 | SD-only media resolver/runtime enforcement is executable and regression-covered | S11-CAND-02, GATE-S11-02 | required_before_ebin_device_dev |
| PRE-EBIN-04 | EBIN ABI/manifest compatibility contract is frozen (v1) with deterministic validator outcomes (`EBIN_NOT_FOUND`, `EBIN_ABI_MISMATCH`, `EBIN_DEPENDENCY_MISSING`) | API spec section 8 + profile wiring guard contracts | required_before_ebin_device_dev |
| PRE-EBIN-05 | Security invariants for EBIN/file paths remain enforced (auth scope, path allowlist, traversal rejection, integrity/audit logging) | S9-002 closure controls AUTH/PATH/UPLOAD/EBIN | required_for_first_ebin_integration |
| PRE-EBIN-06 | Current-cycle reconfirmation fixtures exist for input/capture/catalog/save-restore criteria (`GATE-S11-07/09/10/12`) | S11-CAND-04 + OBL-005-02/03 | required_for_first_ebin_integration |
| PRE-EBIN-07 | Sustained SLO measurement windows are available for hard-threshold validation (`GATE-S11-13`) | S11-CAND-03 + OBL-005-03 | required_for_release_gate |
| PRE-EBIN-08 | Hard-validation release-gate report can be produced with deterministic decision outcome and evidence links | S9-005 template obligations | required_for_release_gate |

## Comfort threshold to start EBIN device coding

Engineering comfort threshold is met when:
1. `PRE-EBIN-01` through `PRE-EBIN-04` are complete.
2. At least one end-to-end harness run demonstrates the new runtime paths produce schema-consistent evidence artifacts.
3. No regression is introduced in S9-002 security controls.

## Scope guard (anti-deadlock rule)

To prevent process deadlock between pre-implementation and post-implementation criteria:
- `PRE-EBIN-07` and `PRE-EBIN-08` MUST NOT block start of EBIN device coding.
- `PRE-EBIN-07` and `PRE-EBIN-08` DO block first EBIN runtime integration and release-gate signoff.
- `S11-008` decision output must explicitly report separate outcomes for:
	1. start of EBIN device development,
	2. first EBIN runtime integration,
	3. release hard-validation readiness.

## Mapping to S11 tasks

- `PRE-EBIN-01` -> `S11-001`
- `PRE-EBIN-02` -> `S11-001`
- `PRE-EBIN-03` -> `S11-002`
- `PRE-EBIN-04` -> `S11-003`
- `PRE-EBIN-05` -> `S11-004`
- `PRE-EBIN-06` -> `S11-005`
- `PRE-EBIN-07` -> `S11-006`
- `PRE-EBIN-08` -> `S11-007`
- Sprint handoff/closure -> `S11-008`
