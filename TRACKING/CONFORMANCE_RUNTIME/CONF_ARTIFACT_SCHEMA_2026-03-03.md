# Conformance Artifact Schema — 2026-03-03

Scope: P0-3 evidence scaffolding for conformance harness runtime artifacts.

## 1) Run metadata schema (`conformance_run_meta_v1`)

Required fields:

- `run_id` (string)
- `started_at_utc` (string, ISO-8601)
- `finished_at_utc` (string, ISO-8601)
- `firmware_rev` (string)
- `profile_id` (string)
- `manifest_id` (string)
- `unlock_mode` (enum: `unlock_with_conditions`)

## 2) Per-check result schema (`conformance_check_result_v1`)

Required fields:

- `check_id` (string)
- `status` (enum: `pass`, `fail`, `error`, `skipped`)
- `duration_ms` (integer)
- `message` (string)
- `artifact_refs` (array of `artifact_id`)

## 3) Artifact index schema (`conformance_artifact_index_v1`)

Required fields:

- `artifact_id` (string)
- `check_id` (string)
- `kind` (enum: `log`, `json`, `text`, `capture`, `bundle`)
- `path` (string)
- `sha256` (string)
- `size_bytes` (integer)
- `created_at_utc` (string)

## 4) Report bundle schema (`conformance_report_bundle_v1`)

Required fields:

- `run_meta` (`conformance_run_meta_v1`)
- `summary`:
  - `total_checks` (integer)
  - `pass_count` (integer)
  - `fail_count` (integer)
  - `error_count` (integer)
  - `skipped_count` (integer)
- `results` (array of `conformance_check_result_v1`)
- `artifacts` (array of `conformance_artifact_index_v1`)

## 5) Naming convention

- Capture files: `captures/conf_<domain>_<yyyymmdd_hhmmss>.txt`
- Bundle files: `captures/conf_bundle_<run_id>.json`
- Index files: `captures/conf_artifacts_<run_id>.json`

## 6) Determinism requirements

- `run_id` and `manifest_id` must be persisted in every bundle.
- Artifact hashes must be generated after write completion.
- Any rerun must include `rerun_of_run_id` in metadata when applicable.
