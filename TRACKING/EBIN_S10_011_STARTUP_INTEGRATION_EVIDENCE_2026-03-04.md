# EBIN-S10-011 Startup Integration Evidence (2026-03-04)

## Scope

- Task: `EBIN-S10-011`
- Story alignment: `E2-S1-T4` (startup defaults and startup-sequence integration)
- Gate scope: Integration execution evidence (not release authorization)

## Execution command

- `./tools/smoke_ebin_s10_011_startup_integration.sh`

## Runtime outcome

- `auth_header=configured`
- `determinism_runs=3`
- `health_check=ok attempts=1`
- `determinism_check=pass`
- `Smoke PASS`

## Determinism signature (stable across runs)

- `video=pal`
- `boot=floppy`
- `revision=startup_rev_03`
- `baseline=0x00FFFA07:0x00;0x00FFFA13:0x00`
- `phase=ready`
- `status=pass`
- `verify=BOOT-GLUE-RESET:pass;BOOT-MFP-RESET:pass;BOOT-ACIA-RESET:pass;BOOT-INT-MAP:pass`

Per-run signatures from execution output:

- `run_1_signature=video=pal;boot=floppy;revision=startup_rev_03;baseline=0x00FFFA07:0x00;0x00FFFA13:0x00;phase=ready;status=pass;verify=BOOT-GLUE-RESET:pass;BOOT-MFP-RESET:pass;BOOT-ACIA-RESET:pass;BOOT-INT-MAP:pass`
- `run_2_signature=video=pal;boot=floppy;revision=startup_rev_03;baseline=0x00FFFA07:0x00;0x00FFFA13:0x00;phase=ready;status=pass;verify=BOOT-GLUE-RESET:pass;BOOT-MFP-RESET:pass;BOOT-ACIA-RESET:pass;BOOT-INT-MAP:pass`
- `run_3_signature=video=pal;boot=floppy;revision=startup_rev_03;baseline=0x00FFFA07:0x00;0x00FFFA13:0x00;phase=ready;status=pass;verify=BOOT-GLUE-RESET:pass;BOOT-MFP-RESET:pass;BOOT-ACIA-RESET:pass;BOOT-INT-MAP:pass`

## Evidence artifacts

- Primary evidence file:
  - `captures/ebin_s10_011_startup_integration_20260304_162212.txt`
- Supporting script:
  - `tools/smoke_ebin_s10_011_startup_integration.sh`

## Decision note

`EBIN-S10-011` startup integration deterministic smoke execution is validated for this slice (`2026-03-04`) with no observed signature drift across 3 repeated runs.
