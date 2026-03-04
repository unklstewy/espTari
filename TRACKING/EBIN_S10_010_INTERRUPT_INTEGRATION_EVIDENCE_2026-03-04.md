# EBIN-S10-010 Interrupt Integration Evidence (2026-03-04)

## Scope

- Task: `EBIN-S10-010`
- Story alignment: `E2-S1-T3` (interrupt hierarchy and wiring integration)
- Gate scope: Integration execution evidence (not release authorization)

## Execution command

- `./tools/smoke_ebin_s10_010_interrupt_integration.sh`

## Runtime outcome

- `auth_header=configured`
- `determinism_runs=3`
- `health_check=ok attempts=1`
- `determinism_check=pass`
- `Smoke PASS`

## Determinism signature (stable across runs)

- `routes=vbl,mfp,acia,fdc`
- `wiring=irq7:28,irq6:38,irq4:24,irq3:54`
- `results=pass,pass,pass,pass`

Per-run signatures from execution output:

- `run_1_signature=routes=vbl,mfp,acia,fdc;wiring=irq7:28,irq6:38,irq4:24,irq3:54;results=pass,pass,pass,pass`
- `run_2_signature=routes=vbl,mfp,acia,fdc;wiring=irq7:28,irq6:38,irq4:24,irq3:54;results=pass,pass,pass,pass`
- `run_3_signature=routes=vbl,mfp,acia,fdc;wiring=irq7:28,irq6:38,irq4:24,irq3:54;results=pass,pass,pass,pass`

## Evidence artifacts

- Primary evidence file:
  - `captures/ebin_s10_010_interrupt_integration_20260304_155304.txt`
- Supporting script:
  - `tools/smoke_ebin_s10_010_interrupt_integration.sh`

## Decision note

`EBIN-S10-010` interrupt integration deterministic smoke execution is validated for this slice (`2026-03-04`) with no observed signature drift across 3 repeated runs.
