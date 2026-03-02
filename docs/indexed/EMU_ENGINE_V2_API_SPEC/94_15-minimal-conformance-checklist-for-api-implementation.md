# 15. Minimal conformance checklist for API implementation

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 94

## Subsections

- 15.1 Conformance harness scaffold and test manifest loader
- 15.2 Subsystem conformance test scaffold and fixture model
- 15.3 Per-subsystem acceptance suites and reporting output

---

## 15. Minimal conformance checklist for API implementation

1. Start/pause/resume/reset/stop endpoints behave with valid state transitions.
2. SD-card file manager enforces path policy and staging upload workflow.
3. EBIN catalog/validate/load/unload lifecycle functions are operational.
4. Video and audio streams emit metadata + binary payload with consistent timestamps.
5. Input APIs translate keyboard/mouse/game-controller host events into virtual machine events deterministically.
6. Browser input capture supports enable/disable, `mouse_over` mode, `click_to_capture` mode, and escape-sequence release.
7. Register/bus/memory streams support filtering and report dropped-event metrics.
8. Snapshot/checkpoint APIs return consistent and restorable state references.
9. Error envelope and code catalog are consistent across all endpoints.
10. Catalog APIs can identify missing local assets and download from hosted URLs.
11. Link-probe APIs update availability state and support dead-link marking with timestamps/reasons.
12. On-device scraper jobs and periodic schedules execute and expose status/history via API.
13. Suspend-save and restore-resume endpoints support state `suspended` with valid transitions and failure codes.
14. Performance metrics endpoint reports latency, jitter, and dropped-frame percentages against hard targets.
15. Debug clock mode and step endpoints support realtime, slow-motion, and single-step execution control.

### 15.1 Conformance harness scaffold and test manifest loader

Conformance harness scaffold contract:

- `POST /api/v2/conformance/harness/session`
- Request fields:
  - `session_id` (string, required)
  - `manifest_id` (string, required)
  - `run_mode` (enum: `dry_run`, `execute`)
  - `evidence_level` (enum: `summary`, `full`; default `summary`)
- Response `data` fields:
  - `harness_session_id` (string)
  - `session_id` (string)
  - `manifest_id` (string)
  - `state` (enum: `initialized`, `ready`, `running`, `completed`, `failed`)
  - `created_at_us` (uint64)
  - `checks_total` (uint32)
  - `checks_enabled` (uint32)

Test manifest loader contract:

- `POST /api/v2/conformance/manifests/load`
- Request fields:
  - `manifest_id` (string, required)
  - `manifest` (object, required)
- Required manifest schema fields (`conformance_manifest_v1`):
  - `manifest_version` (uint32, must equal `1`)
  - `target_machine` (string)
  - `profile` (string)
  - `suite` (string)
  - `cases` (array, length `>=1`)
- Required `cases[]` fields:
  - `case_id` (string)
  - `kind` (enum: `api`, `stream`)
  - `target` (string)
  - `assertions` (array, length `>=1`)

Harness/manifest deterministic guard failures:

- Invalid payload shape, unknown `run_mode`/`evidence_level`, or invalid manifest schema -> `BAD_REQUEST`.
- Unknown/inactive engine session for harness session creation -> `ENGINE_NOT_RUNNING`.
- Missing target profile binding for manifest (`target_machine`/`profile`) -> `MACHINE_PROFILE_NOT_FOUND`.
- Attempt to start harness with unresolved loader errors -> `INVALID_SESSION_STATE`.

Conformance harness create example:

```json
{
  "session_id": "ses_01H...",
  "manifest_id": "st_core_runtime_v1",
  "run_mode": "dry_run",
  "evidence_level": "summary"
}
```

Conformance harness create response (`data`):

```json
{
  "harness_session_id": "chs_01H...",
  "session_id": "ses_01H...",
  "manifest_id": "st_core_runtime_v1",
  "state": "initialized",
  "created_at_us": 1710000020000,
  "checks_total": 48,
  "checks_enabled": 48
}
```

Manifest load example:

```json
{
  "manifest_id": "st_core_runtime_v1",
  "manifest": {
    "manifest_version": 1,
    "target_machine": "atari_st",
    "profile": "st_520_pal",
    "suite": "core_runtime",
    "cases": [
      {
        "case_id": "clock_step_contract",
        "kind": "api",
        "target": "/api/v2/debug/clock/step",
        "assertions": ["status.ok", "tick_counter_monotonic"]
      }
    ]
  }
}
```

Evidence artifact collection contract:

- `POST /api/v2/conformance/harness/evidence/collect`
- Request fields:
  - `harness_session_id` (string, required)
  - `artifact_types` (array of enum: `logs`, `metrics`, `stream_samples`, `snapshots`, required, length `>=1`)
  - `window` (object, optional):
    - `start_event_seq` (uint64)
    - `end_event_seq` (uint64)
- Response `data` fields:
  - `collection_id` (string)
  - `harness_session_id` (string)
  - `state` (enum: `queued`, `collecting`, `completed`, `failed`)
  - `artifacts` (array)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Report packaging flow contract:

- `POST /api/v2/conformance/harness/report/package`
- Request fields:
  - `harness_session_id` (string, required)
  - `collection_id` (string, required)
  - `format` (enum: `json`, `zip`, required)
- Response `data` fields:
  - `package_id` (string)
  - `harness_session_id` (string)
  - `collection_id` (string)
  - `state` (enum: `building`, `ready`, `failed`)
  - `report_uri` (string or `null`)
  - `sha256` (string or `null`)

Evidence/report deterministic guard failures:

- Invalid payload shape, empty `artifact_types`, or invalid window/order (`start_event_seq > end_event_seq`) -> `BAD_REQUEST`.
- Unknown harness session or collection identifiers -> `NOT_FOUND`.
- Artifact collection requested when harness session is not executable/completed (`initialized`, `failed`) -> `INVALID_SESSION_STATE`.
- Report packaging requested before collection reaches `completed` -> `INVALID_SESSION_STATE`.

Evidence collection request example:

```json
{
  "harness_session_id": "chs_01H...",
  "artifact_types": ["logs", "metrics", "stream_samples"],
  "window": {
    "start_event_seq": 1000,
    "end_event_seq": 2053
  }
}
```

Evidence collection response (`data`) example:

```json
{
  "collection_id": "col_01H...",
  "harness_session_id": "chs_01H...",
  "state": "completed",
  "artifacts": [
    {"type": "logs", "uri": "sdcard/conformance/chs_01H/logs.ndjson", "bytes": 120334},
    {"type": "metrics", "uri": "sdcard/conformance/chs_01H/metrics.json", "bytes": 4421}
  ],
  "started_at_us": 1710000021000,
  "completed_at_us": 1710000022333
}
```

Report package request example:

```json
{
  "harness_session_id": "chs_01H...",
  "collection_id": "col_01H...",
  "format": "zip"
}
```

Report package response (`data`) example:

```json
{
  "package_id": "pkg_01H...",
  "harness_session_id": "chs_01H...",
  "collection_id": "col_01H...",
  "state": "ready",
  "report_uri": "sdcard/conformance/chs_01H/report_pkg_01H.zip",
  "sha256": "9f2f52be6f7d11f8f0a6f7bc62bf4b6c6e5ac6f4c91dc2a82bf32f4c0c55f2a1"
}
```

Acceptance checklist execution runner contract:

- `POST /api/v2/conformance/harness/checklist/run`
- Request fields:
  - `harness_session_id` (string, required)
  - `checklist_id` (string, required)
  - `selection` (object, optional):
    - `include_case_ids` (array of strings)
    - `exclude_case_ids` (array of strings)
  - `stop_on_failure` (bool, default `false`)
- Response `data` fields:
  - `runner_id` (string)
  - `harness_session_id` (string)
  - `checklist_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `cases_total` (uint32)
  - `cases_passed` (uint32)
  - `cases_failed` (uint32)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Runner status and progress contract:

- `GET /api/v2/conformance/harness/checklist/run/status?runner_id=run_01H...`
- Response `data` fields:
  - `runner_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `current_case_id` (string or `null`)
  - `progress` (object):
    - `completed_cases` (uint32)
    - `total_cases` (uint32)
  - `last_transition_at_us` (uint64)

Checklist runner deterministic guard failures:

- Invalid payload shape or contradictory selection (`include_case_ids` and `exclude_case_ids` intersect) -> `BAD_REQUEST`.
- Unknown harness session, checklist, or runner identifier -> `NOT_FOUND`.
- Runner start requested before report package state is `ready` -> `INVALID_SESSION_STATE`.
- Concurrent runner already active for same `harness_session_id` + `checklist_id` -> `CONFLICT`.

Checklist runner request example:

```json
{
  "harness_session_id": "chs_01H...",
  "checklist_id": "st_acceptance_v1",
  "selection": {
    "include_case_ids": ["clock_step_contract", "stream_backpressure_contract"],
    "exclude_case_ids": []
  },
  "stop_on_failure": true
}
```

Checklist runner response (`data`) example:

```json
{
  "runner_id": "run_01H...",
  "harness_session_id": "chs_01H...",
  "checklist_id": "st_acceptance_v1",
  "state": "running",
  "cases_total": 2,
  "cases_passed": 0,
  "cases_failed": 0,
  "started_at_us": 1710000023000,
  "completed_at_us": null
}
```

Checklist runner status response (`data`) example:

```json
{
  "runner_id": "run_01H...",
  "state": "completed",
  "current_case_id": null,
  "progress": {
    "completed_cases": 2,
    "total_cases": 2
  },
  "last_transition_at_us": 1710000024555
}
```

Review pack generation contract:

- `POST /api/v2/conformance/harness/review-pack/generate`
- Request fields:
  - `harness_session_id` (string, required)
  - `runner_id` (string, required)
  - `package_id` (string, required)
  - `include_sections` (array of enum: `summary`, `failures`, `artifacts`, `telemetry`, `checklist`; required, length `>=1`)
- Response `data` fields:
  - `review_pack_id` (string)
  - `state` (enum: `building`, `ready`, `failed`)
  - `harness_session_id` (string)
  - `runner_id` (string)
  - `generated_at_us` (uint64 or `null`)
  - `review_pack_uri` (string or `null`)

Signoff bundle assembly contract:

- `POST /api/v2/conformance/harness/signoff-bundle/assemble`
- Request fields:
  - `review_pack_id` (string, required)
  - `signoff` (object, required):
    - `requested_by` (string)
    - `approver` (string)
    - `label` (string)
- Response `data` fields:
  - `bundle_id` (string)
  - `review_pack_id` (string)
  - `state` (enum: `assembling`, `ready`, `failed`)
  - `bundle_uri` (string or `null`)
  - `sha256` (string or `null`)
  - `assembled_at_us` (uint64 or `null`)

Review-pack/signoff deterministic guard failures:

- Invalid payload shape or empty `include_sections` -> `BAD_REQUEST`.
- Unknown `harness_session_id`, `runner_id`, `package_id`, or `review_pack_id` -> `NOT_FOUND`.
- Review-pack generation requested before runner reaches terminal state (`completed`/`failed`/`aborted`) -> `INVALID_SESSION_STATE`.
- Signoff bundle assembly requested before review pack state is `ready` -> `INVALID_SESSION_STATE`.

Review pack request example:

```json
{
  "harness_session_id": "chs_01H...",
  "runner_id": "run_01H...",
  "package_id": "pkg_01H...",
  "include_sections": ["summary", "failures", "artifacts", "telemetry", "checklist"]
}
```

Review pack response (`data`) example:

```json
{
  "review_pack_id": "rvp_01H...",
  "state": "ready",
  "harness_session_id": "chs_01H...",
  "runner_id": "run_01H...",
  "generated_at_us": 1710000025600,
  "review_pack_uri": "sdcard/conformance/chs_01H/review_pack_rvp_01H.json"
}
```

Signoff bundle request example:

```json
{
  "review_pack_id": "rvp_01H...",
  "signoff": {
    "requested_by": "qa_lead",
    "approver": "product_owner",
    "label": "s4_acceptance"
  }
}
```

Signoff bundle response (`data`) example:

```json
{
  "bundle_id": "sgn_01H...",
  "review_pack_id": "rvp_01H...",
  "state": "ready",
  "bundle_uri": "sdcard/conformance/chs_01H/signoff_bundle_sgn_01H.zip",
  "sha256": "6b2ebf4f95df8a59f2ad8a5e622ecf9f53251b1a17e5dca35d43a83a420ff7d9",
  "assembled_at_us": 1710000025900
}
```

### 15.2 Subsystem conformance test scaffold and fixture model

Subsystem conformance scaffold contract:

- `POST /api/v2/conformance/subsystems/scaffold`
- Request fields:
  - `harness_session_id` (string, required)
  - `subsystem` (enum: `cpu`, `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`, required)
  - `fixture_profile` (string, required)
  - `seed_mode` (enum: `baseline`, `snapshot`, default `baseline`)
- Response `data` fields:
  - `scaffold_id` (string)
  - `harness_session_id` (string)
  - `subsystem` (string)
  - `state` (enum: `initialized`, `prepared`, `ready`, `failed`)
  - `fixture_model_id` (string)
  - `created_at_us` (uint64)

Subsystem fixture model contract:

- `GET /api/v2/conformance/subsystems/fixtures/model?scaffold_id=...`
- Response `data` must satisfy `conformance_fixture_model_v1`.
- `conformance_fixture_model_v1` required fields:
  - `fixture_model_id` (string)
  - `subsystem` (string)
  - `inputs` (array)
  - `expected_outputs` (array)
  - `invariants` (array of strings)
  - `tolerances` (object)
  - `schema_version` (uint32, must equal `1`)

Subsystem scaffold/fixture conformance checks:

- `CONF-FIX-01`: scaffold transitions must follow `initialized -> prepared -> ready` unless terminal `failed`.
- `CONF-FIX-02`: `fixture_model_id` must be stable for identical (`harness_session_id`, `subsystem`, `fixture_profile`, `seed_mode`) inputs.
- `CONF-FIX-03`: fixture model must include at least one `input`, one `expected_output`, and one `invariant`.
- `CONF-FIX-04`: scaffold/fixture timestamps are monotonic (`created_at_us <= prepared_at_us <= ready_at_us`) when those timestamps are present.

Subsystem scaffold/fixture deterministic guard failures:

- Invalid payload shape, unknown `subsystem`/`seed_mode`, or missing required fields -> `BAD_REQUEST`.
- Unknown harness session or invalid scaffold identifier -> `NOT_FOUND`.
- Scaffold request submitted before harness session reaches `ready` -> `INVALID_SESSION_STATE`.
- Fixture model fetch requested before scaffold state is `ready` -> `INVALID_SESSION_STATE`.

Subsystem scaffold request example:

```json
{
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "fixture_profile": "mfp_timer_irq_smoke",
  "seed_mode": "baseline"
}
```

Subsystem scaffold response (`data`) example:

```json
{
  "scaffold_id": "scf_01H...",
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "state": "ready",
  "fixture_model_id": "fxm_01H...",
  "created_at_us": 1710000026400,
  "prepared_at_us": 1710000026440,
  "ready_at_us": 1710000026488
}
```

Subsystem fixture model response (`data`) example:

```json
{
  "fixture_model_id": "fxm_01H...",
  "subsystem": "mfp",
  "inputs": [
    {
      "signal": "timer_a_start",
      "value": "0x01"
    }
  ],
  "expected_outputs": [
    {
      "signal": "irq6_assert",
      "within_ticks": 32
    }
  ],
  "invariants": ["event_seq_monotonic", "timestamp_us_monotonic"],
  "tolerances": {
    "tick_jitter_max": 2
  },
  "schema_version": 1
}
```

### 15.3 Per-subsystem acceptance suites and reporting output

Per-subsystem acceptance suite execution contract:

- `POST /api/v2/conformance/subsystems/suites/run`
- Request fields:
  - `harness_session_id` (string, required)
  - `scaffold_id` (string, required)
  - `suite_id` (string, required)
  - `selection` (object, optional):
    - `include_case_ids` (array of strings)
    - `exclude_case_ids` (array of strings)
  - `stop_on_failure` (bool, default `false`)
- Response `data` fields:
  - `suite_run_id` (string)
  - `harness_session_id` (string)
  - `subsystem` (string)
  - `suite_id` (string)
  - `state` (enum: `queued`, `running`, `completed`, `failed`, `aborted`)
  - `cases_total` (uint32)
  - `cases_passed` (uint32)
  - `cases_failed` (uint32)
  - `started_at_us` (uint64)
  - `completed_at_us` (uint64 or `null`)

Per-subsystem reporting output contract:

- `GET /api/v2/conformance/subsystems/suites/report?suite_run_id=...`
- Response `data` must satisfy `subsystem_suite_report_v1`.
- `subsystem_suite_report_v1` required fields:
  - `suite_run_id` (string)
  - `subsystem` (string)
  - `summary` (object):
    - `cases_total` (uint32)
    - `cases_passed` (uint32)
    - `cases_failed` (uint32)
    - `pass_rate` (number, `0..1`)
  - `case_results` (array)
  - `evidence_uris` (array of strings)
  - `generated_at_us` (uint64)

Per-subsystem suite/report conformance checks:

- `SUB-SUITE-01`: `cases_passed + cases_failed == cases_total` for terminal suite states.
- `SUB-SUITE-02`: report `pass_rate == cases_passed / cases_total` when `cases_total > 0`.
- `SUB-SUITE-03`: `generated_at_us >= completed_at_us` for completed/failed/aborted suite runs.
- `SUB-SUITE-04`: `case_results` ordering must be deterministic by (`case_id`, `assertion_id`) stable sort.

Per-subsystem suite/report deterministic guard failures:

- Invalid payload shape, contradictory selection, or invalid identifiers -> `BAD_REQUEST`.
- Unknown harness/scaffold/suite run identifiers -> `NOT_FOUND`.
- Suite run requested before scaffold state is `ready` -> `INVALID_SESSION_STATE`.
- Report requested before suite run reaches terminal state (`completed`, `failed`, `aborted`) -> `INVALID_SESSION_STATE`.

Per-subsystem suite run request example:

```json
{
  "harness_session_id": "chs_01H...",
  "scaffold_id": "scf_01H...",
  "suite_id": "mfp_acceptance_v1",
  "selection": {
    "include_case_ids": ["timer_a_irq", "vector_routing"],
    "exclude_case_ids": []
  },
  "stop_on_failure": true
}
```

Per-subsystem suite run response (`data`) example:

```json
{
  "suite_run_id": "ssr_01H...",
  "harness_session_id": "chs_01H...",
  "subsystem": "mfp",
  "suite_id": "mfp_acceptance_v1",
  "state": "completed",
  "cases_total": 2,
  "cases_passed": 2,
  "cases_failed": 0,
  "started_at_us": 1710000027000,
  "completed_at_us": 1710000027240
}
```

Per-subsystem suite report response (`data`) example:

```json
{
  "suite_run_id": "ssr_01H...",
  "subsystem": "mfp",
  "summary": {
    "cases_total": 2,
    "cases_passed": 2,
    "cases_failed": 0,
    "pass_rate": 1.0
  },
  "case_results": [
    {
      "case_id": "timer_a_irq",
      "assertion_id": "irq6_asserted",
      "result": "pass"
    }
  ],
  "evidence_uris": ["sdcard/conformance/chs_01H/suites/ssr_01H/mfp_report.json"],
  "generated_at_us": 1710000027260
}
```

---
