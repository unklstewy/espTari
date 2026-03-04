# EBIN-S10-009 Arbitration Integration Evidence (2026-03-04)

## Scope

- Task: `EBIN-S10-009`
- Story alignment: `E2-S1-T2` (arbitration integration)
- Gate scope: Integration start execution evidence (not release authorization)

## Execution command

- `./tools/smoke_ebin_s10_009_arbitration_integration.sh`

## Runtime outcome

- `auth_header=configured`
- `determinism_runs=3`
- `health_check=ok attempts=1`
- `determinism_check=pass`
- `Smoke PASS`

## Determinism signature (stable across runs)

- `owner_norm=0,1,2,3,4`
- `wait_norm=0,1,2,0,1`
- `dma_grants=granted,deferred,denied`
- `dma_requesters=fdc,blitter,memory_refresh`

Per-run signatures from execution output:

- `run_1_signature=owner_norm=0,1,2,3,4;wait_norm=0,1,2,0,1;dma_grants=granted,deferred,denied;dma_requesters=fdc,blitter,memory_refresh`
- `run_2_signature=owner_norm=0,1,2,3,4;wait_norm=0,1,2,0,1;dma_grants=granted,deferred,denied;dma_requesters=fdc,blitter,memory_refresh`
- `run_3_signature=owner_norm=0,1,2,3,4;wait_norm=0,1,2,0,1;dma_grants=granted,deferred,denied;dma_requesters=fdc,blitter,memory_refresh`

## Evidence artifacts

- Primary evidence file:
  - `captures/ebin_s10_009_arbitration_integration_20260304_152554.txt`
- Supporting script:
  - `tools/smoke_ebin_s10_009_arbitration_integration.sh`

## Decision note

`EBIN-S10-009` arbitration integration deterministic smoke execution is validated for this slice (`2026-03-04`) with no observed signature drift across 3 repeated runs.
