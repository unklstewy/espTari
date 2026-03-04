# S11 Open-Gates Acceptance Test Set (2026-03-04)

## Scope

Executable acceptance test set for currently open/conditional gates:
- GATE-S11-02
- GATE-S11-05
- GATE-S11-06
- GATE-S11-07
- GATE-S11-09
- GATE-S11-10
- GATE-S11-12
- GATE-S11-13

## Script

- tools/smoke_s11_open_gates_coverage.sh
- tools/smoke_s11_gate_07_input_translation.sh
- tools/smoke_s11_gate_09_capture_policy.sh
- tools/smoke_s11_gate_10_catalog_missing_asset.sh
- tools/smoke_s11_gate_12_save_restore.sh
- tools/smoke_s11_gate_13_sustained_slo.sh

## Commands

Run all open gates in one bundle:
- ./tools/smoke_s11_open_gates_coverage.sh

Run one gate only:
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-02
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-05
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-06
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-07
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-09
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-10
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-12
- ./tools/smoke_s11_open_gates_coverage.sh --gate GATE-S11-13

Run focused dedicated fixtures:
- ./tools/smoke_s11_gate_07_input_translation.sh
- ./tools/smoke_s11_gate_09_capture_policy.sh
- ./tools/smoke_s11_gate_10_catalog_missing_asset.sh
- ./tools/smoke_s11_gate_12_save_restore.sh
- SAMPLES=24 INTERVAL_SEC=0.5 ./tools/smoke_s11_gate_13_sustained_slo.sh

Follow-along in a visible terminal with periodic output:
- SAMPLES=120 INTERVAL_SEC=0.5 PROGRESS_EVERY=2 ./tools/smoke_s11_gate_13_sustained_slo.sh | tee captures/s11_gate_13_live_follow.log

## Output artifacts

- captures/s11_open_gates_coverage_<run_id>.txt
- captures/s11_open_gates_coverage_<run_id>.json

## Status interpretation

- pass: target route set is reachable and basic acceptance conditions are satisfied.
- conditional: route set exists but evidence depth is insufficient for hard closure.
- fail: required route set is missing or critical checks fail.

## Intended usage

1. Run after runtime changes for S11-001 and S11-002.
2. Attach bundle to S11 gate report refresh.
3. Use per-gate runs for focused debugging during implementation.
4. Prefer focused fixture scripts for closure evidence on `07/09/10/12/13`.
