# EBIN-S10-013 Conformance Integration Evidence (2026-03-04)

## Scope

- Task: `EBIN-S10-013`
- Story alignment: `E2-S1-T6` (conformance harness/subsystem integration)
- Gate scope: Integration execution evidence (not release authorization)

## Execution command

- `./tools/smoke_ebin_s10_013_conformance_integration.sh`

## Runtime outcome

- `auth_header=configured`
- `determinism_runs=3`
- `health_check=ok attempts=1`
- `determinism_check=pass`
- `Smoke PASS`

## Determinism signature (stable across runs)

- `manifest=1:1`
- `artifacts=logs,metrics`
- `suite=2:2:0`
- `report=2:2:0`
- `cases=timer_a_irq:pass`

Per-run signatures from execution output:

- `run_1_signature=manifest=1:1;artifacts=logs,metrics;suite=2:2:0;report=2:2:0;cases=timer_a_irq:pass`
- `run_2_signature=manifest=1:1;artifacts=logs,metrics;suite=2:2:0;report=2:2:0;cases=timer_a_irq:pass`
- `run_3_signature=manifest=1:1;artifacts=logs,metrics;suite=2:2:0;report=2:2:0;cases=timer_a_irq:pass`

## Evidence artifacts

- Primary evidence file:
  - `captures/ebin_s10_013_conformance_integration_20260304_163001.txt`
- Supporting script:
  - `tools/smoke_ebin_s10_013_conformance_integration.sh`

## Decision note

`EBIN-S10-013` conformance integration deterministic smoke execution is validated for this slice (`2026-03-04`) with no observed signature drift across 3 repeated runs.
