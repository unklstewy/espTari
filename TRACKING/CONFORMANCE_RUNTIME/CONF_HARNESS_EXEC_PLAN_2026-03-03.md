# Conformance Harness Execution Plan — 2026-03-03

Scope: P0-3 activation for `T-092` and `T-093`.

## Objective

Define the first executable conformance-harness flow and evidence pipeline skeleton for runtime validation under `unlock_with_conditions` controls.

## Task linkage

- `T-092` (In Progress): harness scaffold + test manifest loader
- `T-093` (In Progress): artifact collection + report packaging flow
- Follow-on: `T-094`, `T-095` (Ready)

## Execution model

1. Load conformance manifest (`conformance_manifest_v1`) from deterministic path.
2. Resolve selected checks/scenarios to runnable suite entries.
3. Execute suite with deterministic run metadata (`run_id`, timestamp, firmware revision, profile).
4. Collect raw evidence artifacts per check (stdout/json/log snippets).
5. Produce normalized report package for acceptance review.

## Planned harness interfaces (scaffold)

- `manifest_load(path) -> manifest`
- `suite_select(manifest, selectors) -> suite`
- `suite_run(suite, context) -> run_result`
- `artifact_collect(run_result, out_dir) -> artifact_index`
- `report_package(artifact_index, out_dir) -> review_bundle`

## Minimal deliverables for this activation

- Manifest loader skeleton with strict schema validation stubs.
- Run context schema (session/profile/build metadata) definition.
- Artifact index schema (`artifact_id`, `check_id`, `path`, `hash`, `kind`).
- Report package envelope (`summary`, `pass_count`, `fail_count`, `artifacts[]`).

## Condition guardrails (must remain true)

- Startup readiness gate passes before any runtime harness execution.
- Rollback pathway is documented and available for each run iteration.
- Regression reruns preserve deterministic fixture controls and run metadata.

## Planned acceptance checkpoints

1. Harness scaffold can parse a manifest and emit deterministic selection output.
2. Artifact collector writes index and hash metadata for each artifact.
3. Report packager emits a single review bundle file with pass/fail counts.

## Output targets

- `TRACKING/CONFORMANCE_RUNTIME/CONF_ARTIFACT_SCHEMA_2026-03-03.md`
- `captures/conf_harness_smoke_20260303.txt`
