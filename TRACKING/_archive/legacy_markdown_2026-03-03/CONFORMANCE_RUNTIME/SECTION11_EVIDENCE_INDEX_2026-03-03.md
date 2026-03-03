# Section 11 Evidence Index — 2026-03-03

Scope: Canonical evidence index for Section 11 acceptance criteria execution.

Related matrix: `TRACKING/CONFORMANCE_RUNTIME/SECTION11_ACCEPTANCE_MATRIX_2026-03-03.md`

## Existing evidence linked (baseline)

- `TRACKING/ACCEPTANCE_LOG.md`
- `TRACKING/PRQ_WORKING/PRQ-001_REVIEW_PACKET.md`
- `TRACKING/PRQ_WORKING/PRQ-002_FIXTURE_SCENARIO_PACKAGE.md`
- `TRACKING/PRQ_WORKING/PRQ-003_DEPLOYMENT_WORKFLOW_RUNBOOK.md`
- `TRACKING/PRQ_WORKING/PRQ-004_UNLOCK_REVIEW_PACKET.md`
- `captures/lifecycle_residual_closure_20260302.txt`
- `captures/input_mapping_negcase_20260302.txt`
- `captures/clock_mode_semantics_20260302_191117.txt`
- `captures/clock_step_semantics_20260302_191501.txt`
- `captures/clock_step_610a_runtime_20260302_192854.txt`
- `captures/metrics_611_semantics_20260302_193256.txt`
- `captures/catalog_79_semantics_1772498372.txt`
- `captures/catalog_710_semantics_1772498777.txt`

## Criterion-to-evidence mapping table

| Criterion # | Required Evidence Artifact(s) | Existing Baseline | New Artifact Slot(s) |
|---|---|---|---|
| 1 | Lifecycle transition conformance run | Partial | `captures/s11_lifecycle_conformance_<ts>.txt` |
| 2 | SD-card-only media path enforcement run | Partial | `captures/s11_media_sd_only_<ts>.txt` |
| 3 | EBIN remote lifecycle validation run | Partial | `captures/s11_ebin_lifecycle_<ts>.txt` |
| 4 | Video/audio stream conformance run | Partial | `captures/s11_stream_av_<ts>.txt` |
| 5 | Register snapshot-stream consistency run | Partial | `captures/s11_register_consistency_<ts>.txt` |
| 6 | Bus/memory filter conformance run | Partial | `captures/s11_bus_memory_filters_<ts>.txt` |
| 7 | Input translation + mapping conformance run | Partial | `captures/s11_input_translation_<ts>.txt` |
| 8 | Backpressure/drop metrics load-run | Partial | `captures/s11_backpressure_load_<ts>.txt` |
| 9 | Capture policy/mode transition matrix run | Partial | `captures/s11_capture_policy_<ts>.txt` |
| 10 | Catalog-backed missing asset workflow run | Partial | `captures/s11_catalog_missing_asset_<ts>.txt` |
| 11 | Dead-link + schedule refresh reliability run | Partial | `captures/s11_dead_link_schedule_<ts>.txt` |
| 12 | Save/restore compatibility run | Partial | `captures/s11_save_restore_compat_<ts>.txt` |
| 13 | SLO measurement report | Partial | `captures/s11_slo_validation_<ts>.txt` |
| 14 | Debug clock observability correctness run | Partial | `captures/s11_debug_clock_obs_<ts>.txt` |

## Reporting bundle slots

- `captures/s11_bundle_<run_id>.json`
- `captures/s11_artifacts_<run_id>.json`
- `TRACKING/CONFORMANCE_RUNTIME/S11_RUN_REPORT_<run_id>.md`

## Maintenance rule

Every time a Section 11 criterion is upgraded to `PASS`, append the exact artifact paths and update the corresponding row in `SECTION11_ACCEPTANCE_MATRIX_2026-03-03.md`.
