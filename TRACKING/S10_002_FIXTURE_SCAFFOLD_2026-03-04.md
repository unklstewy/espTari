# S10-002 Fixture / Harness Scaffold (2026-03-04)

## Scope

- Task: `S10-002`
- Purpose: provide deterministic placeholder fixture paths for emulated-hardware-dependent criteria.
- Output model: `baseline_observed`, `needs_data`, `blocked_by_missing_engine`.

## Placeholder fixture registry

| Fixture ID | Target criterion(s) | Invocation path | Expected readiness marker |
|---|---|---|---|
| FX-S10-002-REG | GATE-S11-05 | `register_snapshot_stream_parity_probe` | `blocked_by_missing_engine` until register stream implementation path is available |
| FX-S10-002-BUS | GATE-S11-06 | `bus_memory_filter_probe` | `blocked_by_missing_engine` until bus/memory filter stream path is available |
| FX-S10-002-SD | GATE-S11-02 | `sd_only_media_enforcement_probe` | `blocked_by_missing_engine` until SD-only runtime media resolver path is fully executable |
| FX-S10-002-CAPTURE | GATE-S11-09 | `browser_capture_mode_matrix_probe` | `needs_data` until capture mode transitions are observed in current-cycle runtime |

## Harness command

- Script: `tools/smoke_s10_integration_readiness_002.sh`
- Command: `./tools/smoke_s10_integration_readiness_002.sh`
- Outputs:
  - `captures/s10_integration_readiness_002_<run_id>.txt`
  - `captures/s10_integration_readiness_002_<run_id>.json`

## Deterministic marker policy

- A fixture is emitted as `blocked_by_missing_engine` when the corresponding runtime capability is not implemented or not executable in current phase.
- A fixture is emitted as `needs_data` when endpoint/harness path exists but sampled coverage is insufficient.
- A fixture is emitted as `baseline_observed` when the path executes and emits schema-shaped evidence.
